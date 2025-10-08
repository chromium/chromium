// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_top_toolbar.h"

#import <objc/runtime.h>

#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/location.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/features.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_page_control.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbar_background.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbar_scrolling_background.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbars_grid_delegate.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbars_utils.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Font size for the selection string.
const CGFloat kSelectionFontSize = 17;
// Horizontal margin between the elements.
const CGFloat kHorizontalMarginiOS26 = 8;

const CGFloat kHorizontalMarginPre26 = 4;
const CGFloat kLeadingTrailingMargin = 12;
// Button minimal width.
const CGFloat kButtonMinWidth = 44;

// The size of top toolbar search symbol image.
const CGFloat kSymbolSearchImagePointSize = 22;

// Returns the horizontal margin to be used, depending on the OS version.
CGFloat HorizontalMargin() {
  if (@available(iOS 26, *)) {
    return kHorizontalMarginiOS26;
  }

  return kHorizontalMarginPre26;
}

}  // namespace

@interface TabGridTopToolbar ()
@end

@implementation TabGridTopToolbar {
  UIButton* _selectAllButton;
  UILabel* _selectedTabsLabel;
  UIButton* _searchButton;
  UIButton* _doneButton;
  UIButton* _undoButton;
  UIButton* _editButton;
  // Search mode
  UISearchBar* _searchBar;
  UIButton* _cancelSearchButton;
  // Constraint to be activated when the search bar is presented in regular
  // width.
  NSLayoutConstraint* _searchRegularWidthConstraint;
  // Constraints for the positioning of the search button.
  NSLayoutConstraint* _searchFirstConstraint;
  NSLayoutConstraint* _searchAfterUndoConstraint;
  NSLayoutConstraint* _searchAfterEditConstraint;

  NSArray<UIView*>* _allViews;

  BOOL _undoActive;

  BOOL _scrolledToEdge;
  TabGridToolbarBackground* _backgroundView;
  TabGridToolbarScrollingBackground* _scrollBackgroundView;

  // Configures the responder following the receiver in the responder chain.
  UIResponder* _followingNextResponder;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setupViews];
    [self setButtonsForTraitCollection:self.traitCollection];
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(
        @[ UITraitVerticalSizeClass.class, UITraitHorizontalSizeClass.class ]);
    __weak TabGridTopToolbar* weakSelf = self;
    [weakSelf
        registerForTraitChanges:traits
                    withHandler:^(id<UITraitEnvironment> traitEnvironment,
                                  UITraitCollection* previousCollection) {
                      [weakSelf
                          setButtonsForTraitCollection:weakSelf
                                                           .traitCollection];
                    }];
  }
  return self;
}

- (void)setPage:(TabGridPage)page {
  if (_page == page) {
    return;
  }
  _page = page;
  [self setButtonsForTraitCollection:self.traitCollection];
}

- (void)setMode:(TabGridMode)mode {
  if (_mode == mode) {
    return;
  }
  // Reset search state when exiting search mode.
  if (_mode == TabGridMode::kSearch) {
    _searchBar.text = @"";
    [_searchBar resignFirstResponder];
  }
  _mode = mode;
  // Reset selected tabs count when mode changes.
  self.selectedTabsCount = 0;
  // Reset the Select All button to its default title.
  [self configureSelectionButtonTitleSelectAll:YES];
  [self setButtonsForTraitCollection:self.traitCollection];
  if (mode == TabGridMode::kSearch) {
    // Focus the search bar, and make it a first responder once the user enter
    // to search mode. Doing that here instead in `setButtonsForTraitCollection`
    // makes sure it's only called once and allows VoiceOver to transition
    // smoothly and to say that there is a search field opened.
    // It is done on the next turn of the runloop as it has been seen to collide
    // with other animations on some devices.
    __weak __typeof(_searchBar) weakSearchBar = _searchBar;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(^{
          [weakSearchBar becomeFirstResponder];
        }));
  }
}

