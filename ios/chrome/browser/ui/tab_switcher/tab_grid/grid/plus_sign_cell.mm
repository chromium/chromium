// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/plus_sign_cell.h"

#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PlusSignCell ()
@property(nonatomic, weak) UIView* plusSignView;
@end

@implementation PlusSignCell

// `-dequeueReusableCellWithReuseIdentifier:forIndexPath:` calls this method to
// initialize a cell.
- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    if (UseSymbols()) {
      self.layer.cornerRadius = kGridCellCornerRadius;
    } else {
      self.layer.cornerRadius = kLegacyGridCellCornerRadius;
    }
    self.layer.masksToBounds = YES;
    UIImageView* plusSignView = [[UIImageView alloc]
        initWithImage:[UIImage imageNamed:@"grid_cell_plus_sign"]];
    [self.contentView addSubview:plusSignView];
    plusSignView.translatesAutoresizingMaskIntoConstraints = NO;
    _plusSignView = plusSignView;

    AddSameCenterConstraints(plusSignView, self.contentView);

    self.accessibilityTraits |= UIAccessibilityTraitButton;
    self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  }
  return self;
}

#pragma mark - UIAccessibility

- (BOOL)isAccessibilityElement {
  // This makes the whole cell tappable in VoiceOver rather than the plus sign.
  return YES;
}

#pragma mark - Public

// Updates the theme to either dark or light. Updating is only done if the
// current theme is not the desired theme.
- (void)setTheme:(GridTheme)theme {
  if (_theme == theme)
    return;

  switch (theme) {
    case GridThemeDark:
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_INCOGNITO_TAB);
      break;
    case GridThemeLight:
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_TAB);
      break;
  }

  self.backgroundView = [[UIView alloc] init];
  self.backgroundView.backgroundColor =
      [UIColor colorNamed:kPlusSignCellBackgroundColor];

  // selectedBackgroundView is used for highlighting as well.
  self.selectedBackgroundView = [[UIView alloc] init];
  UIColor* highlightedBackgroundColor =
      [UIColor colorNamed:kTertiaryBackgroundColor];
  self.selectedBackgroundView.backgroundColor = highlightedBackgroundColor;

  _theme = theme;
}

@end
