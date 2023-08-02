// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_top_toolbar.h"

#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/location.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_page_control.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_utils.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// The space after the new tab toolbar button item. Calculated to have
// approximately 33 pts between the plus button and the done button.
const int kIconButtonAdditionalSpace = 20;
const int kSelectionModeButtonSize = 17;
const int kSearchBarTrailingSpace = 24;

// The size of top toolbar search symbol image.
const CGFloat kSymbolSearchImagePointSize = 22;

}  // namespace

@interface TabGridTopToolbar () <UIToolbarDelegate>
@end

@implementation TabGridTopToolbar {
  UIBarButtonItem* _leadingButton;
  UIBarButtonItem* _spaceItem;
  UIBarButtonItem* _iconButtonAdditionalSpaceItem;
  UIBarButtonItem* _selectionModeFixedSpace;
  UIBarButtonItem* _selectAllButton;
  UIBarButtonItem* _selectedTabsItem;
  UIBarButtonItem* _searchButton;
  UIBarButtonItem* _doneButton;
  UIBarButtonItem* _closeAllOrUndoButton;
  UIBarButtonItem* _editButton;
  UIBarButtonItem* _pageControlItem;
  // Search mode
  UISearchBar* _searchBar;
  UIBarButtonItem* _searchBarItem;
  UIBarButtonItem* _cancelSearchButton;
  UIView* _searchBarView;

  BOOL _undoActive;

  BOOL _scrolledToEdge;
  UIView* _scrolledToTopBackgroundView;
  UIView* _scrolledBackgroundView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setupViews];
  }
  return self;
}

- (UIBarButtonItem*)anchorItem {
  return _leadingButton;
}

- (void)setPage:(TabGridPage)page {
  if (_page == page) {
    return;
  }
  _page = page;
  [self setItemsForTraitCollection:self.traitCollection];
}

- (void)setMode:(TabGridMode)mode {
  if (_mode == mode) {
    return;
  }
  // Reset search state when exiting search mode.
  if (_mode == TabGridModeSearch) {
    _searchBar.text = @"";
    [_searchBar resignFirstResponder];
  }
  _mode = mode;
  // Reset selected tabs count when mode changes.
  self.selectedTabsCount = 0;
  // Reset the Select All button to its default title.
  [self configureSelectAllButtonTitle];
  [self setItemsForTraitCollection:self.traitCollection];
  if (mode == TabGridModeSearch) {
    // Focus the search bar, and make it a first responder once the user enter
    // to search mode. Doing that here instead in `setItemsForTraitCollection`
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
    _selectedTabsItem.title =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_SELECT_TABS_TITLE);
  } else {
    _selectedTabsItem.title = l10n_util::GetPluralNSStringF(
        IDS_IOS_TAB_GRID_SELECTED_TABS_TITLE, _selectedTabsCount);
  }
}

- (void)setSelectAllButtonTarget:(id)target action:(SEL)action {
  _selectAllButton.target = target;
  _selectAllButton.action = action;
}

- (void)setSearchButtonTarget:(id)target action:(SEL)action {
  _searchButton.target = target;
  _searchButton.action = action;
}

- (void)setSearchBarDelegate:(id<UISearchBarDelegate>)delegate {
  _searchBar.delegate = delegate;
}

- (void)setDoneButtonTarget:(id)target action:(SEL)action {
  _doneButton.target = target;
  _doneButton.action = action;
}

- (void)setCancelSearchButtonTarget:(id)target action:(SEL)action {
  _cancelSearchButton.target = target;
  _cancelSearchButton.action = action;
}

- (void)setSearchButtonEnabled:(BOOL)enabled {
  _searchButton.enabled = enabled;
}

- (void)setCloseAllButtonTarget:(id)target action:(SEL)action {
  _closeAllOrUndoButton.target = target;
  _closeAllOrUndoButton.action = action;
}

- (void)setCloseAllButtonEnabled:(BOOL)enabled {
  _closeAllOrUndoButton.enabled = enabled;
}

- (void)setSelectAllButtonEnabled:(BOOL)enabled {
  _selectAllButton.enabled = enabled;
}

