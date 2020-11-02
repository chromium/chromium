// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LINK_TO_TEXT_LINK_TO_TEXT_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_LINK_TO_TEXT_LINK_TO_TEXT_TAB_HELPER_H_

#import "base/macros.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/link_to_text/link_to_text_payload.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol CRWWebViewProxy;

// A tab helper that observes WebState and triggers the link to text generation
// Javascript library on it.
class LinkToTextTabHelper : public web::WebStateObserver,
                            public web::WebStateUserData<LinkToTextTabHelper> {
 public:
  ~LinkToTextTabHelper() override;

  // Callback for GetLinkToText.
  typedef void (^LinkToTextCallback)(LinkToTextPayload* payload);

  static void CreateForWebState(web::WebState* web_state);

  // Returns whether the link to text feature should be offered for the current
  // user selection
  bool ShouldOffer();

  // Calls the JavaScript to generate a URL linking to the current
  // selected text. Will invoke |callback| with the returned generated URL,
  // selected text and selected text's position on the page.
  void GetLinkToText(LinkToTextCallback callback);

 private:
  friend class web::WebStateUserData<LinkToTextTabHelper>;

  explicit LinkToTextTabHelper(web::WebState* web_state);

  // Invoked with pending GetLinkToText |callback| and the |response| from
  // the JavaScript call to generate a link to selected text.
  void OnJavaScriptResponseReceived(LinkToTextCallback callback,
                                    const base::Value* response);

  // Converts the given |response| value into a LinkToTextPayload instance.
  LinkToTextPayload* ParseResponse(const base::Value* response);

  // Not copyable or moveable.
  LinkToTextTabHelper(const LinkToTextTabHelper&) = delete;
  LinkToTextTabHelper& operator=(const LinkToTextTabHelper&) = delete;

  // WebStateObserver:
  void WebStateDestroyed(web::WebState* web_state) override;

  // Converts |web_view_rect| into the right coordinates from the browser UI's
  // point of view.
  CGRect ConvertToBrowserRect(CGRect web_view_rect);

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  web::WebState* web_state_ = nullptr;

  // Access to the web view from |web_state_|.
  id<CRWWebViewProxy> web_view_proxy_;

  base::WeakPtrFactory<LinkToTextTabHelper> weak_ptr_factory_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_LINK_TO_TEXT_LINK_TO_TEXT_TAB_HELPER_H_
