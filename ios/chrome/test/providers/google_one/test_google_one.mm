// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/google_one/test_google_one.h"

#import <UIKit/UIKit.h>

namespace {
id<GoogleOneControllerFactory> g_google_one_controller_factory;
}

@implementation GoogleOneConfiguration

@end

namespace ios {
namespace provider {

id<GoogleOneController> CreateGoogleOneController(
    GoogleOneConfiguration* configuration) {
  return [g_google_one_controller_factory
      createControllerWithConfiguration:configuration];
}

namespace test {

void SetGoogleOneControllerFactory(id<GoogleOneControllerFactory> factory) {
  g_google_one_controller_factory = factory;
}

}  // namespace test
}  // namespace provider
}  // namespace ios
