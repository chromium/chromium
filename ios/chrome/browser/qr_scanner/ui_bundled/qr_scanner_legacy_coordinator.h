// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_QR_SCANNER_UI_BUNDLED_QR_SCANNER_LEGACY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_QR_SCANNER_UI_BUNDLED_QR_SCANNER_LEGACY_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// QRScannerLegacyCoordinator presents the public interface for the QR scanner
// feature.
@interface QRScannerLegacyCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

// The view controller this coordinator was initialized with.
@property(weak, nonatomic) UIViewController* baseViewController;

@end

#endif  // IOS_CHROME_BROWSER_QR_SCANNER_UI_BUNDLED_QR_SCANNER_LEGACY_COORDINATOR_H_
