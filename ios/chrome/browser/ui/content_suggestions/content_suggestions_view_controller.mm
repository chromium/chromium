// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/time/time.h"
#import "components/segmentation_platform/public/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/drag_and_drop/url_drag_drop_handler.h"
#import "ios/chrome/browser/ntp/set_up_list_item.h"
#import "ios/chrome/browser/ntp/set_up_list_item_type.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_return_to_recent_tab_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_selection_actions.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_shortcut_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_layout_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/magic_stack_module_container.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/magic_stack_module_container_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/multi_row_container_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/parcel_tracking_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/parcel_tracking_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/query_suggestion_view.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_menu_provider.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_state.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_view.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/types.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/utils.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view_data.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_view.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/utils.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_view.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_view_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "third_party/abseil-cpp/absl/types/optional.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// The bottom padding for the vertical stack view.
const float kBottomStackViewPadding = 6.0f;
const float kBottomStackViewExtraPadding = 14.0f;

// The minimum scroll velocity in order to swipe between modules in the Magic
// Stack.
const float kMagicStackMinimumPaginationScrollVelocity = 0.2f;

// The spacing between modules in the Magic Stack.
const float kMagicStackSpacing = 10.0f;

// The corner radius of the Magic Stack.
const float kMagicStackCornerRadius = 16.0f;

// The max width of the SetUpList on phone and tablet.
const CGFloat kSetUpListWidthRegular = 393;
const CGFloat kSetUpListWidthWide = 418;

// The distance in which a replaced/replacing module will fade out/in of view.
const float kMagicStackReplaceModuleFadeAnimationDistance = 50;

// The size configs of the Magic Stack edit button.
const float kMagicStackEditButtonWidth = 61;
const float kMagicStackEditButtonIconPointSize = 22;

// Margin spacing between Magic Stack Edit button and horizontal neighboring
// views.
const float kMagicStackEditButtonMargin = 32;

// The duration of the animation that hides the Set Up List.
const base::TimeDelta kSetUpListHideAnimationDuration = base::Milliseconds(250);

}  // namespace

@interface ContentSuggestionsViewController () <
    UIGestureRecognizerDelegate,
    ContentSuggestionsSelectionActions,
    MagicStackModuleContainerDelegate,
    ParcelTrackingViewDelegate,
    SetUpListItemViewTapDelegate,
    TabResumptionViewDelegate,
    URLDropDelegate,
    UIScrollViewDelegate,
    UIScrollViewAccessibilityDelegate>

@property(nonatomic, strong) URLDragDropHandler* dragDropHandler;

// StackView holding all subviews.
@property(nonatomic, strong) UIStackView* verticalStackView;

// List of all UITapGestureRecognizers created for the Most Visisted tiles.
@property(nonatomic, strong)
    NSMutableArray<UITapGestureRecognizer*>* mostVisitedTapRecognizers;
// The UITapGestureRecognizer for the Return To Recent Tab tile.
@property(nonatomic, strong)
    UITapGestureRecognizer* returnToRecentTabTapRecognizer;

// The Return To Recent Tab view.
@property(nonatomic, strong)
    ContentSuggestionsReturnToRecentTabView* returnToRecentTabTile;
// StackView holding all of `mostVisitedViews`.
@property(nonatomic, strong) UIStackView* mostVisitedStackView;
// Module Container for the `mostVisitedViews` when being shown in Magic Stack.
@property(nonatomic, strong)
    MagicStackModuleContainer* mostVisitedModuleContainer;
// Module Container for the tab resumption tile.
@property(nonatomic, strong)
    MagicStackModuleContainer* tabResumptionModuleContainer;
// Width Anchor of the Most Visited Tiles container.
@property(nonatomic, strong)
    NSLayoutConstraint* mostVisitedContainerWidthAnchor;
// List of all of the Most Visited views.
@property(nonatomic, strong)
    NSMutableArray<ContentSuggestionsMostVisitedTileView*>* mostVisitedViews;
// Module Container for the Shortcuts when being shown in Magic Stack.
@property(nonatomic, strong)
    MagicStackModuleContainer* shortcutsModuleContainer;
// StackView holding all of `shortcutsViews`.
@property(nonatomic, strong) UIStackView* shortcutsStackView;
// List of all of the Shortcut views.
@property(nonatomic, strong)
    NSMutableArray<ContentSuggestionsShortcutTileView*>* shortcutsViews;
// The SetUpListView, if it is currently being displayed.
@property(nonatomic, strong) SetUpListView* setUpListView;
// The current state of the Safety Check.
@property(nonatomic, strong) SafetyCheckState* safetyCheckState;
// The SafetyCheckView, if it is currently being displayed.
@property(nonatomic, strong) SafetyCheckView* safetyCheckView;
// Module Container for the `safetyCheckView` when being shown in Magic Stack.
@property(nonatomic, strong)
    MagicStackModuleContainer* safetyCheckModuleContainer;

@end

@implementation ContentSuggestionsViewController {
  // Width Anchor of the Return To Recent Tab tile.
  NSLayoutConstraint* _returnToRecentTabWidthAnchor;
  UIScrollView* _magicStackScrollView;
  UIStackView* _magicStack;
  BOOL _magicStackRankReceived;
  NSMutableArray<NSNumber*>* _magicStackModuleOrder;
  NSLayoutConstraint* _magicStackScrollViewWidthAnchor;
  NSArray<SetUpListItemViewData*>* _savedSetUpListItems;
  SetUpListItemView* _setUpListSyncItemView;
  SetUpListItemView* _setUpListDefaultBrowserItemView;
  SetUpListItemView* _setUpListAutofillItemView;
  SetUpListItemView* _setUpListAllSetItemView;
  NSMutableArray<SetUpListItemView*>* _compactedSetUpListViews;
  TabResumptionView* _tabResumptionView;
  NSMutableArray<MagicStackModuleContainer*>* _parcelTrackingModuleContainers;
}

