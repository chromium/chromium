// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MINI_MAP_UI_BUNDLED_MINI_MAP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_MINI_MAP_UI_BUNDLED_MINI_MAP_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import <UIKit/UIKit.h>

namespace web {
class WebState;
}

enum class MiniMapMode {
  kMap,
  kDirections,
};

// A coordinator to display mini maps showing an address.
@interface MiniMapCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  webState:(web::WebState*)webState
                                      text:(NSString*)text
                           consentRequired:(BOOL)consentRequired
                                      mode:(MiniMapMode)mode
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_MINI_MAP_UI_BUNDLED_MINI_MAP_COORDINATOR_H_
