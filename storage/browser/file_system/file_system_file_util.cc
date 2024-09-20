// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "storage/browser/file_system/file_system_file_util.h"

namespace storage {

// TODO(b/329523214): remove this placeholder implementation.
base::File::Error FileSystemFileUtil::AbstractFileEnumerator::GetError() {
  return base::File::FILE_OK;
}

base::FilePath FileSystemFileUtil::EmptyFileEnumerator::Next() {
  return base::FilePath();
}

base::File::Error FileSystemFileUtil::EmptyFileEnumerator::GetError() {
  return base::File::FILE_OK;
}

base::FilePath FileSystemFileUtil::EmptyFileEnumerator::GetName() {
  return base::FilePath();
}

int64_t FileSystemFileUtil::EmptyFileEnumerator::Size() {
  return 0;
}

base::Time FileSystemFileUtil::EmptyFileEnumerator::LastModifiedTime() {
  return base::Time();
}

bool FileSystemFileUtil::EmptyFileEnumerator::IsDirectory() {
  return false;
}

}  // namespace storage
