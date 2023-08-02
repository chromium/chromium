// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_coordinator.h"

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_view_controller.h"

@interface FamilyPickerCoordinator ()

// Main view controller for this coordinator.
@property(nonatomic, strong) FamilyPickerViewController* viewController;

@end

@implementation FamilyPickerCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  return self;
}

- (void)start {
  [super start];
  self.viewController = [[FamilyPickerViewController alloc] init];
  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  self.viewController = nil;
}

@end
