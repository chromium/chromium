// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_UPGRADE_TEST_UTIL_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_UPGRADE_TEST_UTIL_H_

#include <set>
#include <string>
#include "ios/components/security_interstitials/https_only_mode/https_upgrade_service.h"

// Fake service that stores allowlist decisions in memory.
class FakeHttpsUpgradeService : public HttpsUpgradeService {
 public:
  FakeHttpsUpgradeService();
  ~FakeHttpsUpgradeService() override;

  // HttpsUpgradeService methods:
  bool IsHttpAllowedForHost(const std::string& host) const override;
  void AllowHttpForHost(const std::string& host) override;
  void ClearAllowlist(base::Time delete_begin, base::Time delete_end) override;

 private:
  std::set<std::string> allowed_http_hosts_;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_UPGRADE_TEST_UTIL_H_
