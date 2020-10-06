// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LINK_TO_TEXT_LINK_TO_TEXT_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_LINK_TO_TEXT_LINK_TO_TEXT_TAB_HELPER_H_

#import "base/macros.h"
#import "ios/chrome/browser/link_to_text/shared_highlight.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

// A tab helper that observes WebState and triggers the link to text generation
// Javascript library on it.
class LinkToTextTabHelper : public web::WebStateObserver,
                            public web::WebStateUserData<LinkToTextTabHelper> {
 public:
  ~LinkToTextTabHelper() override;

  static void CreateForWebState(web::WebState* web_state);

  // Returns whether the link to text feature should be offered for the current
  // user selection
  bool ShouldOffer();

  // Synchronously calls the JavaScript to generate a URL linking to the current
  // selected text. Returns the generated URL along with the selected text.
  SharedHighlight GetLinkToSelectedTextAndQuote();

 private:
  friend class web::WebStateUserData<LinkToTextTabHelper>;

  explicit LinkToTextTabHelper(web::WebState* web_state);

  // Not copyable or moveable.
  LinkToTextTabHelper(const LinkToTextTabHelper&) = delete;
  LinkToTextTabHelper& operator=(const LinkToTextTabHelper&) = delete;

  // WebStateObserver:
  void WebStateDestroyed(web::WebState* web_state) override;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  web::WebState* web_state_ = nullptr;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_LINK_TO_TEXT_LINK_TO_TEXT_TAB_HELPER_H_
