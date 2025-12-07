// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_bottom_toolbar.h"

#import <objc/runtime.h>

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/features.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_new_tab_button.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbar_background.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbar_scrolling_background.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbars_grid_delegate.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbars_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Minimal width for text buttons.
const CGFloat kButtonMinWidth = 44;

// Height for text buttons.
const CGFloat kButtonHeight = 44;
// Button font size.
const CGFloat kButtonFontSize = 17;
// Horizontal padding for buttons in compact mode.

const CGFloat kCompactButtonHorizontalPadding = 16;

const CGFloat kCompactButtonHorizontalPaddingPreiOS26 = 12;
// Minimum spacing between buttons.
const CGFloat kCompactMinButtonSpacing = 8;

// Returns the padding depending on the OS version.
CGFloat CompactButtonHorizontalPadding() {
  if (@available(iOS 26, *)) {
    return kCompactButtonHorizontalPadding;
  }

  return kCompactButtonHorizontalPaddingPreiOS26;
}

}  // namespace

@implementation TabGridBottomToolbar {
  UIToolbar* _containerToolbar;
  TabGridNewTabButton* _smallNewTabButton;
  TabGridNewTabButton* _largeNewTabButton;
  UIButton* _doneButton;
  UIButton* _undoButton;
  UIButton* _editButton;
  UIButton* _addToButton;
  UIButton* _closeTabsButton;
  UIButton* _shareButton;
  BOOL _undoActive;
  BOOL _scrolledToEdge;
  TabGridToolbarBackground* _backgroundView;
  TabGridToolbarScrollingBackground* _scrollBackgroundView;
  // Configures the responder following the receiver in the responder chain.
  UIResponder* _followingNextResponder;
  NSLayoutConstraint* _viewTopConstraint;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    if (IsDiamondPrototypeEnabled()) {
      return self;
    }
    [self setupViews];
    [self updateLayout];
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(
        @[ UITraitVerticalSizeClass.class, UITraitHorizontalSizeClass.class ]);
    [self registerForTraitChanges:traits withAction:@selector(updateLayout)];
  }
  return self;
}

#pragma mark - UIView

- (void)didMoveToSuperview {
  if (IsIOSSoftLockEnabled()) {
    if (_scrollBackgroundView) {
      [self.superview.bottomAnchor
          constraintEqualToAnchor:_scrollBackgroundView.bottomAnchor]
          .active = YES;
    }
  } else {
    if (_backgroundView) {
      [self.superview.bottomAnchor
          constraintEqualToAnchor:_backgroundView.bottomAnchor]
          .active = YES;
    }
  }
  [super didMoveToSuperview];
}

// Returns intrinsicContentSize based on the content of the toolbar.
// When showing the floating Button the contentsize for the toolbar should be
// zero so that the toolbar isn't accounted for when calculating the bottom
// insets of the container view.
- (CGSize)intrinsicContentSize {
  if (!_largeNewTabButton.hidden) {
    return CGSizeZero;
  }
  return _containerToolbar.intrinsicContentSize;
}

#pragma mark - Public

- (void)setIsInTabGroupView:(BOOL)isInTabGroupView {
  if (_isInTabGroupView == isInTabGroupView) {
    return;
  }
  _isInTabGroupView = isInTabGroupView;
  [self updateLayout];
}

- (void)setPage:(TabGridPage)page {
  if (_page == page) {
    return;
  }
  _page = page;
  _smallNewTabButton.page = page;
  _largeNewTabButton.page = page;
  [self updateLayout];
}

- (void)setMode:(TabGridMode)mode {
  if (_mode == mode) {
    return;
  }
  _mode = mode;
  // Reset selected tabs count when mode changes.
  self.selectedTabsCount = 0;
  [self updateLayout];
}

- (void)setSelectedTabsCount:(int)count {
  _selectedTabsCount = count;
  [self updateCloseTabsButtonTitle];
}

- (void)setNewTabButtonEnabled:(BOOL)enabled {
  _smallNewTabButton.enabled = enabled;
  _largeNewTabButton.enabled = enabled;
}

- (void)setDoneButtonEnabled:(BOOL)enabled {
  _doneButton.enabled = enabled;
}

- (void)setCloseAllButtonEnabled:(BOOL)enabled {
  _undoButton.enabled = enabled;
}

