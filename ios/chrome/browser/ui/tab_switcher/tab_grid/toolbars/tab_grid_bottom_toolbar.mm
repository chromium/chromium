// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_bottom_toolbar.h"

#import <objc/runtime.h>

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_new_tab_button.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_grid_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_utils.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation TabGridBottomToolbar {
  UIToolbar* _toolbar;
  UIBarButtonItem* _newTabButtonItem;
  UIBarButtonItem* _spaceItem;
  NSArray<NSLayoutConstraint*>* _compactConstraints;
  NSArray<NSLayoutConstraint*>* _floatingConstraints;
  NSLayoutConstraint* _largeNewTabButtonBottomAnchor;
  TabGridNewTabButton* _smallNewTabButton;
  TabGridNewTabButton* _largeNewTabButton;
  UIBarButtonItem* _doneButton;
  UIBarButtonItem* _closeAllOrUndoButton;
  UIBarButtonItem* _editButton;
  UIBarButtonItem* _addToButton;
  UIBarButtonItem* _closeTabsButton;
  UIBarButtonItem* _shareButton;
  BOOL _undoActive;
  BOOL _scrolledToEdge;
  UIView* _scrolledBackgroundView;
  // Configures the responder following the receiver in the responder chain.
  UIResponder* _followingNextResponder;
  UIView* _scrolledToBottomBackgroundView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setupViews];
    [self updateLayout];
    if (@available(iOS 17, *)) {
      NSArray<UITrait>* traits = TraitCollectionSetForTraits(
          @[ UITraitVerticalSizeClass.self, UITraitHorizontalSizeClass.self ]);
      [self registerForTraitChanges:traits withAction:@selector(updateLayout)];
    }
  }
  return self;
}

#pragma mark - UIView

- (void)didMoveToSuperview {
  if (_scrolledBackgroundView) {
    [self.superview.bottomAnchor
        constraintEqualToAnchor:_scrolledBackgroundView.bottomAnchor]
        .active = YES;
  }
  [super didMoveToSuperview];
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  if ((self.traitCollection.verticalSizeClass !=
       previousTraitCollection.verticalSizeClass) ||
      (self.traitCollection.horizontalSizeClass !=
       previousTraitCollection.horizontalSizeClass)) {
    [self updateLayout];
  }
}
#endif

// `pointInside` is called as long as this view is on the screen (even if its
// size is zero). It controls hit testing of the bottom toolbar. When the
// toolbar is transparent and has the `_largeNewTabButton`, only respond to
// tapping on that button.
- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
  if ([self isShowingFloatingButton]) {
    // Only floating new tab button is tappable.
    return [_largeNewTabButton
        pointInside:[self convertPoint:point toView:_largeNewTabButton]
          withEvent:event];
  }
  return [super pointInside:point withEvent:event];
}

// Returns intrinsicContentSize based on the content of the toolbar.
// When showing the floating Button the contentsize for the toolbar should be
// zero so that the toolbar isn't accounted for when calculating the bottom
// insets of the container view.
- (CGSize)intrinsicContentSize {
  if ([self isShowingFloatingButton] || self.subviews.count == 0) {
    return CGSizeZero;
  }
  return _toolbar.intrinsicContentSize;
}

#pragma mark - Public

- (void)setPage:(TabGridPage)page {
  if (_page == page) {
    return;
  }
  _page = page;
  _smallNewTabButton.page = page;
  _largeNewTabButton.page = page;
  // Reset the title of UIBarButtonItem to update the title in a11y modal panel.
  _newTabButtonItem.title = _largeNewTabButton.accessibilityLabel;
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

- (void)setDoneButtonHidden:(BOOL)hidden {
  _doneButton.hidden = hidden;
}

- (void)setCloseAllButtonEnabled:(BOOL)enabled {
  _closeAllOrUndoButton.enabled = enabled;
}

- (void)useUndoCloseAll:(BOOL)useUndo {
  _closeAllOrUndoButton.enabled = YES;
  if (useUndo) {
    _closeAllOrUndoButton.title =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_UNDO_CLOSE_ALL_BUTTON);
    // Setting the `accessibilityIdentifier` seems to trigger layout, which
    // causes an infinite loop.
    if (_closeAllOrUndoButton.accessibilityIdentifier !=
        kTabGridUndoCloseAllButtonIdentifier) {
      _closeAllOrUndoButton.accessibilityIdentifier =
          kTabGridUndoCloseAllButtonIdentifier;
    }
  } else {
    _closeAllOrUndoButton.title =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_CLOSE_ALL_BUTTON);
    // Setting the `accessibilityIdentifier` seems to trigger layout, which
    // causes an infinite loop.
    if (_closeAllOrUndoButton.accessibilityIdentifier !=
        kTabGridCloseAllButtonIdentifier) {
      _closeAllOrUndoButton.accessibilityIdentifier =
          kTabGridCloseAllButtonIdentifier;
    }
  }
  if (_undoActive != useUndo) {
    _undoActive = useUndo;
    [self updateLayout];
  }
}

