// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_special_storage_policy.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "net/cookies/cookie_util.h"

namespace content {

MockSpecialStoragePolicy::MockSpecialStoragePolicy() : all_unlimited_(false) {}

bool MockSpecialStoragePolicy::IsStorageProtected(const GURL& origin) {
  return base::Contains(protected_, origin);
}

bool MockSpecialStoragePolicy::IsStorageUnlimited(const GURL& origin) {
  if (all_unlimited_)
    return true;
  return base::Contains(unlimited_, origin);
}

bool MockSpecialStoragePolicy::IsStorageSessionOnly(const GURL& origin) {
  return base::Contains(session_only_, origin);
}

network::SessionCleanupCookieStore::DeleteCookiePredicate
MockSpecialStoragePolicy::CreateDeleteCookieOnExitPredicate() {
  return base::BindRepeating(
      &MockSpecialStoragePolicy::ShouldDeleteCookieOnExit,
      base::Unretained(this));
}

bool MockSpecialStoragePolicy::ShouldDeleteCookieOnExit(
    const std::string& domain,
    bool is_https) {
  GURL origin = net::cookie_util::CookieOriginToURL(domain, is_https);
  return IsStorageSessionOnly(origin);
}

bool MockSpecialStoragePolicy::HasIsolatedStorage(const GURL& origin) {
  return base::Contains(isolated_, origin);
}

bool MockSpecialStoragePolicy::HasSessionOnlyOrigins() {
  return !session_only_.empty();
}

bool MockSpecialStoragePolicy::IsStorageDurable(const GURL& origin) {
  return base::Contains(durable_, origin);
}

MockSpecialStoragePolicy::~MockSpecialStoragePolicy() = default;

}  // namespace content