- (instancetype)init {
  return [super initWithNibName:nil bundle:nil];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.dragDropHandler = [[URLDragDropHandler alloc] init];
  self.dragDropHandler.dropDelegate = self;
  [self.view addInteraction:[[UIDropInteraction alloc]
                                initWithDelegate:self.dragDropHandler]];
  if (IsMagicStackEnabled()) {
    self.view.backgroundColor = [UIColor clearColor];
  } else {
    self.view.backgroundColor = ntp_home::NTPBackgroundColor();
  }
  self.view.accessibilityIdentifier = kContentSuggestionsCollectionIdentifier;

  self.verticalStackView = [[UIStackView alloc] init];
  self.verticalStackView.translatesAutoresizingMaskIntoConstraints = NO;
  self.verticalStackView.axis = UILayoutConstraintAxisVertical;
  // A centered alignment will ensure the views are centered.
  self.verticalStackView.alignment = UIStackViewAlignmentCenter;
  // A fill distribution allows for the custom spacing between elements and
  // height/width configurations for each row.
  self.verticalStackView.distribution = UIStackViewDistributionFill;
  [self.view addSubview:self.verticalStackView];

  // Add bottom spacing to the last module by applying it after
  // `_verticalStackView`. If `IsContentSuggestionsUIModuleRefreshEnabled()` is
  // YES, and ShouldMinimizeSpacingForModuleRefresh() is YES, then no space is
  // added after the last module. Otherwise we add kModuleVerticalSpacing. If
  // `IsContentSuggestionsUIModuleRefreshEnabled()` is NO, then we add
  // `kBottomStackViewPadding`
  CGFloat bottomSpacing = kBottomStackViewPadding;
  if (IsMagicStackEnabled()) {
    // Add more spacing between magic stack and feed header.
    bottomSpacing = kBottomStackViewExtraPadding;
  }
  [NSLayoutConstraint activateConstraints:@[
    [self.verticalStackView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.verticalStackView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.verticalStackView.topAnchor
        constraintEqualToAnchor:self.view.topAnchor
                       constant:content_suggestions::HeaderBottomPadding()],
    [self.verticalStackView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor
                       constant:-bottomSpacing]
  ]];

  if (self.returnToRecentTabTile) {
    [self addUIElement:self.returnToRecentTabTile
        withCustomBottomSpacing:
            IsMagicStackEnabled()
                ? kMostVisitedBottomMargin
                : content_suggestions::kReturnToRecentTabSectionBottomMargin];
    [self layoutReturnToRecentTabTile];
  }
  if ([self.mostVisitedViews count] > 0) {
    [self createAndInsertMostVisitedModule];
    [self populateMostVisitedModule];
  }
  if (_savedSetUpListItems) {
    [self showSetUpListWithItems:_savedSetUpListItems];
  }
  if (self.shortcutsViews) {
    if (!IsMagicStackEnabled()) {
      [self addUIElement:self.shortcutsStackView
          withCustomBottomSpacing:kMostVisitedBottomMargin];
      CGFloat width =
          MostVisitedTilesContentHorizontalSpace(self.traitCollection);
      CGFloat height =
          MostVisitedCellSize(self.traitCollection.preferredContentSizeCategory)
              .height;
      [NSLayoutConstraint activateConstraints:@[
        [self.shortcutsStackView.widthAnchor constraintEqualToConstant:width],
        [self.shortcutsStackView.heightAnchor
            constraintGreaterThanOrEqualToConstant:height]
      ]];
    }
  }

  if (IsSafetyCheckMagicStackEnabled() && self.safetyCheckState) {
    [self createSafetyCheck:self.safetyCheckState];
  }

  // Only Create Magic Stack if the ranking has been received. It can be delayed
  // to after -viewDidLoad if fecthing from Segmentation Platform.
  if (IsMagicStackEnabled()) {
    [self createMagicStack];
    if (_magicStackRankReceived) {
      [self populateMagicStack];
    } else if (base::FeatureList::IsEnabled(
                   segmentation_platform::features::
                       kSegmentationPlatformIosModuleRanker)) {
      // If Magic Stack rank has not been received from Segmentation, add
      // placeholders
      [self populateMagicStackWithPlaceholders];
    }
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [self.audience viewWillDisappear];
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  return touch.view.accessibilityIdentifier !=
             ntp_home::FakeOmniboxAccessibilityID() &&
         touch.view.superview.accessibilityIdentifier !=
             ntp_home::FakeOmniboxAccessibilityID();
}

#pragma mark - URLDropDelegate

- (BOOL)canHandleURLDropInView:(UIView*)view {
  return YES;
}

- (void)view:(UIView*)view didDropURL:(const GURL&)URL atPoint:(CGPoint)point {
  self.urlLoadingBrowserAgent->Load(UrlLoadParams::InCurrentTab(URL));
}

#pragma mark - ContentSuggestionsConsumer

- (void)showReturnToRecentTabTileWithConfig:
    (ContentSuggestionsReturnToRecentTabItem*)config {
  if (self.returnToRecentTabTile) {
    [self.returnToRecentTabTile removeFromSuperview];
  }

  self.returnToRecentTabTile = [[ContentSuggestionsReturnToRecentTabView alloc]
      initWithConfiguration:config];
  self.returnToRecentTabTapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(contentSuggestionsElementTapped:)];
  [self.returnToRecentTabTile
      addGestureRecognizer:self.returnToRecentTabTapRecognizer];
  self.returnToRecentTabTapRecognizer.enabled = YES;
  // If the Content Suggestions is already shown, add the Return to Recent Tab
  // tile to the StackView, otherwise, add to the verticalStackView.
  if (self.isViewLoaded) {
    [self.verticalStackView insertArrangedSubview:self.returnToRecentTabTile
                                          atIndex:0];
    [self.verticalStackView
        setCustomSpacing:IsMagicStackEnabled()
                             ? kMostVisitedBottomMargin
                             : content_suggestions::
                                   kReturnToRecentTabSectionBottomMargin
               afterView:self.returnToRecentTabTile];
    [self layoutReturnToRecentTabTile];
    [self.audience returnToRecentTabWasAdded];
  }
  // Trigger a relayout so that the Return To Recent Tab view will be counted in
  // the Content Suggestions height. Upon app startup when this is often added
  // asynchronously as the NTP is constructing the entire surface, so accurate
  // height info is very important to prevent pushing content below the Return
  // To Recent Tab view down as opposed to pushing the content above the view up
  // if it is not counted in the height.
  // This only has to happen after `-viewDidLoad` has completed since it is
  // adding views after the initial layout construction in `-viewDidLoad`.
  if (self.viewLoaded) {
    [self.view setNeedsLayout];
    [self.view layoutIfNeeded];
  }
}

- (void)updateReturnToRecentTabTileWithConfig:
    (ContentSuggestionsReturnToRecentTabItem*)config {
  if (config.icon) {
    self.returnToRecentTabTile.iconImageView.image = config.icon;
    self.returnToRecentTabTile.iconImageView.hidden = NO;
  }
  if (config.title) {
    self.returnToRecentTabTile.subtitleLabel.text = config.subtitle;
  }
}

- (void)hideReturnToRecentTabTile {
  [self.returnToRecentTabTile removeFromSuperview];
  self.returnToRecentTabTile = nil;
}

- (void)setMostVisitedTilesWithConfigs:
    (NSArray<ContentSuggestionsMostVisitedItem*>*)configs {
  if (!configs) {
    return;
  }
  if ([self.mostVisitedViews count]) {
    for (ContentSuggestionsMostVisitedTileView* view in self.mostVisitedViews) {
      [view removeFromSuperview];
    }
    [self.mostVisitedViews removeAllObjects];
    [self.mostVisitedTapRecognizers removeAllObjects];
  } else {
    self.mostVisitedViews = [NSMutableArray array];
  }

  if ([configs count] == 0) {
    // No Most Visited Tiles to show. Remove module.
    [self.mostVisitedStackView removeFromSuperview];
    return;
  }
  NSInteger index = 0;
  for (ContentSuggestionsMostVisitedItem* item in configs) {
    ContentSuggestionsMostVisitedTileView* view =
        [[ContentSuggestionsMostVisitedTileView alloc]
            initWithConfiguration:item];
    view.menuProvider = self.menuProvider;
    view.accessibilityIdentifier = [NSString
        stringWithFormat:
            @"%@%li",
            kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix, index];
    [self.mostVisitedViews addObject:view];
    index++;
  }
  // If viewDidLoad has been called before the first valid Most Visited Tiles
  // are available, construct `mostVisitedStackView`.
  if (self.verticalStackView && !self.mostVisitedStackView) {
    [self createAndInsertMostVisitedModule];
  }
  [self populateMostVisitedModule];
  [self.contentSuggestionsMetricsRecorder recordMostVisitedTilesShown];
  if (IsMagicStackEnabled() && [self hasMagicStackLoaded]) {
    [self logTopModuleImpressionForType:ContentSuggestionsModuleType::
                                            kMostVisited];
  }
  // Trigger a relayout so that the MVTs will be counted in the Content
  // Suggestions height. Upon app startup when this is often added
  // asynchronously as the NTP is constructing the entire surface, so accurate
  // height info is very important to simulate pushing content below the MVT
  // down as opposed to pushing the content above the MVT up if the MVTs are not
  // counted in the height.
  // This only has to happen after `-viewDidLoad` has completed since it is
  // adding views after the initial layout construction in `-viewDidLoad`.
  if (self.viewLoaded) {
    [self.view setNeedsLayout];
    [self.view layoutIfNeeded];
  }
}

- (void)setShortcutTilesWithConfigs:
    (NSArray<ContentSuggestionsMostVisitedActionItem*>*)configs {
  if (!self.shortcutsViews) {
    self.shortcutsViews = [NSMutableArray array];
  }
  // Assumes this only called before viewDidLoad, so there is no need to add the
  // views into the view hierarchy here.
  for (ContentSuggestionsMostVisitedActionItem* item in configs) {
    ContentSuggestionsShortcutTileView* view =
        [[ContentSuggestionsShortcutTileView alloc] initWithConfiguration:item];
    [self.shortcutsViews addObject:view];
  }
  self.shortcutsStackView = [self createShortcutsStackView];
  if (IsMagicStackEnabled()) {
    self.shortcutsModuleContainer = [[MagicStackModuleContainer alloc]
        initWithContentView:self.shortcutsStackView
                       type:ContentSuggestionsModuleType::kShortcuts
                   delegate:self];
    if ([self hasMagicStackLoaded]) {
      [self insertModuleIntoMagicStack:self.shortcutsModuleContainer];
      [self logTopModuleImpressionForType:ContentSuggestionsModuleType::
                                              kShortcuts];
    }
  }
}

