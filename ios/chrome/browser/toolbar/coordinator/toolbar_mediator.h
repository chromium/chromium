// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_COORDINATOR_TOOLBAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TOOLBAR_COORDINATOR_TOOLBAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/toolbar/ui/toolbar_mutator.h"

class FullscreenController;
@protocol ToolbarConsumer;
@protocol ToolbarHeightDelegate;
class WebNavigationBrowserAgent;
class WebStateList;

namespace web {
class WebState;
}

// Mediator for the toolbar.
@interface ToolbarMediator : NSObject <ToolbarMutator>

// The consumer for this mediator.
@property(nonatomic, weak) id<ToolbarConsumer> consumer;

// Helper for web navigation.
@property(nonatomic, assign) WebNavigationBrowserAgent* navigationBrowserAgent;

// Delegate that handles the toolbars height.
@property(nonatomic, weak) id<ToolbarHeightDelegate> toolbarHeightDelegate;

// Initializer.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                fullscreenController:(FullscreenController*)fullscreenController
                         topPosition:(BOOL)topPosition
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Updates the consumer with the current state of the web state.
- (void)updateConsumerWithWebState:(web::WebState*)webState;

// Disconnects observations.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_COORDINATOR_TOOLBAR_MEDIATOR_H_
