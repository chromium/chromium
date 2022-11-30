// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_DATABASE_VFS_BACKEND_H_
#define STORAGE_BROWSER_DATABASE_VFS_BACKEND_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/process/process.h"

namespace base {
class FilePath;
}

namespace storage {

class COMPONENT_EXPORT(STORAGE_BROWSER) VfsBackend {
 public:
   static base::File OpenFile(const base::FilePath& file_path,
                              int desired_flags);

  static base::File OpenTempFileInDirectory(const base::FilePath& dir_path,
                                            int desired_flags);

  static int DeleteFile(const base::FilePath& file_path, bool sync_dir);

  static uint32_t GetFileAttributes(const base::FilePath& file_path);

  static bool SetFileSize(const base::FilePath& file_path, int64_t size);

  // Used to make decisions in the DatabaseDispatcherHost.
  static bool OpenTypeIsReadWrite(int desired_flags);

 private:
  static bool OpenFileFlagsAreConsistent(int desired_flags);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_DATABASE_VFS_BACKEND_H_
