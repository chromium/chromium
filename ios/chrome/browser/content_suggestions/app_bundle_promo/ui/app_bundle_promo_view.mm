// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/app_bundle_promo/ui/app_bundle_promo_view.h"

#import "ios/chrome/browser/content_suggestions/app_bundle_promo/public/app_bundle_promo_constants.h"
#import "ios/chrome/browser/content_suggestions/app_bundle_promo/ui/app_bundle_promo_audience.h"
#import "ios/chrome/browser/content_suggestions/app_bundle_promo/ui/app_bundle_promo_config.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/public/magic_stack_constants.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_detail_view.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_detail_view_configuration.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_view.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_view_configuration.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_updating.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

/// Constants related to the icon container view.
constexpr CGFloat kAppBundleIconSize = 40;

}  // namespace

@interface AppBundlePromoView () <IconDetailViewTapDelegate,
                                  NewTabPageColorUpdating>
@end

@implementation AppBundlePromoView {
  // The current configuration of the App Bundle promo module.
  AppBundlePromoConfig* _config;
  // The root view of the App Bundle promo module.
  UIView* _contentView;
  // The background color of the container if the icon is rendered in a
  // container.
  UIColor* _containerBackgroundColor;
}

- (instancetype)initWithConfig:(AppBundlePromoConfig*)config {
  if ((self = [super initWithFrame:CGRectZero])) {
    _config = config;
    if (IsNTPBackgroundCustomizationEnabled()) {
      [self registerForTraitChanges:@[ NewTabPageTrait.class ]
                         withAction:@selector(applyBackgroundColors)];
    }
    [self applyBackgroundColors];
  }
  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];
  [self createSubviews];
}

#pragma mark - IconDetailViewTapDelegate

- (void)didTapIconDetailView:(IconDetailView*)view {
  [self.audience didSelectAppBundlePromo];
}

#pragma mark - NewTabPageColorUpdating

- (void)applyBackgroundColors {
  NewTabPageColorPalette* colorPalette =
      [self.traitCollection objectForNewTabPageTrait];

  _containerBackgroundColor = colorPalette ? colorPalette.tertiaryColor
                                           : [UIColor colorNamed:kGrey100Color];

  // Redraws the view by removing and recreating the content view.
  [_contentView removeFromSuperview];
  [self createSubviews];
}

#pragma mark - Private

// Creates and adds subviews for the promo card.
- (void)createSubviews {
  // Return if the subviews have already been created and added.
  if (!(self.subviews.count == 0)) {
    return;
  }

  self.translatesAutoresizingMaskIntoConstraints = NO;
  _contentView = [self createIconDetailView];
  [self addSubview:_contentView];
  AddSameConstraints(_contentView, self);
  return;
}

// Creates and returns an `IconDetailView` configured for the promo card.
- (IconDetailView*)createIconDetailView {
  IconDetailViewConfiguration* viewConfig = [IconDetailViewConfiguration
      configurationWithTitleText:[self titleText]
                 descriptionText:[self descriptionText]];
  viewConfig.iconName = _config.imageName;
  viewConfig.iconSource = IconViewSourceType::kImage;
  viewConfig.iconContainerBackgroundColor = _containerBackgroundColor;
  viewConfig.iconWidth = kAppBundleIconSize;
  viewConfig.accessibilityIdentifier = kAppBundlePromoViewID;

  IconDetailView* view =
      [[IconDetailView alloc] initWithConfiguration:viewConfig];
  view.tapDelegate = self;
  return view;
}

// Returns the title text for the promo card.
- (NSString*)titleText {
  return l10n_util::GetNSString(
      IDS_IOS_MAGIC_STACK_APP_BUNDLE_PROMO_CARD_TITLE);
}

// Returns the description text for the promo card.
- (NSString*)descriptionText {
  return (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
             ? l10n_util::GetNSString(
                   IDS_IOS_MAGIC_STACK_APP_BUNDLE_PROMO_CARD_IPAD_DESCRIPTION)
             : l10n_util::GetNSString(
                   IDS_IOS_MAGIC_STACK_APP_BUNDLE_PROMO_CARD_IPHONE_DESCRIPTION);
}

@end
