// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_SERVICE_IMPL_H_
#define IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_SERVICE_IMPL_H_

#import <memory>
#import <set>
#import <string>

#import "base/memory/raw_ptr.h"
#import "base/time/time.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/security_interstitials/core/https_only_mode_allowlist.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/components/security_interstitials/https_only_mode/https_upgrade_service.h"

// HttpsUpgradeServiceImpl tracks the allowlist decisions for HTTPS-Only mode.
// Decisions are scoped to the host.
class HttpsUpgradeServiceImpl : public HttpsUpgradeService {
 public:
  explicit HttpsUpgradeServiceImpl(ProfileIOS* context);
  ~HttpsUpgradeServiceImpl() override;

  // HttpsUpgradeService methods:
  bool IsHttpAllowedForHost(const std::string& host) const override;
  void AllowHttpForHost(const std::string& host) override;
  void ClearAllowlist(base::Time delete_begin, base::Time delete_end) override;

 private:
  std::unique_ptr<base::Clock> clock_;
  raw_ptr<ProfileIOS> context_;
  security_interstitials::HttpsOnlyModeAllowlist allowlist_;
};

#endif  // IOS_CHROME_BROWSER_HTTPS_UPGRADES_MODEL_HTTPS_UPGRADE_SERVICE_IMPL_H_
