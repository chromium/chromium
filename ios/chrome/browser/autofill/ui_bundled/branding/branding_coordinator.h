// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BRANDING_BRANDING_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BRANDING_BRANDING_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class BrandingViewController;

// Creates and manages the branding view controller and its delegate that
// controls the visibility of the branding.
@interface BrandingCoordinator : ChromeCoordinator

// The actual branding view controller created by this coordinator; a wrapper
// for the autofill branding icon.
@property(nonatomic, strong, readonly) BrandingViewController* viewController;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BRANDING_BRANDING_COORDINATOR_H_