- (void)setSelectedTabsCount:(int)count {
  _selectedTabsCount = count;
  if (_selectedTabsCount == 0) {
    _selectedTabsLabel.text =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_SELECT_TABS_TITLE);
  } else {
    _selectedTabsLabel.text = l10n_util::GetPluralNSStringF(
        IDS_IOS_TAB_GRID_SELECTED_TABS_TITLE, _selectedTabsCount);
  }
}

- (void)setSearchBarDelegate:(id<UISearchBarDelegate>)delegate {
  _searchBar.delegate = delegate;
}

- (void)setSearchButtonEnabled:(BOOL)enabled {
  _searchButton.enabled = enabled;
}

- (void)setUndoButtonEnabled:(BOOL)enabled {
  _undoButton.enabled = enabled;
}

- (void)setSelectAllButtonEnabled:(BOOL)enabled {
  _selectAllButton.enabled = enabled;
}

- (void)setDoneButtonEnabled:(BOOL)enabled {
  _doneButton.enabled = enabled;
}

- (void)setIncognitoBackgroundHidden:(BOOL)hidden {
  [_scrollBackgroundView hideIncognitoToolbarBackground:hidden];
}

- (void)useUndo:(BOOL)useUndo {
  _undoButton.enabled = YES;
  if (_undoActive != useUndo) {
    _undoActive = useUndo;
    [self setButtonsForTraitCollection:self.traitCollection];
  }
}

- (void)configureSelectionButtonTitleSelectAll:(BOOL)selectAll {
  NSString* title =
      l10n_util::GetNSString(selectAll ? IDS_IOS_TAB_GRID_SELECT_ALL_BUTTON
                                       : IDS_IOS_TAB_GRID_DESELECT_ALL_BUTTON);
  UIButton* selectAllButton = _selectAllButton;
  if (@available(iOS 26, *)) {
    UIButtonConfiguration* conf = _selectAllButton.configuration;
    conf.title = title;
    _selectAllButton.configuration = conf;
  } else {
    [UIView performWithoutAnimation:^{
      [selectAllButton setTitle:title forState:UIControlStateNormal];
    }];
  }
}

- (void)highlightPageControlItem:(TabGridPage)page {
  [self.pageControl highlightPageControlItem:page];
}

- (void)resetLastPageControlHighlight {
  [self.pageControl resetLastPageControlHighlight];
}

- (void)hide {
  if (@available(iOS 26, *)) {
  } else {
    self.backgroundColor = UIColor.blackColor;
  }

  self.pageControl.alpha = 0.0;
}

- (void)show {
  if (@available(iOS 26, *)) {
  } else {
    self.backgroundColor = UIColor.clearColor;
  }

  self.pageControl.alpha = 1.0;
}

- (void)setScrollViewScrolledToEdge:(BOOL)scrolledToEdge {
  if (scrolledToEdge == _scrolledToEdge) {
    return;
  }

  _scrolledToEdge = scrolledToEdge;

  if (IsIOSSoftLockEnabled()) {
    [_scrollBackgroundView updateBackgroundsForPage:self.page
                               scrolledToEdgeHidden:!_scrolledToEdge
                       scrolledBackgroundViewHidden:_scrolledToEdge];
  } else {
    [_backgroundView setScrolledToEdgeBackgroundViewHidden:!_scrolledToEdge];
    [_backgroundView
        setScrolledOverContentBackgroundViewHidden:_scrolledToEdge];
  }

  [_pageControl setScrollViewScrolledToEdge:scrolledToEdge];
}

#pragma mark Edit Button

- (void)setEditButtonMenu:(UIMenu*)menu {
  _editButton.menu = menu;
}

- (void)setEditButtonEnabled:(BOOL)enabled {
  _editButton.enabled = enabled;
}

#pragma mark Search Bar

