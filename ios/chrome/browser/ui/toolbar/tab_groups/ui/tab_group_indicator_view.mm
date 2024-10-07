// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_view.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_height_delegate.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_constants.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_mutator.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/ios/uikit_util.h"

@implementation TabGroupIndicatorView {
  // Stores the tab group informations.
  NSString* _groupTitle;
  UIColor* _groupColor;

  // Tracks if the view is available.
  BOOL _available;

  // View that contains subviews.
  UIView* _containerView;
  // Title label.
  UILabel* _titleView;
  // Dot view.
  UIView* _coloredDotView;
  // Separator view.
  UIView* _separatorView;
  // Button used to display the menu.
  UIButton* _menuButton;
  // Whether the share option is available.
  BOOL _shareAvailable;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    self.accessibilityIdentifier = kTabGroupIndicatorViewIdentifier;
    self.isAccessibilityElement = YES;
    self.accessibilityTraits |= UIAccessibilityTraitButton;

    _containerView = [self containerView];
    _titleView = [self titleView];
    _coloredDotView = [self coloredDotView];
    _separatorView = [self setUpSeparatorView];
    _menuButton = [self menuButton];

    [self addSubview:_containerView];
    [self addSubview:_menuButton];
    [self addSubview:_separatorView];
    [_containerView addSubview:_coloredDotView];
    [_containerView addSubview:_titleView];

    [self setContraints];
  }
  return self;
}

#pragma mark - TabGroupIndicatorConsumer

- (void)setTabGroupTitle:(NSString*)groupTitle groupColor:(UIColor*)groupColor {
  if (groupTitle == _groupTitle && groupColor == _groupColor) {
    [self updateVisibility];
    return;
  }

  [self setGroupTitle:groupTitle];
  [self setGroupColor:groupColor];
  [self updateVisibility];
}

- (void)setShareAvailable:(BOOL)shareAvailable {
  _shareAvailable = shareAvailable;
  [self configureMenuButton];
}

#pragma mark - Private

// Updates the view's visibility.
- (void)updateVisibility {
  BOOL hidden = _groupTitle == nil || !_available;
  if (hidden == self.hidden) {
    return;
  }
  self.hidden = hidden;
  [_toolbarHeightDelegate toolbarsHeightChanged];
}

// Returns the container view.
- (UIView*)containerView {
  UIView* containerView = [[UIView alloc] initWithFrame:CGRectZero];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  return containerView;
}

// Returns the title label view.
- (UILabel*)titleView {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.numberOfLines = 1;
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  return titleLabel;
}

// Returns the group color dot view.
- (UIView*)coloredDotView {
  UIView* dotView = [[UIView alloc] initWithFrame:CGRectZero];
  dotView.translatesAutoresizingMaskIntoConstraints = NO;
  dotView.layer.cornerRadius = kTabGroupIndicatorColoredDotSize / 2;

  [NSLayoutConstraint activateConstraints:@[
    [dotView.heightAnchor
        constraintEqualToConstant:kTabGroupIndicatorColoredDotSize],
    [dotView.widthAnchor
        constraintEqualToConstant:kTabGroupIndicatorColoredDotSize],
  ]];

  return dotView;
}

// Sets the separator view up.
- (UIView*)setUpSeparatorView {
  UIView* separatorView = [[UIView alloc] init];
  separatorView.backgroundColor = [UIColor colorNamed:kToolbarShadowColor];
  separatorView.translatesAutoresizingMaskIntoConstraints = NO;
  return separatorView;
}

