// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_BWG_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_BWG_COORDINATOR_H_

#import "ios/chrome/browser/promos_manager/ui_bundled/promos_manager_ui_handler.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace bwg {

// Different BWG entry points.
// Logged as IOSBWGEntryPoint enum for the IOS.BWG.EntryPoint histogram.
// LINT.IfChange(IOSBWGEntryPoint)
typedef NS_ENUM(NSInteger, EntryPoint) {
  EntryPointPromo,
  EntryPointOverflow,
  EntryPointAIHub,
  EntryPointOmniboxChip,
  kMaxValue = EntryPointOmniboxChip,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSBWGEntryPoint)

}  // namespace bwg

// Coordinator that manages the first run and any BWG triggers.
@interface BWGCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                            fromEntryPoint:(bwg::EntryPoint)entryPoint
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The promos manager ui handler to alert about UI changes.
@property(nonatomic, weak) id<PromosManagerUIHandler> promosUIHandler;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_BWG_COORDINATOR_H_
