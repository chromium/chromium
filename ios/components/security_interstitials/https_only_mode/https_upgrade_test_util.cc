// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/components/security_interstitials/https_only_mode/https_upgrade_test_util.h"
#include "base/containers/contains.h"

FakeHttpsUpgradeService::FakeHttpsUpgradeService() = default;
FakeHttpsUpgradeService::~FakeHttpsUpgradeService() = default;

bool FakeHttpsUpgradeService::IsHttpAllowedForHost(
    const std::string& host) const {
  return base::Contains(allowed_http_hosts_, host);
}

void FakeHttpsUpgradeService::AllowHttpForHost(const std::string& host) {
  allowed_http_hosts_.insert(host);
}

void FakeHttpsUpgradeService::ClearAllowlist(base::Time delete_begin,
                                             base::Time delete_end) {
  allowed_http_hosts_.clear();
}
