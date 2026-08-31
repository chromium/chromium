// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AR_QUICK_LOOK_MEDIATOR_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AR_QUICK_LOOK_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol ARQuickLookMediatorDelegate;
class WebStateList;

// Mediator that monitors the active WebState and notifies the delegate when
// the USDZ preview should be dismissed (e.g., active tab navigation, active
// tab change, or active tab hidden/destroyed).
@interface ARQuickLookMediator : NSObject

// Delegate to receive dismissal requests.
@property(nonatomic, weak) id<ARQuickLookMediatorDelegate> delegate;

// Initializes the mediator with the WebStateList to observe.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnects WebStateList and WebState observations.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AR_QUICK_LOOK_MEDIATOR_H_
