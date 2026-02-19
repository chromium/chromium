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
#import "ios/chrome/browser/content_suggestions/tips/ui/tips_module_config.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

using segmentation_platform::NameForTipIdentifier;
using segmentation_platform::TipIdentifier;
using segmentation_platform::TipIdentifierForName;

namespace {

// `TipsModuleView` Accessibility ID.
NSString* const kTipsModuleViewID = @"kTipsModuleViewID";

}  // namespace

@implementation TipsModuleView {
  // The current configuration of the Tips module.
  TipsModuleConfig* _config;

  // The root view of the Tips module.
  UIView* _contentView;
}

- (instancetype)initWithConfig:(TipsModuleConfig*)config {
  if ((self = [super init])) {
    _config = config;
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
  BOOL hideSeparator = _config.productImageData.length > 0;
  [_contentViewDelegate updateSeparatorVisibility:hideSeparator];

  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.accessibilityIdentifier = kTipsModuleViewID;

  _contentView = [self createIconDetailView:_config.identifier];
  [self addSubview:_contentView];
  AddSameConstraints(_contentView, self);
  return;
}

// Creates and returns an `IconDetailView` configured for the `tip`.
- (IconDetailView*)createIconDetailView:(TipIdentifier)tip {
  IconDetailView* view = [[IconDetailView alloc] initWithConfig:_config];
  view.identifier = base::SysUTF8ToNSString(NameForTipIdentifier(tip));
  view.tapDelegate = _config;
  return view;
}

@end
