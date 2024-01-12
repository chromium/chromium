// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_DATABASE_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_DATABASE_H_

#include "base/sequence_checker.h"
#include "content/browser/media/media_license_storage_host.h"
#include "media/cdm/cdm_type.h"
#include "sql/database.h"

namespace content {

// Helper class which encapsulates all database logic for storing media license
// data.
//
// This class must be constructed and used on a sequence which allows blocking.
class MediaLicenseDatabase {
 public:
  // The database will be in-memory if `path` is empty.
  explicit MediaLicenseDatabase(const base::FilePath& path);
  ~MediaLicenseDatabase();

  MediaLicenseStorageHost::MediaLicenseStorageHostOpenError OpenFile(
      const media::CdmType& cdm_type,
      const std::string& file_name);
  std::optional<std::vector<uint8_t>> ReadFile(const media::CdmType& cdm_type,
                                               const std::string& file_name);
  bool WriteFile(const media::CdmType& cdm_type,
                 const std::string& file_name,
                 const std::vector<uint8_t>& data);
  bool DeleteFile(const media::CdmType& cdm_type, const std::string& file_name);

  bool ClearDatabase();

  uint64_t GetDatabaseSize();

 private:
  // Opens and sets up a database if one is not already set up.
  MediaLicenseStorageHost::MediaLicenseStorageHostOpenError OpenDatabase(
      bool is_retry = false);

  void OnDatabaseError(int error, sql::Statement* stmt);

  SEQUENCE_CHECKER(sequence_checker_);

  // Empty if the database is in-memory.
  const base::FilePath path_;

  // A descriptor of the last SQL statement that was executed, used for metrics.
  std::optional<std::string> last_operation_;

  // Integer of last file size that the CDM sent to be written, used for
  // metrics.
  std::optional<int> last_write_file_size_;

  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_DATABASE_H_
