// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/link_to_text_tab_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

LinkToTextTabHelper::LinkToTextTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

LinkToTextTabHelper::~LinkToTextTabHelper() {}

// static
void LinkToTextTabHelper::CreateForWebState(web::WebState* web_state) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(), base::WrapUnique(new LinkToTextTabHelper(web_state)));
  }
}

bool LinkToTextTabHelper::ShouldOffer() {
  // TODO(crbug.com/1134708): add more checks, like text only.
  return true;
}

SharedHighlight LinkToTextTabHelper::GetLinkToSelectedTextAndQuote() {
  // TODO(crbug.com/1091918): Call into JS to generate the quote and URL.
  SharedHighlight highlight(
      GURL("http://example.com/#:~:text=You%20may%20use%20this%20domain"),
      "You may use this domain");
  return highlight;
}

void LinkToTextTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  // The call to RemoveUserData cause the destruction of the current instance,
  // so nothing should be done after that point (this is like "delete this;").
  // Unregistration as an observer happens in the destructor.
  web_state_->RemoveUserData(UserDataKey());
}

WEB_STATE_USER_DATA_KEY_IMPL(LinkToTextTabHelper)
