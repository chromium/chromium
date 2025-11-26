// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_view_controller.h"

#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/most_visited_tiles_config.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_image_data_source.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_module_container.h"
#import "ios/chrome/browser/drag_and_drop/model/url_drag_drop_handler.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"

@interface ContentSuggestionsViewController () <
    UIGestureRecognizerDelegate,
    URLDropDelegate,
    UIScrollViewDelegate,
    UIScrollViewAccessibilityDelegate>

@property(nonatomic, strong) URLDragDropHandler* dragDropHandler;

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
  if (_mostVisitedTileConfig.mostVisitedItems.count > 0) {
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
  self.mostVisitedModuleContainer = [[MagicStackModuleContainer alloc]
      initWithFrame:CGRectZero
            noInset:IsContentSuggestionsCustomizable()];
  [self.mostVisitedModuleContainer configureWithConfig:_mostVisitedTileConfig];
  // If viewDidLoad has been called before the first valid Most Visited Tiles
  // are available, construct the most visited tiles.
  if (_mostVisitedTileConfig.mostVisitedItems.count > 0) {
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

- (void)createAndInsertMostVisitedModule {
  [self.view addSubview:self.mostVisitedModuleContainer];
  self.mostVisitedModuleContainer.translatesAutoresizingMaskIntoConstraints =
      NO;
  [NSLayoutConstraint activateConstraints:@[
    [self.mostVisitedModuleContainer.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.mostVisitedModuleContainer.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.mostVisitedModuleContainer.topAnchor
        constraintEqualToAnchor:self.view.topAnchor
                       constant:content_suggestions::HeaderBottomPadding(
                                    self.traitCollection)],
    [self.mostVisitedModuleContainer.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor]
  ]];
  [self.view layoutIfNeeded];
}

@end
