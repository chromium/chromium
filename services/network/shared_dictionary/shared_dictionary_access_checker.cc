// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_access_checker.h"

#include "net/base/isolation_info.h"
#include "net/extras/shared_dictionary/shared_dictionary_isolation_key.h"
#include "services/network/cookie_manager.h"
#include "services/network/cookie_settings.h"
#include "services/network/network_context.h"

namespace network {

SharedDictionaryAccessChecker::SharedDictionaryAccessChecker(
    NetworkContext& context)
    : context_(context) {}

SharedDictionaryAccessChecker::~SharedDictionaryAccessChecker() = default;

bool SharedDictionaryAccessChecker::IsAllowedToWrite(
    const GURL& dictionary_url,
    const net::SiteForCookies& site_for_cookies,
    const net::IsolationInfo& isolation_info) {
  // TODO(crbug.com/1413922): Notify the usage and the check result to the
  // browser process.
  return IsAllowedToUseSharedDictionary(dictionary_url, site_for_cookies,
                                        isolation_info);
}

bool SharedDictionaryAccessChecker::IsAllowedToRead(
    const GURL& target_resource_url,
    const net::SiteForCookies& site_for_cookies,
    const net::IsolationInfo& isolation_info) {
  // TODO(crbug.com/1413922): Notify the usage and the check result to the
  // browser process.
  return IsAllowedToUseSharedDictionary(target_resource_url, site_for_cookies,
                                        isolation_info);
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
