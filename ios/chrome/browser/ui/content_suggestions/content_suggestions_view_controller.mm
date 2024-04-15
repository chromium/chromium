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
#import "ios/chrome/browser/drag_and_drop/model/url_drag_drop_handler.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/shared/public/commands/parcel_tracking_opt_in_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_shortcut_tile_view.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_layout_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/shortcuts_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/shortcuts_config.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_image_data_source.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_menu_provider.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_container.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_container_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/most_visited_tiles_config.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/placeholder_config.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_item.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_state.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/types.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/utils.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_config.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view_data.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/utils.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_item.h"
#import "ios/chrome/browser/ui/ntp/metrics/home_metrics.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// The corner radius of the Magic Stack.
const float kMagicStackCornerRadius = 16.0f;

// The distance in which a replaced/replacing module will fade out/in of view.
const float kMagicStackReplaceModuleFadeAnimationDistance = 50;

}  // namespace

@interface ContentSuggestionsViewController () <
    UIGestureRecognizerDelegate,
    MagicStackModuleContainerDelegate,
    URLDropDelegate,
    UIScrollViewDelegate,
    UIScrollViewAccessibilityDelegate>

@property(nonatomic, strong) URLDragDropHandler* dragDropHandler;

// StackView holding all subviews.
@property(nonatomic, strong) UIStackView* verticalStackView;

// List of all UITapGestureRecognizers created for the Most Visisted tiles.
@property(nonatomic, strong)
    NSMutableArray<UITapGestureRecognizer*>* mostVisitedTapRecognizers;

// Module Container for the Most Visited Tiles when being shown in Magic Stack.
@property(nonatomic, strong)
    MagicStackModuleContainer* mostVisitedModuleContainer;
// Module Container for the tab resumption tile.
@property(nonatomic, strong)
    MagicStackModuleContainer* tabResumptionModuleContainer;
// Module Container for the Shortcuts when being shown in Magic Stack.
@property(nonatomic, strong)
    MagicStackModuleContainer* shortcutsModuleContainer;
// The current state of the Safety Check.
@property(nonatomic, strong) SafetyCheckState* safetyCheckState;
// Module Container for the `safetyCheckView` when being shown in Magic Stack.
@property(nonatomic, strong)
    MagicStackModuleContainer* safetyCheckModuleContainer;

@end

@implementation ContentSuggestionsViewController {
  UIScrollView* _magicStackScrollView;
  UIStackView* _magicStack;
  // A layout guide used to define the width of MagicStack modules.
  UILayoutGuide* _magicStackModuleLayoutGuide;
  // The constraint that controls the width of the
  // `_magicStackModuleLayoutGuide`.
  NSLayoutConstraint* _magicStackModuleWidth;
  BOOL _magicStackRankReceived;
  NSMutableArray<NSNumber*>* _magicStackModuleOrder;
  MagicStackModuleContainer* _setUpListSyncModule;
  MagicStackModuleContainer* _setUpListDefaultBrowserModule;
  MagicStackModuleContainer* _setUpListAutofillModule;
  MagicStackModuleContainer* _setUpListNotificationsModule;
  MagicStackModuleContainer* _setUpListCompactedModule;
  MagicStackModuleContainer* _setUpListAllSetModule;
  MagicStackModuleContainer* _parcelTrackingModuleContainer;
  // The most recently selected MagicStack module's page index.
  NSUInteger _magicStackPage;
  MostVisitedTilesConfig* _mostVisitedTileConfig;
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
  self.view.backgroundColor = [UIColor clearColor];
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
                       constant:-kBottomMagicStackPadding]
  ]];

  if (_mostVisitedTileConfig && !ShouldPutMostVisitedSitesInMagicStack()) {
    [self createAndInsertMostVisitedModule];
  }

  // Only Create Magic Stack if the ranking has been received. It can be delayed
  // to after -viewDidLoad if fecthing from Segmentation Platform.
  if (!IsIOSMagicStackCollectionViewEnabled()) {
    [self createMagicStack];
    if (_magicStackRankReceived) {
      [self populateMagicStack];
    } else {
      // If Magic Stack rank has not been received from Segmentation, add
      // placeholders
      [self populateMagicStackWithPlaceholders];
    }
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  // Now that `view.window` is available, it is possible to tell whether the
  // window is Landscape, which affects whether the MagicStack should be
  // masked.
  [self updateMagicStackMasking];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [self.audience viewWillDisappear];
}

