// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/https_upgrades/https_upgrade_service_impl.h"

#import "base/containers/contains.h"
#import "base/time/default_clock.h"
#import "base/time/time.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The default expiration for certificate error and HTTPS-First Mode bypasses is
// one week.
const base::TimeDelta kDeltaDefaultExpiration = base::Days(7);
}  // namespace

HttpsUpgradeServiceImpl::HttpsUpgradeServiceImpl(ChromeBrowserState* context)
    : clock_(new base::DefaultClock()),
      context_(context),
      allowlist_(
          ios::HostContentSettingsMapFactory::GetForBrowserState(context),
          clock_.get(),
          kDeltaDefaultExpiration) {
  DCHECK(context_);
}

HttpsUpgradeServiceImpl::~HttpsUpgradeServiceImpl() = default;

bool HttpsUpgradeServiceImpl::IsHttpAllowedForHost(
    const std::string& host) const {
  return allowlist_.IsHttpAllowedForHost(host, context_->IsOffTheRecord());
}

void HttpsUpgradeServiceImpl::AllowHttpForHost(const std::string& host) {
  allowlist_.AllowHttpForHost(host, context_->IsOffTheRecord());
}

void HttpsUpgradeServiceImpl::ClearAllowlist(base::Time delete_begin,
                                             base::Time delete_end) {
  allowlist_.ClearAllowlist(delete_begin, delete_end);
}