- (void)setDoneButtonEnabled:(BOOL)enabled {
  _doneButton.enabled = enabled;
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
    [self setItemsForTraitCollection:self.traitCollection];
  }
}

- (void)configureDeselectAllButtonTitle {
  _selectAllButton.title =
      l10n_util::GetNSString(IDS_IOS_TAB_GRID_DESELECT_ALL_BUTTON);
}

- (void)configureSelectAllButtonTitle {
  _selectAllButton.title =
      l10n_util::GetNSString(IDS_IOS_TAB_GRID_SELECT_ALL_BUTTON);
}

- (void)hide {
  self.backgroundColor = UIColor.blackColor;
  self.pageControl.alpha = 0.0;
}

- (void)show {
  self.backgroundColor = UIColor.clearColor;
  self.pageControl.alpha = 1.0;
}

- (void)setScrollViewScrolledToEdge:(BOOL)scrolledToEdge {
  if (scrolledToEdge == _scrolledToEdge) {
    return;
  }

  _scrolledToEdge = scrolledToEdge;

  _scrolledToTopBackgroundView.hidden = !scrolledToEdge;
  _scrolledBackgroundView.hidden = scrolledToEdge;
  [_pageControl setScrollViewScrolledToEdge:scrolledToEdge];
}

#pragma mark Edit Button

- (void)setEditButtonMenu:(UIMenu*)menu {
  _editButton.menu = menu;
}

- (void)setEditButtonEnabled:(BOOL)enabled {
  _editButton.enabled = enabled;
}

#pragma mark - UIView

- (CGSize)intrinsicContentSize {
  return CGSizeMake(UIViewNoIntrinsicMetric, kTabGridTopToolbarHeight);
}

- (void)didMoveToSuperview {
  if (_scrolledBackgroundView) {
    [self.superview.topAnchor
        constraintEqualToAnchor:_scrolledBackgroundView.topAnchor]
        .active = YES;
  }
  [super didMoveToSuperview];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self setItemsForTraitCollection:self.traitCollection];
}

#pragma mark - UIBarPositioningDelegate

// Returns UIBarPositionTopAttached, otherwise the toolbar's translucent
// background won't extend below the status bar.
- (UIBarPosition)positionForBar:(id<UIBarPositioning>)bar {
  return UIBarPositionTopAttached;
}

#pragma mark - Private

- (void)configureSearchModeForTraitCollection:
    (UITraitCollection*)traitCollection {
  DCHECK_EQ(_mode, TabGridModeSearch);
  CGFloat widthModifier = 1;

  // In the landscape mode the search bar size should only span half of the
  // width of the toolbar.
  if (![self shouldUseCompactLayout:traitCollection]) {
    widthModifier = kTabGridSearchBarNonCompactWidthRatioModifier;
  }

  CGFloat cancelWidth = [_cancelSearchButton.title sizeWithAttributes:@{
                          NSFontAttributeName : [UIFont
                              preferredFontForTextStyle:UIFontTextStyleBody]
                        }]
                            .width;
  CGFloat barWidth =
      (self.bounds.size.width - kSearchBarTrailingSpace - cancelWidth) *
      kTabGridSearchBarWidthRatio * widthModifier;
  // Update the search bar size based on the container size.
  _searchBar.frame = CGRectMake(0, 0, barWidth, kTabGridSearchBarHeight);
  _searchBarView.frame = CGRectMake(0, 0, barWidth, kTabGridSearchBarHeight);
  [self setNeedsLayout];
  [self setItems:@[ _searchBarItem, _spaceItem, _cancelSearchButton ]
        animated:YES];
}

