// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/js_window_error_manager.h"

#import "base/values.h"
#import "ios/web/public/js_messaging/web_frame.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kCommandPrefix[] = "window";
}

namespace web {

JsWindowErrorManager::JsWindowErrorManager(WebState* web_state)
    : web_state_impl_(web_state) {
  subscription_ = web_state_impl_->AddScriptCommandCallback(
      base::BindRepeating(&JsWindowErrorManager::OnJsMessage,
                          base::Unretained(this)),
      kCommandPrefix);
}

JsWindowErrorManager::~JsWindowErrorManager() {}

void JsWindowErrorManager::OnJsMessage(const base::DictionaryValue& message,
                                       const GURL& page_url,
                                       bool user_is_interacting,
                                       WebFrame* sender_frame) {
  DCHECK(sender_frame->IsMainFrame());
  std::string error_message;
  if (!message.GetString("message", &error_message)) {
    DLOG(WARNING) << "JS message parameter not found: message";
    return;
  }
  web::URLVerificationTrustLevel trust_level =
      web::URLVerificationTrustLevel::kNone;
  const GURL current_url = web_state_impl_->GetCurrentURL(&trust_level);

  DLOG(ERROR) << "JavaScript error: " << error_message
              << " URL:" << current_url.spec();
}

}  // namespace web
