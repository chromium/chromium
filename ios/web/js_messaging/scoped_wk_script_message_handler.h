// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_SCOPED_WK_SCRIPT_MESSAGE_HANDLER_H_
#define IOS_WEB_JS_MESSAGING_SCOPED_WK_SCRIPT_MESSAGE_HANDLER_H_

#import <WebKit/WebKit.h>

#include "base/callback.h"

@class CRWScriptMessageHandler;

using ScriptMessageCallback =
    base::RepeatingCallback<void(WKScriptMessage* message)>;

// Instances of this class register and unregister itself as a
// WKUserContentController script message handler upon construction and
// deconstruction respectively.
class ScopedWKScriptMessageHandler {
 public:
  // Registers `script_handler_name` with `user_content_controller`. `callback`
  // will be called whenever JavaScript sends a post message to
  // `script_handler_name` within the page content world.
  // Ex: window.webkit.messageHandlers['script_handler_name'].postMessage(10);
  ScopedWKScriptMessageHandler(WKUserContentController* user_content_controller,
                               NSString* script_handler_name,
                               ScriptMessageCallback callback);

  // Registers `script_handler_name` with `user_content_controller` within
  // `content_world`. `callback` will be called whenever JavaScript
  // sends a post message to `script_handler_name` within `content_world`
  // Ex: window.webkit.messageHandlers['script_handler_name'].postMessage(10);
  ScopedWKScriptMessageHandler(WKUserContentController* user_content_controller,
                               NSString* script_handler_name,
                               WKContentWorld* content_world,
                               ScriptMessageCallback callback);

  ~ScopedWKScriptMessageHandler();

 private:
  // The content world associated with this feature. May be null which
  // represents the main world that the page content itself uses. (May also be
  // [WKContentWorld pageWorld] on iOS 14 and later.)
  WKContentWorld* content_world_ = nullptr;

  __weak WKUserContentController* user_content_controller_;
  NSString* script_handler_name_;

  CRWScriptMessageHandler* script_message_handler_;

  ScopedWKScriptMessageHandler(const ScopedWKScriptMessageHandler&) = delete;
  ScopedWKScriptMessageHandler& operator=(const ScopedWKScriptMessageHandler&) =
      delete;
};

#endif  // IOS_WEB_JS_MESSAGING_SCOPED_WK_SCRIPT_MESSAGE_HANDLER_H_
