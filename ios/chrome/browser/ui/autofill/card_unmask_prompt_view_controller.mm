// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_controller.h"

#import "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller.h"
#import "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CardUnmaskPromptViewController () {
  // Owns `self`.
  autofill::CardUnmaskPromptViewBridge* _bridge;  // weak
}

@end

@implementation CardUnmaskPromptViewController

- (instancetype)initWithBridge:(autofill::CardUnmaskPromptViewBridge*)bridge {
  self = [super initWithStyle:ChromeTableViewStyle()];

  if (self) {
    _bridge = bridge;
  }

  return self;
}

@end
