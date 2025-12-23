// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/default_browser/ui/default_browser_view.h"

#import "ios/chrome/browser/content_suggestions/default_browser/ui/default_browser_commands.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_detail_view.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_detail_view_configuration.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_view_configuration.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// `DefaultBrowserView` accessibility ID.
NSString* const kDefaultBrowserViewAccessibilityId =
    @"DefaultBrowserViewAccessibilityId";

//  Constants for the icon container view.
constexpr CGFloat kIconSize = 40;

}  // namespace

@interface DefaultBrowserView () <IconDetailViewTapDelegate>
@end

@implementation DefaultBrowserView {
  // Module config.
  DefaultBrowserConfig* _config;
  // The root view of this Default Browser promo module.
  UIView* _contentView;
}

- (instancetype)initWithConfig:(DefaultBrowserConfig*)config {
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

#pragma mark - IconDetailViewTapDelegate

- (void)didTapIconDetailView:(IconDetailView*)view {
  [self.defaultBrowserHandler didTapDefaultBrowserPromo];
}

#pragma mark - Private

// Creates and adds subviews for the promo card.
- (void)createSubviews {
  // Return if the subviews have already been created and added.
  if (!(self.subviews.count == 0)) {
    return;
  }

  self.translatesAutoresizingMaskIntoConstraints = NO;
  _contentView = [self iconDetailView];
  [self addSubview:_contentView];

  AddSameConstraints(_contentView, self);
  return;
}

// Creates and returns an `IconDetailView` configured for the promo card.
- (IconDetailView*)iconDetailView {
  IconDetailViewConfiguration* viewConfig = [IconDetailViewConfiguration
      configurationWithTitleText:[self titleText]
                 descriptionText:[self descriptionText]];
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  viewConfig.iconName = kMulticolorChromeballSymbol;
#else
  viewConfig.iconName = kChromeProductSymbol;
#endif  // BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  viewConfig.iconSource = IconViewSourceType::kSymbol;
  viewConfig.symbolColorPalette = nil;
  viewConfig.symbolBackgroundColor = [UIColor clearColor];
  viewConfig.iconWidth = kIconSize;
  viewConfig.accessibilityIdentifier = kDefaultBrowserViewAccessibilityId;

  IconDetailView* view =
      [[IconDetailView alloc] initWithConfiguration:viewConfig];
  view.tapDelegate = self;
  return view;
}

// Returns the title text for the promo card.
- (NSString*)titleText {
  return l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_TITLE);
}

// Returns the description text for the promo card.
- (NSString*)descriptionText {
  return l10n_util::GetNSString(
      IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_MAGIC_STACK_DESCRIPTION);
}

@end