// Returns the menu button.
- (UIButton*)menuButton {
  UIButton* button = [[UIButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.showsMenuAsPrimaryAction = YES;
  [button addTarget:self
                action:@selector(menuButtonTapped:)
      forControlEvents:UIControlEventMenuActionTriggered];
  return button;
}

// Handles taps on the menu button.
- (void)menuButtonTapped:(id)sender {
  base::RecordAction(base::UserMetricsAction(
      _displayedOnNTP ? "MobileTabGroupIndicatorShowNTPMenu"
                      : "MobileTabGroupIndicatorShowMenu"));
}

// Configures the menu of `menuButton`.
- (void)configureMenuButton {
  __weak __typeof(self) weakSelf = self;
  MenuScenarioHistogram scenario =
      _displayedOnNTP ? kMenuScenarioHistogramTabGroupIndicatorNTPEntry
                      : kMenuScenarioHistogramTabGroupIndicatorEntry;
  ActionFactory* actionFactory =
      [[ActionFactory alloc] initWithScenario:scenario];

  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];
  if (_shareAvailable) {
    [menuElements addObject:[actionFactory actionToShareWithBlock:^{
                    [weakSelf.mutator showShareKitUI];
                  }]];
  }
  [menuElements addObject:[actionFactory actionToRenameTabGroupWithBlock:^{
                  [weakSelf.mutator showTabGroupEdition];
                }]];
  [menuElements addObject:[actionFactory actionToAddNewTabInGroupWithBlock:^{
                  [weakSelf.mutator addNewTabInGroup];
                }]];
  [menuElements addObject:[actionFactory actionToUngroupTabGroupWithBlock:^{
                  [weakSelf.mutator unGroupWithConfirmation:YES];
                }]];
  if (IsTabGroupSyncEnabled()) {
    [menuElements addObject:[actionFactory actionToCloseTabGroupWithBlock:^{
                    [weakSelf.mutator closeGroup];
                  }]];
    if (!_incognito) {
      [menuElements addObject:[actionFactory actionToDeleteTabGroupWithBlock:^{
                      [weakSelf.mutator deleteGroupWithConfirmation:YES];
                    }]];
    }
  } else {
    [menuElements addObject:[actionFactory actionToDeleteTabGroupWithBlock:^{
                    [weakSelf.mutator deleteGroupWithConfirmation:NO];
                  }]];
  }

  _menuButton.menu = [UIMenu menuWithChildren:menuElements];
}

// Sets the constraints of the view.
- (void)setContraints {
  [NSLayoutConstraint activateConstraints:@[
    [_containerView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.leadingAnchor
                                    constant:kTabGroupIndicatorVerticalMargin],
    [_containerView.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.trailingAnchor
                                 constant:-kTabGroupIndicatorVerticalMargin],
    [_containerView.topAnchor constraintEqualToAnchor:self.topAnchor],
    [_containerView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    [_containerView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],

    [_separatorView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [_separatorView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    [_separatorView.topAnchor constraintEqualToAnchor:self.bottomAnchor],
    [_separatorView.heightAnchor
        constraintEqualToConstant:ui::AlignValueToUpperPixel(
                                      kToolbarSeparatorHeight)],

    [_titleView.leadingAnchor
        constraintEqualToAnchor:_coloredDotView.trailingAnchor
                       constant:kTabGroupIndicatorSeparationMargin],
    [_coloredDotView.centerYAnchor
        constraintEqualToAnchor:_containerView.centerYAnchor],
    [_coloredDotView.leadingAnchor
        constraintEqualToAnchor:_containerView.leadingAnchor],
    [_titleView.trailingAnchor
        constraintEqualToAnchor:_containerView.trailingAnchor],
    [_titleView.topAnchor constraintEqualToAnchor:_containerView.topAnchor],
    [_titleView.bottomAnchor
        constraintEqualToAnchor:_containerView.bottomAnchor],
  ]];
  AddSameConstraints(_menuButton, _containerView);
}

#pragma mark - Setters

- (void)setAvailable:(BOOL)available {
  _available = available;
  [self updateVisibility];
}

- (void)setIncognito:(BOOL)incognito {
  _incognito = incognito;
  [self configureMenuButton];
}

- (void)setShowSeparator:(BOOL)showSeparator {
  _showSeparator = showSeparator;
  _separatorView.hidden = !showSeparator;
}

- (void)setGroupTitle:(NSString*)title {
  _groupTitle = [title copy];
  _titleView.text = title;

  self.accessibilityLabel =
      l10n_util::GetNSStringF(IDS_IOS_TAB_GROUP_INDICATOR_ACCESSIBILITY_TITLE,
                              base::SysNSStringToUTF16(title));
}

- (void)setGroupColor:(UIColor*)color {
  _groupColor = color;
  _coloredDotView.backgroundColor = color;
}

@end
