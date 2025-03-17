// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/os_registrations_table.h"

#include <set>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/attribution_reporting/constants.h"
#include "content/browser/attribution_reporting/attribution_resolver_delegate.h"
#include "content/browser/attribution_reporting/sql_queries.h"
#include "content/browser/attribution_reporting/sql_utils.h"
#include "content/public/browser/storage_partition.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "sql/transaction.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace {
// Conservatively keep OS registration data for a buffer window of 15 days
// beyond max source expiry.
constexpr base::TimeDelta kOsRegistrationsExpiry =
    attribution_reporting::kMaxSourceExpiry + base::Days(15);
}  // namespace

namespace content {

OsRegistrationsTable::OsRegistrationsTable(
    const AttributionResolverDelegate* delegate)
    : delegate_(
          raw_ref<const AttributionResolverDelegate>::from_ptr(delegate)) {}

OsRegistrationsTable::~OsRegistrationsTable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool OsRegistrationsTable::CreateTable(sql::Database* db) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    return false;
  }

  static constexpr char kOsRegistrationsTableSql[] =
      "CREATE TABLE os_registrations("
      "registration_origin TEXT NOT NULL,"
      "time INTEGER NOT NULL,"
      "PRIMARY KEY(registration_origin,time))WITHOUT ROWID";
  if (!db->Execute(kOsRegistrationsTableSql)) {
    return false;
  }

  static constexpr char kOsRegistrationsTimeIndexSql[] =
      "CREATE INDEX os_registrations_time_idx "
      "ON os_registrations(time)";
  if (!db->Execute(kOsRegistrationsTimeIndexSql)) {
    return false;
  }

  return transaction.Commit();
}

void OsRegistrationsTable::AddOsRegistrations(
    sql::Database* db,
    const base::flat_set<url::Origin>& origins) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Only delete expired OS registrations periodically to avoid excessive DB
  // operations.
  const base::TimeDelta delete_frequency =
      delegate_->GetDeleteExpiredOsRegistrationsFrequency();
  CHECK_GE(delete_frequency, base::TimeDelta());
  const base::Time now = base::Time::Now();
  if (now - last_cleared_ >= delete_frequency &&
      DeleteExpiredOsRegistrations(db)) {
    last_cleared_ = now;
  }

  // The primary key is (registration_origin,time), therefore adding `OR IGNORE`
  // to avoid the constraint error on duplicate primary key in the edge case of
  // concurrent registrations.
  static constexpr char kInsertOsRegistrationsSql[] =
      "INSERT OR IGNORE INTO os_registrations(registration_origin,time)"
      "VALUES(?,?)";
  sql::Statement insert_os_registration_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kInsertOsRegistrationsSql));
  insert_os_registration_statement.BindTime(1, now);
  for (const url::Origin& origin : origins) {
    insert_os_registration_statement.Reset(/*clear_bound_vars=*/false);
    insert_os_registration_statement.BindString(0, origin.Serialize());
    insert_os_registration_statement.Run();
  }
}

bool OsRegistrationsTable::DeleteExpiredOsRegistrations(sql::Database* db) {
  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kDeleteExpiredOsRegistrationsSql));
  statement.BindTime(0, base::Time::Now() - kOsRegistrationsExpiry);
  return statement.Run();
}

void OsRegistrationsTable::ClearAllDataAllTime(sql::Database* db) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static constexpr char kDeleteAllOsRegistrations[] =
      "DELETE FROM os_registrations";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteAllOsRegistrations));
  statement.Run();
}

void OsRegistrationsTable::ClearDataForOriginsInRange(
    sql::Database* db,
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (filter.is_null()) {
    ClearAllDataInRange(db, delete_begin, delete_end);
    return;
  }

  sql::Statement delete_statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kDeleteOsRegistrationAtTimeSql));

  sql::Statement select_statement(db->GetCachedStatement(
      SQL_FROM_HERE,
      attribution_queries::kSelectOsRegistrationsForDeletionSql));
  select_statement.BindTime(0, delete_begin);
  select_statement.BindTime(1, delete_end);

  while (select_statement.Step()) {
    std::string_view registration_origin = select_statement.ColumnStringView(0);
    if (filter.Run(blink::StorageKey::CreateFirstParty(
            DeserializeOrigin(registration_origin)))) {
      // See https://www.sqlite.org/isolation.html for why it's OK for this
      // DELETE to be interleaved in the surrounding SELECT.
      delete_statement.Reset(/*clear_bound_vars=*/false);
      delete_statement.BindString(0, registration_origin);
      delete_statement.BindTime(1, select_statement.ColumnTime(1));
      delete_statement.Run();
    }
  }
}

void OsRegistrationsTable::ClearAllDataInRange(sql::Database* db,
                                               base::Time delete_begin,
                                               base::Time delete_end) {
  CHECK(!((delete_begin.is_null() || delete_begin.is_min()) &&
          delete_end.is_max()));

  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kDeleteOsRegistrationsRangeSql));
  statement.BindTime(0, delete_begin);
  statement.BindTime(1, delete_end);
  statement.Run();
}

void OsRegistrationsTable::ClearDataForRegistrationOrigin(
    sql::Database* db,
    base::Time delete_begin,
    base::Time delete_end,
    const url::Origin& registration_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kDeleteOsRegistrationSql));
  statement.BindString(0, registration_origin.Serialize());
  statement.BindTime(1, delete_begin);
  statement.BindTime(2, delete_end);
  statement.Run();
}

void OsRegistrationsTable::AppendOsRegistrationDataKeys(
    sql::Database* db,
    std::set<AttributionDataModel::DataKey>& keys) {
  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetOsRegistrationDataKeysSql));

  while (statement.Step()) {
    url::Origin registration_origin =
        DeserializeOrigin(statement.ColumnStringView(0));
    if (registration_origin.opaque()) {
      continue;
    }
    keys.emplace(std::move(registration_origin));
  }
}

void OsRegistrationsTable::SetDelegate(
    const AttributionResolverDelegate& delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_ = delegate;
}

}  // namespace content
