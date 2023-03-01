// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LINK_TO_TEXT_LINK_TO_TEXT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_LINK_TO_TEXT_LINK_TO_TEXT_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/link_to_text/link_to_text_delegate.h"

@protocol ActivityServiceCommands;
@protocol EditMenuAlertDelegate;
class WebStateList;

// Mediator that mediates between the browser container views and the
// link_to_text tab helpers.
@interface LinkToTextMediator : NSObject <LinkToTextDelegate>

// Initializer for a mediator. `webStateList` is the WebStateList for the
// Browser whose content is shown within the BrowserContainerConsumer. It must
// be non-null. `consumer` is the consumer of link-to-text updates.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The delegate to present error message alerts.
@property(nonatomic, weak) id<EditMenuAlertDelegate> alertDelegate;

// The handler for activity services commands.
@property(nonatomic, weak) id<ActivityServiceCommands> activityServiceHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_LINK_TO_TEXT_LINK_TO_TEXT_MEDIATOR_H_
