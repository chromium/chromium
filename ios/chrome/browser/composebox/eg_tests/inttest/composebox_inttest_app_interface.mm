// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/eg_tests/inttest/composebox_inttest_app_interface.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/composebox/eg_tests/inttest/composebox_inttest_coordinator.h"
#import "ios/chrome/test/earl_grey/chrome_coordinator_app_interface.h"

@implementation ComposeboxInttestAppInterface

#pragma mark - Private

/// Returns the inttest coordinator.
+ (ComposeboxInttestCoordinator*)inttestCoordinator {
  ComposeboxInttestCoordinator* coordinator =
      base::apple::ObjCCastStrict<ComposeboxInttestCoordinator>(
          ChromeCoordinatorAppInterface.coordinator);
  return coordinator;
}

@end
