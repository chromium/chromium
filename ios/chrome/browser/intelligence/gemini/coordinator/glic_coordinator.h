// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_COORDINATOR_GLIC_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_COORDINATOR_GLIC_COORDINATOR_H_

#import "ios/chrome/browser/promos_manager/ui_bundled/promos_manager_ui_handler.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace glic {

// Different GLIC entry points.
typedef NS_ENUM(NSInteger, EntryPoint) {
  EntryPointPromo,
  EntryPointOverflow,
  EntryPointAIHub,
};

}  // namespace glic

// Coordinator that manages the first run and any GLIC triggers.
@interface GLICCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                            fromEntryPoint:(glic::EntryPoint)entryPoint
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The promos manager ui handler to alert about UI changes.
@property(nonatomic, weak) id<PromosManagerUIHandler> promosUIHandler;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_COORDINATOR_GLIC_COORDINATOR_H_
