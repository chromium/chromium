// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_access_checker.h"

#include "net/base/isolation_info.h"
#include "net/shared_dictionary/shared_dictionary_isolation_key.h"
#include "services/network/cookie_manager.h"
#include "services/network/cookie_settings.h"
#include "services/network/network_context.h"

namespace network {

SharedDictionaryAccessChecker::SharedDictionaryAccessChecker(
    NetworkContext& context,
    mojo::PendingRemote<mojom::SharedDictionaryAccessObserver>
        shared_dictionary_observer_remote)
    : context_(context),
      shared_dictionary_observer_remote_(
          std::move(shared_dictionary_observer_remote)),
      shared_dictionary_observer_(shared_dictionary_observer_remote_.get()) {}

SharedDictionaryAccessChecker::SharedDictionaryAccessChecker(
    NetworkContext& context,
    mojom::SharedDictionaryAccessObserver* shared_dictionary_observer)
    : context_(context),
      shared_dictionary_observer_(shared_dictionary_observer) {}

SharedDictionaryAccessChecker::~SharedDictionaryAccessChecker() = default;

bool SharedDictionaryAccessChecker::CheckAllowedToWriteAndReport(
    const GURL& dictionary_url,
    const net::SiteForCookies& site_for_cookies,
    const net::IsolationInfo& isolation_info) {
  std::optional<net::SharedDictionaryIsolationKey> isolation_key =
      net::SharedDictionaryIsolationKey::MaybeCreate(isolation_info);
  CHECK(isolation_key);

  bool allowed = IsAllowedToUseSharedDictionary(
      dictionary_url, site_for_cookies, isolation_info);
  if (shared_dictionary_observer_) {
    // Asynchronously reports the usage to the browser process to show a UI that
    // indicates that site data was used or blocked.
    shared_dictionary_observer_->OnSharedDictionaryAccessed(
        mojom::SharedDictionaryAccessDetails::New(
            mojom::SharedDictionaryAccessDetails::Type::kWrite, dictionary_url,
            *isolation_key, !allowed));
  }
  return allowed;
}

bool SharedDictionaryAccessChecker::CheckAllowedToReadAndReport(
    const GURL& target_resource_url,
    const net::SiteForCookies& site_for_cookies,
    const net::IsolationInfo& isolation_info) {
  std::optional<net::SharedDictionaryIsolationKey> isolation_key =
      net::SharedDictionaryIsolationKey::MaybeCreate(isolation_info);
  CHECK(isolation_key);

  bool allowed = IsAllowedToUseSharedDictionary(
      target_resource_url, site_for_cookies, isolation_info);
  if (shared_dictionary_observer_) {
    // Asynchronously reports the usage to the browser process to show a UI that
    // indicates that site data was used or blocked.
    shared_dictionary_observer_->OnSharedDictionaryAccessed(
        mojom::SharedDictionaryAccessDetails::New(
            mojom::SharedDictionaryAccessDetails::Type::kRead,
            target_resource_url, *isolation_key, !allowed));
  }
  return allowed;
}

bool SharedDictionaryAccessChecker::IsAllowedToUseSharedDictionary(
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const net::IsolationInfo& isolation_info) {
  return context_->cookie_manager()
      ->cookie_settings()
      .IsFullCookieAccessAllowed(url, site_for_cookies,
                                 isolation_info.top_frame_origin(),
                                 net::CookieSettingOverrides());
}

}  // namespace network