- (void)updateShortcutTileConfig:
    (ContentSuggestionsMostVisitedActionItem*)config {
  for (ContentSuggestionsShortcutTileView* view in self.shortcutsViews) {
    if (view.config == config) {
      [view updateConfiguration:config];
      return;
    }
  }
}

- (void)updateMostVisitedTileConfig:(ContentSuggestionsMostVisitedItem*)config {
  for (ContentSuggestionsMostVisitedTileView* view in self.mostVisitedViews) {
    if (view.config == config) {
      dispatch_async(dispatch_get_main_queue(), ^{
        [view.faviconView configureWithAttributes:config.attributes];
      });
      return;
    }
  }
}

- (void)setMagicStackOrder:(NSArray<NSNumber*>*)order {
  CHECK([order count] > 0);
  _magicStackRankReceived = YES;
  _magicStackModuleOrder = [order mutableCopy];
  if (self.viewLoaded &&
      base::FeatureList::IsEnabled(segmentation_platform::features::
                                       kSegmentationPlatformIosModuleRanker)) {
    // Magic Stack order is only passed to the VC late when fetching it from the
    // Segmentation Platform
    [self populateMagicStack];
  }
}

- (void)updateMagicStackOrder:(MagicStackOrderChange)change {
  switch (change.type) {
    case MagicStackOrderChange::Type::kInsert:
      [_magicStackModuleOrder insertObject:@(int(change.new_module))
                                   atIndex:change.index];
      break;
    case MagicStackOrderChange::Type::kRemove: {
      ContentSuggestionsModuleType moduleType = (ContentSuggestionsModuleType)
          [_magicStackModuleOrder[change.index] intValue];
      CHECK(moduleType == change.old_module);
      [_magicStackModuleOrder removeObjectAtIndex:change.index];
      UIView* moduleToRemove = _magicStack.arrangedSubviews[change.index];
      [moduleToRemove removeFromSuperview];
      break;
    }
    case MagicStackOrderChange::Type::kReplace: {
      ContentSuggestionsModuleType moduleType = (ContentSuggestionsModuleType)
          [_magicStackModuleOrder[change.index] intValue];
      CHECK(moduleType == change.old_module);
      _magicStackModuleOrder[change.index] = @(int(change.new_module));
      break;
    }
  }
}

- (void)scrollToNextMagicStackModuleForCompletedModule:
    (ContentSuggestionsModuleType)moduleType {
  ContentSuggestionsModuleType currentModule = [self currentlyShownModule];
  // Do not scroll if the completed module is not the currently shown module.
  if (currentModule != moduleType) {
    return;
  }
  CGFloat nextPageContentOffsetX = [self
      getNextPageOffsetForOffset:_magicStackScrollView.contentOffset.x
                        velocity:kMagicStackMinimumPaginationScrollVelocity +
                                 1];
  [_magicStackScrollView
      setContentOffset:CGPointMake(nextPageContentOffsetX,
                                   _magicStackScrollView.contentOffset.y)
              animated:YES];
}

- (void)showSetUpListWithItems:(NSArray<SetUpListItemViewData*>*)items {
  if (!self.viewLoaded) {
    _savedSetUpListItems = items;
    return;
  }
  NSUInteger index = [self.verticalStackView.arrangedSubviews
      indexOfObject:self.mostVisitedStackView];
  if (index == NSNotFound && self.returnToRecentTabTile) {
    index = [self.verticalStackView.arrangedSubviews
        indexOfObject:self.returnToRecentTabTile];
  }
  if (index == NSNotFound) {
    index = 0;
  } else {
    index++;
  }
  if (IsMagicStackEnabled()) {
    BOOL shouldShowCompactedSetUpListModule =
        set_up_list_utils::ShouldShowCompactedSetUpListModule();
    if (shouldShowCompactedSetUpListModule) {
      _compactedSetUpListViews = [NSMutableArray array];
    }
    if ([self hasMagicStackLoaded]) {
      ContentSuggestionsModuleType firstItemType =
          SetUpListModuleTypeForSetUpListType([items firstObject].type);
      [self logTopModuleImpressionForType:shouldShowCompactedSetUpListModule
                                              ? ContentSuggestionsModuleType::
                                                    kCompactedSetUpList
                                              : firstItemType];
    }
    for (SetUpListItemViewData* data in items) {
      data.compactLayout = shouldShowCompactedSetUpListModule;
      data.heroCellMagicStackLayout = !shouldShowCompactedSetUpListModule;
      SetUpListItemView* view = [[SetUpListItemView alloc] initWithData:data];
      view.tapDelegate = self;
      ContentSuggestionsModuleType type =
          SetUpListModuleTypeForSetUpListType(data.type);
      if (shouldShowCompactedSetUpListModule) {
        [_compactedSetUpListViews addObject:view];
      }
      switch (type) {
        case ContentSuggestionsModuleType::kSetUpListSync:
          _setUpListSyncItemView = view;
          break;
        case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
          _setUpListDefaultBrowserItemView = view;
          break;
        case ContentSuggestionsModuleType::kSetUpListAutofill:
          _setUpListAutofillItemView = view;
          break;
        case ContentSuggestionsModuleType::kSetUpListAllSet:
          _setUpListAllSetItemView = view;
          break;
        default:
          break;
      }
      // Only add it to the Magic Stack here if it is after the inital
      // construction of the Magic Stack.
      if (_magicStackRankReceived) {
        if (shouldShowCompactedSetUpListModule) {
          MultiRowContainerView* multiRowContainer =
              [[MultiRowContainerView alloc]
                  initWithViews:_compactedSetUpListViews];
          MagicStackModuleContainer* setUpListCompactedModule =
              [[MagicStackModuleContainer alloc]
                  initWithContentView:multiRowContainer
                                 type:ContentSuggestionsModuleType::
                                          kCompactedSetUpList
                             delegate:self];
          [self insertModuleIntoMagicStack:setUpListCompactedModule];
        } else {
          MagicStackModuleContainer* setUpListModule =
              [[MagicStackModuleContainer alloc] initWithContentView:view
                                                                type:type
                                                            delegate:self];
          [self insertModuleIntoMagicStack:setUpListModule];
        }
      }
    }
  } else {
    SetUpListView* setUpListView =
        [[SetUpListView alloc] initWithItems:items rootView:self.view];
    setUpListView.delegate = self.setUpListViewDelegate;
    self.setUpListView = setUpListView;
    [self.verticalStackView insertArrangedSubview:setUpListView atIndex:index];

    // The width of the SetUpList should match the Discover Feed. This seems to
    // closely match the feed's logic.
    CGFloat width = kSetUpListWidthRegular;
    CGSize viewSize = self.view.frame.size;
    if (MIN(viewSize.width, viewSize.height) >= kSetUpListWidthWide) {
      width = kSetUpListWidthWide;
    }
    // Since this view is put into a StackView, this width constraint acts as
    // a max width constraint - if the StackView is narrower, it will make the
    // SetUpListView narrower.
    [NSLayoutConstraint activateConstraints:@[
      [setUpListView.widthAnchor constraintEqualToConstant:width],
    ]];
  }
}

- (void)markSetUpListItemComplete:(SetUpListItemType)type
                       completion:(ProceduralBlock)completion {
  if (IsMagicStackEnabled()) {
    switch (type) {
      case SetUpListItemType::kSignInSync:
        [_setUpListSyncItemView markCompleteWithCompletion:completion];
        break;
      case SetUpListItemType::kDefaultBrowser:
        [_setUpListDefaultBrowserItemView
            markCompleteWithCompletion:completion];
        break;
      case SetUpListItemType::kAutofill:
        [_setUpListAutofillItemView markCompleteWithCompletion:completion];
        break;
      default:
        break;
    }
  } else {
    [self.setUpListView markItemComplete:type completion:completion];
  }
}

