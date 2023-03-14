// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

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
  // accessibility value will always be equal to `tabCount`.
  NSString* tabStripButtonValue = [NSString stringWithFormat:@"%d", tabCount];
  self.tabCountLabel.attributedText =
      TextForTabCount(tabCount, kTabGridButtonFontSize);
  [self setAccessibilityValue:tabStripButtonValue];
}

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  if (highlighted) {
    self.tabCountLabel.textColor =
        self.toolbarConfiguration.buttonsTintColorHighlighted;
  } else {
    self.tabCountLabel.textColor = self.toolbarConfiguration.buttonsTintColor;
  }
}

- (UILabel*)tabCountLabel {
  if (!_tabCountLabel) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(accessibilityBoldTextStatusDidChange)
               name:UIAccessibilityBoldTextStatusDidChangeNotification
             object:nil];

    _tabCountLabel = [[UILabel alloc] init];
    [self addSubview:_tabCountLabel];

    _tabCountLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_tabCountLabel.widthAnchor constraintEqualToConstant:kLabelSize],
      [_tabCountLabel.heightAnchor constraintEqualToConstant:kLabelSize],
    ]];
    AddSameCenterConstraints(self, _tabCountLabel);

    _tabCountLabel.adjustsFontSizeToFitWidth = YES;
    _tabCountLabel.minimumScaleFactor = 0.1;
    _tabCountLabel.baselineAdjustment = UIBaselineAdjustmentAlignCenters;
    _tabCountLabel.textAlignment = NSTextAlignmentCenter;
    _tabCountLabel.textColor = self.toolbarConfiguration.buttonsTintColor;
  }
  return _tabCountLabel;
}

#pragma mark - Private

// Callback for the notification that the user changed the bold status.
- (void)accessibilityBoldTextStatusDidChange {
  // Reset the attributed string to pick up the new font.
  self.tabCountLabel.attributedText =
      TextForTabCount(self.tabCount, kTabGridButtonFontSize);
}

@end
