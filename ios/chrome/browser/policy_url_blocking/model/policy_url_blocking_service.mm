// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_service.h"

PolicyBlocklistService::PolicyBlocklistService(
    std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager)
    : url_blocklist_manager_(std::move(url_blocklist_manager)) {}

PolicyBlocklistService::~PolicyBlocklistService() = default;

policy::URLBlocklist::URLBlocklistState
PolicyBlocklistService::GetURLBlocklistState(const GURL& url) const {
  return url_blocklist_manager_->GetURLBlocklistState(url);
}
