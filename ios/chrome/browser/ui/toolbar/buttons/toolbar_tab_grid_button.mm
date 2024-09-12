// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

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

#pragma mark - UIControl

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  [self updateTabCountLabelTextColor];
}

#pragma mark - ToolbarButton

// TODO(crbug.com/40265763): Rename all the references of 'iphHighlighted' to
// 'customHighlighted' as the highlighting UI wont be limited to IPH cases.
- (void)setIphHighlighted:(BOOL)iphHighlighted {
  if (self.iphHighlighted == iphHighlighted) {
    return;
  }
  [super setIphHighlighted:iphHighlighted];
  [self updateTabCountLabelTextColor];
}

#pragma mark - Private

// Loads the tab count label lazily.
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
    [self updateTabCountLabelTextColor];
  }
  return _tabCountLabel;
}

// Callback for the notification that the user changed the bold status.
- (void)accessibilityBoldTextStatusDidChange {
  // Reset the attributed string to pick up the new font.
  self.tabCountLabel.attributedText =
      TextForTabCount(self.tabCount, kTabGridButtonFontSize);
}

// Updates the tab count text label color based on the current style.
- (void)updateTabCountLabelTextColor {
  if (self.iphHighlighted) {
    self.tabCountLabel.textColor =
        self.toolbarConfiguration.buttonsTintColorIPHHighlighted;
  } else if (self.highlighted) {
    self.tabCountLabel.textColor =
        self.toolbarConfiguration.buttonsTintColorHighlighted;
  } else {
    self.tabCountLabel.textColor =
        self.toolbarConfiguration.buttonsTintColor;
  }
}

@end
