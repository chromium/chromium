// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_STORAGE_DATABASE_H_
#define CONTENT_BROWSER_MEDIA_CDM_STORAGE_DATABASE_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/sequence_checker.h"
#include "content/browser/media/cdm_storage_common.h"
#include "content/common/content_export.h"
#include "content/public/browser/cdm_storage_data_model.h"
#include "content/public/browser/storage_partition.h"
#include "media/cdm/cdm_type.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

// Helper class which encapsulates all database logic for storing cdm data.
//
// This class must be constructed and used on a sequence which allows blocking.
class CONTENT_EXPORT CdmStorageDatabase {
 public:
  // The database will be in-memory if `path` is empty.
  explicit CdmStorageDatabase(const base::FilePath& path);
  ~CdmStorageDatabase();

  CdmStorageOpenError EnsureOpen();

  std::optional<std::vector<uint8_t>> ReadFile(
      const blink::StorageKey& storage_key,
      const media::CdmType& cdm_type,
      const std::string& file_name);

  bool WriteFile(const blink::StorageKey& storage_key,
                 const media::CdmType& cdm_type,
                 const std::string& file_name,
                 const std::vector<uint8_t>& data);

  std::optional<uint64_t> GetSizeForFile(const blink::StorageKey& storage_key,
                                         const media::CdmType& cdm_type,
                                         const std::string& file_name);

  std::optional<uint64_t> GetSizeForStorageKey(
      const blink::StorageKey& storage_key,
      const base::Time begin = base::Time::Min(),
      const base::Time end = base::Time::Max());

  std::optional<uint64_t> GetSizeForTimeFrame(const base::Time begin,
                                              const base::Time end);

  CdmStorageKeyUsageSize GetUsagePerAllStorageKeys(
      const base::Time begin = base::Time::Min(),
      const base::Time end = base::Time::Max());

  bool DeleteFile(const blink::StorageKey& storage_key,
                  const media::CdmType& cdm_type,
                  const std::string& file_name);

  bool DeleteData(
      const StoragePartition::StorageKeyMatcherFunction& storage_key_matcher,
      const blink::StorageKey& storage_key,
      const base::Time begin = base::Time::Min(),
      const base::Time end = base::Time::Max());

  bool ClearDatabase();

  uint64_t GetDatabaseSize();

  void CloseDatabaseForTesting();

 private:
  bool DeleteDataForFilter(
      StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
      const base::Time begin,
      const base::Time end);

  bool DeleteDataForStorageKey(const blink::StorageKey& storage_key,
                               const base::Time begin,
                               const base::Time end);

  bool DeleteDataForTimeFrame(const base::Time begin, const base::Time end);

  // On a delete operation, check if database is empty. If empty, then clear the
  // database.
  bool DeleteIfEmptyDatabase(bool last_operation_success);

  // Opens and sets up a database if one is not already set up.
  CdmStorageOpenError OpenDatabase(bool is_retry = false);

  bool UpgradeDatabaseSchema(sql::MetaTable* meta_table);

  void OnDatabaseError(int error, sql::Statement* stmt);

  bool in_memory() const { return path_.empty(); }

  SEQUENCE_CHECKER(sequence_checker_);

  // Empty if the database is in-memory.
  const base::FilePath path_;

  // A descriptor of the last SQL statement that was executed, used for metrics.
  std::optional<std::string> last_operation_;

  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_STORAGE_DATABASE_H_