- (void)hideSetUpListWithAnimations:(ProceduralBlock)animations {
  if (IsMagicStackEnabled()) {
    // Remove Modules with animation
    [self removeSetUpListItemsWithNewModule:nil];
    return;
  }

  CHECK(self.setUpListView);
  NSInteger index = [self.verticalStackView.arrangedSubviews
      indexOfObject:self.setUpListView];
  CHECK_NE(index, NSNotFound);

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kSetUpListHideAnimationDuration.InSecondsF()
      animations:^{
        __typeof(self) strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        strongSelf.setUpListView.hidden = YES;
        strongSelf.setUpListView.alpha = 0;
        [strongSelf.view setNeedsLayout];
        [strongSelf.view layoutIfNeeded];
        if (animations) {
          animations();
        }
      }
      completion:^(BOOL finished) {
        __typeof(self) strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        [strongSelf.setUpListView removeFromSuperview];
        strongSelf.setUpListView.delegate = nil;
        strongSelf.setUpListView = nil;
      }];
}

- (void)showSetUpListDoneWithAnimations:(ProceduralBlock)animations {
  if (IsMagicStackEnabled()) {
    SetUpListItemViewData* allSetData =
        [[SetUpListItemViewData alloc] initWithType:SetUpListItemType::kAllSet
                                           complete:NO];
    allSetData.heroCellMagicStackLayout =
        !set_up_list_utils::ShouldShowCompactedSetUpListModule();
    SetUpListItemView* view =
        [[SetUpListItemView alloc] initWithData:allSetData];
    MagicStackModuleContainer* allSetModule = [[MagicStackModuleContainer alloc]
        initWithContentView:view
                       type:ContentSuggestionsModuleType::kSetUpListAllSet
                   delegate:self];
    // Determine which module to swap out.
    [self removeSetUpListItemsWithNewModule:allSetModule];
    return;
  }
  __weak __typeof(self) weakSelf = self;
  [self.setUpListView showDoneWithAnimations:^{
    [weakSelf.view setNeedsLayout];
    [weakSelf.view layoutIfNeeded];
    if (animations) {
      animations();
    }
  }];
}

// Shows the Safety Check (Magic Stack) module with `state`.
- (void)showSafetyCheck:(SafetyCheckState*)state {
  _safetyCheckState = state;

  if (!_magicStackRankReceived) {
    return;
  }

  __block NSUInteger safetyCheckModuleOrderIndex = NSNotFound;

  [_magicStackModuleOrder enumerateObjectsUsingBlock:^(NSNumber* moduleValue,
                                                       NSUInteger idx,
                                                       BOOL* stop) {
    ContentSuggestionsModuleType type =
        (ContentSuggestionsModuleType)[moduleValue intValue];

    if (type == ContentSuggestionsModuleType::kSafetyCheck ||
        type == ContentSuggestionsModuleType::kSafetyCheckMultiRow ||
        type == ContentSuggestionsModuleType::kSafetyCheckMultiRowOverflow) {
      safetyCheckModuleOrderIndex = idx;

      *stop = YES;
    }
  }];

  __block NSUInteger safetyCheckModuleIndex = NSNotFound;

  if (self.safetyCheckModuleContainer) {
    // If there's an existing Safety Check module, find its current index.
    [_magicStack.arrangedSubviews
        enumerateObjectsUsingBlock:^(MagicStackModuleContainer* moduleContainer,
                                     NSUInteger idx, BOOL* stop) {
          if (moduleContainer.type ==
                  ContentSuggestionsModuleType::kSafetyCheck ||
              moduleContainer.type ==
                  ContentSuggestionsModuleType::kSafetyCheckMultiRow ||
              moduleContainer.type ==
                  ContentSuggestionsModuleType::kSafetyCheckMultiRowOverflow) {
            safetyCheckModuleIndex = idx;

            *stop = YES;
          }
        }];

    // Assert the updated Safety Check module will be replaced at the same index
    // as the current module.
    CHECK_EQ(safetyCheckModuleOrderIndex, safetyCheckModuleIndex);

    [self.safetyCheckModuleContainer removeFromSuperview];
  }

  [self createSafetyCheck:state];

  UMA_HISTOGRAM_BOOLEAN("IOS.SafetyCheck.MagicStack.ModuleExistsInModuleOrder",
                        safetyCheckModuleOrderIndex != NSNotFound);

  if (safetyCheckModuleOrderIndex != NSNotFound) {
    _magicStackModuleOrder[safetyCheckModuleOrderIndex] =
        @(int(self.safetyCheckModuleContainer.type));

    [self logTopModuleImpressionForType:self.safetyCheckModuleContainer.type];

    [self insertModuleIntoMagicStack:self.safetyCheckModuleContainer];
  }
}

- (CGFloat)contentSuggestionsHeight {
  CGFloat height = 0;
  if ([self.mostVisitedViews count] > 0 &&
      !ShouldPutMostVisitedSitesInMagicStack()) {
    height += MostVisitedCellSize(
                  UIApplication.sharedApplication.preferredContentSizeCategory)
                  .height +
              kMostVisitedBottomMargin;
  }
  if (IsMagicStackEnabled()) {
    height += _magicStackScrollView.contentSize.height;
  } else {
    if ([self.shortcutsViews count] > 0) {
      height +=
          MostVisitedCellSize(
              UIApplication.sharedApplication.preferredContentSizeCategory)
              .height;
    }
  }
  if (self.returnToRecentTabTile) {
    height += ReturnToRecentTabHeight();
  }
  if (self.setUpListView && !self.setUpListView.isHidden) {
    height += self.setUpListView.frame.size.height;
  }
  return height;
}

- (void)showTabResumptionWithItem:(TabResumptionItem*)item {
  CHECK(IsTabResumptionEnabled());
  if ([self hasMagicStackLoaded]) {
    [self logTopModuleImpressionForType:ContentSuggestionsModuleType::
                                            kTabResumption];
  }
  _tabResumptionView = [[TabResumptionView alloc] initWithItem:item];
  _tabResumptionView.delegate = self;
  [_tabResumptionModuleContainer removeFromSuperview];
  _tabResumptionModuleContainer = [[MagicStackModuleContainer alloc]
      initWithContentView:_tabResumptionView
                     type:ContentSuggestionsModuleType::kTabResumption
                 delegate:self];

  if (_magicStackRankReceived) {
    [self insertModuleIntoMagicStack:self.tabResumptionModuleContainer];
  }
}

- (void)hideTabResumption {
  NSUInteger moduleIndex = [self
      indexForMagicStackModule:ContentSuggestionsModuleType::kTabResumption];
  [_tabResumptionModuleContainer removeFromSuperview];
  [_magicStackModuleOrder removeObjectAtIndex:moduleIndex];
}

- (void)showParcelTrackingItems:(NSArray<ParcelTrackingItem*>*)items {
  _parcelTrackingModuleContainers = [NSMutableArray array];

  if ([items count] > 2) {
    ParcelTrackingModuleView* parcelTrackingModuleView =
        [[ParcelTrackingModuleView alloc] initWithFrame:CGRectZero];
    parcelTrackingModuleView.delegate = self;
    [parcelTrackingModuleView configureView:items[0]];
    MagicStackModuleContainer* parcelTrackingModuleContainer =
        [[MagicStackModuleContainer alloc]
            initWithContentView:parcelTrackingModuleView
                           type:ContentSuggestionsModuleType::
                                    kParcelTrackingSeeMore
                       delegate:self];
    [_parcelTrackingModuleContainers addObject:parcelTrackingModuleContainer];
  } else {
    for (ParcelTrackingItem* item in items) {
      ParcelTrackingModuleView* parcelTrackingModuleView =
          [[ParcelTrackingModuleView alloc] initWithFrame:CGRectZero];
      parcelTrackingModuleView.delegate = self;
      [parcelTrackingModuleView configureView:item];
      MagicStackModuleContainer* parcelTrackingModuleContainer =
          [[MagicStackModuleContainer alloc]
              initWithContentView:parcelTrackingModuleView
                             type:ContentSuggestionsModuleType::kParcelTracking
                         delegate:self];
      [_parcelTrackingModuleContainers addObject:parcelTrackingModuleContainer];
    }
  }

  if (_magicStackRankReceived) {
    for (MagicStackModuleContainer* parcelTrackingModuleContainer in
             _parcelTrackingModuleContainers) {
      [self insertModuleIntoMagicStack:parcelTrackingModuleContainer];
    }
  }
}