- (void)setSearchBarText:(NSString*)text {
  _searchBar.text = text;
  if ([_searchBar.delegate respondsToSelector:@selector(searchBar:
                                                    textDidChange:)]) {
    [_searchBar.delegate searchBar:_searchBar textDidChange:text];
  }
}

#pragma mark - UIView

- (CGSize)intrinsicContentSize {
  // In portrait orientation, UIKit returns a default height of 44 on iOS 18 and
  // earlier, while iOS 26 defaults to 48. In landscape orientation, iOS 18 and
  // earlier return 32 by default, whereas iOS 26 defaults to 44. It is unclear
  // what caused it. Therefore, intrinsicContentSize must be set to a fixed
  // height.
  return CGSizeMake(UIViewNoIntrinsicMetric, kTabGridTopToolbarHeight);
}

- (void)didMoveToSuperview {
  if (IsIOSSoftLockEnabled()) {
    if (_scrollBackgroundView) {
      [self.superview.topAnchor
          constraintEqualToAnchor:_scrollBackgroundView.topAnchor]
          .active = YES;
    }
  } else {
    if (_backgroundView) {
      [self.superview.topAnchor
          constraintEqualToAnchor:_backgroundView.topAnchor]
          .active = YES;
    }
  }
  [super didMoveToSuperview];
}

#pragma mark - Private

// Returns a new button to be used.
- (UIButton*)createButtonWithImage:(UIImage*)image
                             title:(NSString*)title
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

  button.translatesAutoresizingMaskIntoConstraints = NO;

  [button.heightAnchor constraintGreaterThanOrEqualToConstant:kButtonMinWidth]
      .active = YES;
  [button.widthAnchor constraintGreaterThanOrEqualToAnchor:button.heightAnchor]
      .active = YES;

  if (targetSelector) {
    [button addTarget:self
                  action:targetSelector
        forControlEvents:UIControlEventTouchUpInside];
  }

  return button;
}

// Sets up the buttons for the `traitCollection`.
- (void)setButtonsForTraitCollection:(UITraitCollection*)traitCollection {
  for (UIView* view in _allViews) {
    view.hidden = YES;
  }
  _searchFirstConstraint.active = NO;
  _searchAfterEditConstraint.active = NO;
  _searchAfterUndoConstraint.active = NO;
  if ([self shouldUseCompactLayout:traitCollection]) {
    switch (_mode) {
      case TabGridMode::kNormal:
        _searchFirstConstraint.active = YES;
        _searchButton.hidden = NO;
        _pageControl.hidden = NO;
        break;
      case TabGridMode::kSearch:
        _searchRegularWidthConstraint.active = NO;
        _searchBar.hidden = NO;
        _cancelSearchButton.hidden = NO;
        break;
      case TabGridMode::kSelection:
        _selectAllButton.hidden = NO;
        _selectedTabsLabel.hidden = NO;
        _doneButton.hidden = NO;
        break;
    }
  } else {
    switch (_mode) {
      case TabGridMode::kNormal: {
        if (_undoActive) {
          _undoButton.hidden = NO;
          _searchAfterUndoConstraint.active = YES;
        } else {
          _editButton.hidden = NO;
          _searchAfterEditConstraint.active = YES;
        }
        _searchButton.hidden = NO;
        _pageControl.hidden = NO;
        _doneButton.hidden = NO;
        break;
      }
      case TabGridMode::kSearch:
        _searchRegularWidthConstraint.active = YES;
        _searchBar.hidden = NO;
        _cancelSearchButton.hidden = NO;
        break;
      case TabGridMode::kSelection:
        _selectAllButton.hidden = NO;
        _selectedTabsLabel.hidden = NO;
        _doneButton.hidden = NO;
        break;
    }
  }
}

