// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_FONT_SIZE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_FONT_SIZE_TAB_HELPER_H_

#include "base/macros.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}  // namespace web

// Adjusts font size of web page by mapping
// |UIApplication.sharedApplication.preferredContentSizeCategory| to a scaling
// percentage and setting it to "-webkit-font-size-adjust" style on <body> when
// the page is successfully loaded or system font size changes.
class FontSizeTabHelper : public web::WebStateObserver,
                          public web::WebStateUserData<FontSizeTabHelper> {
 public:
  ~FontSizeTabHelper() override;

 private:
  friend class web::WebStateUserData<FontSizeTabHelper>;

  explicit FontSizeTabHelper(web::WebState* web_state);

  // Sets font size in web page by scaling percentage.
  void SetPageFontSize(int size);

  // Returns system suggested font size in scaling percentage(e.g. 150 for
  // 150%).
  int GetSystemSuggestedFontSize() const;

  // web::WebStateObserver overrides:
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Observer id returned by registering at NSNotificationCenter.
  id content_size_did_change_observer_ = nil;

  // WebState this tab helper is attached to.
  web::WebState* web_state_ = nullptr;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(FontSizeTabHelper);
};

#endif  // IOS_CHROME_BROWSER_WEB_FONT_SIZE_TAB_HELPER_H_