- (void)setItemsForTraitCollection:(UITraitCollection*)traitCollection {
  if (_mode == TabGridModeSearch) {
    [self configureSearchModeForTraitCollection:traitCollection];
    return;
  }
  UIBarButtonItem* centralItem = _pageControlItem;
  UIBarButtonItem* trailingButton = _doneButton;
  _selectionModeFixedSpace.width = 0;
  if ([self shouldUseCompactLayout:traitCollection]) {
    if (_mode == TabGridModeNormal) {
      _leadingButton = _searchButton;
    } else {
      _leadingButton = _spaceItem;
    }

    if (_mode == TabGridModeSelection) {
      // In the selection mode, Done button is much smaller than SelectAll
      // we need to calculate the difference on the width and use it as a
      // fixed space to make sure that the title is still centered.
      _selectionModeFixedSpace.width = [self selectionModeFixedSpaceWidth];
      [self setItems:@[
        _selectAllButton, _spaceItem, _selectedTabsItem, _spaceItem,
        _selectionModeFixedSpace, trailingButton
      ]];
    } else {
      trailingButton = _spaceItem;
      [self setItems:@[
        _leadingButton, _spaceItem, centralItem, _spaceItem, trailingButton
      ]];
    }
    return;
  }
  // In Landscape normal mode leading button is always "closeAll", or "Edit" if
  // bulk actions feature is enabled.
  if (!_undoActive) {
    _leadingButton = _editButton;
  } else {
    _leadingButton = _closeAllOrUndoButton;
  }

  if (_mode == TabGridModeSelection) {
    // In the selection mode, Done button is much smaller than SelectAll
    // we need to calculate the difference on the width and use it as a
    // fixed space to make sure that the title is still centered.
    _selectionModeFixedSpace.width = [self selectionModeFixedSpaceWidth];
    centralItem = _selectedTabsItem;
    _leadingButton = _selectAllButton;
  }

  // Build item list based on priority: tab search takes precedence over thumb
  // strip.

  BOOL animated = NO;
  NSMutableArray* items = [[NSMutableArray alloc] init];

  [items addObject:_leadingButton];

  if (_mode == TabGridModeNormal) {
    animated = YES;
    [items
        addObjectsFromArray:@[ _iconButtonAdditionalSpaceItem, _searchButton ]];
  }

  [items addObjectsFromArray:@[ _spaceItem, centralItem, _spaceItem ]];

  if (_mode != TabGridModeNormal) {
    [items addObject:_selectionModeFixedSpace];
  }

  [items addObject:trailingButton];

  [self setItems:items animated:animated];
}

// Calculates the space width to use for selection mode.
- (CGFloat)selectionModeFixedSpaceWidth {
  NSDictionary* selectAllFontAttrs = @{
    NSFontAttributeName : [UIFont systemFontOfSize:kSelectionModeButtonSize]
  };
  CGFloat selectAllTextWidth =
      [_selectAllButton.title sizeWithAttributes:selectAllFontAttrs].width;
  NSDictionary* DonefontAttr = @{
    NSFontAttributeName : [UIFont systemFontOfSize:kSelectionModeButtonSize
                                            weight:UIFontWeightSemibold]
  };
  return selectAllTextWidth -
         [_doneButton.title sizeWithAttributes:DonefontAttr].width;
}

