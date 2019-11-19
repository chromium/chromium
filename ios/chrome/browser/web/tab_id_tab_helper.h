// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_TAB_ID_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_TAB_ID_TAB_HELPER_H_

#include "base/macros.h"
#import "ios/web/public/web_state_user_data.h"

// Handles creating a unique identifier, which is stable across cold starts.
class TabIdTabHelper : public web::WebStateUserData<TabIdTabHelper> {
 public:
  ~TabIdTabHelper() override;

  // Returns a unique identifier for this tab.
  NSString* tab_id() const { return tab_id_; }

 private:
  friend class web::WebStateUserData<TabIdTabHelper>;

  explicit TabIdTabHelper(web::WebState* web_state);
  __strong NSString* tab_id_;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(TabIdTabHelper);
};

#endif  // IOS_CHROME_BROWSER_WEB_TAB_ID_TAB_HELPER_H_
