// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/tips/ui/tips_module_view.h"

#import <optional>
#import <string>

#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module_content_view_delegate.h"
#import "ios/chrome/browser/content_suggestions/tips/ui/tips_module_state.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

using segmentation_platform::NameForTipIdentifier;
using segmentation_platform::TipIdentifier;
using segmentation_platform::TipIdentifierForName;

namespace {

// `TipsModuleView` Accessibility ID.
NSString* const kTipsModuleViewID = @"kTipsModuleViewID";

}  // namespace

@implementation TipsModuleView {
  // The current state of the Tips module.
  TipsModuleState* _state;

  // The root view of the Tips module.
  UIView* _contentView;
}

- (instancetype)initWithState:(TipsModuleState*)state {
  if ((self = [super init])) {
    _state = state;
  }
  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  [self createSubviews];
}

#pragma mark - Private methods

- (void)createSubviews {
  if (_contentView) {
    [_contentView removeFromSuperview];
  }

  // Determine whether the separator should be hidden.
  BOOL hideSeparator = _state.productImageData.length > 0;
  [_contentViewDelegate updateSeparatorVisibility:hideSeparator];

  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.accessibilityIdentifier = kTipsModuleViewID;

  _contentView = [self createIconDetailView:_state.identifier];
  [self addSubview:_contentView];
  AddSameConstraints(_contentView, self);
  return;
}

// Creates and returns an `IconDetailView` configured for the `tip`.
- (IconDetailView*)createIconDetailView:(TipIdentifier)tip {
  IconDetailView* view = [[IconDetailView alloc] initWithConfiguration:_state];
  view.identifier = base::SysUTF8ToNSString(NameForTipIdentifier(tip));
  view.tapDelegate = _state;
  return view;
}

@end
