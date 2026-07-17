// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_constants.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_empty_state_view_controller.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_view_controller.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_no_data_view_controller.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_query_unsupported_view_controller.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_recent_fills_view_controller.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_item.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_result_item.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_results_view_controller.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_view_controller.h"
#import "ios/chrome/browser/autofill/atmemory/utils/atmemory_ui_util.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Constants for layout.
constexpr CGFloat kSearchFieldMargin = 16.0;
constexpr CGFloat kHeaderBarHeight = 56.0;
constexpr CGFloat kCloseButtonMargin = 16.0;
}  // namespace

@interface AtMemoryViewController () <
    UISearchBarDelegate,
    AtMemoryRecentFillsViewControllerDelegate,
    AtMemorySearchViewControllerDelegate,
    AtMemorySearchResultsViewControllerDelegate,
    AtMemoryGranularFillViewControllerDelegate,
    AtMemoryQueryUnsupportedViewControllerDelegate>
@end

@implementation AtMemoryViewController {
  // The search bar in the top header.
  UISearchBar* _searchBar;
  // The header bar containing the title and actions.
  UIView* _headerBar;
  // The label showing the title of the active screen.
  UILabel* _titleLabel;
  // The back button to navigate to the previous screen.
  UIButton* _backButton;
  // The currently presented child view controller.
  UIViewController* _childViewController;
  // The container view where child view controllers are loaded.
  UIView* _containerView;
  // The view controller for empty state.
  AtMemoryEmptyStateViewController* _emptyStateViewController;
  // The view controller for previously filled data list.
  AtMemoryRecentFillsViewController* _recentFillsViewController;
  // The view controller for granular fill list.
  AtMemoryGranularFillViewController* _granularFillViewController;
  // The view controller for the search input state.
  AtMemorySearchViewController* _searchViewController;
  // The view controller for search results.
  AtMemorySearchResultsViewController* _searchResultsViewController;
  // The view controller for empty search results.
  AtMemoryNoDataViewController* _noDataViewController;
  // The view controller for unsupported search queries.
  AtMemoryQueryUnsupportedViewController* _queryUnsupportedViewController;
  // The active view state of the screen.
  autofill::AtMemoryViewState _viewState;

  // Layout constraint linking container bottom to search bar top.
  NSLayoutConstraint* _containerBottomToSearchBarConstraint;
  // Layout constraint linking container bottom to safe area bottom.
  NSLayoutConstraint* _containerBottomToSafeAreaConstraint;
  // Cached list of previously filled items.
  NSArray<AtMemorySearchItem*>* _recentFills;
  // Cached list of granular fill items.
  NSArray<AtMemoryGranularFillItem*>* _granularFillItems;
  // The current search text.
  NSString* _searchQuery;
  // Whether search results are loading.
  BOOL _searchLoading;
  // The list of search results.
  NSArray<AtMemorySearchResultItem*>* _searchResults;
  // The title for the granular fill view controller.
  NSString* _granularFillTitle;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

  [self setupHeaderBar];
  [self setupSearchBar];
  [self setupContainerView];
  [self setupLayoutConstraints];

  [self applyViewState];
}

- (void)setupContainerView {
  _containerView = [[UIView alloc] init];
  _containerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_containerView];
}

- (void)setChildViewController:(UIViewController*)childViewController {
  if (_childViewController == childViewController) {
    return;
  }

  if (_childViewController) {
    [_childViewController willMoveToParentViewController:nil];
    [_childViewController.view removeFromSuperview];
    [_childViewController removeFromParentViewController];
  }

  _childViewController = childViewController;

  if (!_childViewController) {
    _titleLabel.text = nil;
    return;
  }

  _titleLabel.text = _childViewController.title;

  _childViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self addChildViewController:_childViewController];
  [_containerView addSubview:_childViewController.view];
  [_childViewController didMoveToParentViewController:self];

  AddSameConstraints(_containerView, _childViewController.view);
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [_searchBar.searchTextField becomeFirstResponder];
}

#pragma mark - UI Setup

