// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_STORAGE_DATABASE_H_
#define CONTENT_BROWSER_MEDIA_CDM_STORAGE_DATABASE_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/sequence_checker.h"
#include "content/browser/media/cdm_storage_common.h"
#include "content/common/content_export.h"
#include "media/cdm/cdm_type.h"
#include "sql/database.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

  absl::optional<std::vector<uint8_t>> ReadFile(
      const blink::StorageKey& storage_key,
      const media::CdmType& cdm_type,
      const std::string& file_name);

  bool WriteFile(const blink::StorageKey& storage_key,
                 const media::CdmType& cdm_type,
                 const std::string& file_name,
                 const std::vector<uint8_t>& data);

  bool DeleteFile(const blink::StorageKey& storage_key,
                  const media::CdmType& cdm_type,
                  const std::string& file_name);

  bool DeleteDataForStorageKey(const blink::StorageKey& storage_key);

  bool ClearDatabase();

 private:
  // Opens and sets up a database if one is not already set up.
  CdmStorageOpenError OpenDatabase(bool is_retry = false);

  void OnDatabaseError(int error, sql::Statement* stmt);

  SEQUENCE_CHECKER(sequence_checker_);

  // Empty if the database is in-memory.
  const base::FilePath path_;

  // A descriptor of the last SQL statement that was executed, used for metrics.
  absl::optional<std::string> last_operation_;

  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_STORAGE_DATABASE_H_
