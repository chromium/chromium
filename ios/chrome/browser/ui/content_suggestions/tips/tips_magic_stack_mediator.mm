// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tips/tips_magic_stack_mediator.h"

#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_module_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_module_state.h"

using segmentation_platform::TipIdentifier;

@interface TipsMagicStackMediator () <TipsModuleAudience>
@end

@implementation TipsMagicStackMediator

- (instancetype)initWithIdentifier:(TipIdentifier)identifier {
  self = [super init];

  if (self) {
    _state = [[TipsModuleState alloc] initWithIdentifier:identifier];
    _state.audience = self;
  }

  return self;
}

- (void)reconfigureWithTipIdentifier:(TipIdentifier)identifier {
  _state = [[TipsModuleState alloc] initWithIdentifier:identifier];
  _state.audience = self;
}

#pragma mark - TipsModuleAudience

- (void)didSelectTip:(TipIdentifier)tip {
  [self.presentationAudience didSelectTip:tip];
}

@end
