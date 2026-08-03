// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_tab_grid_badge_button.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The font size for the tab count label.
constexpr CGFloat kTabGridFontSize = 11;
// Offset of the tab count label in the tab grid button tab group state.
constexpr CGFloat kTabGroupLabelOffset = 1.5;
// Legacy offset of the tab count label in the tab grid button tab group state.
constexpr CGFloat kLegacyTabGroupLabelOffset = 3.0;
// The size of the button image.
constexpr CGFloat kButtonImageSize = 23;

}  // namespace

@implementation ToolbarTabGridBadgeButton {
  UIView* _tabGridContentView;
  UIImageView* _tabGridSymbolView;
  UILabel* _tabCountLabel;
  NSArray<NSLayoutConstraint*>* _tabGridButtonNormalStateConstraints;
  NSArray<NSLayoutConstraint*>* _tabGridButtonTabGroupStateConstraints;
}

- (instancetype)initWithImageLoader:(ToolbarButtonImageLoader)imageLoader
                          incognito:(BOOL)incognito {
  // We pass an empty image loader to prevent ToolbarButton from rendering the
  // standard image in control state checks.
  self = [super
      initWithImageLoader:^UIImage* {
        return nil;
      }
                incognito:incognito];
  if (self) {
    self.isAccessibilityElement = YES;
    _tabGridContentView = [[UIView alloc] init];
    _tabGridContentView.translatesAutoresizingMaskIntoConstraints = NO;
    _tabGridContentView.userInteractionEnabled = NO;
    [self addSubview:_tabGridContentView];

    // Align custom content view inside the button bounds.
    if (IsNextOldDesignEnabled()) {
      AddSameConstraints(_tabGridContentView, self);
    } else {
      [NSLayoutConstraint activateConstraints:@[
        [_tabGridContentView.centerXAnchor
            constraintEqualToAnchor:self.centerXAnchor],
        [_tabGridContentView.centerYAnchor
            constraintEqualToAnchor:self.centerYAnchor],
        [_tabGridContentView.widthAnchor
            constraintEqualToConstant:kButtonImageSize],
        [_tabGridContentView.heightAnchor
            constraintEqualToAnchor:_tabGridContentView.widthAnchor],
      ]];
    }

    _tabGridSymbolView = [[UIImageView alloc] init];
    _tabGridSymbolView.translatesAutoresizingMaskIntoConstraints = NO;
    if (IsNextOldDesignEnabled()) {
      _tabGridSymbolView.contentMode = UIViewContentModeCenter;
    }
    [_tabGridContentView addSubview:_tabGridSymbolView];
    AddSameConstraints(_tabGridSymbolView, _tabGridContentView);

    _tabCountLabel = [[UILabel alloc] init];
    _tabCountLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _tabCountLabel.textColor = self.tintColor;
    if (IsNextOldDesignEnabled()) {
      _tabCountLabel.adjustsFontSizeToFitWidth = YES;
      _tabCountLabel.minimumScaleFactor = 0.1;
      _tabCountLabel.baselineAdjustment = UIBaselineAdjustmentAlignCenters;
      _tabCountLabel.textAlignment = NSTextAlignmentCenter;
    }
    [_tabGridContentView addSubview:_tabCountLabel];

    CGFloat labelOffset = IsNextOldDesignEnabled() ? kLegacyTabGroupLabelOffset
                                                   : kTabGroupLabelOffset;

    _tabGridButtonNormalStateConstraints = @[
      [_tabCountLabel.centerXAnchor
          constraintEqualToAnchor:_tabGridContentView.centerXAnchor],
      [_tabCountLabel.centerYAnchor
          constraintEqualToAnchor:_tabGridContentView.centerYAnchor],
    ];

    _tabGridButtonTabGroupStateConstraints = @[
      [_tabCountLabel.centerXAnchor
          constraintEqualToAnchor:_tabGridContentView.centerXAnchor
                         constant:labelOffset],
      [_tabCountLabel.centerYAnchor
          constraintEqualToAnchor:_tabGridContentView.centerYAnchor
                         constant:labelOffset],
    ];

    if (IsNextOldDesignEnabled()) {
      [NSLayoutConstraint activateConstraints:@[
        [_tabCountLabel.widthAnchor constraintEqualToConstant:14],
        [_tabCountLabel.heightAnchor constraintEqualToConstant:14],
      ]];
    }

    [_tabGridContentView bringSubviewToFront:_tabCountLabel];
    [self updateTabGridButtonAppearance];
  }
  return self;
}

#pragma mark - Accessors & Mutators

- (void)setTabCount:(NSUInteger)tabCount {
  if (_tabCount == tabCount) {
    return;
  }
  _tabCount = tabCount;
  CGFloat fontSize = IsNextOldDesignEnabled() ? 13 : kTabGridFontSize;
  _tabCountLabel.attributedText = TextForTabCount(tabCount, fontSize);
  [self setAccessibilityValue:[NSString stringWithFormat:@"%lu", tabCount]];
}

- (void)setInTabGroup:(BOOL)inTabGroup {
  if (_inTabGroup == inTabGroup) {
    return;
  }
  _inTabGroup = inTabGroup;
  [self updateTabGridButtonAppearance];
}

- (NSString*)accessibilityLabel {
  return l10n_util::GetNSString(self.inTabGroup ? IDS_IOS_TOOLBAR_SHOW_TAB_GROUP
                                                : IDS_IOS_TOOLBAR_SHOW_TABS);
}

#pragma mark - UIView

- (void)tintColorDidChange {
  [super tintColorDidChange];
  if (IsNextOldDesignEnabled() && _inTabGroup) {
    _tabCountLabel.textColor = [UIColor colorNamed:kBackgroundColor];
  } else {
    _tabCountLabel.textColor = self.tintColor;
  }
}

- (UIBezierPath*)visiblePath {
  return [UIBezierPath bezierPathWithRoundedRect:self.bounds
                                    cornerRadius:self.bounds.size.width / 2];
}

#pragma mark - Private

- (void)updateTabGridButtonAppearance {
  if (IsNextOldDesignEnabled()) {
    Symbol symbol =
        _inTabGroup ? SymbolSquareFilledOnSquare : SymbolSquareNumber;
    _tabGridSymbolView.image = SymbolWithPointSize(symbol, 24);
    _tabCountLabel.textColor =
        _inTabGroup ? [UIColor colorNamed:kBackgroundColor] : self.tintColor;
  } else {
    Symbol symbol = _inTabGroup ? SymbolTabs : SymbolApp;
    UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
        configurationWithPointSize:kButtonImageSize
                            weight:UIImageSymbolWeightSemibold
                             scale:UIImageSymbolScaleMedium];
    _tabGridSymbolView.image = SymbolWithConfiguration(symbol, symbolConfig);
    _tabCountLabel.textColor = self.tintColor;
  }

  if (_inTabGroup) {
    [NSLayoutConstraint
        deactivateConstraints:_tabGridButtonNormalStateConstraints];
    [NSLayoutConstraint
        activateConstraints:_tabGridButtonTabGroupStateConstraints];
  } else {
    [NSLayoutConstraint
        deactivateConstraints:_tabGridButtonTabGroupStateConstraints];
    [NSLayoutConstraint
        activateConstraints:_tabGridButtonNormalStateConstraints];
  }
  [self setNeedsLayout];
}

@end
