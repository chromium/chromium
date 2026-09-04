// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_PASS_KIT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_PASS_KIT_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol WebContentCommands;
class WebStateList;

// Mediator that monitors the active WebState in WebStateList and dispatches
// WebContentCommands to dismiss any presented PassKit UI when navigation or
// WebState switching occurs.
@interface PassKitMediator : NSObject

// Designated initializer.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                   webContentHandler:(id<WebContentCommands>)webContentHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects observations.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_PASS_KIT_MEDIATOR_H_
