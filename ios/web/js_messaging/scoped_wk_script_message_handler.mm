// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/scoped_wk_script_message_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWScriptMessageHandler : NSObject <WKScriptMessageHandler>

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithCallback:(ScriptMessageCallback)callback
    NS_DESIGNATED_INITIALIZER;

@property(nonatomic) ScriptMessageCallback callback;

@end

@implementation CRWScriptMessageHandler

- (instancetype)initWithCallback:(ScriptMessageCallback)callback {
  if (self = [super init]) {
    _callback = callback;
  }
  return self;
}

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  _callback.Run(message);
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
