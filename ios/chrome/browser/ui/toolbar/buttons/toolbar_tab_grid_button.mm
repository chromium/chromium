// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button.h"

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/public/features.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kLabelSize = 14;
}  // namespace

@interface ToolbarTabGridButton ()

// Label containing the number of tabs. The title of the button isn't used as a
// workaround for https://crbug.com/828767.
@property(nonatomic, strong) UILabel* tabCountLabel;

@end

@implementation ToolbarTabGridButton

@synthesize tabCountLabel = _tabCountLabel;
@synthesize tabCount = _tabCount;

- (void)setTabCount:(int)tabCount {
  _tabCount = tabCount;
  // Update the text shown in the title of this button. Note that
  // the button's title may be empty or contain an easter egg, but the
  // accessibility value will always be equal to |tabCount|.
  NSString* tabStripButtonValue = [NSString stringWithFormat:@"%d", tabCount];
  self.tabCountLabel.text = TextForTabCount(tabCount);
  [self setAccessibilityValue:tabStripButtonValue];
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  if (highlighted) {
    self.tabCountLabel.textColor =
        self.configuration.buttonsTintColorHighlighted;
  } else {
    self.tabCountLabel.textColor = self.configuration.buttonsTintColor;
  }
}

- (UILabel*)tabCountLabel {
  if (!_tabCountLabel) {
    _tabCountLabel = [[UILabel alloc] init];
    [self addSubview:_tabCountLabel];

    _tabCountLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_tabCountLabel.widthAnchor constraintEqualToConstant:kLabelSize],
      [_tabCountLabel.heightAnchor constraintEqualToConstant:kLabelSize],
    ]];
    AddSameCenterConstraints(self, _tabCountLabel);

    _tabCountLabel.font = [UIFont systemFontOfSize:kTabGridButtonFontSize
                                            weight:UIFontWeightBold];
    _tabCountLabel.adjustsFontSizeToFitWidth = YES;
    _tabCountLabel.minimumScaleFactor = 0.1;
    _tabCountLabel.baselineAdjustment = UIBaselineAdjustmentAlignCenters;
    _tabCountLabel.textAlignment = NSTextAlignmentCenter;
    _tabCountLabel.textColor = self.configuration.buttonsTintColor;
  }
  return _tabCountLabel;
}

@end
