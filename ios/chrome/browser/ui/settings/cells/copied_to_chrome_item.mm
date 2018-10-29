// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/copied_to_chrome_item.h"

#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_constants.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used on the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 16;

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 16;
}  // namespace

@implementation CopiedToChromeItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [CopiedToChromeCell class];
  }
  return self;
}

@end

@implementation CopiedToChromeCell

@synthesize textLabel = _textLabel;
@synthesize button = _button;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    UIView* contentView = self.contentView;

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.text =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_DESCRIBE_LOCAL_COPY);
    _textLabel.font = [UIFont systemFontOfSize:kUIKitMainFontSize];
    _textLabel.textColor = UIColorFromRGB(kUIKitMainTextColor);
    [_textLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [contentView addSubview:_textLabel];

    _button = [UIButton buttonWithType:UIButtonTypeCustom];
    [_button setTitleColor:UIColorFromRGB(kUIKitFooterLinkColor)
                  forState:UIControlStateNormal];

    _button.translatesAutoresizingMaskIntoConstraints = NO;
    [_button
        setTitle:l10n_util::GetNSString(IDS_AUTOFILL_CLEAR_LOCAL_COPY_BUTTON)
        forState:UIControlStateNormal];
    [contentView addSubview:_button];

    // Set up the constraints.
    [NSLayoutConstraint activateConstraints:@[
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_textLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_button.leadingAnchor
                                   constant:-kHorizontalPadding],
      [_textLabel.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_button.trailingAnchor constraintEqualToAnchor:contentView.trailingAnchor
                                             constant:-kHorizontalPadding],
      [_button.firstBaselineAnchor
          constraintEqualToAnchor:_textLabel.firstBaselineAnchor],
    ]];
    AddOptionalVerticalPadding(contentView, _textLabel, kVerticalPadding);
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [self.button removeTarget:nil
                     action:nil
           forControlEvents:UIControlEventAllEvents];
}

@end