- (void)setIncognitoBackgroundHidden:(BOOL)hidden {
  [_scrollBackgroundView hideIncognitoToolbarBackground:hidden];
  [self updateBackgroundVisibility];
}

- (void)useUndoCloseAll:(BOOL)useUndo {
  _undoButton.enabled = YES;
  if (_undoActive != useUndo) {
    _undoActive = useUndo;
    [self updateLayout];
  }
}

- (void)hide {
  _smallNewTabButton.alpha = 0.0;
  _largeNewTabButton.alpha = 0.0;
}

- (void)show {
  _smallNewTabButton.alpha = 1.0;
  _largeNewTabButton.alpha = 1.0;
}

- (void)setScrollViewScrolledToEdge:(BOOL)scrolledToEdge {
  if (scrolledToEdge == _scrolledToEdge) {
    return;
  }

  _scrolledToEdge = scrolledToEdge;

  [self updateBackgroundVisibility];
}

- (void)setBackgroundContentOffset:(CGPoint)backgroundContentOffset
                          animated:(BOOL)animated {
  [_scrollBackgroundView setContentOffset:backgroundContentOffset
                                 animated:animated];
}

#pragma mark Close Tabs

- (void)setCloseTabsButtonEnabled:(BOOL)enabled {
  _closeTabsButton.enabled = enabled;
}

#pragma mark Share Tabs

- (void)setShareTabsButtonEnabled:(BOOL)enabled {
  _shareButton.enabled = enabled;
}

#pragma mark Add To

- (void)setAddToButtonMenu:(UIMenu*)menu {
  _addToButton.menu = menu;
}

- (void)setAddToButtonEnabled:(BOOL)enabled {
  _addToButton.enabled = enabled;
}

#pragma mark Edit Button

- (void)setEditButtonMenu:(UIMenu*)menu {
  _editButton.menu = menu;
}

- (void)setEditButtonEnabled:(BOOL)enabled {
  _editButton.enabled = enabled;
}

#pragma mark - Private

// Returns a new button to be used.
- (UIButton*)createButtonWithTitle:(NSString*)title
                             image:(UIImage*)image
                    targetSelector:(SEL)targetSelector {
  UIButton* button;

  if (@available(iOS 26, *)) {
    UIButtonConfiguration* buttonConfiguration;
    if ([UIButtonConfiguration
            respondsToSelector:@selector(prominentGlassButtonConfiguration)]) {
      buttonConfiguration =
          [UIButtonConfiguration prominentGlassButtonConfiguration];
    } else {
      buttonConfiguration = [UIButtonConfiguration glassButtonConfiguration];
    }
    buttonConfiguration.title = title;
    buttonConfiguration.image = image;
    button = [UIButton buttonWithConfiguration:buttonConfiguration
                                 primaryAction:nil];
    button.tintColor = TabGridGlassButtonTintColor();
  } else {
    button = [UIButton systemButtonWithPrimaryAction:nil];
    button.tintColor = UIColor.whiteColor;
    [button setTitle:title forState:UIControlStateNormal];
    [button setImage:image forState:UIControlStateNormal];
  }

  button.titleLabel.font = [UIFont systemFontOfSize:kButtonFontSize];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button.heightAnchor constraintEqualToConstant:kButtonHeight].active = YES;

  if (@available(iOS 26, *)) {
    [button.widthAnchor constraintGreaterThanOrEqualToConstant:kButtonMinWidth]
        .active = YES;
  }

  if (targetSelector) {
    [button addTarget:self
                  action:targetSelector
        forControlEvents:UIControlEventTouchUpInside];
  }

  return button;
}

