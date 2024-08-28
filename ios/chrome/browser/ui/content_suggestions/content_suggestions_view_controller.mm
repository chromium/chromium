// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"

#import "ios/chrome/browser/drag_and_drop/model/url_drag_drop_handler.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_config.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_image_data_source.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_container.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"

@interface ContentSuggestionsViewController () <
    UIGestureRecognizerDelegate,
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

@end

@implementation ContentSuggestionsViewController {
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
                       constant:
                           (IsHomeCustomizationEnabled()
                                ? 0
                                : content_suggestions::HeaderBottomPadding())],
    [self.verticalStackView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor
                       constant:(IsHomeCustomizationEnabled()
                                     ? 0
                                     : -kBottomMagicStackPadding)]
  ]];

  if (_mostVisitedTileConfig && !ShouldPutMostVisitedSitesInMagicStack()) {
    [self createAndInsertMostVisitedModule];
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

    [self.contentSuggestionsMetricsRecorder recordMostVisitedTilesShown];
}

#pragma mark - Private

- (void)addUIElement:(UIView*)view withCustomBottomSpacing:(CGFloat)spacing {
  [self.verticalStackView addArrangedSubview:view];
  if (spacing > 0) {
    [self.verticalStackView setCustomSpacing:spacing afterView:view];
  }
}

- (void)createAndInsertMostVisitedModule {
  if (!ShouldPutMostVisitedSitesInMagicStack()) {
    [self.verticalStackView
        insertArrangedSubview:self.mostVisitedModuleContainer
                      atIndex:0];
    [self.verticalStackView setCustomSpacing:(IsHomeCustomizationEnabled()
                                                  ? 0
                                                  : kMostVisitedBottomMargin)
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

@end
