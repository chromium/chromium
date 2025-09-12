// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_COORDINATOR_H_

#import <PhotosUI/PhotosUI.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class Browser;
enum class AIMPrototypeEntrypoint;

/// AIMPrototypeCoordinator presents AIM with an omnibox.
@interface AIMPrototypeCoordinator : ChromeCoordinator

/// Init the AIM Prototype opened from `entrypoint` with an optional `query` in
/// the omnibox.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                entrypoint:(AIMPrototypeEntrypoint)entrypoint
                                     query:(NSString*)query
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_COORDINATOR_H_
