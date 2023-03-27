// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_USER_CONTENT_CONTROLLER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_USER_CONTENT_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVUserScript;

// Allows injecting custom scripts into CWVWebView created with the
// configuration.
CWV_EXPORT
@interface CWVUserContentController : NSObject

// The user scripts associated with the configuration.
@property(nonatomic, copy, readonly) NSArray<CWVUserScript*>* userScripts;

- (instancetype)init NS_UNAVAILABLE;

// Adds a user script.
- (void)addUserScript:(CWVUserScript*)userScript;

// Removes all associated user scripts.
- (void)removeAllUserScripts;

// Adds a message handler for messages sent from JavaScript for all CWVWebViews
// associated with a CWVWebViewConfiguration referencing this
// `userContentController`.
// `handler` will be called each time a message is sent with the corresponding
// value of `command`. To send messages from JavaScript, use the WebKit
// message handler `CWVWebViewMessage` and provide values for the `command` and
// `payload` keys.
// `command` must be a string and match the registered handler `command` string
// `payload` must be a dictionary.
//
// Example call from JavaScript:
//
//  let message = {
//    'command': 'myFeatureMessage',
//    'payload' : {'key1':'value1', 'key2':42}
//  }
//  window.webkit.messageHandlers['CWVWebViewMessage'].postMessage(message);
//
// NOTE: Only a single `handler` may be registered for a given `command` per
// user content controller.
- (void)addMessageHandler:(void (^)(NSDictionary* payload))handler
               forCommand:(NSString*)command;

// Removes the message handler associated with `command` previously added with
// `addMessageHandler:forCommand:`.
- (void)removeMessageHandlerForCommand:(NSString*)nsCommand;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_USER_CONTENT_CONTROLLER_H_
