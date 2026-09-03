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
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_entrypoint_view.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/menu/ui_bundled/action_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/scene_layout_state.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
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
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
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

@interface TabGridTopToolbar () <SceneLayoutStateObserver>
@end

@implementation TabGridTopToolbar {
  UIButton* _selectAllButton;
  UILabel* _selectedTabsLabel;
  UIButton* _searchButton;
  UIButton* _exitTabGridButton;
  UIButton* _exitSelectionButton;
  UIButton* _overflowMenuButton;
  // Search mode
  UISearchBar* _searchBar;
  UIButton* _cancelSearchButton;
  // Constraint to be activated when the search bar is presented in regular
  // width.
  NSLayoutConstraint* _searchRegularWidthConstraint;
  // Constraints for the positioning of the search button.
  NSLayoutConstraint* _searchFirstConstraint;
  // Constraints for the positioning of the search button.
  NSLayoutConstraint* _pageActionMenuEntrypointFirstConstraint;
  NSLayoutConstraint* _pageActionMenuEntrypointBeforeDoneConstraint;
  NSLayoutConstraint* _searchAfterOverflowConstraint;
  // Constraints for the positioning of the overflow menu button.
  NSLayoutConstraint* _overflowMenuConstraint;
  NSLayoutConstraint* _overflowMenuBeforeDoneConstraint;

  NSArray<UIView*>* _allViews;

  BOOL _selectTabsActionEnabled;
  BOOL _closeAllActionEnabled;
  BOOL _closeOtherTabsEnabled;

  BOOL _scrolledToEdge;
  TabGridToolbarBackground* _backgroundView;
  TabGridToolbarScrollingBackground* _scrollBackgroundView;

  // The button to access the page action menu.
  PageActionMenuEntrypointView* _pageActionMenuEntrypointView;

  // The layout guide center for this view.
  LayoutGuideCenter* _layoutGuideCenter;

  // Constraints for selection mode, activated the first time selection mode is
  // entered.
  NSArray<NSLayoutConstraint*>* _selectionModeConstraints;
}

- (instancetype)initWithLayoutGuideCenter:
    (LayoutGuideCenter*)layoutGuideCenter {
  // Use a non-zero frame to avoid breaking constraints.
  self = [super initWithFrame:CGRectMake(0, 0, 100, 100)];
  if (self) {
    _layoutGuideCenter = layoutGuideCenter;
    [self setupViews];
    [self setButtonsForTraitCollection:self.traitCollection];
  }
  return self;
}

