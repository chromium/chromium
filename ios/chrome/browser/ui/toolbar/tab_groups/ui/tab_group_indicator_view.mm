// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_view.h"

#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_constants.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_mutator.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

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
  // Button used to display the menu.
  UIButton* _menuButton;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    self.accessibilityIdentifier = kTabGroupIndicatorViewIdentifier;

    _containerView = [self containerView];
    _titleView = [self titleView];
    _coloredDotView = [self coloredDotView];
    _menuButton = [self menuButton];

    [self addSubview:_containerView];
    [self addSubview:_menuButton];
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

#pragma mark - Private

// Updates the view's visibility.
- (void)updateVisibility {
  self.hidden = _groupTitle == nil || !_available;
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

// Returns the menu button.
- (UIButton*)menuButton {
  UIButton* button = [[UIButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.showsMenuAsPrimaryAction = YES;

  __weak __typeof(self) weakSelf = self;
  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:kMenuScenarioHistogramTabGroupIndicatorEntry];
  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];
  [menuElements addObject:[actionFactory actionToRenameTabGroupWithBlock:^{
                  [weakSelf.mutator showTabGroupEdition];
                }]];
  [menuElements addObject:[actionFactory actionToAddNewTabInGroupWithBlock:^{
                  [weakSelf.mutator addNewTabInGroup];
                }]];
  [menuElements addObject:[actionFactory actionToUngroupTabGroupWithBlock:^{
                  [weakSelf.mutator unGroup];
                }]];
  [menuElements addObject:[actionFactory actionToCloseTabGroupWithBlock:^{
                  [weakSelf.mutator closeGroup];
                }]];
  button.menu = [UIMenu menuWithChildren:menuElements];
  return button;
}

// Sets the constraints of the view.
- (void)setContraints {
  [NSLayoutConstraint activateConstraints:@[
    [_containerView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.leadingAnchor],
    [_containerView.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.trailingAnchor],
    [_containerView.topAnchor constraintEqualToAnchor:self.topAnchor],
    [_containerView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    [_containerView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],

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
  AddSameConstraints(_menuButton, self);
}

#pragma mark - Setters

- (void)setAvailable:(BOOL)available {
  _available = available;
  [self updateVisibility];
}

- (void)setGroupTitle:(NSString*)title {
  _groupTitle = title;
  _titleView.text = title;
}

- (void)setGroupColor:(UIColor*)color {
  _groupColor = color;
  _coloredDotView.backgroundColor = color;
}

@end
