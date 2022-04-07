// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/https_only_mode/https_only_mode_allowlist.h"

#include "base/containers/contains.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WEB_STATE_USER_DATA_KEY_IMPL(HttpsOnlyModeAllowlist)

HttpsOnlyModeAllowlist::HttpsOnlyModeAllowlist(web::WebState* web_state) {}

HttpsOnlyModeAllowlist::HttpsOnlyModeAllowlist(HttpsOnlyModeAllowlist&& other) =
    default;

HttpsOnlyModeAllowlist& HttpsOnlyModeAllowlist::operator=(
    HttpsOnlyModeAllowlist&& other) = default;

HttpsOnlyModeAllowlist::~HttpsOnlyModeAllowlist() = default;

bool HttpsOnlyModeAllowlist::IsHttpAllowedForHost(
    const std::string& host) const {
  return base::Contains(allowed_http_hosts_, host);
}

void HttpsOnlyModeAllowlist::AllowHttpForHost(const std::string& host) {
  allowed_http_hosts_.insert(host);
}
