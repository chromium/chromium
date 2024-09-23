// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tab_grid_button_style.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Size of the tab count label.
const CGFloat kLabelSize = 14;
// Offset of the tab count label when in kTabGroup style.
const CGFloat kLabelOffset = 3;
}  // namespace

@interface ToolbarTabGridButton ()

// Label containing the number of tabs. The title of the button isn't used as a
// workaround for https://crbug.com/828767.
@property(nonatomic, strong) UILabel* tabCountLabel;

@end

@implementation ToolbarTabGridButton {
  // The positioning constraints for the tab count label in kNormal style.
  NSArray<NSLayoutConstraint*>* _normalStyleConstraints;
  // The positioning constraints for the tab count label in kTabGroup style.
  NSArray<NSLayoutConstraint*>* _tabGroupStyleConstraints;
  // The image loader for the button in kNormal style.
  ToolbarButtonImageLoader _normalStyleImageLoader;
  // The image loader for the button in kTabGroup style.
  ToolbarButtonImageLoader _tabGroupStyleImageLoader;
}

@synthesize tabCountLabel = _tabCountLabel;
@synthesize tabCount = _tabCount;

- (instancetype)initWithStyledImageLoader:
    (ToolbarTabGridButtonImageLoader)styledImageLoader {
  CHECK(styledImageLoader);
  self = [super
      initWithImageLoader:^{
        return [[UIImage alloc] init];
      }
      IPHHighlightedImageLoader:^{
        return [[UIImage alloc] init];
      }];
  if (self) {
    _normalStyleImageLoader = ^{
      return styledImageLoader(ToolbarTabGridButtonStyle::kNormal);
    };
    _tabGroupStyleImageLoader = ^{
      return styledImageLoader(ToolbarTabGridButtonStyle::kTabGroup);
    };
    [self updateImageLoader];
  }
  return self;
}

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

- (void)setTabGridButtonStyle:(ToolbarTabGridButtonStyle)tabGridButtonStyle {
  CHECK(IsTabGroupIndicatorEnabled() ||
        tabGridButtonStyle == ToolbarTabGridButtonStyle::kNormal);
  if (_tabGridButtonStyle == tabGridButtonStyle) {
    return;
  }
  _tabGridButtonStyle = tabGridButtonStyle;
  [self updateImageLoader];
  [self updatePositionConstraints];
  [self updateTabCountLabelTextColor];
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityIdentifier {
  return kToolbarStackButtonIdentifier;
}

- (NSString*)accessibilityLabel {
  switch (self.tabGridButtonStyle) {
    case ToolbarTabGridButtonStyle::kNormal:
      return l10n_util::GetNSString(IDS_IOS_TOOLBAR_SHOW_TABS);
    case ToolbarTabGridButtonStyle::kTabGroup:
      return l10n_util::GetNSString(IDS_IOS_TOOLBAR_SHOW_TAB_GROUP);
  }
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
    _normalStyleConstraints = @[
      [_tabCountLabel.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
      [_tabCountLabel.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    ];
    _tabGroupStyleConstraints = @[
      [_tabCountLabel.centerXAnchor constraintEqualToAnchor:self.centerXAnchor
                                                   constant:kLabelOffset],
      [_tabCountLabel.centerYAnchor constraintEqualToAnchor:self.centerYAnchor
                                                   constant:kLabelOffset],
    ];
    [self updatePositionConstraints];

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
  switch (self.tabGridButtonStyle) {
    case ToolbarTabGridButtonStyle::kNormal:
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
      break;
    case ToolbarTabGridButtonStyle::kTabGroup:
      self.tabCountLabel.textColor = self.toolbarConfiguration.backgroundColor;
      break;
  }
}

// Swaps the image loader based on the current style.
- (void)updateImageLoader {
  switch (self.tabGridButtonStyle) {
    case ToolbarTabGridButtonStyle::kNormal:
      [self setImageLoader:_normalStyleImageLoader];
      break;
    case ToolbarTabGridButtonStyle::kTabGroup:
      [self setImageLoader:_tabGroupStyleImageLoader];
      break;
  }
}

// Swaps the tab count label positioning constraints based on the current style.
- (void)updatePositionConstraints {
  switch (self.tabGridButtonStyle) {
    case ToolbarTabGridButtonStyle::kNormal:
      [NSLayoutConstraint deactivateConstraints:_tabGroupStyleConstraints];
      [NSLayoutConstraint activateConstraints:_normalStyleConstraints];
      break;
    case ToolbarTabGridButtonStyle::kTabGroup:
      [NSLayoutConstraint deactivateConstraints:_normalStyleConstraints];
      [NSLayoutConstraint activateConstraints:_tabGroupStyleConstraints];
      break;
  }
}

@end
