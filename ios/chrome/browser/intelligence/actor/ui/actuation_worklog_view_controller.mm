// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_controller.h"

#import "base/notreached.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_suggestions_presenter.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_compact_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_consumer.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_mutator.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data_builders.h"
#import "ios/chrome/browser/intelligence/ui/intelligence_header_configuration.h"
#import "ios/chrome/browser/intelligence/ui/intelligence_header_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Text used to measure the header as if it displayed a title and a subtitle.
NSString* const kHeaderSizingPlaceholderText = @"Placeholder";

// Returns the header density for `mode`. The minimized header is condensed so
// that a title and a subtitle fit in about the same height as a title-only
// header.
IntelligenceHeaderDensity HeaderDensityForDisplayMode(
    ActuationWorklogDisplayMode mode) {
  return mode == ActuationWorklogDisplayModeMinimized
             ? IntelligenceHeaderDensity::kCondensed
             : IntelligenceHeaderDensity::kRegular;
}

}  // namespace

@interface ActuationWorklogViewController () <
    ActuationWorklogCompactViewDelegate,
    IntelligenceHeaderViewDelegate>
@end

@implementation ActuationWorklogViewController {
  // Header view that sits right above the worklog.
  // TODO(crbug.com/583897274): Move ownership of `_headerView` to
  // `GeminiContainerViewController` so a single persistent header instance is
  // shared across container states.
  IntelligenceHeaderView* _headerView;
  // Current step layout. Visible in `ActuationWorklogDisplayModeCompact`.
  ActuationWorklogCompactView* _compactView;
  // Height of `_compactView`. Adjusted dynamically when its content changes.
  NSLayoutConstraint* _compactHeightConstraint;
  // Mode-specific header content applied to `_headerView`.
  ActuationHeaderContentVariants* _headerContentVariants;
  // Offscreen header for content-independent height calculation.
  IntelligenceHeaderView* _sizingHeaderView;

  BOOL _actuationActive;
}

#pragma mark - Public

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _displayMode = ActuationWorklogDisplayModeCompact;
    _actuationActive = NO;
  }
  return self;
}

- (void)setDisplayMode:(ActuationWorklogDisplayMode)displayMode {
  if (_displayMode == displayMode) {
    return;
  }
  _displayMode = displayMode;
  [self updateHeader];
  [self updateVisibility];
}

- (CGFloat)heightForDisplayMode:(ActuationWorklogDisplayMode)mode {
  CGFloat headerHeight = [self headerHeightForDisplayMode:mode];
  switch (mode) {
    case ActuationWorklogDisplayModeMinimized:
    case ActuationWorklogDisplayModeExpanded:
      return headerHeight;
    case ActuationWorklogDisplayModeCompact:
      return headerHeight + _compactHeightConstraint.constant;
  }
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  [self setupSubviews];
  [self setupConstraints];
  [self updateHeader];
  [self updateVisibility];
}

#pragma mark - ActuationWorklogConsumer

- (void)setActuationActive:(BOOL)active {
  if (_actuationActive == active) {
    return;
  }
  _actuationActive = active;
  [self updateHeader];
}

- (void)setHeaderContentVariants:(ActuationHeaderContentVariants*)variants {
  _headerContentVariants = variants;
  [self updateHeader];
}

- (void)updateWorklogWithItem:(ActuationWorklogItem*)item
                         chip:(ActuationWorklogChip*)chip
                     animated:(BOOL)animated {
  if (!item) {
    return;
  }
  [_compactView transitionToItem:item chip:chip animated:animated];
}

- (void)setLastItem:(ActuationWorklogItem*)item
               chip:(ActuationWorklogChip*)chip {
  [_compactView transitionToItem:item chip:chip animated:NO];
  [self notifyHeightDidChange];
}

- (void)reset {
  _headerContentVariants = nil;
  [self updateHeader];
  [_compactView reset];
  _compactHeightConstraint.constant = 0.0;
  [self notifyHeightDidChange];
}

#pragma mark - ActuationWorklogCompactViewDelegate

- (void)worklogCompactView:(ActuationWorklogCompactView*)view
           didChangeHeight:(CGFloat)targetHeight {
  _compactHeightConstraint.constant = targetHeight;
  [self notifyHeightDidChange];
}

#pragma mark - ActuationWorklogItemViewDelegate

- (void)worklogItemViewDidTapItem:(ActuationWorklogItemView*)itemView {
  // Compact worklog items are not collapsible.
}

- (void)worklogItemView:(ActuationWorklogItemView*)itemView
    didTapAccessoryItem:(ActuationWorklogAccessoryItem*)accessoryItem {
  [self.suggestionsPresenter showSuggestionsPickerForAccessory:accessoryItem];
}

#pragma mark - Private

