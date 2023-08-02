// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/orientation_limiting_navigation_controller.h"

#import "ui/base/device_form_factor.h"

@implementation OrientationLimitingNavigationController

- (NSUInteger)supportedInterfaceOrientations {
  return (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
             ? [super supportedInterfaceOrientations]
             : UIInterfaceOrientationMaskPortrait;
}

- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation {
  return (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
             ? [super preferredInterfaceOrientationForPresentation]
             : UIInterfaceOrientationPortrait;
}

- (BOOL)shouldAutorotate {
  return (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
             ? [super shouldAutorotate]
             : NO;
}

@end
