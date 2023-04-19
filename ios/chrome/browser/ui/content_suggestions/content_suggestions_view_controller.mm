// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/drag_and_drop/url_drag_drop_handler.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/action_list_module.h"
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
#import "ios/chrome/browser/ui/content_suggestions/cells/query_suggestion_view.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_menu_provider.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The bottom padding for the vertical stack view.
const float kBottomStackViewPadding = 6.0f;

}  // namespace

@interface ContentSuggestionsViewController () <
    UIGestureRecognizerDelegate,
    ContentSuggestionsSelectionActions,
    URLDropDelegate>

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
// Width Anchor of the Most Visited Tiles container.
@property(nonatomic, strong)
    NSLayoutConstraint* mostVisitedContainerWidthAnchor;
// List of all of the Most Visited views.
@property(nonatomic, strong)
    NSMutableArray<ContentSuggestionsMostVisitedTileView*>* mostVisitedViews;
// Module Container for the Shortcuts when being shown in Magic Stack.
@property(nonatomic, strong) ActionListModule* shortcutsModuleContainer;
// StackView holding all of `shortcutsViews`.
@property(nonatomic, strong) UIStackView* shortcutsStackView;
// List of all of the Shortcut views.
@property(nonatomic, strong)
    NSMutableArray<ContentSuggestionsShortcutTileView*>* shortcutsViews;
@end

