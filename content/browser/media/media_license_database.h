// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_DATABASE_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_DATABASE_H_

#include "base/sequence_checker.h"
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

  bool OpenFile(const media::CdmType& cdm_type, const std::string& file_name);
  absl::optional<std::vector<uint8_t>> ReadFile(const media::CdmType& cdm_type,
                                                const std::string& file_name);
  bool WriteFile(const media::CdmType& cdm_type,
                 const std::string& file_name,
                 const std::vector<uint8_t>& data);
  bool DeleteFile(const media::CdmType& cdm_type, const std::string& file_name);

  bool ClearDatabase();

 private:
  // Opens and sets up a database if one is not already set up.
  bool OpenDatabase(bool is_retry = false);

  SEQUENCE_CHECKER(sequence_checker_);

  // Empty if the database is in-memory.
  const base::FilePath path_;

  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_LICENSE_DATABASE_H_