#pragma mark - SetUpListItemViewTapDelegate methods

- (void)didTapSetUpListItemView:(SetUpListItemView*)view {
  [self.audience didSelectSetUpListItem:view.type];
}

#pragma mark - TabResumptionViewDelegate methods

- (void)tabResumptionViewTapped {
  [self.suggestionCommandHandler openTabResumptionItem];
}

#pragma mark - ParcelTrackingViewDelegate methods

- (void)loadParcelTrackingPage:(GURL)parcelTrackingURL {
  [self.suggestionCommandHandler loadParcelTrackingPage:parcelTrackingURL];
}

#pragma mark - ContentSuggestionsSelectionActions

- (void)contentSuggestionsElementTapped:(UIGestureRecognizer*)sender {
  if ([sender.view
          isKindOfClass:[ContentSuggestionsMostVisitedTileView class]]) {
    ContentSuggestionsMostVisitedTileView* mostVisitedView =
        static_cast<ContentSuggestionsMostVisitedTileView*>(sender.view);
    [self.suggestionCommandHandler
        openMostVisitedItem:mostVisitedView.config
                    atIndex:mostVisitedView.config.index];
  } else if ([sender.view
                 isKindOfClass:[ContentSuggestionsShortcutTileView class]]) {
    ContentSuggestionsShortcutTileView* shortcutView =
        static_cast<ContentSuggestionsShortcutTileView*>(sender.view);
    int index = static_cast<int>(shortcutView.config.index);
    [self.suggestionCommandHandler openMostVisitedItem:shortcutView.config
                                               atIndex:index];
  } else if ([sender.view isKindOfClass:[ContentSuggestionsReturnToRecentTabView
                                            class]]) {
    ContentSuggestionsReturnToRecentTabView* returnToRecentTabView =
        static_cast<ContentSuggestionsReturnToRecentTabView*>(sender.view);
    __weak ContentSuggestionsReturnToRecentTabView* weakRecentTabView =
        returnToRecentTabView;
    UIGestureRecognizerState state = sender.state;
    if (state == UIGestureRecognizerStateChanged ||
        state == UIGestureRecognizerStateCancelled) {
      // Do nothing if isn't a gesture start or end.
      // If the gesture was cancelled by the system, then reset the background
      // color since UIGestureRecognizerStateEnded will not be received.
      if (state == UIGestureRecognizerStateCancelled) {
        returnToRecentTabView.backgroundColor = [UIColor clearColor];
      }
      return;
    }
    BOOL touchBegan = state == UIGestureRecognizerStateBegan;
    [UIView transitionWithView:returnToRecentTabView
                      duration:kMaterialDuration8
                       options:UIViewAnimationOptionCurveEaseInOut
                    animations:^{
                      weakRecentTabView.backgroundColor =
                          touchBegan ? [UIColor colorNamed:kGrey100Color]
                                     : [UIColor clearColor];
                    }
                    completion:nil];
    if (state == UIGestureRecognizerStateEnded) {
      CGPoint point = [sender locationInView:returnToRecentTabView];
      if (point.x < 0 || point.y < 0 ||
          point.x > kReturnToRecentTabSize.width ||
          point.y > ReturnToRecentTabHeight()) {
        // Reset the highlighted state and do nothing if the gesture ended
        // outside of the tile.
        returnToRecentTabView.backgroundColor = [UIColor clearColor];
        return;
      }
      [self.suggestionCommandHandler openMostRecentTab];
    }
  }
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (content_suggestions::ShouldShowWiderMagicStackLayer(self.traitCollection,
                                                          self.view.window)) {
    if (_returnToRecentTabWidthAnchor) {
      // Match Module width when Magic Stack is enabled.
      _returnToRecentTabWidthAnchor.constant = kMagicStackWideWidth;
    }
    _magicStackScrollView.clipsToBounds = YES;
    _magicStackScrollViewWidthAnchor.constant = kMagicStackWideWidth;
  } else {
    if (_returnToRecentTabWidthAnchor) {
      _returnToRecentTabWidthAnchor.constant =
          content_suggestions::SearchFieldWidth(self.view.bounds.size.width,
                                                self.traitCollection);
    }
    _magicStackScrollView.clipsToBounds = NO;
    _magicStackScrollViewWidthAnchor.constant = [MagicStackModuleContainer
        moduleWidthForHorizontalTraitCollection:self.traitCollection];
  }
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  DCHECK(IsMagicStackEnabled());
  targetContentOffset->x =
      [self getNextPageOffsetForOffset:scrollView.contentOffset.x
                              velocity:velocity.x];
}

#pragma mark - UIScrollViewAccessibilityDelegate

// This reads out the new page whenever the user scrolls in VoiceOver.
- (NSString*)accessibilityScrollStatusForScrollView:(UIScrollView*)scrollView {
  return [MagicStackModuleContainer
      titleStringForModule:[self currentlyShownModule]];
}

#pragma mark - MagicStackModuleContainerDelegate

- (BOOL)doesMagicStackShowOnlyOneModule:(ContentSuggestionsModuleType)type {
  // Return NO if Most Visited Module is asking while it is not in the Magic
  // Stack.
  if (type == ContentSuggestionsModuleType::kMostVisited &&
      !ShouldPutMostVisitedSitesInMagicStack()) {
    return NO;
  }
  if (!_magicStackRankReceived &&
      base::FeatureList::IsEnabled(segmentation_platform::features::
                                       kSegmentationPlatformIosModuleRanker)) {
    // There are two placeholders shown in the Magic Stack.
    return NO;
  }
  ContentSuggestionsModuleType firstModuleType = (ContentSuggestionsModuleType)[
      [_magicStackModuleOrder objectAtIndex:0] intValue];
  return [_magicStackModuleOrder count] == 1 && firstModuleType == type;
}

- (void)seeMoreWasTappedForModuleType:(ContentSuggestionsModuleType)type {
  switch (type) {
    case ContentSuggestionsModuleType::kSafetyCheckMultiRowOverflow:
      [self.audience didSelectSafetyCheckItem:SafetyCheckItemType::kDefault];
      break;
    case ContentSuggestionsModuleType::kCompactedSetUpList:
      [self.audience showSetUpListShowMoreMenu];
      break;
    case ContentSuggestionsModuleType::kParcelTrackingSeeMore:
      [self.audience showMagicStackParcelList];
      break;
    default:
      break;
  }
}

- (void)neverShowModuleType:(ContentSuggestionsModuleType)type {
  [self.audience neverShowModuleType:type];
}

// Returns the module's subtitle, if any, given the Magic Stack module `type`.
- (NSString*)subtitleStringForModule:(ContentSuggestionsModuleType)type {
  if (type == ContentSuggestionsModuleType::kSafetyCheck ||
      type == ContentSuggestionsModuleType::kSafetyCheckMultiRow) {
    return FormatElapsedTimeSinceLastSafetyCheck(_safetyCheckState.lastRunTime);
  }

  return @"";
}

#pragma mark - Private

// Returns whether the Magic Stack has been constructed and has already been
// populated with modules.
- (BOOL)hasMagicStackLoaded {
  return self.viewLoaded && _magicStackRankReceived;
}

- (void)addUIElement:(UIView*)view withCustomBottomSpacing:(CGFloat)spacing {
  [self.verticalStackView addArrangedSubview:view];
  if (spacing > 0) {
    [self.verticalStackView setCustomSpacing:spacing afterView:view];
  }
}

