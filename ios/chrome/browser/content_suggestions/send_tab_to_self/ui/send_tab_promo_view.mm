// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/send_tab_to_self/ui/send_tab_promo_view.h"

#import "base/check_op.h"
#import "ios/chrome/browser/content_suggestions/send_tab_to_self/ui/send_tab_promo_audience.h"
#import "ios/chrome/browser/content_suggestions/send_tab_to_self/ui/send_tab_promo_item.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/standalone_module_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@interface SendTabPromoView () <StandaloneModuleViewTapDelegate>

@end

@implementation SendTabPromoView {
  // The current configuration of the Send Tab promo module.
  SendTabPromoItem* _config;
  // The root view of the Send Tab promo module.
  UIView* _contentView;
}

- (instancetype)initWithConfig:(SendTabPromoItem*)config {
  if ((self = [super initWithFrame:CGRectZero])) {
    _config = config;
  }
  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];
  [self createSubviews];
}

#pragma mark - StandaloneModuleViewTapDelegate

- (void)buttonTappedForModuleType:(ContentSuggestionsModuleType)moduleType {
  CHECK_EQ(_config.type, moduleType);
  [self.audience didSelectSendTabPromo];
}

#pragma mark - Private

// Creates and adds subviews for the promo card.
- (void)createSubviews {
  // Return if the subviews have already been created and added.
  if (!(self.subviews.count == 0)) {
    return;
  }

  self.translatesAutoresizingMaskIntoConstraints = NO;

  _contentView = [self standaloneModuleView];
  [self addSubview:_contentView];
  AddSameConstraints(_contentView, self);
  return;
}

// Creates and returns a `StandaloneModuleView` configured for the promo card.
- (StandaloneModuleView*)standaloneModuleView {
  StandaloneModuleView* view =
      [[StandaloneModuleView alloc] initWithFrame:CGRectZero];
  [view configureView:_config];
  view.tapDelegate = self;
  return view;
}

@end
