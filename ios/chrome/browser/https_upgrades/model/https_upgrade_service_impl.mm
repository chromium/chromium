// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/https_upgrades/model/https_upgrade_service_impl.h"

#import "base/containers/contains.h"
#import "base/time/default_clock.h"
#import "base/time/time.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {
// The default expiration for HTTPS-First Mode bypasses is 15 days.
const base::TimeDelta kDeltaDefaultExpiration = base::Days(15);
}  // namespace

HttpsUpgradeServiceImpl::HttpsUpgradeServiceImpl(ProfileIOS* context)
    : clock_(new base::DefaultClock()),
      context_(context),
      allowlist_(ios::HostContentSettingsMapFactory::GetForProfile(context),
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
