// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_OS_REGISTRATIONS_TABLE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_OS_REGISTRATIONS_TABLE_H_

#include <set>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/storage_partition.h"

namespace sql {
class Database;
}  // namespace sql

namespace url {
class Origin;
}  // namespace url

namespace content {

class AttributionResolverDelegate;

class CONTENT_EXPORT OsRegistrationsTable {
 public:
  explicit OsRegistrationsTable(const AttributionResolverDelegate*);
  OsRegistrationsTable(const OsRegistrationsTable&) = delete;
  OsRegistrationsTable& operator=(const OsRegistrationsTable&) = delete;
  OsRegistrationsTable(OsRegistrationsTable&&) = delete;
  OsRegistrationsTable& operator=(OsRegistrationsTable&&) = delete;
  ~OsRegistrationsTable();

  [[nodiscard]] bool CreateTable(sql::Database*);

  void AddOsRegistrations(sql::Database*, const base::flat_set<url::Origin>&);

  void ClearAllDataAllTime(sql::Database* db);
  void ClearDataForOriginsInRange(
      sql::Database* db,
      base::Time delete_begin,
      base::Time delete_end,
      StoragePartition::StorageKeyMatcherFunction filter);
  void ClearDataForRegistrationOrigin(sql::Database* db,
                                      base::Time delete_begin,
                                      base::Time delete_end,
                                      const url::Origin&);

  void AppendOsRegistrationDataKeys(
      sql::Database* db,
      std::set<AttributionDataModel::DataKey>& keys);

  void SetDelegate(const AttributionResolverDelegate&);

 private:
  void ClearAllDataInRange(sql::Database* db,
                           base::Time delete_begin,
                           base::Time delete_end)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  [[nodiscard]] bool DeleteExpiredOsRegistrations(sql::Database* db)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  raw_ref<const AttributionResolverDelegate> delegate_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::Time last_cleared_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_OS_REGISTRATIONS_TABLE_H_
