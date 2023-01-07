// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_cookie_checker_impl.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "content/browser/storage_partition_impl.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

void ProcessCookies(base::OnceCallback<void(bool)> callback,
                    const std::vector<net::CookieWithAccessResult>& cookies,
                    const std::vector<net::CookieWithAccessResult>& excluded) {
  bool is_debug_cookie_set =
      base::ranges::any_of(cookies, [](const net::CookieWithAccessResult& c) {
        // It is not possible to create a `SameSite: None` cookie insecurely, so
        // we only DCHECK this for now.
        DCHECK(c.cookie.IsSecure());
        return c.cookie.IsHttpOnly() && !c.cookie.IsPartitioned() &&
               c.cookie.Name() == "ar_debug";
      });

  std::move(callback).Run(is_debug_cookie_set);
}

}  // namespace

AttributionCookieCheckerImpl::AttributionCookieCheckerImpl(
    StoragePartitionImpl* storage_partition)
    : storage_partition_(storage_partition) {
  DCHECK(storage_partition_);
}

AttributionCookieCheckerImpl::~AttributionCookieCheckerImpl() = default;

// TODO(apaseltiner): Consider caching the results of this check to avoid
// repeated lookups for the same origins in close temporal proximity.
void AttributionCookieCheckerImpl::IsDebugCookieSet(
    const url::Origin& origin,
    base::OnceCallback<void(bool)> callback) {
  // We only care about `SameSite: None` cookies, but
  // `SameSiteCookieContext::ContextType::CROSS_SITE` is the default.
  net::CookieOptions options;
  options.set_include_httponly();
  options.set_do_not_update_access_time();

  storage_partition_->GetCookieManagerForBrowserProcess()->GetCookieList(
      origin.GetURL(), options, net::CookiePartitionKeyCollection(),
      base::BindOnce(&ProcessCookies, std::move(callback)));
}

}  // namespace content
