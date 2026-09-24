// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_view_controller.h"

#import "ios/chrome/browser/content_suggestions/model/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_item.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_collection_view.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_config.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_image_data_source.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/drag_and_drop/model/url_drag_drop_handler.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/ntp_card_background_view.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Corner radius for the most visited tiles container.
constexpr CGFloat kModuleContainerCornerRadius = 24.0;

// Bottom padding for the most visited tiles container.
constexpr CGFloat kMVTContainerBottomPadding = 10.0;
// TODO(crbug.com/542594099): Remove "UICleanup" suffix when the NTP clean up is
// launched.
constexpr CGFloat kMVTContainerBottomPaddingUICleanup = 16.0;

// Spacing between content suggestions modules in the stack view.
constexpr CGFloat kStackViewSpacing = 12.0;

}  // namespace

@interface ContentSuggestionsViewController () <UIGestureRecognizerDelegate,
                                                URLDropDelegate>

@property(nonatomic, strong) URLDragDropHandler* dragDropHandler;

@end

@implementation ContentSuggestionsViewController {
  // Most Visited tiles collection displayed as a module in the
  // `_contentSuggestionsModuleStackView`.
  UIView* _mostVisitedView;
  // Horizontal stack view containing content suggestions modules.
  UIStackView* _contentSuggestionsModuleStackView;
}

- (instancetype)init {
  return [super initWithNibName:nil bundle:nil];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.dragDropHandler = [[URLDragDropHandler alloc] init];
  self.dragDropHandler.dropDelegate = self;
  [self.view addInteraction:[[UIDropInteraction alloc]
                                initWithDelegate:self.dragDropHandler]];
  self.view.backgroundColor = [UIColor clearColor];
  self.view.accessibilityIdentifier = kContentSuggestionsCollectionIdentifier;

  _contentSuggestionsModuleStackView = [self createModuleStackView];
  _contentSuggestionsModuleStackView.translatesAutoresizingMaskIntoConstraints =
      NO;
  [self.view addSubview:_contentSuggestionsModuleStackView];

  [NSLayoutConstraint activateConstraints:@[
    [_contentSuggestionsModuleStackView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_contentSuggestionsModuleStackView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_contentSuggestionsModuleStackView.topAnchor
        constraintEqualToAnchor:self.view.topAnchor
                       constant:content_suggestions::HeaderBottomPadding(
                                    self.traitCollection)],
    [_contentSuggestionsModuleStackView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
  ]];

  if (_mostVisitedView) {
    [self embedMostVisitedView];
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
  if (_mostVisitedView) {
    [_mostVisitedView removeFromSuperview];
  }
  if (!config) {
    _mostVisitedView = nil;
    return;
  }

  MostVisitedTilesCollectionView* collectionView =
      [[MostVisitedTilesCollectionView alloc] initWithConfig:config];

  _mostVisitedView =
      [self createContainerForContentSuggestionsModule:collectionView];

  if (self.isViewLoaded) {
    [self embedMostVisitedView];
  }

  for (MostVisitedItem* item in config.mostVisitedItems) {
    [ContentSuggestionsMetricsRecorder recordMostVisitedTileShown:item
                                                          atIndex:item.index];
  }

  [ContentSuggestionsMetricsRecorder recordMostVisitedTilesShown];
}

#pragma mark - Private

// Adds the most visited tiles to the `_contentSuggestionsModuleStackView`.
- (void)embedMostVisitedView {
  if (!_mostVisitedView || !_contentSuggestionsModuleStackView) {
    return;
  }
  [_contentSuggestionsModuleStackView addArrangedSubview:_mostVisitedView];
  // Force layout to make sure the subviews correctly calculates its frame size.
  [self.view setNeedsLayout];
  [self.view layoutIfNeeded];
}

// Creates a horizontal stack view for content suggestions modules.
- (UIStackView*)createModuleStackView {
  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.distribution = UIStackViewDistributionFill;
  stackView.alignment = UIStackViewAlignmentFill;
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.spacing = kStackViewSpacing;
  return stackView;
}

// Returns a container holding the given content suggestions `moduleView`.
- (UIView*)createContainerForContentSuggestionsModule:(UIView*)moduleView {
  UIView* container = [[UIView alloc] init];
  container.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* backgroundView = [[NTPCardBackgroundView alloc] init];
  backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  container.layer.cornerRadius = kModuleContainerCornerRadius;
  container.clipsToBounds = YES;

  [container addSubview:backgroundView];
  AddSameConstraints(container, backgroundView);

  [container addSubview:moduleView];
  CGFloat bottomPadding = IsNewTabPageUICleanupEnabled()
                              ? kMVTContainerBottomPaddingUICleanup
                              : kMVTContainerBottomPadding;
  [NSLayoutConstraint activateConstraints:@[
    [moduleView.topAnchor constraintEqualToAnchor:container.topAnchor],
    [moduleView.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
    [moduleView.trailingAnchor
        constraintEqualToAnchor:container.trailingAnchor],
    [moduleView.bottomAnchor constraintEqualToAnchor:container.bottomAnchor
                                            constant:-bottomPadding],
  ]];
  return container;
}

@end
