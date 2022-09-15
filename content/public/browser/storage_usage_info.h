// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_STORAGE_USAGE_INFO_H_
#define CONTENT_PUBLIC_BROWSER_STORAGE_USAGE_INFO_H_

#include <stdint.h>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {

// Used to report per-origin storage info for a storage type. The storage type
// (Cache API, Indexed DB, Local Storage, etc) is implied by context.
struct CONTENT_EXPORT StorageUsageInfo {
  StorageUsageInfo(const blink::StorageKey& storage_key,
                   int64_t total_size_bytes,
                   base::Time last_modified)
      : storage_key(storage_key),
        origin(storage_key.origin()),
        total_size_bytes(total_size_bytes),
        last_modified(last_modified) {}

  // DEPRECATED: Use the above StorageKey constructor
  // (https://crbug.com/1361545).
  StorageUsageInfo(const url::Origin& origin,
                   int64_t total_size_bytes,
                   base::Time last_modified)
      : storage_key(blink::StorageKey(origin)),
        origin(origin),
        total_size_bytes(total_size_bytes),
        last_modified(last_modified) {}

  // For assignment into maps without wordy emplace(std::make_pair()) syntax.
  StorageUsageInfo() = default;

  // The storage key this object is describing.
  blink::StorageKey storage_key;

  // The origin this object is describing.
  // DEPRECATED, use storage_key instead (https://crbug.com/1361545).
  url::Origin origin;

  // The total size, including resources, in bytes.
  int64_t total_size_bytes;

  // Last modification time of the data for this origin.
  base::Time last_modified;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_STORAGE_USAGE_INFO_H_
