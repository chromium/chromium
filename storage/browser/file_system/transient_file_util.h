// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_TRANSIENT_FILE_UTIL_H_
#define STORAGE_BROWSER_FILE_SYSTEM_TRANSIENT_FILE_UTIL_H_

#include "base/component_export.h"
#include "storage/browser/file_system/local_file_util.h"

namespace storage {

class FileSystemOperationContext;

class COMPONENT_EXPORT(STORAGE_BROWSER) TransientFileUtil
    : public LocalFileUtil {
 public:
  TransientFileUtil() = default;

  TransientFileUtil(const TransientFileUtil&) = delete;
  TransientFileUtil& operator=(const TransientFileUtil&) = delete;

  ~TransientFileUtil() override = default;

  // LocalFileUtil overrides.
  ScopedFile CreateSnapshotFile(FileSystemOperationContext* context,
                                const FileSystemURL& url,
                                base::File::Error* error,
                                base::File::Info* file_info,
                                base::FilePath* platform_path) override;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_TRANSIENT_FILE_UTIL_H_
