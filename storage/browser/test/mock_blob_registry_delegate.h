// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_MOCK_BLOB_REGISTRY_DELEGATE_H_
#define STORAGE_BROWSER_TEST_MOCK_BLOB_REGISTRY_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "storage/browser/blob/blob_registry_impl.h"

namespace storage {

class MockBlobRegistryDelegate
    : public BlobRegistryImpl::Delegate,
      public base::SupportsWeakPtr<MockBlobRegistryDelegate> {
 public:
  MockBlobRegistryDelegate() = default;
  ~MockBlobRegistryDelegate() override = default;

  bool CanReadFile(const base::FilePath& file) override;
  bool CanAccessDataForOrigin(const url::Origin& origin) override;

  bool can_read_file_result = true;
  bool can_access_data_for_origin = true;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_MOCK_BLOB_REGISTRY_DELEGATE_H_