// Constraints for the selection mode.
- (NSArray<NSLayoutConstraint*>*)constraintsForSelectionMode {
  NSMutableArray<NSLayoutConstraint*>* constraints =
      [NSMutableArray arrayWithArray:@[
        // Vertical layout:
        [_closeTabsButton.centerYAnchor
            constraintEqualToAnchor:_containerToolbar.centerYAnchor],
        [_shareButton.centerYAnchor
            constraintEqualToAnchor:_containerToolbar.centerYAnchor],
        [_addToButton.centerYAnchor
            constraintEqualToAnchor:_containerToolbar.centerYAnchor],

        // Horizontal layout:
        [_closeTabsButton.leadingAnchor
            constraintEqualToAnchor:_containerToolbar.leadingAnchor
                           constant:CompactButtonHorizontalPadding()],
        [_shareButton.centerXAnchor
            constraintEqualToAnchor:_containerToolbar.centerXAnchor],
        [_addToButton.trailingAnchor
            constraintEqualToAnchor:_containerToolbar.trailingAnchor
                           constant:-CompactButtonHorizontalPadding()],
        [_shareButton.leadingAnchor
            constraintGreaterThanOrEqualToAnchor:_closeTabsButton.trailingAnchor
                                        constant:kCompactMinButtonSpacing],
        [_addToButton.leadingAnchor
            constraintGreaterThanOrEqualToAnchor:_shareButton.trailingAnchor
                                        constant:kCompactMinButtonSpacing],
      ]];
  return constraints;
}

// Constraints for the regular compact mode.
- (NSArray<NSLayoutConstraint*>*)constraintsForCompactMode {
  NSMutableArray<NSLayoutConstraint*>* constraints =
      [NSMutableArray arrayWithArray:@[
        // Vertical layout:
        [_editButton.centerYAnchor
            constraintEqualToAnchor:_containerToolbar.centerYAnchor],
        [_undoButton.centerYAnchor
            constraintEqualToAnchor:_containerToolbar.centerYAnchor],
        [_smallNewTabButton.centerYAnchor
            constraintEqualToAnchor:_containerToolbar.centerYAnchor],
        [_doneButton.centerYAnchor
            constraintEqualToAnchor:_containerToolbar.centerYAnchor],

        // Horizontal layout:
        [_editButton.leadingAnchor
            constraintEqualToAnchor:_containerToolbar.leadingAnchor
                           constant:CompactButtonHorizontalPadding()],
        [_undoButton.leadingAnchor
            constraintEqualToAnchor:_containerToolbar.leadingAnchor
                           constant:CompactButtonHorizontalPadding()],
        [_doneButton.trailingAnchor
            constraintEqualToAnchor:_containerToolbar.trailingAnchor
                           constant:-CompactButtonHorizontalPadding()],
        [_smallNewTabButton.centerXAnchor
            constraintEqualToAnchor:_containerToolbar.centerXAnchor],
        [_smallNewTabButton.leadingAnchor
            constraintGreaterThanOrEqualToAnchor:_editButton.trailingAnchor
                                        constant:kCompactMinButtonSpacing],
        [_smallNewTabButton.leadingAnchor
            constraintGreaterThanOrEqualToAnchor:_undoButton.trailingAnchor
                                        constant:kCompactMinButtonSpacing],
        [_doneButton.leadingAnchor
            constraintGreaterThanOrEqualToAnchor:_smallNewTabButton
                                                     .trailingAnchor
                                        constant:kCompactMinButtonSpacing],
      ]];
  return constraints;
}

// Constraints for the floating mode.
- (NSArray<NSLayoutConstraint*>*)constraintsForFloatingMode {
  CGFloat largeButtonHorizontalInset =
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
          ? kTabGridFloatingButtonInsetIPad
          : kTabGridFloatingButtonInset;
  CGFloat largeButtonVerticalInset =
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
          ? kTabGridFloatingButtonInsetIPad
          : kTabGridFloatingButtonInset;

  NSLayoutConstraint* largeButtonTrailingSafeAreaConstraint =
      [_largeNewTabButton.trailingAnchor
          constraintEqualToAnchor:self.safeAreaLayoutGuide.trailingAnchor];
  largeButtonTrailingSafeAreaConstraint.priority = UILayoutPriorityDefaultHigh;
  NSLayoutConstraint* largeButtonBottomSafeAreaConstraint =
      [_largeNewTabButton.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor];
  largeButtonBottomSafeAreaConstraint.priority = UILayoutPriorityDefaultHigh;

  NSMutableArray<NSLayoutConstraint*>* constraints =
      [NSMutableArray arrayWithArray:@[
        // Vertical layout:
        [_largeNewTabButton.bottomAnchor
            constraintLessThanOrEqualToAnchor:self.bottomAnchor
                                     constant:-largeButtonVerticalInset],
        largeButtonBottomSafeAreaConstraint,
        // Horizontal layout:
        [_largeNewTabButton.trailingAnchor
            constraintLessThanOrEqualToAnchor:self.trailingAnchor
                                     constant:-largeButtonHorizontalInset],
        largeButtonTrailingSafeAreaConstraint,
      ]];
  return constraints;
}

