// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_system_request_info.h"

#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace storage {

FileSystemRequestInfo::FileSystemRequestInfo(
    const GURL& url,
    const std::string& storage_domain,
    int content_id,
    const blink::StorageKey& storage_key)
    : url(url),
      storage_domain(storage_domain),
      content_id(content_id),
      storage_key(storage_key) {}

}  // namespace storage
