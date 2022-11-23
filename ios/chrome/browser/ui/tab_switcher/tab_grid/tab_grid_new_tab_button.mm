// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_new_tab_button.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
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

// The size of the small symbol image.
const CGFloat kSmallSymbolSize = 24;
// The size of the large symbol image.
const CGFloat kLargeSymbolSize = 37;

}  // namespace

@interface TabGridNewTabButton ()

// Images for the open new tab button.
@property(nonatomic, strong) UIImage* regularImage;
@property(nonatomic, strong) UIImage* incognitoImage;

@property(nonatomic, strong) UIImage* symbol;

@end

@implementation TabGridNewTabButton

- (instancetype)initWithLargeSize:(BOOL)largeSize {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(UseSymbols());
    CGFloat symbolSize = largeSize ? kLargeSymbolSize : kSmallSymbolSize;
    _symbol = CustomSymbolWithPointSize(kPlusCircleFillSymbol, symbolSize);
    [self setImage:_symbol forState:UIControlStateNormal];
    self.pointerInteractionEnabled = YES;
    self.pointerStyleProvider = CreateLiftEffectCirclePointerStyleProvider();
  }
  return self;
}

- (instancetype)initWithRegularImage:(UIImage*)regularImage
                      incognitoImage:(UIImage*)incognitoImage {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _regularImage = regularImage;
    _incognitoImage = incognitoImage;

    self.pointerInteractionEnabled = YES;
    self.pointerStyleProvider = CreateLiftEffectCirclePointerStyleProvider();
  }
  return self;
}

#pragma mark - Public

- (void)setPage:(TabGridPage)page {
  if (@available(iOS 15, *)) {
    if (UseSymbols()) {
      [self setSymbolPage:page];
    } else {
      [self setIconPage:page];
    }
  } else {
    [self setIconPage:page];
  }
}

#pragma mark - Private

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
- (void)setSymbolPage:(TabGridPage)page API_AVAILABLE(ios(15)) {
  switch (page) {
    case TabGridPageIncognitoTabs:
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_INCOGNITO_TAB);
      [self
          setImage:SymbolWithPalette(
                       self.symbol, @[ UIColor.blackColor, UIColor.whiteColor ])
          forState:UIControlStateNormal];
      break;
    case TabGridPageRegularTabs:
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_TAB);
      [self
          setImage:SymbolWithPalette(self.symbol,
                                     @[
                                       UIColor.blackColor,
                                       [UIColor colorNamed:kStaticBlue400Color]
                                     ])
          forState:UIControlStateNormal];
      break;
    case TabGridPageRemoteTabs:
      break;
  }
  _page = page;
}

@end
