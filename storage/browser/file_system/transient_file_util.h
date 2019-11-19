// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_TRANSIENT_FILE_UTIL_H_
#define STORAGE_BROWSER_FILE_SYSTEM_TRANSIENT_FILE_UTIL_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "storage/browser/file_system/local_file_util.h"

namespace storage {

class FileSystemOperationContext;

class COMPONENT_EXPORT(STORAGE_BROWSER) TransientFileUtil
    : public LocalFileUtil {
 public:
  TransientFileUtil() {}
  ~TransientFileUtil() override {}

  // LocalFileUtil overrides.
  storage::ScopedFile CreateSnapshotFile(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      base::File::Error* error,
      base::File::Info* file_info,
      base::FilePath* platform_path) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TransientFileUtil);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_TRANSIENT_FILE_UTIL_H_