- (void)layoutReturnToRecentTabTile {
  CGFloat cardWidth = content_suggestions::SearchFieldWidth(
      self.view.bounds.size.width, self.traitCollection);
  if (IsMagicStackEnabled() &&
      content_suggestions::ShouldShowWiderMagicStackLayer(self.traitCollection,
                                                          self.view.window)) {
    // Match Module width when Magic Stack is enabled.
    cardWidth = kMagicStackWideWidth;
  }
  _returnToRecentTabWidthAnchor =
      [_returnToRecentTabTile.widthAnchor constraintEqualToConstant:cardWidth];
  [NSLayoutConstraint activateConstraints:@[
    _returnToRecentTabWidthAnchor,
    [_returnToRecentTabTile.heightAnchor
        constraintEqualToConstant:ReturnToRecentTabHeight()]
  ]];
}

- (void)createAndInsertMostVisitedModule {
  CGFloat horizontalSpacing =
      ContentSuggestionsTilesHorizontalSpacing(self.traitCollection);
  self.mostVisitedStackView = [[UIStackView alloc] init];
  self.mostVisitedStackView.axis = UILayoutConstraintAxisHorizontal;
  self.mostVisitedStackView.distribution = UIStackViewDistributionFillEqually;
  self.mostVisitedStackView.spacing = horizontalSpacing;
  self.mostVisitedStackView.alignment = UIStackViewAlignmentTop;

  // Find correct insertion position in the stack.
  int insertionIndex = 0;
  if (self.returnToRecentTabTile) {
    insertionIndex++;
  }
  if (IsMagicStackEnabled()) {
    self.mostVisitedModuleContainer = [[MagicStackModuleContainer alloc]
        initWithContentView:self.mostVisitedStackView
                       type:ContentSuggestionsModuleType::kMostVisited
                   delegate:self];
    if (ShouldPutMostVisitedSitesInMagicStack()) {
      // Only add it to the Magic Stack here if it is after the inital
      // construction of the Magic Stack.
      if (_magicStackRankReceived) {
        [self insertModuleIntoMagicStack:self.mostVisitedModuleContainer];
      }
    } else {
      [self.verticalStackView
          insertArrangedSubview:self.mostVisitedModuleContainer
                        atIndex:insertionIndex];
      [self.verticalStackView setCustomSpacing:kMostVisitedBottomMargin
                                     afterView:self.mostVisitedModuleContainer];
    }
  } else {
    [self.verticalStackView insertArrangedSubview:self.mostVisitedStackView
                                          atIndex:insertionIndex];
    [self.verticalStackView setCustomSpacing:kMostVisitedBottomMargin
                                   afterView:self.mostVisitedStackView];
    CGFloat width =
        MostVisitedTilesContentHorizontalSpace(self.traitCollection);
    CGSize size =
        MostVisitedCellSize(self.traitCollection.preferredContentSizeCategory);
    [NSLayoutConstraint activateConstraints:@[
      [self.mostVisitedStackView.widthAnchor constraintEqualToConstant:width],
      [self.mostVisitedStackView.heightAnchor
          constraintEqualToConstant:size.height]
    ]];
  }
}

// Creates the Safety Check (Magic Stack) module using the Safety Check `state`.
- (void)createSafetyCheck:(SafetyCheckState*)state {
  self.safetyCheckState = state;

  self.safetyCheckView = [[SafetyCheckView alloc] initWithState:state];

  self.safetyCheckView.delegate = self.audience;

  int checkIssuesCount = CheckIssuesCount(state);

  ContentSuggestionsModuleType type =
      ContentSuggestionsModuleType::kSafetyCheck;

  if (checkIssuesCount > 2) {
    type = ContentSuggestionsModuleType::kSafetyCheckMultiRowOverflow;
  } else if (checkIssuesCount > 1) {
    type = ContentSuggestionsModuleType::kSafetyCheckMultiRow;
  }

  self.safetyCheckModuleContainer = [[MagicStackModuleContainer alloc]
      initWithContentView:self.safetyCheckView
                     type:type
                 delegate:self];
}

// Add the elements in `mostVisitedViews` into `verticalStackView`.
- (void)populateMostVisitedModule {
  for (ContentSuggestionsMostVisitedTileView* view in self.mostVisitedViews) {
    view.menuProvider = self.menuProvider;
    UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(contentSuggestionsElementTapped:)];
    [view addGestureRecognizer:tapRecognizer];
    tapRecognizer.enabled = YES;
    [self.mostVisitedTapRecognizers addObject:tapRecognizer];
    [self.mostVisitedStackView addArrangedSubview:view];
  }
}

- (UIStackView*)createShortcutsStackView {
  UIStackView* shortcutsStackView = [[UIStackView alloc] init];
  shortcutsStackView.axis = UILayoutConstraintAxisHorizontal;
  shortcutsStackView.distribution = UIStackViewDistributionFillEqually;
  shortcutsStackView.spacing =
      ContentSuggestionsTilesHorizontalSpacing(self.traitCollection);
  shortcutsStackView.alignment = UIStackViewAlignmentTop;
  NSUInteger index = 0;
  for (ContentSuggestionsShortcutTileView* view in self.shortcutsViews) {
    view.accessibilityIdentifier = [NSString
        stringWithFormat:
            @"%@%li", kContentSuggestionsShortcutsAccessibilityIdentifierPrefix,
            index];
    UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(contentSuggestionsElementTapped:)];
    [view addGestureRecognizer:tapRecognizer];
    [self.mostVisitedTapRecognizers addObject:tapRecognizer];
    [shortcutsStackView addArrangedSubview:view];
    index++;
  }
  return shortcutsStackView;
}

// Logs `type` as the top module shown if it is first in
// `_magicStackModuleOrder`
- (void)logTopModuleImpressionForType:(ContentSuggestionsModuleType)type {
  ContentSuggestionsModuleType firstModuleType = (ContentSuggestionsModuleType)[
      [_magicStackModuleOrder objectAtIndex:0] intValue];
  if (firstModuleType == type) {
    [self.contentSuggestionsMetricsRecorder
        recordMagicStackTopModuleImpressionForType:type];
  }
}

// Constructs the Magic Stack module only. No modules are added in this
// implementation.
- (void)createMagicStack {
  _magicStackScrollView = [[UIScrollView alloc] init];
  [_magicStackScrollView setShowsHorizontalScrollIndicator:NO];
  _magicStackScrollView.clipsToBounds =
      content_suggestions::ShouldShowWiderMagicStackLayer(self.traitCollection,
                                                          self.view.window);
  _magicStackScrollView.layer.cornerRadius = kMagicStackCornerRadius;
  _magicStackScrollView.delegate = self;
  _magicStackScrollView.decelerationRate = UIScrollViewDecelerationRateFast;
  _magicStackScrollView.accessibilityIdentifier =
      kMagicStackScrollViewAccessibilityIdentifier;
  [self addUIElement:_magicStackScrollView
      withCustomBottomSpacing:kMostVisitedBottomMargin];

  _magicStack = [[UIStackView alloc] init];
  _magicStack.translatesAutoresizingMaskIntoConstraints = NO;
  _magicStack.axis = UILayoutConstraintAxisHorizontal;
  _magicStack.distribution = UIStackViewDistributionEqualSpacing;
  _magicStack.spacing = kMagicStackSpacing;
  _magicStack.accessibilityIdentifier = kMagicStackViewAccessibilityIdentifier;
  // Ensures modules take up entire height of the Magic Stack.
  _magicStack.alignment = UIStackViewAlignmentFill;
  [_magicStackScrollView addSubview:_magicStack];

  AddSameConstraints(_magicStack, _magicStackScrollView);
  // Define width of ScrollView. Instrinsic content height of the
  // StackView within the ScrollView will define the height of the
  // ScrollView.
  CGFloat width = [MagicStackModuleContainer
      moduleWidthForHorizontalTraitCollection:self.traitCollection];
  // Magic Stack has a wider width for wider screens so that clipToBounds can be
  // YES with a peeking module still visible.
  if (content_suggestions::ShouldShowWiderMagicStackLayer(self.traitCollection,
                                                          self.view.window)) {
    width = kMagicStackWideWidth;
  }
  _magicStackScrollViewWidthAnchor =
      [_magicStackScrollView.widthAnchor constraintEqualToConstant:width];
  [NSLayoutConstraint activateConstraints:@[
    // Ensures only horizontal scrolling
    [_magicStack.heightAnchor
        constraintEqualToAnchor:_magicStackScrollView.heightAnchor],
    _magicStackScrollViewWidthAnchor
  ]];
}

