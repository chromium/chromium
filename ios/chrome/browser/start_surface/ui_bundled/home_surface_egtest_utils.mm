// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/start_surface/ui_bundled/home_surface_egtest_utils.h"

#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"

namespace {
// HomeSurfaceDuration key, should match the one in `system_flags`.
NSString* const kHomeSurfaceDuration = @"HomeSurfaceDuration";
}  // namespace

void MakeHomeSurfaceOpenImmediately() {
  [ChromeEarlGrey setUserDefaultsObject:@-1 forKey:kHomeSurfaceDuration];
}

void ResetMakeHomeSurfaceOpenImmediately() {
  [ChromeEarlGrey removeUserDefaultsObjectForKey:kHomeSurfaceDuration];
}
