// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SESSION_STORAGE_USAGE_INFO_H_
#define CONTENT_PUBLIC_BROWSER_SESSION_STORAGE_USAGE_INFO_H_

#include <tuple>

#include "content/common/content_export.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

// Used to report Session Storage usage info by DOMStorageContext.
struct CONTENT_EXPORT SessionStorageUsageInfo {
  blink::StorageKey storage_key;
  std::string namespace_id;

  bool operator==(const SessionStorageUsageInfo& other) const {
    return std::tie(namespace_id, storage_key) ==
           std::tie(other.namespace_id, other.storage_key);
  }

  bool operator<(const SessionStorageUsageInfo& other) const {
    return std::tie(namespace_id, storage_key) <
           std::tie(other.namespace_id, other.storage_key);
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SESSION_STORAGE_USAGE_INFO_H_
