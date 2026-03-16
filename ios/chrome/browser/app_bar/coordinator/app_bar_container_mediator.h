// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_CONTAINER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_CONTAINER_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol FullscreenUIElement;
class FullscreenController;

// Mediator for the AppBarContainer.
@interface AppBarContainerMediator : NSObject

// The consumer of this mediator.
@property(nonatomic, weak) id<FullscreenUIElement> consumer;

// Initializes the mediator.
- (instancetype)initWithRegularFullscreenController:
                    (FullscreenController*)regularFullscreenController
                      incognitoFullscreenController:
                          (FullscreenController*)incognitoFullscreenController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

// Resets the incognito fullscreen controller.
- (void)setIncognitoFullscreenController:
    (FullscreenController*)incognitoFullscreenController;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_CONTAINER_MEDIATOR_H_
