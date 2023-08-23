// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/lookalikes/lookalike_url_tab_allow_list.h"

#import "base/containers/contains.h"
#import "ios/web/public/web_state.h"

WEB_STATE_USER_DATA_KEY_IMPL(LookalikeUrlTabAllowList)

LookalikeUrlTabAllowList::LookalikeUrlTabAllowList(web::WebState* web_state) {}

LookalikeUrlTabAllowList::LookalikeUrlTabAllowList(
    LookalikeUrlTabAllowList&& other) = default;

LookalikeUrlTabAllowList& LookalikeUrlTabAllowList::operator=(
    LookalikeUrlTabAllowList&& other) = default;

LookalikeUrlTabAllowList::~LookalikeUrlTabAllowList() = default;

bool LookalikeUrlTabAllowList::IsDomainAllowed(const std::string& domain) {
  return base::Contains(allowed_domains_, domain);
}

void LookalikeUrlTabAllowList::AllowDomain(const std::string& domain) {
  allowed_domains_.insert(domain);
}
