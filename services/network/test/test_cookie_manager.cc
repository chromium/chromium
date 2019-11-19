// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_cookie_manager.h"

#include <utility>

#include "base/callback.h"
#include "net/cookies/canonical_cookie.h"

namespace network {

TestCookieManager::TestCookieManager() = default;

TestCookieManager::~TestCookieManager() = default;

void TestCookieManager::SetCanonicalCookie(
    const net::CanonicalCookie& cookie,
    const std::string& source_scheme,
    const net::CookieOptions& cookie_options,
    SetCanonicalCookieCallback callback) {
  if (callback) {
    std::move(callback).Run(net::CanonicalCookie::CookieInclusionStatus(
        net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR));
  }
}

void TestCookieManager::AddCookieChangeListener(
    const GURL& url,
    const base::Optional<std::string>& name,
    mojo::PendingRemote<network::mojom::CookieChangeListener> listener) {
  mojo::Remote<network::mojom::CookieChangeListener> listener_remote(
      std::move(listener));
  cookie_change_listeners_.push_back(std::move(listener_remote));
}

void TestCookieManager::DispatchCookieChange(
    const net::CookieChangeInfo& change) {
  for (auto& cookie_change_listener_ : cookie_change_listeners_) {
    cookie_change_listener_->OnCookieChange(change);
  }
}

}  // namespace network
