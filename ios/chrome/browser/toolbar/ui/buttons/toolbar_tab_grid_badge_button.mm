// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_tab_grid_badge_button.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The font size for the tab count label.
constexpr CGFloat kTabGridFontSize = 11;
// Offset of the tab count label in the tab grid button tab group state.
constexpr CGFloat kTabGroupLabelOffset = 1.5;
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
    _tabGridContentView = [[UIView alloc] init];
    _tabGridContentView.translatesAutoresizingMaskIntoConstraints = NO;
    _tabGridContentView.userInteractionEnabled = NO;
    [self addSubview:_tabGridContentView];

    // Align custom content view inside the button bounds.
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

    _tabGridSymbolView = [[UIImageView alloc] init];
    _tabGridSymbolView.translatesAutoresizingMaskIntoConstraints = NO;
    [_tabGridContentView addSubview:_tabGridSymbolView];
    AddSameConstraints(_tabGridSymbolView, _tabGridContentView);

    _tabCountLabel = [[UILabel alloc] init];
    _tabCountLabel.translatesAutoresizingMaskIntoConstraints = NO;
    // Use tintColor to match normal or incognito mode colors automatically.
    _tabCountLabel.textColor = self.tintColor;
    [_tabGridContentView addSubview:_tabCountLabel];

    _tabGridButtonNormalStateConstraints = @[
      [_tabCountLabel.centerXAnchor
          constraintEqualToAnchor:_tabGridContentView.centerXAnchor],
      [_tabCountLabel.centerYAnchor
          constraintEqualToAnchor:_tabGridContentView.centerYAnchor],
    ];

    _tabGridButtonTabGroupStateConstraints = @[
      [_tabCountLabel.centerXAnchor
          constraintEqualToAnchor:_tabGridContentView.centerXAnchor
                         constant:kTabGroupLabelOffset],
      [_tabCountLabel.centerYAnchor
          constraintEqualToAnchor:_tabGridContentView.centerYAnchor
                         constant:kTabGroupLabelOffset],
    ];

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
  _tabCountLabel.attributedText = TextForTabCount(tabCount, kTabGridFontSize);
}

- (void)setInTabGroup:(BOOL)inTabGroup {
  if (_inTabGroup == inTabGroup) {
    return;
  }
  _inTabGroup = inTabGroup;
  [self updateTabGridButtonAppearance];
}

#pragma mark - UIView

- (void)tintColorDidChange {
  [super tintColorDidChange];
  _tabCountLabel.textColor = self.tintColor;
}

- (UIBezierPath*)visiblePath {
  return [UIBezierPath bezierPathWithRoundedRect:self.bounds
                                    cornerRadius:self.bounds.size.width / 2];
}

#pragma mark - Private

- (void)updateTabGridButtonAppearance {
  NSString* symbolName = _inTabGroup ? kTabsSymbol : kAppSymbol;

  // Point size configuration matching point size of standard symbols.
  UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:kButtonImageSize
                          weight:UIImageSymbolWeightSemibold
                           scale:UIImageSymbolScaleMedium];
  _tabGridSymbolView.image =
      DefaultSymbolWithConfiguration(symbolName, symbolConfig);

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