@implementation ContentSuggestionsViewController {
  UIScrollView* _magicStackScrollView;
  BOOL _shouldShowMagicStack;
  NSArray<NSNumber*>* _magicStackModuleOrder;
  NSLayoutConstraint* _magicStackScrollViewWidthAnchor;
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
  self.view.backgroundColor = ntp_home::NTPBackgroundColor();
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
  // Add bottom spacing to last module by applying it after
  // `_verticalStackView`. If ShouldMinimizeSpacingForModuleRefresh() is YES,
  // then no space is added after the last module.

  // Add bottom spacing to the last module by applying it after
  // `_verticalStackView`. If `IsContentSuggestionsUIModuleRefreshEnabled()` is
  // YES, and ShouldMinimizeSpacingForModuleRefresh() is YES, then no space is
  // added after the last module. Otherwise we add kModuleVerticalSpacing. If
  // `IsContentSuggestionsUIModuleRefreshEnabled()` is NO, then we add
  // `kBottomStackViewPadding`
  CGFloat bottomSpacing = kBottomStackViewPadding;
  [NSLayoutConstraint activateConstraints:@[
    [self.verticalStackView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.verticalStackView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.verticalStackView.topAnchor
        constraintEqualToAnchor:self.view.topAnchor],
    [self.verticalStackView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor
                       constant:-bottomSpacing]
  ]];

  CGFloat horizontalSpacing =
      ContentSuggestionsTilesHorizontalSpacing(self.traitCollection);
  if (self.returnToRecentTabTile) {
    UIView* parentView = self.returnToRecentTabTile;
    [self addUIElement:self.returnToRecentTabTile
        withCustomBottomSpacing:content_suggestions::
                                    kReturnToRecentTabSectionBottomMargin];
    CGFloat cardWidth = content_suggestions::SearchFieldWidth(
        self.view.bounds.size.width, self.traitCollection);
    [NSLayoutConstraint activateConstraints:@[
      [parentView.widthAnchor constraintEqualToConstant:cardWidth],
      [parentView.heightAnchor
          constraintEqualToConstant:ReturnToRecentTabHeight()]
    ]];
  }
  if ([self.mostVisitedViews count] > 0) {
    self.mostVisitedStackView = [[UIStackView alloc] init];
    self.mostVisitedStackView.axis = UILayoutConstraintAxisHorizontal;
    self.mostVisitedStackView.distribution = UIStackViewDistributionFillEqually;
    self.mostVisitedStackView.spacing = horizontalSpacing;
    self.mostVisitedStackView.alignment = UIStackViewAlignmentTop;
    [self addUIElement:self.mostVisitedStackView
        withCustomBottomSpacing:kMostVisitedBottomMargin];
    CGFloat width =
        MostVisitedTilesContentHorizontalSpace(self.traitCollection);
    CGFloat height =
        MostVisitedCellSize(self.traitCollection.preferredContentSizeCategory)
            .height;
    [NSLayoutConstraint activateConstraints:@[
      [self.mostVisitedStackView.widthAnchor constraintEqualToConstant:width],
      [self.mostVisitedStackView.heightAnchor
          constraintGreaterThanOrEqualToConstant:height]
    ]];
    [self populateMostVisitedModule];
  }
  if (self.shortcutsViews) {
    self.shortcutsStackView = [self createShortcutsStackView];
    if (!_shouldShowMagicStack) {
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

  if (_shouldShowMagicStack) {
    CHECK(IsMagicStackEnabled());
    [self createMagicStack];
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  if (ShouldShowReturnToMostRecentTabForStartSurface()) {
    [self.audience viewWillDisappear];
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
    UIView* parentView = self.returnToRecentTabTile;
    [self.verticalStackView insertArrangedSubview:self.returnToRecentTabTile
                                          atIndex:0];
    [self.verticalStackView
        setCustomSpacing:content_suggestions::
                             kReturnToRecentTabSectionBottomMargin
               afterView:self.returnToRecentTabTile];
    CGFloat cardWidth = content_suggestions::SearchFieldWidth(
        self.view.bounds.size.width, self.traitCollection);
    [NSLayoutConstraint activateConstraints:@[
      [parentView.widthAnchor constraintEqualToConstant:cardWidth],
      [parentView.heightAnchor
          constraintEqualToConstant:ReturnToRecentTabHeight()]
    ]];
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
  [self populateMostVisitedModule];
  [self.contentSuggestionsMetricsRecorder recordMostVisitedTilesShown];
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
  _shouldShowMagicStack = YES;
  _magicStackModuleOrder = order;
}

- (CGFloat)contentSuggestionsHeight {
  CGFloat height = 0;
  if ([self.mostVisitedViews count] > 0) {
    height += MostVisitedCellSize(
                  UIApplication.sharedApplication.preferredContentSizeCategory)
                  .height +
              kMostVisitedBottomMargin;
  }
  if (_shouldShowMagicStack) {
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
  return height;
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
  if (previousTraitCollection.horizontalSizeClass !=
      self.traitCollection.horizontalSizeClass) {
    _magicStackScrollViewWidthAnchor.constant = [MagicStackModuleContainer
        moduleWidthForHorizontalTraitCollection:self.traitCollection];
  }
}

#pragma mark - Private

- (void)addUIElement:(UIView*)view withCustomBottomSpacing:(CGFloat)spacing {
  [self.verticalStackView addArrangedSubview:view];
  if (spacing > 0) {
    [self.verticalStackView setCustomSpacing:spacing afterView:view];
  }
}

// Add the elements in `mostVisitedViews` into `verticalStackView`, constructing
// `verticalStackView` beforehand if it has not been yet.
- (void)populateMostVisitedModule {
  // If viewDidLoad has been called before the first valid Most Visited Tiles
  // are available, construct `mostVisitedStackView`.
  if (self.verticalStackView && !self.mostVisitedStackView) {
    self.mostVisitedStackView = [[UIStackView alloc] init];
    self.mostVisitedStackView.axis = UILayoutConstraintAxisHorizontal;
    self.mostVisitedStackView.alignment = UIStackViewAlignmentTop;
    self.mostVisitedStackView.distribution = UIStackViewDistributionFillEqually;
    self.mostVisitedStackView.spacing =
        ContentSuggestionsTilesHorizontalSpacing(self.traitCollection);
    // Find correct insertion position in the stack.
    int insertionIndex = 0;
    if (self.returnToRecentTabTile) {
      insertionIndex++;
    }
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

- (void)createMagicStack {
  CGFloat width = [MagicStackModuleContainer
      moduleWidthForHorizontalTraitCollection:self.traitCollection];
  _magicStackScrollView = [[UIScrollView alloc] init];
  [_magicStackScrollView setShowsHorizontalScrollIndicator:NO];
  _magicStackScrollView.clipsToBounds = NO;
  [self addUIElement:_magicStackScrollView
      withCustomBottomSpacing:kMostVisitedBottomMargin];

  UIStackView* magicStack = [[UIStackView alloc] init];
  magicStack.translatesAutoresizingMaskIntoConstraints = NO;
  magicStack.axis = UILayoutConstraintAxisHorizontal;
  magicStack.distribution = UIStackViewDistributionEqualSpacing;
  magicStack.spacing = 10;
  magicStack.alignment = UIStackViewAlignmentCenter;
  [_magicStackScrollView addSubview:magicStack];

  // Add Magic Stack modules in order dictated by `_magicStackModuleOrder`.
  for (NSNumber* moduleType in _magicStackModuleOrder) {
    ContentSuggestionsModuleType type =
        (ContentSuggestionsModuleType)[moduleType intValue];
    switch (type) {
      case ContentSuggestionsModuleType::kShortcuts:
        self.shortcutsModuleContainer = [[ActionListModule alloc]
            initWithContentView:self.shortcutsStackView
                           type:type];
        [magicStack addArrangedSubview:self.shortcutsModuleContainer];
        break;
      default:
        break;
    }
  }
  AddSameConstraints(magicStack, _magicStackScrollView);
  // Define width of ScrollView. Instrinsic content height of the
  // StackView within the ScrollView will define the height of the
  // ScrollView.
  _magicStackScrollViewWidthAnchor =
      [_magicStackScrollView.widthAnchor constraintEqualToConstant:width];
  [NSLayoutConstraint activateConstraints:@[
    // Ensures only horizontal scrolling
    [magicStack.heightAnchor
        constraintEqualToAnchor:_magicStackScrollView.heightAnchor],
    _magicStackScrollViewWidthAnchor
  ]];
}

@end