// Creates the view hierarchy.
- (void)setupSubviews {
  _headerView = [[IntelligenceHeaderView alloc] initWithFrame:CGRectZero];
  _headerView.accessibilityIdentifier = kActuationHeaderAccessibilityIdentifier;
  _headerView.delegate = self;
  _headerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_headerView];

  _compactView = [[ActuationWorklogCompactView alloc] init];
  _compactView.accessibilityIdentifier = kCompactWorklogAccessibilityIdentifier;
  _compactView.delegate = self;
  _compactView.itemDelegate = self;
  _compactView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_compactView];
}

// Configures layout constraints.
- (void)setupConstraints {
  _compactHeightConstraint =
      [_compactView.heightAnchor constraintEqualToConstant:0.0];

  AddSameConstraintsToSides(_headerView, self.view,
                            LayoutSides::kTop | LayoutSides::kHorizontal);
  AddSameConstraintsToSides(_compactView, self.view, LayoutSides::kHorizontal);

  [NSLayoutConstraint activateConstraints:@[
    [_compactView.topAnchor constraintEqualToAnchor:_headerView.bottomAnchor],
    _compactHeightConstraint,
  ]];
}

// Applies the current header configuration for `displayMode` to `_headerView`.
- (void)updateHeader {
  if (!_headerView) {
    return;
  }
  [_headerView applyConfiguration:
                   [self headerConfigurationForDisplayMode:self.displayMode]];
  // The header's height depends on its content.
  // TODO(crbug.com/583897274): Remove once the host owns the header.
  [self notifyHeightDidChange];
}

// Builds the header configuration for `mode`, showing the close button when no
// content is available.
- (IntelligenceHeaderConfiguration*)headerConfigurationForDisplayMode:
    (ActuationWorklogDisplayMode)mode {
  ActuationHeaderContent* content =
      [_headerContentVariants contentForDisplayMode:mode];
  IntelligenceHeaderActivity activity =
      _actuationActive ? IntelligenceHeaderActivity::kActive
                       : IntelligenceHeaderActivity::kIdle;
  return [[IntelligenceHeaderConfiguration alloc]
      initWithLeadingIcon:nil
                    title:content.title
                 subtitle:content.subtitle
                 activity:activity
                  density:HeaderDensityForDisplayMode(mode)
              primaryItem:content ? content.primaryItem
                                  : ActuationHeaderCloseItem()
           secondaryItems:nil];
}

// Updates visibility of the subviews for `_displayMode`. The view's own
// visibility is owned by the host.
- (void)updateVisibility {
  _compactView.hidden = _displayMode != ActuationWorklogDisplayModeCompact;
}

// Notifies the delegate that the fitting heights may have changed.
- (void)notifyHeightDidChange {
  [self.delegate worklogViewControllerDidChangeHeight:self];
}

// Returns the header height for any given `mode`. A sizing view ensures we do
// not mutate `_headerView`. The minimized height fits both title and subtitle
// without clippping in the minimized display mode.
- (CGFloat)headerHeightForDisplayMode:(ActuationWorklogDisplayMode)mode {
  if (!_sizingHeaderView) {
    _sizingHeaderView =
        [[IntelligenceHeaderView alloc] initWithFrame:CGRectZero];
  }
  [_sizingHeaderView
      applyConfiguration:[self headerConfigurationForDisplayMode:mode]];
  _sizingHeaderView.activity = IntelligenceHeaderActivity::kIdle;
  if (mode == ActuationWorklogDisplayModeMinimized) {
    _sizingHeaderView.title = kHeaderSizingPlaceholderText;
    _sizingHeaderView.subtitle = kHeaderSizingPlaceholderText;
  }
  return [self fittingHeightForView:_sizingHeaderView
                        targetWidth:self.view.bounds.size.width];
}

// Calculates fitting height for `view` constrained to `targetWidth`.
- (CGFloat)fittingHeightForView:(UIView*)view targetWidth:(CGFloat)targetWidth {
  if (targetWidth <= 0) {
    return
        [view systemLayoutSizeFittingSize:UILayoutFittingCompressedSize].height;
  }
  CGSize targetSize =
      CGSizeMake(targetWidth, UILayoutFittingCompressedSize.height);
  return [view systemLayoutSizeFittingSize:targetSize
             withHorizontalFittingPriority:UILayoutPriorityRequired
                   verticalFittingPriority:UILayoutPriorityFittingSizeLevel]
      .height;
}

#pragma mark - IntelligenceHeaderViewDelegate

- (void)headerView:(IntelligenceHeaderView*)headerView
    didTriggerItemWithIdentifier:(NSString*)identifier {
  if ([identifier isEqualToString:kActuationHeaderPauseItemIdentifier]) {
    [self.mutator pauseActuation];
    return;
  }
  if ([identifier isEqualToString:kActuationHeaderCloseItemIdentifier]) {
    [self.mutator stopActuation];
    return;
  }
  NOTREACHED();
}

@end
