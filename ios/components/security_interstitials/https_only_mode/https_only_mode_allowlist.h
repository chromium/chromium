// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_ONLY_MODE_ALLOWLIST_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_ONLY_MODE_ALLOWLIST_H_

#include <set>
#include <string>

#import "ios/web/public/web_state_user_data.h"

// HttpsOnlyModeAllowlist tracks the allowlist decisions for HTTPS-Only mode.
// Decisions are scoped to the host.
class HttpsOnlyModeAllowlist
    : public web::WebStateUserData<HttpsOnlyModeAllowlist> {
 public:
  // HttpsOnlyModeAllowlist is move-only.
  HttpsOnlyModeAllowlist(HttpsOnlyModeAllowlist&& other);
  HttpsOnlyModeAllowlist& operator=(HttpsOnlyModeAllowlist&& other);
  ~HttpsOnlyModeAllowlist() override;

  // Returns whether |host| can be loaded over http://.
  bool IsHttpAllowedForHost(const std::string& host) const;

  // Allows future navigations to |host| over http://.
  void AllowHttpForHost(const std::string& host);

 private:
  explicit HttpsOnlyModeAllowlist(web::WebState* web_state);
  friend class web::WebStateUserData<HttpsOnlyModeAllowlist>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // Set of allowlisted hostnames.
  std::set<std::string> allowed_http_hosts_;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_ONLY_MODE_ALLOWLIST_H_
