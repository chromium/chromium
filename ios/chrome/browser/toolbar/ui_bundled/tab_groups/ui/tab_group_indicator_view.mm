// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/ui/tab_group_indicator_view.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/menu/ui_bundled/action_factory.h"
#import "ios/chrome/browser/share_kit/model/sharing_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_constants.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_height_delegate.h"
#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/ui/tab_group_indicator_constants.h"
#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/ui/tab_group_indicator_mutator.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/ios/uikit_util.h"

using tab_groups::SharingState;

@implementation TabGroupIndicatorView {
  // Stores the tab group informations.
  NSString* _groupTitle;
  UIColor* _groupColor;

  // Tracks if the view is available.
  BOOL _available;

  // Stack view that contains the group's information.
  UIStackView* _stackView;
  // Title label.
  UILabel* _titleView;
  // Dot view.
  UIView* _coloredDotView;
  // Separator view.
  UIView* _separatorView;
  // Button used to display the menu.
  UIButton* _menuButton;
  // The face pile view that displays the share button or the face pile.
  UIView* _facePileView;
  // Whether the share option is available.
  BOOL _shareAvailable;
  // Sharing state of the saved tab group.
  SharingState _sharingState;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    self.accessibilityIdentifier = kTabGroupIndicatorViewIdentifier;
    self.isAccessibilityElement = YES;
    self.accessibilityTraits |= UIAccessibilityTraitButton;

    _titleView = [self titleView];
    _coloredDotView = [self coloredDotView];

    _stackView = [self stackView];
    _separatorView = [self setUpSeparatorView];
    _menuButton = [self menuButton];

    [_stackView addArrangedSubview:_coloredDotView];
    [_stackView addArrangedSubview:_titleView];

    [self addSubview:_stackView];
    [self addSubview:_menuButton];
    [self addSubview:_separatorView];

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

- (void)setSharingState:(SharingState)state {
  _sharingState = state;
  [self configureMenuButton];
}

- (void)setFacePileView:(UIView*)facePileView {
  if (_facePileView == facePileView) {
    return;
  }

  if (_stackView == _facePileView.superview) {
    [_facePileView removeFromSuperview];
  }

  _facePileView = facePileView;

  if (_facePileView) {
    _facePileView.translatesAutoresizingMaskIntoConstraints = NO;
    [_stackView addArrangedSubview:_facePileView];
  }
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
  [_delegate tabGroupIndicatorViewVisibilityUpdated:!hidden];
}

// Returns the stack view.
- (UIStackView*)stackView {
  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.alignment = UIStackViewAlignmentCenter;
  stackView.spacing = kTabGroupIndicatorSeparationMargin;
  return stackView;
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

  // Shared actions.
  NSMutableArray<UIAction*>* sharedActions = [[NSMutableArray alloc] init];
  if (_sharingState != SharingState::kNotShared) {
    [sharedActions addObject:[actionFactory actionToManageTabGroupWithBlock:^{
                     [weakSelf.mutator manageGroup];
                   }]];
    [sharedActions addObject:[actionFactory actionToShowRecentActivity:^{
                     [weakSelf.mutator showRecentActivity];
                   }]];
  } else if (_shareAvailable) {
    [sharedActions addObject:[actionFactory actionToShareTabGroupWithBlock:^{
                     [weakSelf.mutator shareGroup];
                   }]];
  }
  if ([sharedActions count] > 0) {
    [menuElements addObject:[UIMenu menuWithTitle:@""
                                            image:nil
                                       identifier:nil
                                          options:UIMenuOptionsDisplayInline
                                         children:[sharedActions copy]]];
  }

  // Edit actions.
  NSMutableArray<UIAction*>* editActions = [[NSMutableArray alloc] init];
  [editActions addObject:[actionFactory actionToRenameTabGroupWithBlock:^{
                 [weakSelf.mutator showTabGroupEdition];
               }]];
  [editActions addObject:[actionFactory actionToAddNewTabInGroupWithBlock:^{
                 [weakSelf.mutator addNewTabInGroup];
               }]];
  if (_sharingState == SharingState::kNotShared) {
    [editActions addObject:[actionFactory actionToUngroupTabGroupWithBlock:^{
                   [weakSelf.mutator unGroupWithConfirmation:YES];
                 }]];
  }
  [menuElements addObject:[UIMenu menuWithTitle:@""
                                          image:nil
                                     identifier:nil
                                        options:UIMenuOptionsDisplayInline
                                       children:[editActions copy]]];

  // Destructive actions.
  NSMutableArray<UIAction*>* destructiveActions = [[NSMutableArray alloc] init];
  if (IsTabGroupSyncEnabled()) {
    [destructiveActions
        addObject:[actionFactory actionToCloseTabGroupWithBlock:^{
          [weakSelf.mutator closeGroup];
        }]];
    if (!_incognito) {
      switch (_sharingState) {
        case SharingState::kNotShared: {
          [destructiveActions
              addObject:[actionFactory actionToDeleteTabGroupWithBlock:^{
                [weakSelf.mutator deleteGroupWithConfirmation:YES];
              }]];
          break;
        }
        case SharingState::kShared: {
          [destructiveActions
              addObject:[actionFactory actionToLeaveSharedTabGroupWithBlock:^{
                [weakSelf.mutator leaveSharedGroupWithConfirmation:YES];
              }]];
          break;
        }
        case SharingState::kSharedAndOwned: {
          [destructiveActions
              addObject:[actionFactory actionToDeleteSharedTabGroupWithBlock:^{
                [weakSelf.mutator deleteSharedGroupWithConfirmation:YES];
              }]];
          break;
        }
      }
    }
  } else {
    [destructiveActions
        addObject:[actionFactory actionToDeleteTabGroupWithBlock:^{
          [weakSelf.mutator deleteGroupWithConfirmation:NO];
        }]];
  }
  [menuElements addObject:[UIMenu menuWithTitle:@""
                                          image:nil
                                     identifier:nil
                                        options:UIMenuOptionsDisplayInline
                                       children:[destructiveActions copy]]];

  _menuButton.menu = [UIMenu menuWithChildren:[menuElements copy]];
}

// Sets the constraints of the view.
- (void)setContraints {
  [NSLayoutConstraint activateConstraints:@[
    [_stackView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.leadingAnchor
                                    constant:kTabGroupIndicatorVerticalMargin],
    [_stackView.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.trailingAnchor
                                 constant:-kTabGroupIndicatorVerticalMargin],
    [_stackView.topAnchor constraintEqualToAnchor:self.topAnchor],
    [_stackView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    [_stackView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],

    [_separatorView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [_separatorView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    [_separatorView.topAnchor constraintEqualToAnchor:self.bottomAnchor],
    [_separatorView.heightAnchor
        constraintEqualToConstant:ui::AlignValueToUpperPixel(
                                      kToolbarSeparatorHeight)],
  ]];

  AddSameConstraints(_menuButton, _stackView);
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