- (void)setupViews {
  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  [self createScrolledBackgrounds];
  self.delegate = self;
  [self setShadowImage:[[UIImage alloc] init]
      forToolbarPosition:UIBarPositionAny];

  _closeAllOrUndoButton = [[UIBarButtonItem alloc] init];
  _closeAllOrUndoButton.tintColor =
      UIColorFromRGB(kTabGridToolbarTextButtonColor);
  [self useUndoCloseAll:NO];

  // The segmented control has an intrinsic size.
  _pageControl = [[TabGridPageControl alloc] init];
  _pageControl.translatesAutoresizingMaskIntoConstraints = NO;
  [_pageControl setScrollViewScrolledToEdge:_scrolledToEdge];
  _pageControlItem = [[UIBarButtonItem alloc] initWithCustomView:_pageControl];

  _doneButton = [[UIBarButtonItem alloc] init];
  _doneButton.style = UIBarButtonItemStyleDone;
  _doneButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
  _doneButton.accessibilityIdentifier = kTabGridDoneButtonIdentifier;
  _doneButton.title = l10n_util::GetNSString(IDS_IOS_TAB_GRID_DONE_BUTTON);

  _editButton = [[UIBarButtonItem alloc] init];
  _editButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
  _editButton.title = l10n_util::GetNSString(IDS_IOS_TAB_GRID_EDIT_BUTTON);
  _editButton.accessibilityIdentifier = kTabGridEditButtonIdentifier;

  _selectAllButton = [[UIBarButtonItem alloc] init];
  _selectAllButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
  _selectAllButton.title =
      l10n_util::GetNSString(IDS_IOS_TAB_GRID_SELECT_ALL_BUTTON);
  _selectAllButton.accessibilityIdentifier =
      kTabGridEditSelectAllButtonIdentifier;

  _selectedTabsItem = [[UIBarButtonItem alloc] init];
  _selectedTabsItem.title =
      l10n_util::GetNSString(IDS_IOS_TAB_GRID_SELECT_TABS_TITLE);
  _selectedTabsItem.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
  _selectedTabsItem.action = nil;
  _selectedTabsItem.target = nil;
  _selectedTabsItem.enabled = NO;
  [_selectedTabsItem setTitleTextAttributes:@{
    NSForegroundColorAttributeName :
        UIColorFromRGB(kTabGridToolbarTextButtonColor),
    NSFontAttributeName :
        [[UIFontMetrics metricsForTextStyle:UIFontTextStyleBody]
            scaledFontForFont:[UIFont systemFontOfSize:kSelectionModeButtonSize
                                                weight:UIFontWeightSemibold]]
  }
                                   forState:UIControlStateDisabled];

  UIImage* searchImage =
      DefaultSymbolWithPointSize(kSearchSymbol, kSymbolSearchImagePointSize);
  _searchButton =
      [[UIBarButtonItem alloc] initWithImage:searchImage
                                       style:UIBarButtonItemStylePlain
                                      target:nil
                                      action:nil];

  _searchButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);
  _searchButton.accessibilityIdentifier = kTabGridSearchButtonIdentifier;

  _searchBar = [[UISearchBar alloc] init];
  _searchBar.placeholder =
      l10n_util::GetNSString(IDS_IOS_TAB_GRID_SEARCHBAR_PLACEHOLDER);
  _searchBar.accessibilityIdentifier = kTabGridSearchBarIdentifier;
  // Cancel Button for the searchbar doesn't appear in ipadOS. Disable it and
  // create a custom cancel button.
  _searchBar.showsCancelButton = NO;
  _cancelSearchButton = [[UIBarButtonItem alloc] init];
  _cancelSearchButton.style = UIBarButtonItemStylePlain;
  _cancelSearchButton.tintColor =
      UIColorFromRGB(kTabGridToolbarTextButtonColor);
  _cancelSearchButton.accessibilityIdentifier = kTabGridCancelButtonIdentifier;
  _cancelSearchButton.title =
      l10n_util::GetNSString(IDS_IOS_TAB_GRID_CANCEL_BUTTON);
  _searchBarView = [[UIView alloc] initWithFrame:_searchBar.frame];
  [_searchBarView addSubview:_searchBar];
  [_searchBarView sizeToFit];
  _searchBarItem = [[UIBarButtonItem alloc] initWithCustomView:_searchBarView];

  _iconButtonAdditionalSpaceItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFixedSpace
                           target:nil
                           action:nil];
  _iconButtonAdditionalSpaceItem.width = kIconButtonAdditionalSpace;

  _spaceItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];

  _selectionModeFixedSpace = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFixedSpace
                           target:nil
                           action:nil];

  [self setItemsForTraitCollection:self.traitCollection];
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
      LayoutSides::kLeading | LayoutSides::kBottom | LayoutSides::kTrailing);

  // Background when the content is scrolled to the top.
  _scrolledToTopBackgroundView = CreateTabGridScrolledToEdgeBackground();
  _scrolledToTopBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_scrolledToTopBackgroundView];
  AddSameConstraints(_scrolledBackgroundView, _scrolledToTopBackgroundView);

  // A non-nil UIImage has to be added in the background of the toolbar to avoid
  // having an additional blur effect.
  [self setBackgroundImage:[UIImage new]
        forToolbarPosition:UIBarPositionAny
                barMetrics:UIBarMetricsDefault];
}

// Returns YES if should use compact bottom toolbar layout.
- (BOOL)shouldUseCompactLayout:(UITraitCollection*)traitCollection {
  return traitCollection.verticalSizeClass == UIUserInterfaceSizeClassRegular &&
         traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact;
}

@end
