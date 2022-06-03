// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/legacy_tls/legacy_tls_tab_allow_list.h"

#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WEB_STATE_USER_DATA_KEY_IMPL(LegacyTLSTabAllowList)

LegacyTLSTabAllowList::LegacyTLSTabAllowList(web::WebState* web_state) {}

LegacyTLSTabAllowList::LegacyTLSTabAllowList(LegacyTLSTabAllowList&& other) =
    default;

LegacyTLSTabAllowList& LegacyTLSTabAllowList::operator=(
    LegacyTLSTabAllowList&& other) = default;

LegacyTLSTabAllowList::~LegacyTLSTabAllowList() = default;

bool LegacyTLSTabAllowList::IsDomainAllowed(const std::string& domain) const {
  return allowed_domains_.find(domain) != allowed_domains_.end();
}

void LegacyTLSTabAllowList::AllowDomain(const std::string& domain) {
  allowed_domains_.insert(domain);
}
