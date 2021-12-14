// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_TAB_ID_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_TAB_ID_TAB_HELPER_H_

#import "ios/web/public/web_state_user_data.h"

// Handles creating a unique identifier, which is stable across cold starts.
//
// TODO(crbug.com/1276776): Remove this class once all code has been converted
// to instead use WebState::GetStableIdentifier() instead.
class TabIdTabHelper : public web::WebStateUserData<TabIdTabHelper> {
 public:
  TabIdTabHelper(const TabIdTabHelper&) = delete;
  TabIdTabHelper& operator=(const TabIdTabHelper&) = delete;

  ~TabIdTabHelper() override;

  // Returns a unique identifier for this tab.
  NSString* tab_id() const;

 private:
  friend class web::WebStateUserData<TabIdTabHelper>;

  explicit TabIdTabHelper(web::WebState* web_state);
  web::WebState* web_state_ = nullptr;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_WEB_TAB_ID_TAB_HELPER_H_
