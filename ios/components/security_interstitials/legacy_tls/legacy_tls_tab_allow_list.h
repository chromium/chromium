// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_LEGACY_TLS_LEGACY_TLS_TAB_ALLOW_LIST_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_LEGACY_TLS_LEGACY_TLS_TAB_ALLOW_LIST_H_

#include <set>
#include <string>

#import "ios/web/public/web_state_user_data.h"

// LegacyTLSTabAllowList tracks the allowlist decisions for legacy TLS hosts.
// Decisions are scoped to the domain.
class LegacyTLSTabAllowList
    : public web::WebStateUserData<LegacyTLSTabAllowList> {
 public:
  // LegacyTLSTabAllowList is move-only.
  LegacyTLSTabAllowList(LegacyTLSTabAllowList&& other);
  LegacyTLSTabAllowList& operator=(LegacyTLSTabAllowList&& other);
  ~LegacyTLSTabAllowList() override;

  // Returns whether |domain| has been allowlisted.
  bool IsDomainAllowed(const std::string& domain) const;

  // Allows future navigations to |domain|.
  void AllowDomain(const std::string& domain);

 private:
  explicit LegacyTLSTabAllowList(web::WebState* web_state);
  friend class web::WebStateUserData<LegacyTLSTabAllowList>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // Set of allowlisted domains.
  std::set<std::string> allowed_domains_;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_LEGACY_TLS_LEGACY_TLS_TAB_ALLOW_LIST_H_
