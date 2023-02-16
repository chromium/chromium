// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_selection/web_selection_tab_helper.h"

#import "ios/chrome/browser/web_selection/web_selection_java_script_feature.h"
#import "ios/chrome/browser/web_selection/web_selection_response.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebSelectionTabHelper::WebSelectionTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

WebSelectionTabHelper::~WebSelectionTabHelper() {}

void WebSelectionTabHelper::GetSelectedText(
    base::OnceCallback<void(WebSelectionResponse*)> callback) {
  if (!web_state_) {
    std::move(callback).Run([WebSelectionResponse invalidResponse]);
    return;
  }
  WebSelectionJavaScriptFeature::GetInstance()->GetSelectedText(
      web_state_, std::move(callback));
}

bool WebSelectionTabHelper::CanRetrieveSelectedText() {
  if (!web_state_) {
    return false;
  }
  web::WebFrame* main_frame =
      web_state_->GetPageWorldWebFramesManager()->GetMainWebFrame();
  if (!web_state_->ContentIsHTML() || !main_frame ||
      !main_frame->CanCallJavaScriptFunction()) {
    return false;
  }
  return true;
}

void WebSelectionTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);

  web_state_->RemoveObserver(this);
  web_state_ = nil;
}

WEB_STATE_USER_DATA_KEY_IMPL(WebSelectionTabHelper)
