/* Copyright (c) 2011-2017, AOYAMA Kazuharu
 * All rights reserved.
 *
 * This software may be used and distributed according to the terms of
 * the New BSD License, which is incorporated herein by reference.
 */

#include <TSqlTransaction>
#include <TWebApplication>
#include <TSystemGlobal>
#include <QSqlDriver>
#include "tsqldatabasepool.h"

/*!
  \class TSqlTransaction
  \brief The TSqlTransaction class provides a transaction of database.
*/


TSqlTransaction::TSqlTransaction()
{ }


TSqlTransaction::TSqlTransaction(const TSqlTransaction &other) :
    _enabled(other._enabled),
    _database(other._database),
    _active(other._active)
{ }


TSqlTransaction::~TSqlTransaction()
{
    rollback();
}


TSqlTransaction &TSqlTransaction::operator=(const TSqlTransaction &other)
{
    _enabled = other._enabled;
    _database = other._database;
    _active = other._active;
    return *this;
}


bool TSqlTransaction::begin(QSqlDatabase &database)
{
    if (Q_UNLIKELY(!database.isValid())) {
        tSystemError("Can not begin transaction. Invalid database: %s", qPrintable(database.connectionName()));
        return false;
    }

    if (_database.isValid() && _database.connectionName() != database.connectionName()) {
        tSystemError("Another database set already: %s", qPrintable(database.connectionName()));
        return false;
    }

    if (!_enabled) {
        _database = database;
        return true;
    }

    if (_active) {
        tSystemDebug("Has begun transaction already. database:%s", qPrintable(database.connectionName()));
        return true;
    }

    if (database.driver()->hasFeature(QSqlDriver::Transactions)) {
        int i = 0;
        do {
            _active = database.transaction();
            if (Q_LIKELY(_active)) {
                break;
            }
            database.rollback();
        } while (++i < 2); // try two times

        int id = TSqlDatabasePool::getDatabaseId(_database);
        if (Q_LIKELY(_active)) {
            Tf::traceQueryLog("[BEGIN] [databaseId:%d] %s", id, qPrintable(_database.connectionName()));
        } else {
            Tf::traceQueryLog("[BEGIN Failed] [databaseId:%d] %s", id, qPrintable(_database.connectionName()));
        }
    }

    _database = database;
    return _active;
}


bool TSqlTransaction::rebegin()
{
    return begin(_database);
}


bool TSqlTransaction::commit()
{
    bool res = false;

    if (_active && _database.isValid()) {
        res = _database.commit();

        int id = TSqlDatabasePool::getDatabaseId(_database);
        if (Q_LIKELY(res)) {
            Tf::traceQueryLog("[COMMIT] [databaseId:%d]", id);
        } else {
            Tf::traceQueryLog("[COMMIT Failed] [databaseId:%d]", id);
        }
    }

    _active = false;
    return res;
}


bool TSqlTransaction::rollback()
{
    bool res = false;

    if (_active && _database.isValid()) {
        res = _database.rollback();

        int id = TSqlDatabasePool::getDatabaseId(_database);
        if (Q_LIKELY(res)) {
            Tf::traceQueryLog("[ROLLBACK] [databaseId:%d]", id);
        } else {
            Tf::traceQueryLog("[ROLLBACK Failed] [databaseId:%d]", id);
        }
    }

    _active = false;
    return res;
}
