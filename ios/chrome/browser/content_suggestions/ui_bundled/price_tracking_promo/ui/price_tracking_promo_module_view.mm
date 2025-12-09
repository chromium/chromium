// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/price_tracking_promo/ui/price_tracking_promo_module_view.h"

#import "base/check_op.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/standalone_module_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/price_tracking_promo/ui/price_tracking_promo_commands.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/price_tracking_promo/ui/price_tracking_promo_item.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@interface PriceTrackingPromoModuleView () <StandaloneModuleViewTapDelegate>

@end

@implementation PriceTrackingPromoModuleView {
  // Content view for the Price Tracking Promo module.
  StandaloneModuleView* _contentView;
}

- (void)configureView:(PriceTrackingPromoItem*)config {
  if (!config) {
    return;
  }

  if (!(self.subviews.count == 0)) {
    return;
  }

  self.translatesAutoresizingMaskIntoConstraints = NO;

  _contentView = [self standaloneModuleView:config];
  [self addSubview:_contentView];
  AddSameConstraints(_contentView, self);
  return;
}

#pragma mark - StandaloneModuleViewTapDelegate

- (void)buttonTappedForModuleType:(ContentSuggestionsModuleType)moduleType {
  CHECK_EQ(moduleType, ContentSuggestionsModuleType::kPriceTrackingPromo);
  [self.priceTrackingPromoHandler allowPriceTrackingNotifications];
}

#pragma mark - PriceTrackingPromoFaviconConsumer

- (void)priceTrackingPromoFaviconCompleted:(UIImage*)faviconImage {
  [_contentView updateProductImageViewWithFavicon:faviconImage];
}

#pragma mark - Private

// Returns a configured `StandaloneModuleView` for a `config`.
- (StandaloneModuleView*)standaloneModuleView:(PriceTrackingPromoItem*)config {
  StandaloneModuleView* view = [[StandaloneModuleView alloc] init];
  [view configureView:config];
  view.tapDelegate = self;
  return view;
}

@end