// Setup container toolbar, buttons and constraints.
- (void)setupViews {
  // Background.
  [self createScrolledBackgrounds];

  // Container toolbar.
  _containerToolbar = [[UIToolbar alloc] initWithFrame:CGRectZero];
  _containerToolbar.translatesAutoresizingMaskIntoConstraints = NO;
  [_containerToolbar setBackgroundImage:[[UIImage alloc] init]
                     forToolbarPosition:UIBarPositionAny
                             barMetrics:UIBarMetricsDefault];

  [self addSubview:_containerToolbar];

  // Close button.
  _undoButton =
      [self createButtonWithTitle:l10n_util::GetNSString(
                                      IDS_IOS_TAB_GRID_UNDO_CLOSE_ALL_BUTTON)
                            image:nil
                   targetSelector:@selector(closeAllButtonTapped:)];
  _undoButton.accessibilityIdentifier = kTabGridUndoCloseAllButtonIdentifier;
  [_containerToolbar addSubview:_undoButton];

  // Done button.
  _doneButton = [self
      createButtonWithTitle:l10n_util::GetNSString(IDS_IOS_TAB_GRID_DONE_BUTTON)
                      image:nil
             targetSelector:@selector(doneButtonTapped:)];
  _doneButton.role = UIButtonRolePrimary;
  _doneButton.accessibilityIdentifier = kTabGridDoneButtonIdentifier;
  _doneButton.titleLabel.font = [UIFont boldSystemFontOfSize:kButtonFontSize];
  [_containerToolbar addSubview:_doneButton];

  // Small New Tab button.
  _smallNewTabButton = [[TabGridNewTabButton alloc] initWithLargeSize:NO];
  [_smallNewTabButton addTarget:self
                         action:@selector(newTabButtonTapped:)
               forControlEvents:UIControlEventTouchUpInside];
  _smallNewTabButton.translatesAutoresizingMaskIntoConstraints = NO;
  _smallNewTabButton.page = self.page;
  [_containerToolbar addSubview:_smallNewTabButton];

  // Large New Tab button.
  _largeNewTabButton = [[TabGridNewTabButton alloc] initWithLargeSize:YES];
  [_largeNewTabButton addTarget:self
                         action:@selector(newTabButtonTapped:)
               forControlEvents:UIControlEventTouchUpInside];
  _largeNewTabButton.translatesAutoresizingMaskIntoConstraints = NO;
  _largeNewTabButton.page = self.page;
  [self addSubview:_largeNewTabButton];

  // Edit button.
  _editButton = [self
      createButtonWithTitle:l10n_util::GetNSString(IDS_IOS_TAB_GRID_EDIT_BUTTON)
                      image:nil
             targetSelector:nil];
  _editButton.accessibilityIdentifier = kTabGridEditButtonIdentifier;
  _editButton.showsMenuAsPrimaryAction = YES;
  [_containerToolbar addSubview:_editButton];

  // Add To button.
  _addToButton = [self createButtonWithTitle:l10n_util::GetNSString(
                                                 IDS_IOS_TAB_GRID_ADD_TO_BUTTON)
                                       image:nil
                              targetSelector:nil];
  _addToButton.accessibilityIdentifier = kTabGridEditAddToButtonIdentifier;
  _addToButton.showsMenuAsPrimaryAction = YES;
  [_containerToolbar addSubview:_addToButton];

  // Share button.
  _shareButton =
      [self createButtonWithTitle:nil
                            image:DefaultSymbolWithPointSize(
                                      kShareSymbol, kSymbolActionPointSize)
                   targetSelector:@selector(shareSelectedTabs:)];
  _shareButton.accessibilityIdentifier = kTabGridEditShareButtonIdentifier;
  [_containerToolbar addSubview:_shareButton];

  // Close Tabs button.
  _closeTabsButton = [self createButtonWithTitle:nil
                                           image:nil
                                  targetSelector:@selector(closeSelectedTabs:)];
  _closeTabsButton.accessibilityIdentifier =
      kTabGridEditCloseTabsButtonIdentifier;
  [self updateCloseTabsButtonTitle];
  [_containerToolbar addSubview:_closeTabsButton];

  // Apply constraints.
  [NSLayoutConstraint activateConstraints:@[
    [_containerToolbar.bottomAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.bottomAnchor],
    [_containerToolbar.leadingAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.leadingAnchor],
    [_containerToolbar.trailingAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.trailingAnchor],
    [_containerToolbar.heightAnchor
        constraintEqualToConstant:kTabGridBottomToolbarHeight],
  ]];
  [NSLayoutConstraint activateConstraints:[self constraintsForCompactMode]];
  [NSLayoutConstraint activateConstraints:[self constraintsForSelectionMode]];
  [NSLayoutConstraint activateConstraints:[self constraintsForFloatingMode]];
}

