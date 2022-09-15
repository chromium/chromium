// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_ERROR_DELEGATE_UTIL_H_
#define SQL_ERROR_DELEGATE_UTIL_H_

#include <string>

#include "base/component_export.h"

namespace base {
class FilePath;
}  // namespace base

namespace sql {

// Returns true if `sqlite_error_code` is caused by database corruption.
//
// This method returns true for SQLite error codes that can only be explained by
// some form of corruption in the database's on-disk state. Retrying the failed
// operation will result in the same error, until the on-disk state changes.
// Callers should react to a true return value by deleting the database and
// starting over, or by attempting to recover the data.
//
// Corruption is most often associated with storage media decay (aged disks),
// but it can also be caused by bugs in the storage stack we're using (Chrome,
// SQLite, the filesystem, the OS disk driver or disk firmware), or by some
// other software that modifies the user's Chrome profile.
COMPONENT_EXPORT(SQL) bool IsErrorCatastrophic(int sqlite_error_code);

// Gets diagnostic info of the given |corrupted_file_path| that can be appended
// to a corrupt database diagnostics info. The file info are not localized as
// it's meant to be added to feedback reports and used by developers.
// Also the full file path is not appended as it might contain some PII. Instead
// only the last two components of the path are appended to distinguish between
// default and user profiles.
COMPONENT_EXPORT(SQL)
std::string GetCorruptFileDiagnosticsInfo(
    const base::FilePath& corrupted_file_path);

}  // namespace sql

#endif  // SQL_ERROR_DELEGATE_UTIL_H_