// Creates and sets up the different views of the toolbar.
- (void)setupViews {
  UIToolbarAppearance* appearance = [[UIToolbarAppearance alloc] init];
  [appearance configureWithTransparentBackground];
  [self setStandardAppearance:appearance];

  self.translatesAutoresizingMaskIntoConstraints = NO;
  if (@available(iOS 26, *)) {
  } else {
    self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
    [self createScrolledBackgrounds];
    [self setShadowImage:[[UIImage alloc] init]
        forToolbarPosition:UIBarPositionAny];
  }

  UIView* containerView = [[UIStackView alloc] init];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  [containerView.heightAnchor
      constraintEqualToConstant:kTabGridTopToolbarHeight]
      .active = YES;

  _undoButton =
      [self createButtonWithImage:nil
                            title:l10n_util::GetNSString(
                                      IDS_IOS_TAB_GRID_UNDO_CLOSE_ALL_BUTTON)
                   targetSelector:@selector(closeAllButtonTapped:)];
  _undoButton.accessibilityIdentifier = kTabGridUndoCloseAllButtonIdentifier;
  [self useUndo:NO];

  // The segmented control has an intrinsic size.
  _pageControl = [[TabGridPageControl alloc] init];
  _pageControl.translatesAutoresizingMaskIntoConstraints = NO;

  LayoutGuideCenter* center = LayoutGuideCenterForBrowser(nil);
  [center referenceView:_pageControl underName:kTabGridPageControlGuide];
  [_pageControl setScrollViewScrolledToEdge:_scrolledToEdge];

  _doneButton = [self
      createButtonWithImage:nil
                      title:l10n_util::GetNSString(IDS_IOS_TAB_GRID_DONE_BUTTON)
             targetSelector:@selector(doneButtonTapped:)];
  _doneButton.accessibilityIdentifier = kTabGridDoneButtonIdentifier;

  _editButton = [self
      createButtonWithImage:nil
                      title:l10n_util::GetNSString(IDS_IOS_TAB_GRID_EDIT_BUTTON)
             targetSelector:nil];
  _editButton.showsMenuAsPrimaryAction = YES;
  _editButton.accessibilityIdentifier = kTabGridEditButtonIdentifier;

  _selectAllButton =
      [self createButtonWithImage:nil
                            title:nil
                   targetSelector:@selector(selectAllButtonTapped:)];
  _selectAllButton.accessibilityIdentifier =
      kTabGridEditSelectAllButtonIdentifier;

  _selectedTabsLabel = [[UILabel alloc] init];
  _selectedTabsLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _selectedTabsLabel.text =
      l10n_util::GetNSString(IDS_IOS_TAB_GRID_SELECT_TABS_TITLE);
  _selectedTabsLabel.textColor = UIColor.whiteColor;
  _selectedTabsLabel.adjustsFontSizeToFitWidth = YES;
  _selectedTabsLabel.font =
      [[UIFontMetrics metricsForTextStyle:UIFontTextStyleBody]
          scaledFontForFont:[UIFont systemFontOfSize:kSelectionFontSize
                                              weight:UIFontWeightSemibold]];

  UIImage* searchImage =
      DefaultSymbolWithPointSize(kSearchSymbol, kSymbolSearchImagePointSize);
  _searchButton = [self createButtonWithImage:searchImage
                                        title:nil
                               targetSelector:@selector(searchButtonTapped:)];
  _searchButton.accessibilityIdentifier = kTabGridSearchButtonIdentifier;

  _searchBar = [[UISearchBar alloc] init];
  _searchBar.translatesAutoresizingMaskIntoConstraints = NO;
  _searchBar.placeholder =
      l10n_util::GetNSString(IDS_IOS_TAB_GRID_SEARCHBAR_PLACEHOLDER);
  _searchBar.accessibilityIdentifier = kTabGridSearchBarIdentifier;

  if (@available(iOS 26, *)) {
    _cancelSearchButton =
        [self createButtonWithImage:DefaultCloseButtonForToolbar()
                              title:nil
                     targetSelector:@selector(cancelSearchButtonTapped:)];
  } else {
    _cancelSearchButton =
        [self createButtonWithImage:nil
                              title:l10n_util::GetNSString(
                                        IDS_IOS_TAB_GRID_CANCEL_BUTTON)
                     targetSelector:@selector(cancelSearchButtonTapped:)];
  }

  _cancelSearchButton.accessibilityIdentifier = kTabGridCancelButtonIdentifier;

  [self setUpConstraintsForContainerView:containerView];
}