- (void)hide {
  // The `_editButton` is hidden to dismiss its context menu if it's still
  // presented.
  _editButton.hidden = YES;
  _smallNewTabButton.alpha = 0.0;
  _largeNewTabButton.alpha = 0.0;
}

- (void)show {
  _editButton.hidden = NO;
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

- (void)setEditButtonHidden:(BOOL)hidden {
  _editButton.hidden = hidden;
}

#pragma mark - Private

- (void)setupViews {
  // For Regular(V) x Compact(H) layout, display UIToolbar.
  // In iOS 13, constraints break if the UIToolbar is initialized with a null or
  // zero rect frame. An arbitrary non-zero frame fixes this issue.
  _toolbar = [[UIToolbar alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  _toolbar.translatesAutoresizingMaskIntoConstraints = NO;
  [self createScrolledBackgrounds];
  _toolbar.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  // Remove the border of UIToolbar.
  [_toolbar setShadowImage:[[UIImage alloc] init]
        forToolbarPosition:UIBarPositionAny];
  [_toolbar
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                      forAxis:UILayoutConstraintAxisVertical];

  _closeAllOrUndoButton = [[UIBarButtonItem alloc] init];
  _closeAllOrUndoButton.target = self;
  _closeAllOrUndoButton.action = @selector(closeAllButtonTapped:);
  _closeAllOrUndoButton.tintColor =
      UIColorFromRGB(kTabGridToolbarTextButtonColor);

  _doneButton = [[UIBarButtonItem alloc] init];
  _doneButton.target = self;
  _doneButton.action = @selector(doneButtonTapped:);
  _doneButton.style = UIBarButtonItemStyleDone;
  _doneButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
  _doneButton.title = l10n_util::GetNSString(IDS_IOS_TAB_GRID_DONE_BUTTON);
  _doneButton.accessibilityIdentifier = kTabGridDoneButtonIdentifier;

  _spaceItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];

  _smallNewTabButton = [[TabGridNewTabButton alloc] initWithLargeSize:NO];
  [_smallNewTabButton addTarget:self
                         action:@selector(newTabButtonTapped:)
               forControlEvents:UIControlEventTouchUpInside];
  _smallNewTabButton.translatesAutoresizingMaskIntoConstraints = NO;
  _smallNewTabButton.page = self.page;

  _newTabButtonItem =
      [[UIBarButtonItem alloc] initWithCustomView:_smallNewTabButton];

  // Create selection mode buttons
  _editButton = [[UIBarButtonItem alloc] init];
  _editButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
  _editButton.title = l10n_util::GetNSString(IDS_IOS_TAB_GRID_EDIT_BUTTON);
  _editButton.accessibilityIdentifier = kTabGridEditButtonIdentifier;

  _addToButton = [[UIBarButtonItem alloc] init];
  _addToButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
  _addToButton.title = l10n_util::GetNSString(IDS_IOS_TAB_GRID_ADD_TO_BUTTON);
  _addToButton.accessibilityIdentifier = kTabGridEditAddToButtonIdentifier;
  _shareButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemAction
                           target:self
                           action:@selector(shareSelectedTabs:)];
  _shareButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
  _shareButton.accessibilityIdentifier = kTabGridEditShareButtonIdentifier;
  _closeTabsButton = [[UIBarButtonItem alloc] init];
  _closeTabsButton.target = self;
  _closeTabsButton.action = @selector(closeSelectedTabs:);
  _closeTabsButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
  _closeTabsButton.accessibilityIdentifier =
      kTabGridEditCloseTabsButtonIdentifier;
  [self updateCloseTabsButtonTitle];

  _compactConstraints = @[
    [_toolbar.topAnchor constraintEqualToAnchor:self.topAnchor],
    [_toolbar.bottomAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.bottomAnchor],
    [_toolbar.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [_toolbar.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
  ];

  // For other layout, display a floating new tab button.
  _largeNewTabButton = [[TabGridNewTabButton alloc] initWithLargeSize:YES];
  [_largeNewTabButton addTarget:self
                         action:@selector(newTabButtonTapped:)
               forControlEvents:UIControlEventTouchUpInside];
  // When a11y font size is used, long press on UIBarButtonItem will show a
  // built-in a11y modal panel with image and title if set. The size is not
  // taken into account.
  _newTabButtonItem.image = CustomSymbolWithPointSize(kPlusCircleFillSymbol, 0);
  _largeNewTabButton.translatesAutoresizingMaskIntoConstraints = NO;
  _largeNewTabButton.page = self.page;

  CGFloat floatingButtonVerticalInset = kTabGridFloatingButtonVerticalInset;

  _largeNewTabButtonBottomAnchor = [_largeNewTabButton.bottomAnchor
      constraintEqualToAnchor:self.safeAreaLayoutGuide.bottomAnchor
                     constant:-floatingButtonVerticalInset];

  _floatingConstraints = @[
    [_largeNewTabButton.topAnchor constraintEqualToAnchor:self.topAnchor],
    _largeNewTabButtonBottomAnchor,
    [_largeNewTabButton.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:-kTabGridFloatingButtonHorizontalInset],
  ];

  _newTabButtonItem.title = _largeNewTabButton.accessibilityLabel;
}

- (void)updateCloseTabsButtonTitle {
  _closeTabsButton.title = l10n_util::GetPluralNSStringF(
      IDS_IOS_TAB_GRID_CLOSE_TABS_BUTTON, _selectedTabsCount);
}

- (void)updateLayout {
  // Search mode doesn't have bottom toolbar or floating buttons, Handle it and
  // return early in that case.
  if (self.mode == TabGridMode::kSearch) {
    [NSLayoutConstraint deactivateConstraints:_compactConstraints];
    [NSLayoutConstraint deactivateConstraints:_floatingConstraints];
    [_toolbar removeFromSuperview];
    [_largeNewTabButton removeFromSuperview];
    self.hidden = YES;
    [self updateBackgroundVisibility];
    return;
  }
  _largeNewTabButtonBottomAnchor.constant =
      -kTabGridFloatingButtonVerticalInset;

  if (self.mode == TabGridMode::kSelection) {
    [NSLayoutConstraint deactivateConstraints:_floatingConstraints];
    [_largeNewTabButton removeFromSuperview];
    [_toolbar setItems:@[
      _closeTabsButton, _spaceItem, _shareButton, _spaceItem, _addToButton
    ]];
    [self addSubview:_toolbar];
    [NSLayoutConstraint activateConstraints:_compactConstraints];
    self.hidden = NO;
    [self updateBackgroundVisibility];
    return;
  }
  UIBarButtonItem* leadingButton = _closeAllOrUndoButton;
  if (!_undoActive) {
    leadingButton = _editButton;
  }
  UIBarButtonItem* trailingButton = _doneButton;

  if ([self shouldUseCompactLayout]) {
    [NSLayoutConstraint deactivateConstraints:_floatingConstraints];
    [_largeNewTabButton removeFromSuperview];

    // For incognito/regular pages, display all 3 buttons;
    // For Tab Groups and remote tabs page, only display trailing button.
    if (self.page == TabGridPageRemoteTabs ||
        self.page == TabGridPageTabGroups) {
      [_toolbar setItems:@[ _spaceItem, trailingButton ]];
    } else {
      [_toolbar setItems:@[
        leadingButton, _spaceItem, _newTabButtonItem, _spaceItem, trailingButton
      ]];
    }

    [self addSubview:_toolbar];
    [NSLayoutConstraint activateConstraints:_compactConstraints];
    self.hidden = NO;
  } else {
    [NSLayoutConstraint deactivateConstraints:_compactConstraints];
    [_toolbar removeFromSuperview];
    // Do not display new tab button for Tab Groups and remote tabs page.
    if (self.page == TabGridPageRemoteTabs ||
        self.page == TabGridPageTabGroups) {
      [NSLayoutConstraint deactivateConstraints:_floatingConstraints];
      [_largeNewTabButton removeFromSuperview];
      self.hidden = YES;
    } else {
      [self addSubview:_largeNewTabButton];
      [NSLayoutConstraint activateConstraints:_floatingConstraints];
      self.hidden = NO;
    }
  }

  [self updateBackgroundVisibility];
}

// Returns YES if the `_largeNewTabButton` is showing on the toolbar.
- (BOOL)isShowingFloatingButton {
  return _largeNewTabButton.superview &&
         _largeNewTabButtonBottomAnchor.isActive;
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

  // Background when the content is scrolled to the middle.
  _scrolledBackgroundView = CreateTabGridOverContentBackground();
  _scrolledBackgroundView.hidden = YES;
  _scrolledBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_scrolledBackgroundView];
  AddSameConstraintsToSides(
      self, _scrolledBackgroundView,
      LayoutSides::kLeading | LayoutSides::kTop | LayoutSides::kTrailing);

  // Background when the content is scrolled to the top.
  _scrolledToBottomBackgroundView = CreateTabGridScrolledToEdgeBackground();
  _scrolledToBottomBackgroundView.translatesAutoresizingMaskIntoConstraints =
      NO;
  [self addSubview:_scrolledToBottomBackgroundView];
  AddSameConstraints(_scrolledBackgroundView, _scrolledToBottomBackgroundView);

  // A non-nil UIImage has to be added in the background of the toolbar to avoid
  // having an additional blur effect.
  [_toolbar setBackgroundImage:[[UIImage alloc] init]
            forToolbarPosition:UIBarPositionAny
                    barMetrics:UIBarMetricsDefault];
}

// Updates the visibility of the backgrounds based on the state of the TabGrid.
- (void)updateBackgroundVisibility {
  _scrolledToBottomBackgroundView.hidden =
      _hideScrolledToEdgeBackground ||
      ([self isShowingFloatingButton] || !_scrolledToEdge);
  _scrolledBackgroundView.hidden =
      [self isShowingFloatingButton] || _scrolledToEdge;
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
    return !_undoActive && _closeAllOrUndoButton.enabled;
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
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  [self doneButtonTapped:nil];
}

#pragma mark - Control actions

- (void)closeAllButtonTapped:(id)sender {
  if (_closeAllOrUndoButton.enabled) {
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
