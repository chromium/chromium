// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_STORAGE_COMMON_H_
#define CONTENT_BROWSER_MEDIA_CDM_STORAGE_COMMON_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "media/cdm/cdm_type.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CdmStorageOpenError {
  kOk = -1,
  kNoFileSpecified = 0,    // No file was specified.
  kInvalidFileName = 1,    // File name specified was invalid.
  kDatabaseOpenError = 2,  // Error occurred at the Database level.
  kDatabaseRazeError = 3,  // The database was in an invalid state and failed
                           // to be razed.
  kSQLExecutionError = 4,  // Error executing the SQL statement.
  kAlterTableError = 5,    // Error altering cdm_storage table.
  kMaxValue = kAlterTableError
};

// The file name of the database storing cdm storage data.
const base::FilePath::CharType kCdmStorageDatabaseFileName[] =
    FILE_PATH_LITERAL("CdmStorage.db");

// CdmStorage provides per-storage key, per-CDM type storage.
struct CONTENT_EXPORT CdmStorageBindingContext {
  CdmStorageBindingContext(const blink::StorageKey& storage_key,
                           const media::CdmType& cdm_type)
      : storage_key(storage_key), cdm_type(cdm_type) {}

  const blink::StorageKey storage_key;
  const media::CdmType cdm_type;
};

// A CDM file for a given storage key can be uniquely identified by its name
// and CDM type.
struct CONTENT_EXPORT CdmFileId {
  CdmFileId(const std::string& name, const media::CdmType& cdm_type);
  CdmFileId(const CdmFileId&);
  ~CdmFileId();

  bool operator==(const CdmFileId& rhs) const {
    return (name == rhs.name) && (cdm_type == rhs.cdm_type);
  }
  bool operator<(const CdmFileId& rhs) const {
    return std::tie(name, cdm_type) < std::tie(rhs.name, rhs.cdm_type);
  }

  const std::string name;
  const media::CdmType cdm_type;
};

// As above.
// Only used in the CdmStorage implementation, remove `Two` from name once
// MediaLicense* code is removed.
struct CONTENT_EXPORT CdmFileIdTwo {
  CdmFileIdTwo(const std::string& name,
               const media::CdmType& cdm_type,
               const blink::StorageKey& storage_key);
  CdmFileIdTwo(const CdmFileIdTwo&);
  ~CdmFileIdTwo();

  bool operator==(const CdmFileIdTwo& rhs) const {
    return (name == rhs.name) && (cdm_type == rhs.cdm_type) &&
           (storage_key == rhs.storage_key);
  }
  bool operator<(const CdmFileIdTwo& rhs) const {
    return std::tie(name, cdm_type, storage_key) <
           std::tie(rhs.name, rhs.cdm_type, rhs.storage_key);
  }

  const std::string name;
  const media::CdmType cdm_type;
  const blink::StorageKey storage_key;
};

struct CONTENT_EXPORT CdmFileIdAndContents {
  CdmFileIdAndContents(const CdmFileId& file, std::vector<uint8_t> data);
  CdmFileIdAndContents(const CdmFileIdAndContents&);
  ~CdmFileIdAndContents();

  const CdmFileId file;
  const std::vector<uint8_t> data;
};

// Called in CdmStorageDatabase and CdmStorageManager to get
// CdmStorageManager* metric names.
std::string GetCdmStorageManagerHistogramName(const std::string& operation,
                                              bool in_memory);

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_STORAGE_COMMON_H_
