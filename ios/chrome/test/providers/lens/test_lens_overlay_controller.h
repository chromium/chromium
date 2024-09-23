// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_LENS_TEST_LENS_OVERLAY_CONTROLLER_H_
#define IOS_CHROME_TEST_PROVIDERS_LENS_TEST_LENS_OVERLAY_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/public/provider/chrome/browser/lens/lens_overlay_api.h"

// Fake lens overlay controller used for testing.
@interface TestLensOverlayController : UIViewController <ChromeLensOverlay>
@end

#endif  // IOS_CHROME_TEST_PROVIDERS_LENS_TEST_LENS_OVERLAY_CONTROLLER_H_