- (void)setupHeaderBar {
  _headerBar = [[UIView alloc] init];
  _headerBar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_headerBar];

  _titleLabel = [[UILabel alloc] init];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _titleLabel.textAlignment = NSTextAlignmentCenter;
  [_headerBar addSubview:_titleLabel];

  _backButton = [self createBackButton];
  [_headerBar addSubview:_backButton];

  UIButton* closeButton = [self createCloseButton];
  [_headerBar addSubview:closeButton];

  [NSLayoutConstraint activateConstraints:@[
    [_titleLabel.centerXAnchor
        constraintEqualToAnchor:_headerBar.centerXAnchor],
    [_titleLabel.centerYAnchor
        constraintEqualToAnchor:_headerBar.centerYAnchor],

    [_backButton.centerYAnchor
        constraintEqualToAnchor:_headerBar.centerYAnchor],
    [_backButton.leadingAnchor constraintEqualToAnchor:_headerBar.leadingAnchor
                                              constant:kCloseButtonMargin],

    [closeButton.centerYAnchor
        constraintEqualToAnchor:_headerBar.centerYAnchor],
    [closeButton.trailingAnchor
        constraintEqualToAnchor:_headerBar.trailingAnchor
                       constant:-kCloseButtonMargin],
  ]];
}

- (UIButton*)createCloseButton {
  ExtendedTouchTargetButton* closeButton =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  closeButton.accessibilityIdentifier =
      kAtMemoryCloseButtonAccessibilityIdentifier;
  closeButton.accessibilityLabel = l10n_util::GetNSString(IDS_CLOSE);
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;

  UIImageSymbolConfiguration* symbolConfiguration =
      autofill::GetCloseButtonSymbolConfiguration();
  UIImage* buttonImage =
      SymbolWithPalette(DefaultSymbolWithConfiguration(kXMarkCircleFillSymbol,
                                                       symbolConfiguration),
                        @[
                          autofill::GetCloseButtonForegroundColor(),
                          [UIColor tertiarySystemFillColor]
                        ]);
  [closeButton setImage:buttonImage forState:UIControlStateNormal];

  [closeButton addTarget:self
                  action:@selector(didTapClose)
        forControlEvents:UIControlEventTouchUpInside];
  return closeButton;
}

- (UIButton*)createBackButton {
  ExtendedTouchTargetButton* backButton =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  backButton.accessibilityIdentifier =
      kAtMemoryBackButtonAccessibilityIdentifier;
  backButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_ICON_ARROW_BACK);
  backButton.translatesAutoresizingMaskIntoConstraints = NO;

  UIImageSymbolConfiguration* symbolConfiguration =
      autofill::GetCloseButtonSymbolConfiguration();
  UIImage* buttonImage = SymbolWithPalette(
      DefaultSymbolWithConfiguration(@"chevron.backward.circle.fill",
                                     symbolConfiguration),
      @[
        autofill::GetCloseButtonForegroundColor(),
        [UIColor tertiarySystemFillColor]
      ]);
  [backButton setImage:buttonImage forState:UIControlStateNormal];

  [backButton addTarget:self
                 action:@selector(didTapBack)
       forControlEvents:UIControlEventTouchUpInside];
  return backButton;
}

- (void)setupSearchBar {
  _searchBar = [[UISearchBar alloc] init];
  _searchBar.backgroundImage = [[UIImage alloc] init];
  _searchBar.searchTextField.accessibilityIdentifier =
      kAtMemorySearchBarAccessibilityIdentifier;
  _searchBar.delegate = self;
  _searchBar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_searchBar];
}