- (void)setPage:(TabGridPage)page {
  if (_page == page) {
    return;
  }
  _page = page;
  _overflowMenuButton.menu = [self createOverflowMenu];
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
  if (_mode == TabGridMode::kSelection) {
    [NSLayoutConstraint activateConstraints:_selectionModeConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:_selectionModeConstraints];
  }
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

- (void)setSelectTabsActionEnabled:(BOOL)enabled {
  _selectTabsActionEnabled = enabled;
  _overflowMenuButton.menu = [self createOverflowMenu];
}

- (void)setCloseAllActionEnabled:(BOOL)enabled {
  _closeAllActionEnabled = enabled;
  _overflowMenuButton.menu = [self createOverflowMenu];
}

- (void)setCloseOtherTabsEnabled:(BOOL)enabled {
  _closeOtherTabsEnabled = enabled;
  _overflowMenuButton.menu = [self createOverflowMenu];
}

- (void)setLayoutState:(SceneLayoutState*)layoutState {
  if (_layoutState == layoutState) {
    return;
  }
  if (_layoutState) {
    [_layoutState removeObserver:self];
  }
  _layoutState = layoutState;
  if (_layoutState) {
    [_layoutState addObserver:self];
  }
  [self setButtonsForTraitCollection:self.traitCollection];
}

- (void)setSelectAllButtonEnabled:(BOOL)enabled {
  _selectAllButton.enabled = enabled;
}

- (void)setExitTabGridButtonEnabled:(BOOL)enabled {
  _exitTabGridButton.enabled = enabled;
}

- (void)setIncognitoBackgroundHidden:(BOOL)hidden {
  [_scrollBackgroundView hideIncognitoToolbarBackground:hidden];
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

#pragma mark Page Action Menu Button

- (void)setPageActionMenuButtonEnabled:(BOOL)enabled {
  _pageActionMenuEntrypointView.enabled = enabled;
}

- (void)setPageActionMenuButtonVisible:(BOOL)visible {
  _pageActionMenuEntrypointView.hidden = !visible;
}

#pragma mark Overflow Menu

- (void)setOverflowMenuEnabled:(BOOL)enabled {
  _overflowMenuButton.enabled = enabled;
}

#pragma mark Search Bar

- (void)setSearchBarText:(NSString*)text {
  _searchBar.text = text;
  if ([_searchBar.delegate respondsToSelector:@selector(searchBar:
                                                    textDidChange:)]) {
    [_searchBar.delegate searchBar:_searchBar textDidChange:text];
  }
}

#pragma mark - SceneLayoutStateObserver

- (void)layoutState:(SceneLayoutState*)layoutState
    didChangeAppBarPosition:(AppBarPosition)appBarPosition {
  [self setButtonsForTraitCollection:self.traitCollection];
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

  __weak TabGridTopToolbar* weakSelf = self;
  [weakSelf
      registerForTraitChanges:
          @[ UITraitVerticalSizeClass.class, UITraitHorizontalSizeClass.class ]
                  withHandler:^(id<UITraitEnvironment> traitEnvironment,
                                UITraitCollection* previousCollection) {
                    [weakSelf
                        setButtonsForTraitCollection:weakSelf.traitCollection];
                  }];

  [super didMoveToSuperview];
}

#pragma mark - Private

// Constraints for the selection mode.
- (NSArray<NSLayoutConstraint*>*)constraintsForSelectionModeWithContainerView:
    (UIView*)containerView {
  NSLayoutConstraint* centeredLabelConstraint =
      [_selectedTabsLabel.centerXAnchor
          constraintEqualToAnchor:containerView.centerXAnchor];
  centeredLabelConstraint.priority = UILayoutPriorityDefaultHigh;

  return @[
    // Horizontal layout:
    [_selectAllButton.leadingAnchor
        constraintEqualToAnchor:containerView.leadingAnchor
                       constant:HorizontalMargin()],
    centeredLabelConstraint,
    [_selectedTabsLabel.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:_selectAllButton.trailingAnchor
                                    constant:HorizontalMargin()],
    [_selectedTabsLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_exitSelectionButton.leadingAnchor
                                 constant:-HorizontalMargin()],
    [_exitSelectionButton.trailingAnchor
        constraintEqualToAnchor:containerView.trailingAnchor
                       constant:-HorizontalMargin()],
  ];
}

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
    button.tintColor = UIColor.clearColor;
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
    // The visibility of `_pageActionMenuEntrypointView` is exclusively
    // controlled by the active grid mediator. The
    // `setButtonsForTraitCollection` method should not modify
    // `_pageActionMenuEntrypointView.hidden` to avoid overriding the
    // mediator's configuration settings.
    if (view != _pageActionMenuEntrypointView) {
      view.hidden = YES;
    }
  }
  _searchFirstConstraint.active = NO;
  _pageActionMenuEntrypointFirstConstraint.active = NO;
  _pageActionMenuEntrypointBeforeDoneConstraint.active = NO;
  _searchAfterOverflowConstraint.active = NO;
  _overflowMenuConstraint.active = NO;
  _overflowMenuBeforeDoneConstraint.active = NO;

  _overflowMenuButton.hidden = NO;

  if ([self shouldUseCompactLayout:traitCollection]) {
    switch (_mode) {
      case TabGridMode::kNormal:
        _searchFirstConstraint.active = YES;
        _pageActionMenuEntrypointFirstConstraint.active = YES;
        _searchButton.hidden = NO;
        _pageControl.hidden = NO;
        if (self.page == TabGridPageTabGroups) {
          _overflowMenuButton.hidden = YES;
        } else {
          _overflowMenuConstraint.active = YES;
        }
        break;
      case TabGridMode::kSearch:
        _searchRegularWidthConstraint.active = NO;
        _searchBar.hidden = NO;
        _cancelSearchButton.hidden = NO;
        _overflowMenuButton.hidden = YES;
        break;
      case TabGridMode::kSelection:
        _selectAllButton.hidden = NO;
        _selectedTabsLabel.hidden = NO;
        _exitSelectionButton.hidden = NO;
        _overflowMenuButton.hidden = YES;
        break;
    }
  } else {
    switch (_mode) {
      case TabGridMode::kNormal: {
        _searchFirstConstraint.active = YES;
        if (self.page == TabGridPageTabGroups) {
          _overflowMenuButton.hidden = YES;
        } else {
          _overflowMenuBeforeDoneConstraint.active = YES;
        }
        _pageActionMenuEntrypointBeforeDoneConstraint.active = YES;
        _searchButton.hidden = NO;
        _pageControl.hidden = NO;
        BOOL appBarAvailable =
            self.layoutState.appBarPosition != AppBarPosition::kNone;
        if (IsChromeNextIaEnabled() && appBarAvailable) {
          // When the App Bar is available, there should not be a "Done" button
          // to exit the Tab Grid. The grid is dismissed with the Tab Grid
          // button in the App Bar.
          _overflowMenuConstraint.active = YES;
          _overflowMenuBeforeDoneConstraint.active = NO;
          _pageActionMenuEntrypointFirstConstraint.active = YES;
          _pageActionMenuEntrypointBeforeDoneConstraint.active = NO;
        } else {
          _exitTabGridButton.hidden = NO;
        }
        break;
      }
      case TabGridMode::kSearch:
        _searchRegularWidthConstraint.active = YES;
        _searchBar.hidden = NO;
        _cancelSearchButton.hidden = NO;
        _overflowMenuButton.hidden = YES;
        break;
      case TabGridMode::kSelection:
        _selectAllButton.hidden = NO;
        _selectedTabsLabel.hidden = NO;
        _exitSelectionButton.hidden = NO;
        _overflowMenuButton.hidden = YES;
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
  self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  if (@available(iOS 26, *)) {
  } else {
    [self createScrolledBackgrounds];
    [self setShadowImage:[[UIImage alloc] init]
        forToolbarPosition:UIBarPositionAny];
  }

  UIView* containerView = [[UIStackView alloc] init];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  [containerView.heightAnchor
      constraintEqualToConstant:kTabGridTopToolbarHeight]
      .active = YES;

  // The segmented control has an intrinsic size.
  _pageControl =
      [[TabGridPageControl alloc] initWithLayoutGuideCenter:_layoutGuideCenter];
  _pageControl.translatesAutoresizingMaskIntoConstraints = NO;

  [_layoutGuideCenter referenceView:_pageControl
                          underName:kTabGridPageControlGuide];
  [_pageControl setScrollViewScrolledToEdge:_scrolledToEdge];

  _exitTabGridButton = [self
      createButtonWithImage:nil
                      title:l10n_util::GetNSString(IDS_IOS_TAB_GRID_DONE_BUTTON)
             targetSelector:@selector(exitTabGridButtonTapped:)];
  _exitTabGridButton.accessibilityIdentifier =
      kTabGridExitTabGridButtonIdentifier;

  if (@available(iOS 26, *)) {
    _exitSelectionButton =
        [self createButtonWithImage:DefaultDoneButtonForToolbar()
                              title:nil
                     targetSelector:@selector(exitSelectionButtonTapped:)];
    _exitSelectionButton.tintColor = [UIColor colorNamed:kBlueColor];
  } else {
    _exitSelectionButton =
        [self createButtonWithImage:nil
                              title:l10n_util::GetNSString(
                                        IDS_IOS_TAB_GRID_DONE_BUTTON)
                     targetSelector:@selector(exitSelectionButtonTapped:)];
  }
  _exitSelectionButton.accessibilityIdentifier =
      kTabGridExitSelectionButtonIdentifier;

  UIImage* overflowMenuImage =
      SymbolWithPointSize(SymbolMenu, kSymbolSearchImagePointSize);
  _overflowMenuButton = [self createButtonWithImage:overflowMenuImage
                                              title:nil
                                     targetSelector:nil];
  _overflowMenuButton.showsMenuAsPrimaryAction = YES;
  _overflowMenuButton.accessibilityIdentifier =
      kTabGridOverflowMenuButtonIdentifier;
  _overflowMenuButton.menu = [self createOverflowMenu];

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
      SymbolWithPointSize(SymbolSearch, kSymbolSearchImagePointSize);
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

  _pageActionMenuEntrypointView = [[PageActionMenuEntrypointView alloc] init];
  [_pageActionMenuEntrypointView addTarget:self
                                    action:@selector(pageActionMenuTapped:)
                          forControlEvents:UIControlEventTouchUpInside];

  [self setUpConstraintsForContainerView:containerView];
}

// Configures and returns the overflow menu.
- (UIMenu*)createOverflowMenu {
  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];

  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:kMenuScenarioHistogramTabGridEdit];
  __weak __typeof(self) weakSelf = self;

  [menuElements addObject:[actionFactory actionToCreateEmptyTabGroupWithBlock:^{
                  [weakSelf.buttonsDelegate createNewTabGroupButtonTapped:nil];
                }]];

  // Only display the Select Tabs action if there are tabs.
  if (_selectTabsActionEnabled) {
    [menuElements addObject:[actionFactory actionToSelectTabsWithBlock:^{
                    [weakSelf.buttonsDelegate selectTabsButtonTapped:nil];
                  }]];
  }

  // Only display the Close All Tabs button if there are open tabs or groups.
  if (_closeAllActionEnabled) {
    UIButton* currentOverflowMenuButton = _overflowMenuButton;
    [menuElements addObject:[actionFactory actionToCloseAllTabsWithBlock:^{
                    TabGridTopToolbar* strongSelf = weakSelf;
                    if (!strongSelf) {
                      return;
                    }
                    [strongSelf.buttonsDelegate
                        closeAllButtonTapped:currentOverflowMenuButton];
                  }]];
  }

  if (_closeOtherTabsEnabled) {
    UIAction* closeOtherTabsAction =
        [actionFactory actionToCloseAllOtherTabsWithBlock:^{
          [weakSelf.buttonsDelegate closeOtherTabsButtonTapped:nil];
        }];
    UIMenu* closeOtherMenu = [UIMenu menuWithTitle:@""
                                             image:nil
                                        identifier:nil
                                           options:UIMenuOptionsDisplayInline
                                          children:@[ closeOtherTabsAction ]];
    [menuElements addObject:closeOtherMenu];
  }

  if (_page == TabGridPageRegularTabs) {
    [menuElements
        addObject:[actionFactory actionToDeleteBrowsingDataWithBlock:^{
          [weakSelf.buttonsDelegate deleteBrowsingDataButtonTapped:nil];
        }]];
  }

  return [UIMenu menuWithChildren:menuElements];
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
    _selectAllButton, _overflowMenuButton, _searchButton, _pageControl,
    _selectedTabsLabel, _searchBar, _cancelSearchButton, _exitTabGridButton,
    _exitSelectionButton, _pageActionMenuEntrypointView
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

  _pageActionMenuEntrypointFirstConstraint =
      [_pageActionMenuEntrypointView.trailingAnchor
          constraintEqualToAnchor:containerView.trailingAnchor
                         constant:-HorizontalMargin()];
  _pageActionMenuEntrypointBeforeDoneConstraint =
      [_pageActionMenuEntrypointView.trailingAnchor
          constraintEqualToAnchor:_exitTabGridButton.leadingAnchor
                         constant:-HorizontalMargin()];

  _overflowMenuConstraint = [_overflowMenuButton.trailingAnchor
      constraintEqualToAnchor:containerView.trailingAnchor
                     constant:-HorizontalMargin()];
  _overflowMenuBeforeDoneConstraint = [_overflowMenuButton.trailingAnchor
      constraintEqualToAnchor:_exitTabGridButton.leadingAnchor
                     constant:-HorizontalMargin()];

  [NSLayoutConstraint activateConstraints:@[
    searchBarMaximumWidth,
    [_pageControl.centerXAnchor
        constraintEqualToAnchor:containerView.centerXAnchor],

    [_exitTabGridButton.trailingAnchor
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

  _selectionModeConstraints =
      [self constraintsForSelectionModeWithContainerView:containerView];

  if (_mode == TabGridMode::kSelection) {
    [NSLayoutConstraint activateConstraints:_selectionModeConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:_selectionModeConstraints];
  }

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
    AddSameConstraintsToSides(self, _scrollBackgroundView,
                              LayoutSides::kBottom | LayoutSides::kHorizontal);
  } else {
    _backgroundView =
        [[TabGridToolbarBackground alloc] initWithFrame:self.frame];
    _backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_backgroundView];
    AddSameConstraintsToSides(self, _backgroundView,
                              LayoutSides::kBottom | LayoutSides::kHorizontal);
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

- (void)setBackgroundContentOffset:(CGPoint)backgroundContentOffset
                          animated:(BOOL)animated {
  [_scrollBackgroundView setContentOffset:backgroundContentOffset
                                 animated:animated];
}

#pragma mark - UIResponder

- (NSArray<UIKeyCommand*>*)keyCommands {
  return @[ UIKeyCommand.cr_closeAll, UIKeyCommand.cr_close ];
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  if (sel_isEqual(action, @selector(keyCommand_closeAll))) {
    return _closeAllActionEnabled;
  }
  if (sel_isEqual(action, @selector(keyCommand_close))) {
    return _exitTabGridButton.enabled || _mode == TabGridMode::kSearch ||
           _mode == TabGridMode::kSelection;
  }
  if (sel_isEqual(action, @selector(keyCommand_find))) {
    return _searchButton.enabled;
  }
  return [super canPerformAction:action withSender:sender];
}

- (void)keyCommand_closeAll {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandCloseAll"));
  [self.buttonsDelegate closeAllButtonTapped:nil];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction(kMobileKeyCommandClose));
  switch (_mode) {
    case TabGridMode::kNormal:
      [self exitTabGridButtonTapped:nil];
      break;
    case TabGridMode::kSearch:
      [self cancelSearchButtonTapped:nil];
      break;
    case TabGridMode::kSelection:
      [self exitSelectionButtonTapped:nil];
      break;
  }
}

- (void)keyCommand_find {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandSearchTabs"));
  [self searchButtonTapped:nil];
}

#pragma mark - Control actions

- (void)exitTabGridButtonTapped:(id)sender {
  if (_exitTabGridButton.enabled) {
    [self.buttonsDelegate exitTabGridButtonTapped:sender];
  }
}

- (void)exitSelectionButtonTapped:(id)sender {
  if (_exitSelectionButton.enabled) {
    [self.buttonsDelegate exitSelectionButtonTapped:sender];
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

- (void)pageActionMenuTapped:(id)sender {
  if (_pageActionMenuEntrypointView.enabled) {
    [self.buttonsDelegate pageActionMenuEntrypointTapped:sender];
  }
}

#pragma mark - Accessibility

- (NSArray*)accessibilityElements {
  NSMutableArray* elements = [[NSMutableArray alloc] init];
  if (_selectAllButton && !_selectAllButton.hidden) {
    [elements addObject:_selectAllButton];
  }
  if (_searchButton && !_searchButton.hidden) {
    [elements addObject:_searchButton];
  }
  if (_pageControl && !_pageControl.hidden) {
    [elements addObject:_pageControl];
  }
  if (_selectedTabsLabel && !_selectedTabsLabel.hidden) {
    [elements addObject:_selectedTabsLabel];
  }
  if (_searchBar && !_searchBar.hidden) {
    [elements addObject:_searchBar];
  }
  if (_cancelSearchButton && !_cancelSearchButton.hidden) {
    [elements addObject:_cancelSearchButton];
  }
  if (_pageActionMenuEntrypointView && !_pageActionMenuEntrypointView.hidden) {
    [elements addObject:_pageActionMenuEntrypointView];
  }
  if (_overflowMenuButton && !_overflowMenuButton.hidden) {
    [elements addObject:_overflowMenuButton];
  }
  if (_exitTabGridButton && !_exitTabGridButton.hidden) {
    [elements addObject:_exitTabGridButton];
  }
  if (_exitSelectionButton && !_exitSelectionButton.hidden) {
    [elements addObject:_exitSelectionButton];
  }
  return elements;
}

@end