- (void)moduleWidthDidUpdate {
  if (_magicStackScrollView) {
    [self updateMagicStackMasking];
    [self snapToNearestMagicStackModule];
  }
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

- (void)setMostVisitedTilesConfig:(MostVisitedTilesConfig*)config {
  _mostVisitedTileConfig = config;
    if (self.mostVisitedModuleContainer) {
      [self.mostVisitedModuleContainer removeFromSuperview];
    }
    self.mostVisitedModuleContainer =
        [[MagicStackModuleContainer alloc] initWithFrame:CGRectZero];
    [self.mostVisitedModuleContainer
        configureWithConfig:_mostVisitedTileConfig];
    // If viewDidLoad has been called before the first valid Most Visited Tiles
    // are available, construct `mostVisitedStackView`.
    if (self.verticalStackView &&
        _mostVisitedTileConfig.mostVisitedItems.count > 0) {
      [self createAndInsertMostVisitedModule];
    }

    for (ContentSuggestionsMostVisitedItem* item in _mostVisitedTileConfig
             .mostVisitedItems) {
      [self.contentSuggestionsMetricsRecorder
          recordMostVisitedTileShown:item
                             atIndex:item.index];
    }
    if ([self hasMagicStackLoaded]) {
      [self logTopModuleImpressionForType:ContentSuggestionsModuleType::
                                              kMostVisited];
    }

  [self.contentSuggestionsMetricsRecorder recordMostVisitedTilesShown];
}

- (void)setShortcutTilesConfig:(ShortcutsConfig*)config {
    if (self.shortcutsModuleContainer) {
      [self.shortcutsModuleContainer removeFromSuperview];
    }
    self.shortcutsModuleContainer =
        [[MagicStackModuleContainer alloc] initWithFrame:CGRectZero];
    [self.shortcutsModuleContainer configureWithConfig:config];
    if ([self hasMagicStackLoaded]) {
      [self insertModuleIntoMagicStack:self.shortcutsModuleContainer];
      [self logTopModuleImpressionForType:ContentSuggestionsModuleType::
                                              kShortcuts];
    }
}

- (void)setMagicStackOrder:(NSArray<NSNumber*>*)order {
  CHECK([order count] > 0);
  _magicStackRankReceived = YES;
  _magicStackModuleOrder = [order mutableCopy];
  if (self.viewLoaded) {
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

- (void)showSetUpListModuleWithConfigs:(NSArray<SetUpListConfig*>*)configs {
  for (SetUpListConfig* config in configs) {
    if (config.shouldShowCompactModule) {
      _setUpListCompactedModule = [[MagicStackModuleContainer alloc] init];
      _setUpListCompactedModule.delegate = self;
      [_setUpListCompactedModule configureWithConfig:config];
      // Only add it to the Magic Stack here if it is after the inital
      // construction of the Magic Stack.
      if (_magicStackRankReceived) {
        [self insertModuleIntoMagicStack:_setUpListCompactedModule];
        [self logTopModuleImpressionForType:ContentSuggestionsModuleType::
                                                kCompactedSetUpList];
      }
    } else {
      MagicStackModuleContainer* setUpListModule =
          [[MagicStackModuleContainer alloc] init];
      setUpListModule.delegate = self;
      [setUpListModule configureWithConfig:config];
      SetUpListItemViewData* data = [config.setUpListItems firstObject];
      ContentSuggestionsModuleType type =
          SetUpListModuleTypeForSetUpListType(data.type);
      switch (type) {
        case ContentSuggestionsModuleType::kSetUpListSync:
          _setUpListSyncModule = setUpListModule;
          break;
        case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
          _setUpListDefaultBrowserModule = setUpListModule;
          break;
        case ContentSuggestionsModuleType::kSetUpListAutofill:
          _setUpListAutofillModule = setUpListModule;
          break;
        case ContentSuggestionsModuleType::kSetUpListNotifications:
          _setUpListNotificationsModule = setUpListModule;
          break;
        case ContentSuggestionsModuleType::kSetUpListAllSet:
          _setUpListAllSetModule = setUpListModule;
          break;
        default:
          break;
      }
      // Only add it to the Magic Stack here if it is after the inital
      // construction of the Magic Stack.
      if (_magicStackRankReceived) {
        [self insertModuleIntoMagicStack:setUpListModule];
        ContentSuggestionsModuleType firstItemType =
            SetUpListModuleTypeForSetUpListType(
                [config.setUpListItems firstObject].type);
        [self logTopModuleImpressionForType:firstItemType];
      }
    }
  }
}

- (void)hideSetUpListWithAnimations:(ProceduralBlock)animations {
    // Remove Modules with animation
    [self removeSetUpListItemsWithNewModule:nil];
}

- (void)showSetUpListDoneWithAnimations:(ProceduralBlock)animations {
    SetUpListItemViewData* allSetData =
        [[SetUpListItemViewData alloc] initWithType:SetUpListItemType::kAllSet
                                           complete:NO];
    allSetData.compactLayout = NO;
    allSetData.heroCellMagicStackLayout = YES;

    SetUpListConfig* config = [[SetUpListConfig alloc] init];
    config.setUpListItems = @[ allSetData ];
    MagicStackModuleContainer* allSetModule =
        [[MagicStackModuleContainer alloc] init];
    [allSetModule configureWithConfig:config];
    // Determine which module to swap out.
    [self removeSetUpListItemsWithNewModule:allSetModule];
}

// Shows the Safety Check (Magic Stack) module with `state`.
- (void)showSafetyCheck:(SafetyCheckState*)state {
  _safetyCheckState = state;
  [self.safetyCheckModuleContainer removeFromSuperview];
  self.safetyCheckModuleContainer =
      [[MagicStackModuleContainer alloc] initWithFrame:CGRectZero];
  self.safetyCheckModuleContainer.delegate = self;
  [self.safetyCheckModuleContainer configureWithConfig:_safetyCheckState];

  if (!_magicStackRankReceived) {
    return;
  }

  __block NSUInteger safetyCheckModuleOrderIndex = NSNotFound;

  [_magicStackModuleOrder enumerateObjectsUsingBlock:^(NSNumber* moduleValue,
                                                       NSUInteger idx,
                                                       BOOL* stop) {
    ContentSuggestionsModuleType type =
        (ContentSuggestionsModuleType)[moduleValue intValue];

    if (type == ContentSuggestionsModuleType::kSafetyCheck) {
      safetyCheckModuleOrderIndex = idx;

      *stop = YES;
    }
  }];

  UMA_HISTOGRAM_BOOLEAN("IOS.SafetyCheck.MagicStack.ModuleExistsInModuleOrder",
                        safetyCheckModuleOrderIndex != NSNotFound);

  if (safetyCheckModuleOrderIndex != NSNotFound) {
    [self logTopModuleImpressionForType:self.safetyCheckModuleContainer.type];

    [self insertModuleIntoMagicStack:self.safetyCheckModuleContainer];
  }
}

- (void)showTabResumptionWithItem:(TabResumptionItem*)item {
  CHECK(IsTabResumptionEnabled());
  if ([self hasMagicStackLoaded]) {
    [self logTopModuleImpressionForType:ContentSuggestionsModuleType::
                                            kTabResumption];
  }

  [_tabResumptionModuleContainer removeFromSuperview];
  _tabResumptionModuleContainer = [[MagicStackModuleContainer alloc] init];
  _tabResumptionModuleContainer.delegate = self;
  [_tabResumptionModuleContainer configureWithConfig:item];

  if (_magicStackRankReceived) {
    [self insertModuleIntoMagicStack:self.tabResumptionModuleContainer];
  }
}

- (void)hideTabResumption {
  if (!_tabResumptionModuleContainer) {
    return;
  }

  [_tabResumptionModuleContainer removeFromSuperview];
  _tabResumptionModuleContainer = nil;

  // Only remove from _magicStackModuleOrder if it has been received. Since the
  // Segmentation ranking fetch is asynchronous, it is possible for the most
  // recent local tab to be removed before receiving it.
  if (_magicStackRankReceived) {
    NSUInteger moduleIndex = [self
        indexForMagicStackModule:ContentSuggestionsModuleType::kTabResumption];
    [_magicStackModuleOrder removeObjectAtIndex:moduleIndex];
  }
}

- (void)showParcelTrackingItem:(ParcelTrackingItem*)item {
  _parcelTrackingModuleContainer = [[MagicStackModuleContainer alloc] init];
  _parcelTrackingModuleContainer.delegate = self;
  [_parcelTrackingModuleContainer configureWithConfig:item];

  if (_magicStackRankReceived) {
    [self insertModuleIntoMagicStack:_parcelTrackingModuleContainer];
  }
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
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

- (void)seeMoreWasTappedForModuleType:(ContentSuggestionsModuleType)type {
  switch (type) {
    case ContentSuggestionsModuleType::kSafetyCheck:
      [self.audience didSelectSafetyCheckItem:SafetyCheckItemType::kDefault];
      break;
    case ContentSuggestionsModuleType::kCompactedSetUpList:
      [self.audience showSetUpListShowMoreMenu];
      break;
    case ContentSuggestionsModuleType::kParcelTracking:
      [self.audience showMagicStackParcelList];
      break;
    default:
      break;
  }
}

- (void)neverShowModuleType:(ContentSuggestionsModuleType)type {
  [self.audience neverShowModuleType:type];
}

- (void)enableNotifications:(ContentSuggestionsModuleType)type {
  [self.audience enableNotifications:type];
}

- (void)disableNotifications:(ContentSuggestionsModuleType)type {
  [self.audience disableNotifications:type];
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

- (void)createAndInsertMostVisitedModule {
    if (ShouldPutMostVisitedSitesInMagicStack()) {
      // Only add it to the Magic Stack here if it is after the inital
      // construction of the Magic Stack.
      if (_magicStackRankReceived) {
        [self insertModuleIntoMagicStack:self.mostVisitedModuleContainer];
      }
    } else {
      [self.verticalStackView
          insertArrangedSubview:self.mostVisitedModuleContainer
                        atIndex:0];
      [self.verticalStackView setCustomSpacing:kMostVisitedBottomMargin
                                     afterView:self.mostVisitedModuleContainer];
      [NSLayoutConstraint activateConstraints:@[
        [self.mostVisitedModuleContainer.widthAnchor
            constraintEqualToAnchor:self.view.widthAnchor],
        [self.mostVisitedModuleContainer.centerXAnchor
            constraintEqualToAnchor:self.view.centerXAnchor],
      ]];
      [self.view layoutIfNeeded];
    }
}

// Logs `type` as the top module shown if it is first in
// `_magicStackModuleOrder`
- (void)logTopModuleImpressionForType:(ContentSuggestionsModuleType)type {
  ContentSuggestionsModuleType firstModuleType = (ContentSuggestionsModuleType)[
      [_magicStackModuleOrder objectAtIndex:0] intValue];
  if (firstModuleType == type) {
    LogTopModuleImpressionForType(type);
  }
}

// Constructs the Magic Stack module only. No modules are added in this
// implementation.
- (void)createMagicStack {
  _magicStackScrollView = [[UIScrollView alloc] init];
  [_magicStackScrollView setShowsHorizontalScrollIndicator:NO];
  _magicStackScrollView.clipsToBounds = NO;
  _magicStackScrollView.layer.cornerRadius = kMagicStackCornerRadius;
  _magicStackScrollView.delegate = self;
  _magicStackScrollView.decelerationRate = UIScrollViewDecelerationRateFast;
  _magicStackScrollView.accessibilityIdentifier =
      kMagicStackScrollViewAccessibilityIdentifier;
  _magicStackModuleLayoutGuide = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:_magicStackModuleLayoutGuide];

  [self addUIElement:_magicStackScrollView
      withCustomBottomSpacing:kMostVisitedBottomMargin];
  [self updateMagicStackMasking];

  _magicStackPage = 0;
  _magicStack = [[UIStackView alloc] init];
  _magicStack.translatesAutoresizingMaskIntoConstraints = NO;
  _magicStack.axis = UILayoutConstraintAxisHorizontal;
  _magicStack.distribution = UIStackViewDistributionEqualSpacing;
  _magicStack.spacing = kMagicStackSpacing;
  [self.layoutGuideCenter referenceView:_magicStackScrollView
                              underName:kMagicStackGuide];
  _magicStack.accessibilityIdentifier = kMagicStackViewAccessibilityIdentifier;
  // Ensures modules take up entire height of the Magic Stack.
  _magicStack.alignment = UIStackViewAlignmentFill;
  [_magicStackScrollView addSubview:_magicStack];

  AddSameConstraints(_magicStack, _magicStackScrollView);

  // Defines height, ensuring only horizontal scrolling. Instrinsic content
  // height of the StackView within the ScrollView will define the height of the
  // ScrollView.
  [NSLayoutConstraint activateConstraints:@[
    [_magicStack.heightAnchor
        constraintEqualToAnchor:_magicStackScrollView.heightAnchor],
  ]];

  // Define width of ScrollView.
  [NSLayoutConstraint activateConstraints:@[
    [_magicStackScrollView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_magicStackScrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];
}

// Resets and fills the Magic Stack with modules using `_magicStackModuleOrder`.
// This method loops over any module types listed in `_magicStackModuleOrder`
// and adds their respective views to the Magic Stack.
- (void)populateMagicStack {
    // Clear out any placeholders.
    for (UIView* view in _magicStack.arrangedSubviews) {
      [view removeFromSuperview];
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
        moduleContainer = _setUpListSyncModule;
        break;
      }
      case ContentSuggestionsModuleType::kSetUpListDefaultBrowser: {
        moduleContainer = _setUpListDefaultBrowserModule;
        break;
      }
      case ContentSuggestionsModuleType::kSetUpListAutofill: {
        moduleContainer = _setUpListAutofillModule;
        break;
      }
      case ContentSuggestionsModuleType::kSetUpListNotifications: {
        moduleContainer = _setUpListNotificationsModule;
        break;
      }
      case ContentSuggestionsModuleType::kCompactedSetUpList: {
        moduleContainer = _setUpListCompactedModule;
        break;
      }
      case ContentSuggestionsModuleType::kSetUpListAllSet: {
        moduleContainer = _setUpListAllSetModule;
        break;
      }
      case ContentSuggestionsModuleType::kSafetyCheck: {
        if (IsSafetyCheckMagicStackEnabled()) {
          moduleContainer = self.safetyCheckModuleContainer;
        }
        break;
      }
      case ContentSuggestionsModuleType::kParcelTracking:
        if (IsIOSParcelTrackingEnabled()) {
          // Add parcel tracking module if it hasn't already been added.
          if (![_parcelTrackingModuleContainer superview]) {
            moduleContainer = _parcelTrackingModuleContainer;
            break;
          }
        }
        break;
      default:
        NOTREACHED_NORETURN();
    }
    if (moduleContainer) {
      [_magicStack addArrangedSubview:moduleContainer];
      [self addWidthConstraintToMagicStackModule:moduleContainer];
      [self logTopModuleImpressionForType:moduleContainer.type];
    }
  }

  // Add Edit Button.
  UIButton* editButton = [UIButton buttonWithType:UIButtonTypeSystem];
  editButton.translatesAutoresizingMaskIntoConstraints = NO;
  UIImage* image = DefaultSymbolTemplateWithPointSize(
      kSliderHorizontalSymbol, kMagicStackEditButtonIconPointSize);
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
  [self.parcelTrackingCommandHandler showParcelTrackingIPH];
}

// Adds two placeholder modules to Magic Stack.
- (void)populateMagicStackWithPlaceholders {
  CHECK(_magicStack);
  CHECK([_magicStack.arrangedSubviews count] == 0);

  for (int i = 0; i < 2; i++) {
    PlaceholderConfig* config = [[PlaceholderConfig alloc] init];
    MagicStackModuleContainer* moduleContainer =
        [[MagicStackModuleContainer alloc] init];
    [moduleContainer configureWithConfig:config];
    [_magicStack addArrangedSubview:moduleContainer];
  }
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
  if (!_magicStack || !_magicStackRankReceived) {
    // If the MagicStack hasn't been instantiated yet or ranking has not been
    // received yet, the module will be inserted later.
    return;
  }

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
  [self addWidthConstraintToMagicStackModule:moduleToInsert];
}

// Returns the current width of MagicStack modules.
- (CGFloat)magicStackModuleWidth {
  return _magicStackModuleLayoutGuide.layoutFrame.size.width;
}

// Returns the `ContentSuggestionsModuleType` type of the module being currently
// shown in the Magic Stack.
- (ContentSuggestionsModuleType)currentlyShownModule {
  CGFloat offset = _magicStackScrollView.contentOffset.x;
  CGFloat moduleWidth = [self magicStackModuleWidth];
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
      case ContentSuggestionsModuleType::kSetUpListNotifications:
      case ContentSuggestionsModuleType::kCompactedSetUpList:
        [viewIndicesToRemove addObject:@(index)];
        break;
      default:
        break;
    }
    index++;
  }

  __weak __typeof(self) weakSelf = self;
  CGFloat moduleWidth = [self magicStackModuleWidth];
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
    [viewIndicesToRemove removeLastObject];
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
  __weak __typeof(_magicStack) weakMagicStack = _magicStack;

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
        [weakMagicStack removeArrangedSubview:moduleToHide];
        [weakMagicStack insertArrangedSubview:newModule atIndex:index];
        [strongSelf addWidthConstraintToMagicStackModule:newModule];

        newModule.transform = CGAffineTransformTranslate(
            CGAffineTransformIdentity,
            -kMagicStackReplaceModuleFadeAnimationDistance, 0);
        [moduleToHide removeFromSuperview];
        [weakMagicStack setNeedsLayout];
        [weakMagicStack layoutIfNeeded];

        animateInNewModule();
      }];
}

