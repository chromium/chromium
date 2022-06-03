// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_blob_registry_delegate.h"

namespace storage {

bool MockBlobRegistryDelegate::CanReadFile(const base::FilePath& file) {
  return can_read_file_result;
}
bool MockBlobRegistryDelegate::CanReadFileSystemFile(const FileSystemURL& url) {
  return can_read_file_system_file_result;
}
bool MockBlobRegistryDelegate::CanCommitURL(const GURL& url) {
  return can_commit_url_result;
}

}  // namespace storage