// Adds the different views to the view hierarchy and setup their constraints.
- (void)setUpConstraintsForContainerView:(UIView*)containerView {
  [self addSubview:containerView];
  UILayoutGuide* safeAreaLayoutGuide = self.safeAreaLayoutGuide;
  CGFloat containerSideMargin;
  if (@available(iOS 26, *)) {
    containerSideMargin = 0;
  } else {
    containerSideMargin = kLeadingTrailingMargin;
  }

  [NSLayoutConstraint activateConstraints:@[
    [containerView.leadingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.leadingAnchor
                       constant:containerSideMargin],
    [safeAreaLayoutGuide.trailingAnchor
        constraintEqualToAnchor:containerView.trailingAnchor
                       constant:containerSideMargin],
    [containerView.topAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.topAnchor],
    [containerView.bottomAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.bottomAnchor],
  ]];

  _allViews = @[
    _selectAllButton, _editButton, _undoButton, _searchButton, _pageControl,
    _selectedTabsLabel, _searchBar, _cancelSearchButton, _doneButton
  ];
  for (UIView* view in _allViews) {
    [containerView addSubview:view];
    [view.centerYAnchor constraintEqualToAnchor:containerView.centerYAnchor]
        .active = YES;
  }

  _searchRegularWidthConstraint = [_searchBar.widthAnchor
      constraintEqualToAnchor:self.safeAreaLayoutGuide.widthAnchor
                   multiplier:kTabGridSearchBarNonCompactWidthRatioModifier];
  _searchRegularWidthConstraint.priority = UILayoutPriorityDefaultHigh - 1;
  NSLayoutConstraint* searchBarMaximumWidth = [_searchBar.widthAnchor
      constraintEqualToAnchor:self.safeAreaLayoutGuide.widthAnchor];
  searchBarMaximumWidth.priority = _searchRegularWidthConstraint.priority - 1;

  _searchFirstConstraint = [_searchButton.leadingAnchor
      constraintEqualToAnchor:containerView.leadingAnchor
                     constant:HorizontalMargin()];
  _searchAfterUndoConstraint = [_searchButton.leadingAnchor
      constraintEqualToAnchor:_undoButton.trailingAnchor
                     constant:HorizontalMargin()];
  _searchAfterEditConstraint = [_searchButton.leadingAnchor
      constraintEqualToAnchor:_editButton.trailingAnchor
                     constant:HorizontalMargin()];

  NSLayoutConstraint* centeredLabelConstraint =
      [_selectedTabsLabel.centerXAnchor
          constraintEqualToAnchor:containerView.centerXAnchor];
  centeredLabelConstraint.priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    searchBarMaximumWidth,

    [_undoButton.leadingAnchor
        constraintEqualToAnchor:containerView.leadingAnchor
                       constant:HorizontalMargin()],
    [_editButton.leadingAnchor
        constraintEqualToAnchor:containerView.leadingAnchor
                       constant:HorizontalMargin()],
    [_selectAllButton.leadingAnchor
        constraintEqualToAnchor:containerView.leadingAnchor
                       constant:HorizontalMargin()],
    [_pageControl.centerXAnchor
        constraintEqualToAnchor:containerView.centerXAnchor],

    centeredLabelConstraint,
    [_selectedTabsLabel.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:_selectAllButton.trailingAnchor
                                    constant:HorizontalMargin()],
    [_selectedTabsLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_doneButton.leadingAnchor
                                 constant:-HorizontalMargin()],

    [_doneButton.trailingAnchor
        constraintEqualToAnchor:containerView.trailingAnchor
                       constant:-HorizontalMargin()],

    [_searchBar.leadingAnchor
        constraintEqualToAnchor:containerView.leadingAnchor
                       constant:HorizontalMargin()],
    [_searchBar.trailingAnchor
        constraintEqualToAnchor:_cancelSearchButton.leadingAnchor
                       constant:-HorizontalMargin()],
    [_cancelSearchButton.trailingAnchor
        constraintLessThanOrEqualToAnchor:containerView.trailingAnchor
                                 constant:-HorizontalMargin()],
  ]];

  [self setButtonsForTraitCollection:self.traitCollection];
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
    [self insertSubview:_scrollBackgroundView atIndex:0];
    AddSameConstraintsToSides(
        self, _scrollBackgroundView,
        LayoutSides::kLeading | LayoutSides::kBottom | LayoutSides::kTrailing);
  } else {
    _backgroundView =
        [[TabGridToolbarBackground alloc] initWithFrame:self.frame];
    _backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_backgroundView];
    AddSameConstraintsToSides(
        self, _backgroundView,
        LayoutSides::kLeading | LayoutSides::kBottom | LayoutSides::kTrailing);
  }

  // A non-nil UIImage has to be added in the background of the toolbar to
  // avoid having an additional blur effect.
  [self setBackgroundImage:[[UIImage alloc] init]
        forToolbarPosition:UIBarPositionAny
                barMetrics:UIBarMetricsDefault];
}