// Returns the extra offset needed to have a MagicStack module be left, center,
// or right aligned depending on whether the module is first, in the middle, or
// last.
- (CGFloat)peekOffsetForMagicStackPage:(NSUInteger)page {
  if (page == 0) {
    // The first module should be leading aligned so that the next module peeks
    // in from the trailing edge.
    return 0;
  } else if (page == [_magicStackModuleOrder count] - 1) {
    // The last module should be trailing aligned so the previous module peeks.
    return [self magicStackPeekInset];
  } else {
    // Modules in the middle should show peek on both sides.
    return [self magicStackPeekInset] / 2.0;
  }
}

// Determines the final page offset given the scroll `offset` and the `velocity`
// scroll. If the drag is slow enough, then the closest page is the final state.
// If the drag is in the negative direction, then go to the page previous to the
// closest current page. If the drag is in the positive direction, then go to
// the page after the closest current page.
- (CGFloat)getNextPageOffsetForOffset:(CGFloat)offset
                             velocity:(CGFloat)velocity {
  CGFloat moduleWidth = [self magicStackModuleWidth];
  NSUInteger moduleCount = [_magicStackModuleOrder count];

  // Find closest page to the current scroll offset.
  CGFloat closestPage = roundf(offset / moduleWidth);
  closestPage = fminf(closestPage, moduleCount);

  if (fabs(velocity) >= kMagicStackMinimumPaginationScrollVelocity) {
    if (velocity < 0) {
      closestPage--;
    } else {
      closestPage++;
    }
  }
  _magicStackPage = closestPage;
  return _magicStackPage * (moduleWidth + kMagicStackSpacing) -
         [self peekOffsetForMagicStackPage:_magicStackPage];
}