// Updates the `_closeTabsButton` title.
- (void)updateCloseTabsButtonTitle {
  [_closeTabsButton
      setTitle:l10n_util::GetPluralNSStringF(IDS_IOS_TAB_GRID_CLOSE_TABS_BUTTON,
                                             _selectedTabsCount)
      forState:UIControlStateNormal];
}

// Updates the bottom toolbar layout.
- (void)updateLayout {
  if (IsDiamondPrototypeEnabled()) {
    return;
  }

  // Search mode doesn't have bottom toolbar or floating buttons, Handle it and
  // return early in that case.
  [self hideAllButtons];

  BOOL useCompactLayout = [self shouldUseCompactLayout];
  BOOL hideToolbar;
  if (base::FeatureList::IsEnabled(kTabRecallNewTabGroupButton)) {
    hideToolbar = self.mode == TabGridMode::kSearch;
  } else {
    hideToolbar = self.mode == TabGridMode::kSearch ||
                  (!useCompactLayout && (self.page == TabGridPageTabGroups));
  }
  if (hideToolbar) {
    self.hidden = YES;
    [self updateBackgroundVisibility];
    return;
  }

  self.hidden = NO;
  _viewTopConstraint.active = NO;

  if (self.mode == TabGridMode::kSelection) {
    _closeTabsButton.hidden = NO;
    _shareButton.hidden = NO;
    _addToButton.hidden = NO;
    _viewTopConstraint =
        [self.topAnchor constraintEqualToAnchor:_containerToolbar.topAnchor];
    _viewTopConstraint.active = YES;
    _containerToolbar.hidden = NO;
    [self updateBackgroundVisibility];
    return;
  }

  if (useCompactLayout) {
    if (self.page == TabGridPageTabGroups) {
      _doneButton.hidden = NO;

      if (base::FeatureList::IsEnabled(kTabRecallNewTabGroupButton)) {
        _smallNewTabButton.hidden = NO;
      }
    } else if (self.isInTabGroupView) {
      _smallNewTabButton.hidden = NO;
    } else {
      if (_undoActive) {
        _undoButton.hidden = NO;
      } else {
        _editButton.hidden = NO;
      }
      _smallNewTabButton.hidden = NO;
      _doneButton.hidden = NO;
    }
    _viewTopConstraint =
        [self.topAnchor constraintEqualToAnchor:_containerToolbar.topAnchor];
    _viewTopConstraint.active = YES;
    _containerToolbar.hidden = NO;
    [self updateBackgroundVisibility];
    return;
  }

  _largeNewTabButton.hidden = NO;
  _viewTopConstraint =
      [self.topAnchor constraintEqualToAnchor:_largeNewTabButton.topAnchor];
  _viewTopConstraint.active = YES;
  _containerToolbar.hidden = YES;
  [self updateBackgroundVisibility];
}

// Returns YES if the `_largeNewTabButton` is showing on the toolbar.
- (BOOL)isShowingFloatingButton {
  return !_largeNewTabButton.hidden;
}

// Returns YES if should use compact bottom toolbar layout.
- (BOOL)shouldUseCompactLayout {
  return self.traitCollection.verticalSizeClass ==
             UIUserInterfaceSizeClassRegular &&
         self.traitCollection.horizontalSizeClass ==
             UIUserInterfaceSizeClassCompact;
}