- (void)setupLayoutConstraints {
  _containerBottomToSearchBarConstraint =
      [_containerView.bottomAnchor constraintEqualToAnchor:_searchBar.topAnchor
                                                  constant:-kSearchFieldMargin];
  _containerBottomToSafeAreaConstraint = [_containerView.bottomAnchor
      constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor];

  [NSLayoutConstraint activateConstraints:@[
    // Header constraints
    [_headerBar.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [_headerBar.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [_headerBar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_headerBar.heightAnchor constraintEqualToConstant:kHeaderBarHeight],

    // Container View constraints
    [_containerView.topAnchor constraintEqualToAnchor:_headerBar.bottomAnchor],
    [_containerView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_containerView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    _containerBottomToSearchBarConstraint,

    // Search Bar constraints
    [_searchBar.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor
                                             constant:kSearchFieldMargin],
    [_searchBar.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor
                                              constant:-kSearchFieldMargin],
    [_searchBar.bottomAnchor
        constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor
                       constant:-kSearchFieldMargin],
  ]];
}

- (void)didTapClose {
  [self.atMemoryHandler dismissAtMemory];
}

- (void)didTapBack {
  [self setViewState:autofill::AtMemoryViewState::kRecentFills];
}

#pragma mark - AtMemoryRecentFillsViewControllerDelegate

- (void)recentFillsViewController:
            (AtMemoryRecentFillsViewController*)viewController
                didTapInfoForItem:(AtMemorySearchItem*)item {
  _granularFillTitle = [item.itemType copy];
  [self setViewState:autofill::AtMemoryViewState::kGranularFill];
}

- (void)recentFillsViewController:
            (AtMemoryRecentFillsViewController*)viewController
                 didSelectContent:(NSString*)content {
  [self.delegate atMemoryViewController:self didSelectContent:content];
}

#pragma mark - AtMemoryGranularFillViewControllerDelegate

- (void)granularFillViewController:
            (AtMemoryGranularFillViewController*)viewController
                  didSelectContent:(NSString*)content {
  [self.delegate atMemoryViewController:self didSelectContent:content];
}

#pragma mark - UISearchBarDelegate

- (void)searchBar:(UISearchBar*)searchBar textDidChange:(NSString*)searchText {
  [self.delegate atMemoryViewController:self didChangeSearchText:searchText];
}

#pragma mark - AtMemoryConsumer

- (void)setViewState:(autofill::AtMemoryViewState)viewState {
  _viewState = viewState;
  if (!self.isViewLoaded) {
    return;
  }
  [self applyViewState];
}

- (void)setRecentFills:(NSArray<AtMemorySearchItem*>*)recentFills {
  _recentFills = [recentFills copy];
  if (_recentFillsViewController) {
    _recentFillsViewController.items = _recentFills;
  }
}

- (void)setGranularFillItems:(NSArray<AtMemoryGranularFillItem*>*)items {
  _granularFillItems = [items copy];
  if (_granularFillViewController) {
    _granularFillViewController.items = _granularFillItems;
  }
}

- (void)setSearchQuery:(NSString*)query {
  _searchQuery = [query copy];
  if (_searchViewController) {
    _searchViewController.query = _searchQuery;
  }
}

- (void)setSearchLoading:(BOOL)loading {
  _searchLoading = loading;
  if (_searchViewController) {
    _searchViewController.loading = _searchLoading;
  }
}

- (void)setSearchResults:(NSArray<AtMemorySearchResultItem*>*)results {
  _searchResults = [results copy];
  if (_searchResultsViewController) {
    _searchResultsViewController.results = _searchResults;
  }
}

- (void)applyViewState {
  switch (_viewState) {
    case autofill::AtMemoryViewState::kEmpty:
      [self configureUIWithBackButtonHidden:YES
                            searchBarHidden:NO
                   safeAreaConstraintActive:NO
                  searchBarConstraintActive:YES];
      if (!_emptyStateViewController) {
        _emptyStateViewController =
            [[AtMemoryEmptyStateViewController alloc] init];
      }
      [self setChildViewController:_emptyStateViewController];
      break;
    case autofill::AtMemoryViewState::kRecentFills:
      [self configureUIWithBackButtonHidden:YES
                            searchBarHidden:NO
                   safeAreaConstraintActive:NO
                  searchBarConstraintActive:YES];
      if (!_recentFillsViewController) {
        _recentFillsViewController = [[AtMemoryRecentFillsViewController alloc]
            initWithStyle:ChromeTableViewStyle()];
        _recentFillsViewController.delegate = self;
      }
      _recentFillsViewController.items = _recentFills;
      [self setChildViewController:_recentFillsViewController];
      break;
    case autofill::AtMemoryViewState::kGranularFill:
      [self configureUIWithBackButtonHidden:NO
                            searchBarHidden:YES
                   safeAreaConstraintActive:YES
                  searchBarConstraintActive:NO];
      if (!_granularFillViewController) {
        _granularFillViewController =
            [[AtMemoryGranularFillViewController alloc]
                initWithStyle:ChromeTableViewStyle()];
        _granularFillViewController.delegate = self;
      }
      _granularFillViewController.title = _granularFillTitle;
      _granularFillViewController.items = _granularFillItems;
      [self setChildViewController:_granularFillViewController];
      break;
    case autofill::AtMemoryViewState::kSearch:
      [self configureUIWithBackButtonHidden:YES
                            searchBarHidden:NO
                   safeAreaConstraintActive:NO
                  searchBarConstraintActive:YES];
      if (!_searchViewController) {
        _searchViewController = [[AtMemorySearchViewController alloc]
            initWithStyle:ChromeTableViewStyle()];
        _searchViewController.delegate = self;
      }
      _searchViewController.query = _searchQuery;
      _searchViewController.loading = _searchLoading;
      [self setChildViewController:_searchViewController];
      break;
    case autofill::AtMemoryViewState::kSearchResults:
      [self configureUIWithBackButtonHidden:YES
                            searchBarHidden:NO
                   safeAreaConstraintActive:NO
                  searchBarConstraintActive:YES];
      if (!_searchResultsViewController) {
        _searchResultsViewController =
            [[AtMemorySearchResultsViewController alloc]
                initWithStyle:ChromeTableViewStyle()];
        _searchResultsViewController.delegate = self;
      }
      _searchResultsViewController.results = _searchResults;
      [self setChildViewController:_searchResultsViewController];
      break;
    case autofill::AtMemoryViewState::kNoData:
      [self configureUIWithBackButtonHidden:YES
                            searchBarHidden:NO
                   safeAreaConstraintActive:NO
                  searchBarConstraintActive:YES];
      if (!_noDataViewController) {
        _noDataViewController = [[AtMemoryNoDataViewController alloc]
            initWithStyle:ChromeTableViewStyle()];
      }
      [self setChildViewController:_noDataViewController];
      break;
    case autofill::AtMemoryViewState::kQueryUnsupported:
      [self configureUIWithBackButtonHidden:YES
                            searchBarHidden:NO
                   safeAreaConstraintActive:NO
                  searchBarConstraintActive:YES];
      if (!_queryUnsupportedViewController) {
        _queryUnsupportedViewController =
            [[AtMemoryQueryUnsupportedViewController alloc]
                initWithStyle:ChromeTableViewStyle()];
        _queryUnsupportedViewController.delegate = self;
      }
      [self setChildViewController:_queryUnsupportedViewController];
      break;
    default:
      // TODO(crbug.com/522326512): Handle other states.
      [self setChildViewController:nil];
      break;
  }
}

#pragma mark - Private

// Configures general UI visibility and layout constraints for the active state.
- (void)configureUIWithBackButtonHidden:(BOOL)backButtonHidden
                        searchBarHidden:(BOOL)searchBarHidden
               safeAreaConstraintActive:(BOOL)safeAreaConstraintActive
              searchBarConstraintActive:(BOOL)searchBarConstraintActive {
  _backButton.hidden = backButtonHidden;
  _searchBar.hidden = searchBarHidden;
  _containerBottomToSafeAreaConstraint.active = safeAreaConstraintActive;
  _containerBottomToSearchBarConstraint.active = searchBarConstraintActive;
}

#pragma mark - AtMemorySearchViewControllerDelegate

- (void)searchViewControllerDidTapSearch:
    (AtMemorySearchViewController*)viewController {
  [self.delegate atMemoryViewControllerDidTapSearch:self];
}

- (void)searchViewController:(AtMemorySearchViewController*)viewController
               didTapLinkURL:(CrURL*)URL {
  // TODO(crbug.com/532090671): Remove this check once it becomes required.
  if ([self.atMemoryHandler respondsToSelector:@selector(openURL:)]) {
    [self.atMemoryHandler openURL:URL];
  }
}

#pragma mark - AtMemorySearchResultsViewControllerDelegate

- (void)searchResultsViewControllerDidTapInfo:
    (AtMemorySearchResultsViewController*)viewController {
  [self.delegate atMemoryViewControllerDidTapSearchResultInfo:self];
}

- (void)searchResultsViewController:
            (AtMemorySearchResultsViewController*)viewController
                   didSelectContent:(NSString*)content {
  [self.delegate atMemoryViewController:self didSelectContent:content];
}

#pragma mark - AtMemoryQueryUnsupportedViewControllerDelegate

- (void)queryUnsupportedViewControllerDidTapCell:
    (AtMemoryQueryUnsupportedViewController*)viewController {
  if ([self.atMemoryHandler respondsToSelector:@selector(openURL:)]) {
    [self.atMemoryHandler
        openURL:[[CrURL alloc] initWithGURL:autofill::GetManageYourInfoURL()]];
  }
}

@end
