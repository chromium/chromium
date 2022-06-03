// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/lookalikes/lookalike_url_tab_allow_list.h"

#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WEB_STATE_USER_DATA_KEY_IMPL(LookalikeUrlTabAllowList)

LookalikeUrlTabAllowList::LookalikeUrlTabAllowList(web::WebState* web_state) {}

LookalikeUrlTabAllowList::LookalikeUrlTabAllowList(
    LookalikeUrlTabAllowList&& other) = default;

LookalikeUrlTabAllowList& LookalikeUrlTabAllowList::operator=(
    LookalikeUrlTabAllowList&& other) = default;

LookalikeUrlTabAllowList::~LookalikeUrlTabAllowList() = default;

bool LookalikeUrlTabAllowList::IsDomainAllowed(const std::string& domain) {
  return allowed_domains_.find(domain) != allowed_domains_.end();
}

void LookalikeUrlTabAllowList::AllowDomain(const std::string& domain) {
  allowed_domains_.insert(domain);
}
