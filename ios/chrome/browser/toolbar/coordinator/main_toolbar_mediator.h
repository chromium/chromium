// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_COORDINATOR_MAIN_TOOLBAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TOOLBAR_COORDINATOR_MAIN_TOOLBAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

@class MainToolbarMediator;
class PrefService;

/// Delegate protocol for MainToolbarMediator.
@protocol MainToolbarMediatorDelegate <NSObject>

/// Notifies the delegate that the omnibox position changed.
- (void)mainToolbarMediatorDidChangeOmniboxPosition:
    (MainToolbarMediator*)mediator;

@end

/// Mediator for the main toolbar, observing omnibox position.
@interface MainToolbarMediator : NSObject

/// The delegate for this mediator.
@property(nonatomic, weak) id<MainToolbarMediatorDelegate> delegate;

/// Initializes the mediator with the preference service.
- (instancetype)initWithPrefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

/// Whether the omnibox is in the bottom position.
- (BOOL)isOmniboxInBottomPosition;

/// Disconnects any observations and cleans up objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_COORDINATOR_MAIN_TOOLBAR_MEDIATOR_H_
