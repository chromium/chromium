// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_new_tab_button.h"

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The size of the small symbol image.
const CGFloat kSmallSymbolSize = 24;
// The size of the large symbol image.
const CGFloat kLargeSymbolSize = 37;

}  // namespace

@interface TabGridNewTabButton ()

@property(nonatomic, strong) UIImage* symbol;

@end

@implementation TabGridNewTabButton

- (instancetype)initWithLargeSize:(BOOL)largeSize {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    CGFloat symbolSize = largeSize ? kLargeSymbolSize : kSmallSymbolSize;
    _symbol = CustomSymbolWithPointSize(kPlusCircleFillSymbol, symbolSize);
    [self setImage:_symbol forState:UIControlStateNormal];
    self.pointerInteractionEnabled = YES;
    self.pointerStyleProvider = CreateLiftEffectCirclePointerStyleProvider();
  }
  return self;
}

#pragma mark - Public

- (void)setPage:(TabGridPage)page {
  [self setSymbolPage:page];
}

#pragma mark - Private

// Sets page using a symbol image.
- (void)setSymbolPage:(TabGridPage)page {
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
    case TabGridPageTabGroups:
      break;
  }
  _page = page;
}

@end
