// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_COORDINATOR_MAIN_TOOLBAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TOOLBAR_COORDINATOR_MAIN_TOOLBAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

@class BrowserLayoutState;
class PrefService;

/// Mediator for the main toolbar, observing omnibox position.
@interface MainToolbarMediator : NSObject

/// Initializes the mediator with the preference service and browser layout
/// state.
- (instancetype)initWithPrefService:(PrefService*)prefService
                 browserLayoutState:(BrowserLayoutState*)browserLayoutState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Disconnects any observations and cleans up objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_COORDINATOR_MAIN_TOOLBAR_MEDIATOR_H_
