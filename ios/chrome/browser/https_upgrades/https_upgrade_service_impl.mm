// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/https_upgrades/https_upgrade_service_impl.h"

#include "base/containers/contains.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

HttpsUpgradeServiceImpl::HttpsUpgradeServiceImpl(web::BrowserState* context)
    : context_(context) {
  DCHECK(context_);
}

HttpsUpgradeServiceImpl::~HttpsUpgradeServiceImpl() = default;

bool HttpsUpgradeServiceImpl::IsHttpAllowedForHost(
    const std::string& host) const {
  return base::Contains(allowed_http_hosts_, host);
}

void HttpsUpgradeServiceImpl::AllowHttpForHost(const std::string& host) {
  allowed_http_hosts_.insert(host);
}

void HttpsUpgradeServiceImpl::ClearAllowlist() {
  allowed_http_hosts_.clear();
}
