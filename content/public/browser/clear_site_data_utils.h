// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CLEAR_SITE_DATA_UTILS_H_
#define CONTENT_PUBLIC_BROWSER_CLEAR_SITE_DATA_UTILS_H_

#include "base/containers/enum_set.h"
#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition_config.h"
#include "net/cookies/cookie_partition_key.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace url {
class Origin;
}

namespace content {
class BrowserContext;

enum class ClearSiteDataType {
  kUndefined,
  kCookies,
  kStorage,
  kCache,
  kClientHints,
};
using ClearSiteDataTypeSet = base::EnumSet<ClearSiteDataType,
                                           ClearSiteDataType::kCookies,
                                           ClearSiteDataType::kClientHints>;

// Removes browsing data associated with |origin|. Used when the Clear-Site-Data
// header is sent.
// Has to be called on the UI thread and will execute |callback| on the UI
// thread when done.
CONTENT_EXPORT void ClearSiteData(
    base::WeakPtr<BrowserContext> browser_context,
    std::optional<StoragePartitionConfig> storage_partition_config,
    const url::Origin& origin,
    const ClearSiteDataTypeSet clear_site_data_types,
    const std::set<std::string>& storage_buckets_to_remove,
    bool avoid_closing_connections,
    std::optional<net::CookiePartitionKey> cookie_partition_key,
    std::optional<blink::StorageKey> storage_key,
    bool partitioned_state_allowed_only,
    base::OnceClosure callback);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CLEAR_SITE_DATA_UTILS_H_