// Creates and configures the two background for the scrolled in the
// middle/scrolled to the top states.
- (void)createScrolledBackgrounds {
  _scrolledToEdge = YES;

  if (@available(iOS 26, *)) {
    return;
  }

  if (IsIOSSoftLockEnabled()) {
    _scrollBackgroundView = [[TabGridToolbarScrollingBackground alloc] init];
    _scrollBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_scrollBackgroundView];
    AddSameConstraintsToSides(
        self, _scrollBackgroundView,
        LayoutSides::kLeading | LayoutSides::kTop | LayoutSides::kTrailing);
  } else {
    _backgroundView =
        [[TabGridToolbarBackground alloc] initWithFrame:self.frame];
    _backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_backgroundView];
    AddSameConstraintsToSides(
        self, _backgroundView,
        LayoutSides::kLeading | LayoutSides::kTop | LayoutSides::kTrailing);
  }
}

// Updates the visibility of the backgrounds based on the state of the TabGrid.
- (void)updateBackgroundVisibility {
  BOOL scrolledToBottomHidden =
      _hideScrolledToEdgeBackground ||
      ([self isShowingFloatingButton] || !_scrolledToEdge);
  BOOL scrolledBackgroundViewHidden =
      [self isShowingFloatingButton] || _scrolledToEdge;
  if (IsIOSSoftLockEnabled()) {
    [_scrollBackgroundView
            updateBackgroundsForPage:self.page
                scrolledToEdgeHidden:scrolledToBottomHidden
        scrolledBackgroundViewHidden:scrolledBackgroundViewHidden];
  } else {
    [_backgroundView setScrolledOverContentBackgroundViewHidden:
                         scrolledBackgroundViewHidden];
    [_backgroundView
        setScrolledToEdgeBackgroundViewHidden:scrolledToBottomHidden];
  }
}

// Hides all buttons from superView.
- (void)hideAllButtons {
  _undoButton.hidden = YES;
  _doneButton.hidden = YES;
  _editButton.hidden = YES;
  _addToButton.hidden = YES;
  _closeTabsButton.hidden = YES;
  _shareButton.hidden = YES;
  _smallNewTabButton.hidden = YES;
  _largeNewTabButton.hidden = YES;
}

#pragma mark - Public

- (void)respondBeforeResponder:(UIResponder*)nextResponder {
  _followingNextResponder = nextResponder;
}

#pragma mark - UIResponder

- (UIResponder*)nextResponder {
  return _followingNextResponder;
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  return @[ UIKeyCommand.cr_undo, UIKeyCommand.cr_close ];
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  if (sel_isEqual(action, @selector(keyCommand_closeAll))) {
    return !_undoActive && _undoButton.enabled;
  }
  if (sel_isEqual(action, @selector(keyCommand_undo))) {
    return _undoActive;
  }
  if (sel_isEqual(action, @selector(keyCommand_close))) {
    return _doneButton.enabled;
  }
  return [super canPerformAction:action withSender:sender];
}

- (void)keyCommand_closeAll {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandCloseAll"));
  [self closeAllButtonTapped:nil];
}

- (void)keyCommand_undo {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandUndo"));
  // This function is also responsible for handling undo.
  // TODO(crbug.com/40273478): This should be separated to avoid confusion.
  [self closeAllButtonTapped:nil];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction(kMobileKeyCommandClose));
  [self doneButtonTapped:nil];
}

#pragma mark - Control actions

- (void)closeAllButtonTapped:(id)sender {
  if (_undoButton.enabled) {
    [self.buttonsDelegate closeAllButtonTapped:sender];
  }
}

- (void)doneButtonTapped:(id)sender {
  if (_doneButton.enabled) {
    [self.buttonsDelegate doneButtonTapped:sender];
  }
}

- (void)newTabButtonTapped:(id)sender {
  if (_largeNewTabButton.enabled || _smallNewTabButton.enabled) {
    [self.buttonsDelegate newTabButtonTapped:sender];
  }
}

- (void)closeSelectedTabs:(id)sender {
  if (_closeTabsButton.enabled) {
    [self.buttonsDelegate closeSelectedTabs:sender];
  }
}

- (void)shareSelectedTabs:(id)sender {
  if (_shareButton.enabled) {
    [self.buttonsDelegate shareSelectedTabs:sender];
  }
}

#pragma mark - Setters

- (void)setHideScrolledToEdgeBackground:(BOOL)hideScrolledToEdgeBackground {
  if (_hideScrolledToEdgeBackground == hideScrolledToEdgeBackground) {
    return;
  }
  _hideScrolledToEdgeBackground = hideScrolledToEdgeBackground;
  [self updateBackgroundVisibility];
}

@end
