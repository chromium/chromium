// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_LOAD_NAVIGATION_USER_DATA_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_LOAD_NAVIGATION_USER_DATA_H_

#include <string>

#include "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}

// User data used to signal that a navigation was triggered by Send Tab to Self.
// Stores the GUID of the entry that triggered the navigation.
class SendTabToSelfLoadNavigationUserData
    : public web::WebStateUserData<SendTabToSelfLoadNavigationUserData> {
 public:
  ~SendTabToSelfLoadNavigationUserData() override;

  const std::string& entry_guid() const { return entry_guid_; }

 private:
  explicit SendTabToSelfLoadNavigationUserData(web::WebState* web_state,
                                               const std::string& entry_guid);

  friend class web::WebStateUserData<SendTabToSelfLoadNavigationUserData>;

  std::string entry_guid_;
};

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_LOAD_NAVIGATION_USER_DATA_H_
