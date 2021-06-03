// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_COOKIE_BLOCKING_ERROR_LOGGER_H_
#define IOS_WEB_WEB_STATE_UI_COOKIE_BLOCKING_ERROR_LOGGER_H_

#include "base/macros.h"
#import "ios/web/public/web_state.h"

class GURL;
namespace web {
class WebFrame;

// Handles "cookie.error" message from injected JavaScript and logs it as a
// UMA metric.
class CookieBlockingErrorLogger final {
 public:
  explicit CookieBlockingErrorLogger(WebState* web_state);
  ~CookieBlockingErrorLogger();

 private:
  CookieBlockingErrorLogger(const CookieBlockingErrorLogger&) = delete;
  CookieBlockingErrorLogger& operator=(const CookieBlockingErrorLogger&) =
      delete;

  // Callback called when this class receives a Javascript message from its
  // corresponding web state.
  void OnJavascriptMessageReceived(const base::Value& message,
                                   const GURL& page_url,
                                   bool user_is_interacting,
                                   WebFrame* sender_frame);

  WebState* web_state_impl_ = nullptr;

  // Subscription for JS message.
  base::CallbackListSubscription subscription_;
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_UI_COOKIE_BLOCKING_ERROR_LOGGER_H_