// Snaps the MagicStack ScrollView's contentOffset to the nearest module. Can
// be used after the width of the MagicStack changes to ensure that it doesn't
// end up scrolled to the middle of a module.
- (void)snapToNearestMagicStackModule {
  CGFloat moduleWidth = [self magicStackModuleWidth];
  CGPoint offset = _magicStackScrollView.contentOffset;
  offset.x = _magicStackPage * (moduleWidth + kMagicStackSpacing) -
             [self peekOffsetForMagicStackPage:_magicStackPage];
  // Do not allow scrolling beyond the end of content, which also ensures that
  // the "edit menu" page doesn't end up left-aligned after a rotation.
  CGFloat maxOffset = MAX(0, _magicStackScrollView.contentSize.width -
                                 _magicStackScrollView.bounds.size.width);
  offset.x = MIN(offset.x, maxOffset);
  _magicStackScrollView.contentOffset = offset;
}

// Returns YES if the MagicStack should be masked so that modules only peek in
// from the sides. This is needed in landscape and on iPads.
- (BOOL)shouldMaskMagicStack {
  return self.traitCollection.horizontalSizeClass ==
             UIUserInterfaceSizeClassRegular ||
         IsLandscape(self.view.window);
}

// Returns the amount that MagicStack modules are narrower than the ScrollView,
// in order to allow peeking at the sides.
- (CGFloat)magicStackPeekInset {
  return [self shouldMaskMagicStack] ? kMagicStackPeekInsetLandscape
                                     : kMagicStackPeekInset;
}

// In landscape and iPad the MagicStack masks content outside of its
// ScrollView, and the module peeking at the sides happens inside that width.
// In portrait on iPhone, the ScrollView doesn't mask the content, and peeking
// happens outside to the edge of the screen.
- (void)updateMagicStackMasking {
  _magicStackModuleWidth.active = NO;
  _magicStackModuleWidth = [_magicStackModuleLayoutGuide.widthAnchor
      constraintEqualToAnchor:_magicStackScrollView.widthAnchor
                     constant:-[self magicStackPeekInset]];
  _magicStackScrollView.clipsToBounds = [self shouldMaskMagicStack];
  _magicStackModuleWidth.active = YES;
}

// Adds a layout constraint on the width of the given MagicStack module
// `container`.
- (void)addWidthConstraintToMagicStackModule:
    (MagicStackModuleContainer*)container {
  [container.widthAnchor
      constraintEqualToAnchor:_magicStackModuleLayoutGuide.widthAnchor]
      .active = YES;
}
@end
