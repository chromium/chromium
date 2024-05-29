// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_blob_registry_delegate.h"
#include "base/functional/callback_helpers.h"

namespace storage {

MockBlobRegistryDelegate::MockBlobRegistryDelegate() = default;
MockBlobRegistryDelegate::~MockBlobRegistryDelegate() = default;

bool MockBlobRegistryDelegate::CanReadFile(const base::FilePath& file) {
  return can_read_file_result;
}

bool MockBlobRegistryDelegate::CanAccessDataForOrigin(
    const url::Origin& origin) {
  return can_access_data_for_origin;
}

}  // namespace storage
