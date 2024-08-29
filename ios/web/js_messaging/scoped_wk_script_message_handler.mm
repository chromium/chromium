// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/scoped_wk_script_message_handler.h"

#import "ios/web/js_messaging/web_view_js_utils.h"

@interface CRWScriptMessageHandler : NSObject <WKScriptMessageHandler>

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithCallback:(ScriptMessageCallback)callback
    NS_DESIGNATED_INITIALIZER;

@property(nonatomic) ScriptMessageCallback callback;

@end

@implementation CRWScriptMessageHandler

- (instancetype)initWithCallback:(ScriptMessageCallback)callback {
  if ((self = [super init])) {
    _callback = callback;
  }
  return self;
}

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  _callback.Run(message);
}

@end

@interface CRWScriptMessageHandlerWithReply
    : NSObject <WKScriptMessageHandlerWithReply>

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithCallback:(ScriptMessageWithReplyCallback)callback
    NS_DESIGNATED_INITIALIZER;

@property(nonatomic) ScriptMessageWithReplyCallback callback;

@end

@implementation CRWScriptMessageHandlerWithReply

- (instancetype)initWithCallback:(ScriptMessageWithReplyCallback)callback {
  if ((self = [super init])) {
    _callback = callback;
  }
  return self;
}

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message
                 replyHandler:(void (^)(id, NSString*))replyHandler {
  ScriptMessageReplyHandler replyValueHandler =
      ^(const base::Value* reply, NSString* error_message) {
        // Per the API documentation, specify the result as nil if an error
        // occurred.
        id wkResult = error_message ? nil : web::NSObjectFromValueResult(reply);
        replyHandler(wkResult, error_message);
      };
  _callback.Run(message, replyValueHandler);
}

@end

ScopedWKScriptMessageHandler::ScopedWKScriptMessageHandler(
    WKUserContentController* user_content_controller,
    NSString* script_handler_name,
    ScriptMessageCallback callback)
    : user_content_controller_(user_content_controller),
      script_handler_name_(script_handler_name),
      script_message_handler_(
          [[CRWScriptMessageHandler alloc] initWithCallback:callback]) {
  DCHECK(callback);
  [user_content_controller_ addScriptMessageHandler:script_message_handler_
                                               name:script_handler_name_];
}

ScopedWKScriptMessageHandler::ScopedWKScriptMessageHandler(
    WKUserContentController* user_content_controller,
    NSString* script_handler_name,
    WKContentWorld* content_world,
    ScriptMessageCallback callback)
    : content_world_(content_world),
      user_content_controller_(user_content_controller),
      script_handler_name_(script_handler_name),
      script_message_handler_(
          [[CRWScriptMessageHandler alloc] initWithCallback:callback]) {
  DCHECK(callback);
  if (content_world_) {
    [user_content_controller_ addScriptMessageHandler:script_message_handler_
                                         contentWorld:content_world_
                                                 name:script_handler_name_];
  } else {
    [user_content_controller_ addScriptMessageHandler:script_message_handler_
                                                 name:script_handler_name_];
  }
}

ScopedWKScriptMessageHandler::ScopedWKScriptMessageHandler(
    WKUserContentController* user_content_controller,
    NSString* script_handler_name,
    WKContentWorld* content_world,
    ScriptMessageWithReplyCallback callback)
    : content_world_(content_world),
      user_content_controller_(user_content_controller),
      script_handler_name_(script_handler_name),
      script_message_handler_with_reply_(
          [[CRWScriptMessageHandlerWithReply alloc]
              initWithCallback:callback]) {
  DCHECK(content_world);
  DCHECK(callback);
  [user_content_controller
      addScriptMessageHandlerWithReply:script_message_handler_with_reply_
                          contentWorld:content_world_
                                  name:script_handler_name_];
}

ScopedWKScriptMessageHandler::~ScopedWKScriptMessageHandler() {
  if (content_world_) {
    [user_content_controller_
        removeScriptMessageHandlerForName:script_handler_name_
                             contentWorld:content_world_];
  } else {
    [user_content_controller_
        removeScriptMessageHandlerForName:script_handler_name_];
  }
}
