// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_SCOPED_WK_SCRIPT_MESSAGE_HANDLER_H_
#define IOS_WEB_JS_MESSAGING_SCOPED_WK_SCRIPT_MESSAGE_HANDLER_H_

#import <WebKit/WebKit.h>

#include "base/functional/callback.h"

namespace base {
class Value;
}  // namespace base

@class CRWScriptMessageHandler;
@class CRWScriptMessageHandlerWithReply;

// Callback to receive messages from JavaScript running in a webpage.
using ScriptMessageCallback =
    base::RepeatingCallback<void(WKScriptMessage* message)>;

// A block to be called with the result of processing the message from
// JavaScript.
// 1. Passing a non-nil NSString value to the `error_message` signals an error.
// No matter what value you pass to the `reply`. the Promise will be rejected
// with a JavaScript error object whose message property is set to that
// `error_message` string.
// 2. If the `error_message` is nil, the `reply` will be converted to its
// JavaScript equivalent and the Promise will be fulfilled with the resulting
// value.
//    a. If `reply` is nullptr, then the JavaScript resulting value is
//    `undefined`.
//    b. If `reply` is none type base::Value, then the JavaScript resulting
//    value is `null`.
using ScriptMessageReplyHandler = void (^)(const base::Value* reply,
                                           NSString* error_message);
// Callback to receive messages from JavaScript running in a webpage and
// replying to them asynchronously. The `reply_handler` can be called at most
// once. If the `reply_handler` is deallocated before it is called, the Promise
// will be rejected with a JavaScript Error object with an appropriate message
// indicating the handler was never called.
using ScriptMessageWithReplyCallback =
    base::RepeatingCallback<void(WKScriptMessage* message,
                                 ScriptMessageReplyHandler reply_handler)>;

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

  // Registers `script_handler_name` with `user_content_controller` within
  // `content_world`. `callback` will be called whenever JavaScript
  // sends a post message to `script_handler_name` within `content_world`, and
  // it allows native to reply to JavaScript via `reply_handler` which is passed
  // to `callback`.
  //
  // Ex: let result = await
  // window.webkit.messageHandlers['script_handler_name'].postMessage("42");
  ScopedWKScriptMessageHandler(WKUserContentController* user_content_controller,
                               NSString* script_handler_name,
                               WKContentWorld* content_world,
                               ScriptMessageWithReplyCallback callback);

  ~ScopedWKScriptMessageHandler();

 private:
  // The content world associated with this feature. May be null which
  // represents the main world that the page content itself uses. (May also be
  // [WKContentWorld pageWorld] on iOS 14 and later.)
  WKContentWorld* content_world_ = nullptr;

  __weak WKUserContentController* user_content_controller_;
  NSString* script_handler_name_;

  // Called with messages sent from JavaScript if the constructor accepting a
  // `ScriptMessageCallback` was used, null otherwise.
  CRWScriptMessageHandler* script_message_handler_;
  // Called with messages sent from JavaScript if the constructor accepting a
  // `ScriptMessageWithReplyCallback` was used, null otherwise.
  CRWScriptMessageHandlerWithReply* script_message_handler_with_reply_;

  ScopedWKScriptMessageHandler(const ScopedWKScriptMessageHandler&) = delete;
  ScopedWKScriptMessageHandler& operator=(const ScopedWKScriptMessageHandler&) =
      delete;
};

#endif  // IOS_WEB_JS_MESSAGING_SCOPED_WK_SCRIPT_MESSAGE_HANDLER_H_
