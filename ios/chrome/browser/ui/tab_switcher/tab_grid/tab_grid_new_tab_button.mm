// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_new_tab_button.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/icons/chrome_symbol.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The size of the symbol image.
const CGFloat kSymbolNewTabPointSize = 24;

}  // namespace

@interface TabGridNewTabButton ()

// Images for the open new tab button.
@property(nonatomic, strong) UIImage* regularImage;
@property(nonatomic, strong) UIImage* incognitoImage;

// Image for the open new tab button.
@property(nonatomic, strong) UIImage* tabGridNewTabImage;

// View used as a background for the open new tab button.
@property(nonatomic, strong) UIView* circleBackgroundView;

@end

@implementation TabGridNewTabButton

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(UseSymbols());
    _tabGridNewTabImage = CustomSymbolWithPointSize(kPlusCircleFillSymbol,
                                                    kSymbolNewTabPointSize);
    self.pointerInteractionEnabled = YES;
    self.pointerStyleProvider = CreateLiftEffectCirclePointerStyleProvider();
    [self configureCircleBackgroundView];
  }
  return self;
}

- (instancetype)initWithRegularImage:(UIImage*)regularImage
                      incognitoImage:(UIImage*)incognitoImage {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(!UseSymbols());
    _regularImage = regularImage;
    _incognitoImage = incognitoImage;

    self.pointerInteractionEnabled = YES;
    self.pointerStyleProvider = CreateLiftEffectCirclePointerStyleProvider();
  }
  return self;
}

#pragma mark - Public

- (void)setPage:(TabGridPage)page {
  UseSymbols() ? [self setSymbolPage:page] : [self setIconPage:page];
}

#pragma mark - Private

// Adds a circle background view below the image.
- (void)configureCircleBackgroundView {
  UIView* circleBackgroundView = [[UIView alloc] init];
  circleBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  circleBackgroundView.userInteractionEnabled = NO;
  circleBackgroundView.layer.cornerRadius =
      self.tabGridNewTabImage.size.width / 2;

  // Make sure that the circleBackgroundView is below the image.
  [self insertSubview:circleBackgroundView belowSubview:self.imageView];
  AddSameCenterConstraints(self.imageView, circleBackgroundView);
  [NSLayoutConstraint activateConstraints:@[
    [circleBackgroundView.widthAnchor
        constraintEqualToAnchor:self.imageView.widthAnchor],
    [circleBackgroundView.heightAnchor
        constraintEqualToAnchor:self.imageView.heightAnchor],
  ]];

  self.circleBackgroundView = circleBackgroundView;
}

// Sets page using icon images.
- (void)setIconPage:(TabGridPage)page {
  // self.page is inited to 0 (i.e. TabGridPageIncognito) so do not early return
  // here, otherwise when app is launched in incognito mode the image will be
  // missing.
  UIImage* renderedImage;
  switch (page) {
    case TabGridPageIncognitoTabs:
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_INCOGNITO_TAB);
      renderedImage = [_incognitoImage
          imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
      break;
    case TabGridPageRegularTabs:
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_TAB);
      renderedImage = [_regularImage
          imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
      break;
    case TabGridPageRemoteTabs:
      break;
  }
  _page = page;
  [self setImage:renderedImage forState:UIControlStateNormal];
}

// Sets page using a symbol image.
- (void)setSymbolPage:(TabGridPage)page {
  switch (page) {
    case TabGridPageIncognitoTabs:
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_INCOGNITO_TAB);
      self.tintColor = UIColor.whiteColor;
      self.circleBackgroundView.backgroundColor = UIColor.clearColor;
      break;
    case TabGridPageRegularTabs:
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_TAB);
      self.tintColor = [UIColor colorNamed:kBlueColor];
      self.circleBackgroundView.backgroundColor = UIColor.whiteColor;
      break;
    case TabGridPageRemoteTabs:
      break;
  }
  _page = page;
  [self setImage:self.tabGridNewTabImage forState:UIControlStateNormal];
}

@end
