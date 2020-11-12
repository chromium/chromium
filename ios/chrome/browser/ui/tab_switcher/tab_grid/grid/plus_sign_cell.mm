// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/plus_sign_cell.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PlusSignCell ()
@property(nonatomic, weak) UIView* plusSignView;
@end

@implementation PlusSignCell

// |-dequeueReusableCellWithReuseIdentifier:forIndexPath:| calls this method to
// initialize a cell.
- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    UIView* contentView = self.contentView;
    contentView.layer.cornerRadius = kGridCellCornerRadius;
    contentView.layer.masksToBounds = YES;
    UIImageView* plusSignView = [[UIImageView alloc]
        initWithImage:[UIImage imageNamed:@"grid_cell_plus_sign"]];
    [self.contentView addSubview:plusSignView];
    plusSignView.translatesAutoresizingMaskIntoConstraints = NO;
    _plusSignView = plusSignView;

    AddSameCenterConstraints(plusSignView, self.contentView);
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

  switch (theme) {
    // This is necessary for iOS 13 because on iOS 13, this will return
    // the dynamic color (which will then be colored with the user
    // interface style).
    // On iOS 12, this will always return the dynamic color in the light
    // variant.
    case GridThemeLight:
      self.contentView.backgroundColor =
          [UIColor colorNamed:kPlusSignCellBackgroundColor];
      break;
    // These dark-theme specific colorsets should only be used for iOS 12
    // dark theme, as they will be removed along with iOS 12.
    // TODO (crbug.com/981889): The following lines will be removed
    // along with iOS 12
    case GridThemeDark:
      self.contentView.backgroundColor =
          [UIColor colorNamed:kPlusSignCellBackgroundDarkColor];
      break;
  }

  if (@available(iOS 13, *)) {
    // When iOS 12 is dropped, only the next line is needed for styling.
    // Every other check for |GridThemeDark| can be removed, as well as
    // the dark theme specific assets.
    self.overrideUserInterfaceStyle = (theme == GridThemeDark)
                                          ? UIUserInterfaceStyleDark
                                          : UIUserInterfaceStyleUnspecified;
  }
  _theme = theme;
}

@end
