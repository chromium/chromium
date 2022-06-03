// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_MOCK_BLOB_REGISTRY_DELEGATE_H_
#define STORAGE_BROWSER_TEST_MOCK_BLOB_REGISTRY_DELEGATE_H_

#include "storage/browser/blob/blob_registry_impl.h"

namespace storage {

class MockBlobRegistryDelegate : public BlobRegistryImpl::Delegate {
 public:
  MockBlobRegistryDelegate() = default;
  ~MockBlobRegistryDelegate() override = default;

  bool CanReadFile(const base::FilePath& file) override;
  bool CanReadFileSystemFile(const FileSystemURL& url) override;
  bool CanCommitURL(const GURL& url) override;

  bool can_read_file_result = true;
  bool can_read_file_system_file_result = true;
  bool can_commit_url_result = true;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_MOCK_BLOB_REGISTRY_DELEGATE_H_
