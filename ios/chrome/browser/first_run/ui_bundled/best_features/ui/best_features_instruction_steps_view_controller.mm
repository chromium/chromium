// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_instruction_steps_view_controller.h"

#import "ios/chrome/browser/first_run/public/best_features_item.h"

@implementation BestFeaturesInstructionStepsViewController

- (instancetype)initWithItem:(BestFeaturesItem*)item {
  self = [super init];
  if (self) {
    self.titleString = item.title;
    self.steps = item.instructionSteps;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
}

@end
