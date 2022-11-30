// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_DATABASE_DATABASE_UTIL_H_
#define STORAGE_BROWSER_DATABASE_DATABASE_UTIL_H_

#include <string>

#include "base/component_export.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace storage {

class DatabaseTracker;

class COMPONENT_EXPORT(STORAGE_BROWSER) DatabaseUtil {
 public:
  static const char kJournalFileSuffix[];

  // Extract various information from a database vfs_file_name.  All return
  // parameters are optional.
  static bool CrackVfsFileName(const std::u16string& vfs_file_name,
                               std::string* origin_identifier,
                               std::u16string* database_name,
                               std::u16string* sqlite_suffix);
  static base::FilePath GetFullFilePathForVfsFile(
      DatabaseTracker* db_tracker,
      const std::u16string& vfs_file_name);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_DATABASE_DATABASE_UTIL_H_
