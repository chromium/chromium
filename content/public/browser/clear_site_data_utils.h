// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CLEAR_SITE_DATA_UTILS_H_
#define CONTENT_PUBLIC_BROWSER_CLEAR_SITE_DATA_UTILS_H_

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "net/cookies/cookie_partition_key.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace url {
class Origin;
}

namespace content {
class BrowserContext;

// Removes browsing data associated with |origin|. Used when the Clear-Site-Data
// header is sent.
// Has to be called on the UI thread and will execute |callback| on the UI
// thread when done.
CONTENT_EXPORT void ClearSiteData(
    const base::RepeatingCallback<BrowserContext*()>& browser_context_getter,
    const url::Origin& origin,
    bool clear_cookies,
    bool clear_storage,
    bool clear_cache,
    const std::set<std::string>& storage_buckets_to_remove,
    bool avoid_closing_connections,
    const absl::optional<net::CookiePartitionKey>& cookie_partition_key,
    const absl::optional<blink::StorageKey>& storage_key,
    base::OnceClosure callback);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CLEAR_SITE_DATA_UTILS_H_
