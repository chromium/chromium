// Copyright 2021 The Chromium Authors. All rights reserved.
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

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
ScopedWKScriptMessageHandler::ScopedWKScriptMessageHandler(
    WKUserContentController* user_content_controller,
    NSString* script_handler_name,
    WKContentWorld* content_world,
    ScriptMessageCallback callback) API_AVAILABLE(ios(14.0))
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
#endif  // defined(__IPHONE14_0)

ScopedWKScriptMessageHandler::~ScopedWKScriptMessageHandler() {
#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
  if (@available(iOS 14, *)) {
    if (content_world_) {
      [user_content_controller_
          removeScriptMessageHandlerForName:script_handler_name_
                               contentWorld:content_world_];
      return;
    }
  }
#endif  // defined(__IPHONE14_0)

  [user_content_controller_
      removeScriptMessageHandlerForName:script_handler_name_];
}
