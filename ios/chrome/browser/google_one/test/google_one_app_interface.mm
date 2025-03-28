// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/google_one/test/google_one_app_interface.h"

#import "ios/chrome/browser/google_one/test/test_google_one_controller.h"

@implementation GoogleOneAppInterface

+ (void)overrideGoogleOneController {
  ios::provider::SetGoogleOneControllerFactory(
      [TestGoogleOneControllerFactory sharedInstance]);
}

+ (void)restoreGoogleOneController {
  ios::provider::SetGoogleOneControllerFactory(nil);
}

@end