// Returns YES if should use compact bottom toolbar layout.
- (BOOL)shouldUseCompactLayout:(UITraitCollection*)traitCollection {
  return traitCollection.verticalSizeClass == UIUserInterfaceSizeClassRegular &&
         traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact;
}

#pragma mark - Public

- (void)unfocusSearchBar {
  [_searchBar resignFirstResponder];
}

- (void)respondBeforeResponder:(UIResponder*)nextResponder {
  _followingNextResponder = nextResponder;
}

- (void)setBackgroundContentOffset:(CGPoint)backgroundContentOffset
                          animated:(BOOL)animated {
  [_scrollBackgroundView setContentOffset:backgroundContentOffset
                                 animated:animated];
}

#pragma mark - UIResponder

- (NSArray<UIKeyCommand*>*)keyCommands {
  return @[ UIKeyCommand.cr_undo ];
}

- (UIResponder*)nextResponder {
  return _followingNextResponder;
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  if (sel_isEqual(action, @selector(keyCommand_closeAll))) {
    return !_undoActive && _undoButton.enabled;
  }
  if (sel_isEqual(action, @selector(keyCommand_undo))) {
    return _undoActive;
  }
  if (sel_isEqual(action, @selector(keyCommand_close))) {
    return _doneButton.enabled || _mode == TabGridMode::kSearch;
  }
  if (sel_isEqual(action, @selector(keyCommand_find))) {
    return _searchButton.enabled;
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
  if (_mode == TabGridMode::kSearch) {
    [self cancelSearchButtonTapped:nil];
  } else {
    [self doneButtonTapped:nil];
  }
}

- (void)keyCommand_find {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandSearchTabs"));
  [self searchButtonTapped:nil];
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

- (void)selectAllButtonTapped:(id)sender {
  if (_selectAllButton.enabled) {
    [self.buttonsDelegate selectAllButtonTapped:sender];
  }
}

- (void)searchButtonTapped:(id)sender {
  if (_searchButton.enabled) {
    [self.buttonsDelegate searchButtonTapped:sender];
  }
}

- (void)cancelSearchButtonTapped:(id)sender {
  if (_cancelSearchButton.enabled) {
    [self.buttonsDelegate cancelSearchButtonTapped:sender];
  }
}

@end
