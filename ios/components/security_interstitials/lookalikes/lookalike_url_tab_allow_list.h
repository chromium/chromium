// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_LOOKALIKES_LOOKALIKE_URL_TAB_ALLOW_LIST_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_LOOKALIKES_LOOKALIKE_URL_TAB_ALLOW_LIST_H_

#include <set>
#include <string>

#import "ios/web/public/web_state_user_data.h"

// LookalikeUrlTabAllowList tracks the allowlist decisions for lookalike URLs.
// Decisions are scoped to the domain.
class LookalikeUrlTabAllowList
    : public web::WebStateUserData<LookalikeUrlTabAllowList> {
 public:
  // LookalikeUrlAllowList is move-only.
  LookalikeUrlTabAllowList(LookalikeUrlTabAllowList&& other);
  LookalikeUrlTabAllowList& operator=(LookalikeUrlTabAllowList&& other);
  ~LookalikeUrlTabAllowList() override;

  // Returns whether `domain` has been allowlisted.
  bool IsDomainAllowed(const std::string& domain);

  // Allows future navigations to `domain`.
  void AllowDomain(const std::string& domain);

 private:
  explicit LookalikeUrlTabAllowList(web::WebState* web_state);
  friend class web::WebStateUserData<LookalikeUrlTabAllowList>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // Set of allowlisted domains.
  std::set<std::string> allowed_domains_;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_LOOKALIKES_LOOKALIKE_URL_TAB_ALLOW_LIST_H_