// Resets and fills the Magic Stack with modules using `_magicStackModuleOrder`.
// This method loops over any module types listed in `_magicStackModuleOrder`
// and adds their respective views to the Magic Stack.
- (void)populateMagicStack {
  if (base::FeatureList::IsEnabled(segmentation_platform::features::
                                       kSegmentationPlatformIosModuleRanker)) {
    // Clear out any placeholders.
    for (UIView* view in _magicStack.arrangedSubviews) {
      [view removeFromSuperview];
    }
  }

  // Add Magic Stack modules in order dictated by `_magicStackModuleOrder`.
  for (NSNumber* moduleType in _magicStackModuleOrder) {
    ContentSuggestionsModuleType type =
        (ContentSuggestionsModuleType)[moduleType intValue];
    MagicStackModuleContainer* moduleContainer;
    switch (type) {
      case ContentSuggestionsModuleType::kTabResumption: {
        moduleContainer = _tabResumptionModuleContainer;
        break;
      }
      case ContentSuggestionsModuleType::kShortcuts: {
        moduleContainer = self.shortcutsModuleContainer;
        break;
      }
      case ContentSuggestionsModuleType::kMostVisited: {
        if (ShouldPutMostVisitedSitesInMagicStack()) {
          moduleContainer = self.mostVisitedModuleContainer;
        }
        break;
      }
      case ContentSuggestionsModuleType::kSetUpListSync: {
        MagicStackModuleContainer* setUpListSyncModule =
            [[MagicStackModuleContainer alloc]
                initWithContentView:_setUpListSyncItemView
                               type:type
                           delegate:self];
        moduleContainer = setUpListSyncModule;
        break;
      }
      case ContentSuggestionsModuleType::kSetUpListDefaultBrowser: {
        MagicStackModuleContainer* setUpListDefaultBrowserModule =
            [[MagicStackModuleContainer alloc]
                initWithContentView:_setUpListDefaultBrowserItemView
                               type:type
                           delegate:self];
        moduleContainer = setUpListDefaultBrowserModule;
        break;
      }
      case ContentSuggestionsModuleType::kSetUpListAutofill: {
        MagicStackModuleContainer* setUpListAutofillModule =
            [[MagicStackModuleContainer alloc]
                initWithContentView:_setUpListAutofillItemView
                               type:type
                           delegate:self];
        moduleContainer = setUpListAutofillModule;
        break;
      }
      case ContentSuggestionsModuleType::kCompactedSetUpList: {
        MultiRowContainerView* multiRowContainer =
            [[MultiRowContainerView alloc]
                initWithViews:_compactedSetUpListViews];
        MagicStackModuleContainer* setUpListCompactedModule =
            [[MagicStackModuleContainer alloc]
                initWithContentView:multiRowContainer
                               type:ContentSuggestionsModuleType::
                                        kCompactedSetUpList
                           delegate:self];
        moduleContainer = setUpListCompactedModule;
        break;
      }
      case ContentSuggestionsModuleType::kSetUpListAllSet: {
        MagicStackModuleContainer* setUpListAllSetModule =
            [[MagicStackModuleContainer alloc]
                initWithContentView:_setUpListAllSetItemView
                               type:type
                           delegate:self];
        moduleContainer = setUpListAllSetModule;
        break;
      }
      case ContentSuggestionsModuleType::kSafetyCheck:
      case ContentSuggestionsModuleType::kSafetyCheckMultiRow:
      case ContentSuggestionsModuleType::kSafetyCheckMultiRowOverflow: {
        if (IsSafetyCheckMagicStackEnabled()) {
          moduleContainer = self.safetyCheckModuleContainer;
        }
        break;
      }
      case ContentSuggestionsModuleType::kParcelTracking:
      case ContentSuggestionsModuleType::kParcelTrackingSeeMore:
        if (IsIOSParcelTrackingEnabled()) {
          for (MagicStackModuleContainer* parcelModule in
                   _parcelTrackingModuleContainers) {
            // Find a parcel tracking module that hasn't been added yet.
            if (![parcelModule superview]) {
              moduleContainer = parcelModule;
              break;
            }
          }
        }
        break;
      default:
        break;
    }
    if (moduleContainer) {
      [_magicStack addArrangedSubview:moduleContainer];
      [self logTopModuleImpressionForType:moduleContainer.type];
    }
  }

  // Add Edit Button.
  UIButton* editButton = [UIButton buttonWithType:UIButtonTypeSystem];
  editButton.translatesAutoresizingMaskIntoConstraints = NO;
  UIImage* image = DefaultSymbolTemplateWithPointSize(
      kSliderHorizontal, kMagicStackEditButtonIconPointSize);
  [editButton setImage:image forState:UIControlStateNormal];
  editButton.tintColor = [UIColor colorNamed:kSolidBlackColor];
  editButton.backgroundColor =
      [UIColor colorNamed:@"magic_stack_edit_button_background_color"];
  editButton.layer.cornerRadius = kMagicStackEditButtonWidth / 2;
  [editButton addTarget:self.audience
                 action:@selector(didTapMagicStackEditButton)
       forControlEvents:UIControlEventTouchUpInside];
  editButton.accessibilityIdentifier =
      kMagicStackEditButtonAccessibilityIdentifier;
  editButton.pointerInteractionEnabled = YES;

  UIView* editContainerView = [[UIView alloc] init];
  editContainerView.accessibilityIdentifier =
      kMagicStackEditButtonContainerAccessibilityIdentifier;
  [editContainerView addSubview:editButton];

  [_magicStack addArrangedSubview:editContainerView];

  [NSLayoutConstraint activateConstraints:@[
    [editButton.leadingAnchor
        constraintEqualToAnchor:editContainerView.leadingAnchor
                       constant:kMagicStackEditButtonMargin],
    [editButton.trailingAnchor
        constraintEqualToAnchor:editContainerView.trailingAnchor
                       constant:-kMagicStackEditButtonMargin],
    [editButton.centerYAnchor
        constraintEqualToAnchor:editContainerView.centerYAnchor],
    [editButton.widthAnchor
        constraintEqualToConstant:kMagicStackEditButtonWidth],
    [editButton.heightAnchor constraintEqualToAnchor:editButton.widthAnchor]
  ]];
}

// Adds two placeholder modules to Magic Stack.
- (void)populateMagicStackWithPlaceholders {
  CHECK(_magicStack);
  CHECK([_magicStack.arrangedSubviews count] == 0);
  [_magicStack
      addArrangedSubview:[[MagicStackModuleContainer alloc] initAsPlaceholder]];
  [_magicStack
      addArrangedSubview:[[MagicStackModuleContainer alloc] initAsPlaceholder]];
}

// Returns the index position `moduleType` should be placed in the Magic Stack.
// This should only be used when looking to add a module after the inital Magic
// Stack construction.
- (NSUInteger)indexForMagicStackModule:
    (ContentSuggestionsModuleType)moduleType {
  NSUInteger index = 0;
  for (NSNumber* moduleTypeNum in _magicStackModuleOrder) {
    ContentSuggestionsModuleType type =
        (ContentSuggestionsModuleType)[moduleTypeNum intValue];
    if (type == moduleType) {
      return index;
    }
    index++;
  }
  NOTREACHED_NORETURN();
}

// This method should be the one used to insert a module into the Magic Stack
// after the latter has been already created. This logic is necessary to handle
// situations where modules can become available to show in the Magic Stack
// after initial view construction in no predictable order.
- (void)insertModuleIntoMagicStack:(MagicStackModuleContainer*)moduleToInsert {
  NSUInteger insertingModuleOrderIndex =
      [self indexForMagicStackModule:moduleToInsert.type];

  NSUInteger magicStackIndex = 0;
  for (UIView* view in _magicStack.arrangedSubviews) {
    if (view.accessibilityIdentifier ==
        kMagicStackEditButtonContainerAccessibilityIdentifier) {
      // Reached the edit button (e.g. end of modules).
      break;
    }
    MagicStackModuleContainer* moduleContainer =
        base::apple::ObjCCastStrict<MagicStackModuleContainer>(view);
    if ([self indexForMagicStackModule:moduleContainer.type] >
        insertingModuleOrderIndex) {
      // `moduleToInsert` should be inserted right in front of the first module
      // found with a rank position higher than it.
      break;
    }
    magicStackIndex++;
  }

  // `magicStackIndex` here either represents the position right before the
  // first found module with a rank higher than `moduleToInsert` or just before
  // the last arrangedSubview (e.g. edit button).
  [_magicStack insertArrangedSubview:moduleToInsert atIndex:magicStackIndex];
}

