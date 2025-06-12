// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MINI_MAP_COORDINATOR_MINI_MAP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_MINI_MAP_COORDINATOR_MINI_MAP_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

enum class MiniMapMode {
  kMap,
  kDirections,
};

// A coordinator to display mini maps showing an address.
@interface MiniMapCoordinator : ChromeCoordinator

// Create a MiniMapCoordinator to display `text` or `URL` in `mode`.
// If `consentRequired`, and iph is displayed on first display.
// - `text` must be an address
// - `URL` must be a a universal link to maps.
// Exactly one of these must be set.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      text:(NSString*)text
                                       url:(NSURL*)URL
                                   withIPH:(BOOL)withIPH
                                      mode:(MiniMapMode)mode
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_MINI_MAP_COORDINATOR_MINI_MAP_COORDINATOR_H_
