// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/root_view_controller_test.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

// Sets the current key window's rootViewController and saves a pointer to
// the original VC to allow restoring it at the end of the test.
void RootViewControllerTest::SetRootViewController(
    UIViewController* new_root_view_controller) {
  original_root_view_controller_ = [GetAnyKeyWindow() rootViewController];
  GetAnyKeyWindow().rootViewController = new_root_view_controller;
}

void RootViewControllerTest::TearDown() {
  if (original_root_view_controller_) {
    GetAnyKeyWindow().rootViewController = original_root_view_controller_;
    original_root_view_controller_ = nil;
  }
  PlatformTest::TearDown();
}