// Returns the `ContentSuggestionsModuleType` type of the module being currently
// shown in the Magic Stack.
- (ContentSuggestionsModuleType)currentlyShownModule {
  CGFloat offset = _magicStackScrollView.contentOffset.x;
  CGFloat moduleWidth = [MagicStackModuleContainer
      moduleWidthForHorizontalTraitCollection:self.traitCollection];
  NSUInteger moduleCount = [_magicStackModuleOrder count];
  // Find closest page to the current scroll offset.
  CGFloat closestPage = roundf(offset / moduleWidth);
  closestPage = fminf(closestPage, moduleCount - 1);
  return (ContentSuggestionsModuleType)[_magicStackModuleOrder[(
      NSUInteger)closestPage] intValue];
}

// Replaces the currently visible Set Up List module with `newModule` if valid
// and then removes all other Set Up List modules.
- (void)removeSetUpListItemsWithNewModule:
    (MagicStackModuleContainer*)newModule {
  int index = 0;
  NSMutableArray* viewIndicesToRemove = [NSMutableArray array];
  for (NSNumber* moduleType in _magicStackModuleOrder) {
    ContentSuggestionsModuleType type =
        (ContentSuggestionsModuleType)[moduleType intValue];
    switch (type) {
      case ContentSuggestionsModuleType::kSetUpListSync:
      case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
      case ContentSuggestionsModuleType::kSetUpListAutofill:
      case ContentSuggestionsModuleType::kCompactedSetUpList:
        [viewIndicesToRemove addObject:@(index)];
        break;
      default:
        break;
    }
    index++;
  }

  __weak __typeof(self) weakSelf = self;
  ProceduralBlock removeRemainingModules = ^{
    __typeof(self) strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    // Remove all non-visible modules in reverse order
    int removedModuleCount = [viewIndicesToRemove count];
    if (removedModuleCount > 0) {
      for (int i = removedModuleCount - 1; i >= 0; i--) {
        NSUInteger moduleIndex = [viewIndicesToRemove[i] integerValue];
        UIView* moduleToRemove =
            [strongSelf->_magicStack arrangedSubviews][moduleIndex];
        [moduleToRemove removeFromSuperview];
        [strongSelf->_magicStackModuleOrder removeObjectAtIndex:moduleIndex];
      }
      // Compensate for removed module count so the currently visible module is
      // still displayed.
      CGFloat moduleWidth = [MagicStackModuleContainer
          moduleWidthForHorizontalTraitCollection:self.traitCollection];
      CGFloat offsetRemoved = (removedModuleCount)*moduleWidth +
                              ((removedModuleCount)*kMagicStackSpacing);
      [strongSelf->_magicStackScrollView
          setContentOffset:CGPointMake(strongSelf->_magicStackScrollView
                                               .contentOffset.x -
                                           offsetRemoved,
                                       strongSelf->_magicStackScrollView
                                           .contentOffset.y)
                  animated:NO];
    }
  };

  if (newModule) {
    ProceduralBlock fadeOtherSetUpListItemsOut = ^{
      __typeof(self) strongSelf = weakSelf;
      if (!strongSelf) {
        return;
      }
      for (NSNumber* viewIndex in viewIndicesToRemove) {
        UIView* view = [strongSelf->_magicStack arrangedSubviews]
            [[viewIndex integerValue]];
        // Animate module away in the upward direction.
        view.transform = CGAffineTransformTranslate(
            CGAffineTransformIdentity, 0,
            -kMagicStackReplaceModuleFadeAnimationDistance);
        view.alpha = 0;
      }
    };
    // Replace last Set Up List item with "All Set" hero cell.
    NSUInteger moduleIndexToReplace =
        [[viewIndicesToRemove lastObject] integerValue];
    // Do not remove the replaced module.
    [viewIndicesToRemove removeObjectAtIndex:moduleIndexToReplace];
    [self replaceModuleAtIndex:moduleIndexToReplace
                    withModule:newModule
          additionalAnimations:fadeOtherSetUpListItemsOut
                    completion:removeRemainingModules];
  } else {
    removeRemainingModules();
  }
}

// Replaces the module at `index` with `newModule` in the Magic Stack along with
// any additional animations in `additionalAnimations`, executing `completion`
// after the completion of the replace animation.
- (void)replaceModuleAtIndex:(NSUInteger)index
                  withModule:(MagicStackModuleContainer*)newModule
        additionalAnimations:(ProceduralBlock)additionalAnimations
                  completion:(ProceduralBlock)completion {
  UIView* moduleToHide = [_magicStack arrangedSubviews][index];
  __weak __typeof(self) weakSelf = self;

  ProceduralBlock animateInNewModule = ^{
    [UIView animateWithDuration:0.5
        delay:0.0
        options:UIViewAnimationOptionTransitionCurlDown
        animations:^{
          __typeof(self) strongSelf = weakSelf;
          if (!strongSelf) {
            return;
          }
          // Fade in new module from the left to the final position in the Magic
          // Stack.
          newModule.transform = CGAffineTransformIdentity;
          newModule.alpha = 1;
        }
        completion:^(BOOL finished) {
          if (completion) {
            completion();
          }
        }];
  };

  [UIView animateWithDuration:0.5
      delay:0.0
      options:UIViewAnimationOptionTransitionCurlDown
      animations:^{
        __typeof(self) strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        // Animate module away in the upward direction.
        moduleToHide.transform = CGAffineTransformTranslate(
            CGAffineTransformIdentity, 0,
            -kMagicStackReplaceModuleFadeAnimationDistance);
        moduleToHide.alpha = 0;
        additionalAnimations();
      }
      completion:^(BOOL finished) {
        __typeof(self) strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        // Remove module to hide, add the new module with an initial position to
        // the left and hidden from view in preparation for a fade in.
        newModule.alpha = 0;
        [strongSelf->_magicStack removeArrangedSubview:moduleToHide];
        [strongSelf->_magicStack insertArrangedSubview:newModule atIndex:index];
        newModule.transform = CGAffineTransformTranslate(
            CGAffineTransformIdentity,
            -kMagicStackReplaceModuleFadeAnimationDistance, 0);
        [moduleToHide removeFromSuperview];
        [strongSelf->_magicStack setNeedsLayout];
        [strongSelf->_magicStack layoutIfNeeded];

        animateInNewModule();
      }];
}

// Determines the final page offset given the scroll `offset` and the `velocity`
// scroll. If the drag is slow enough, then the closest page is the final state.
// If the drag is in the negative direction, then go to the page previous to the
// closest current page. If the drag is in the positive direction, then go to
// the page after the closest current page.
- (CGFloat)getNextPageOffsetForOffset:(CGFloat)offset
                             velocity:(CGFloat)velocity {
  CGFloat moduleWidth = [MagicStackModuleContainer
      moduleWidthForHorizontalTraitCollection:self.traitCollection];
  NSUInteger moduleCount = [_magicStackModuleOrder count];

  // Find closest page to the current scroll offset.
  CGFloat closestPage = roundf(offset / moduleWidth);
  closestPage = fminf(closestPage, moduleCount);

  if (fabs(velocity) < kMagicStackMinimumPaginationScrollVelocity) {
    return closestPage * moduleWidth + (closestPage * 10);
  }
  if (velocity < 0) {
    return (closestPage - 1) * moduleWidth +
           ((closestPage - 1) * kMagicStackSpacing);
  }
  return (closestPage + 1) * moduleWidth +
         ((closestPage + 1) * kMagicStackSpacing);
}

@end
