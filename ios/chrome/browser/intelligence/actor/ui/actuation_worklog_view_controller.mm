// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_header_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_intervention_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_compact_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_consumer.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_mutator.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Point size for the header close button symbol.
constexpr CGFloat kCloseButtonPointSize = 14.0;

}  // namespace

using intelligence::actor::kSpacingLarge;

@interface ActuationWorklogViewController () <
    ActuationInterventionViewDelegate,
    ActuationWorklogCompactViewDelegate>
@end

@implementation ActuationWorklogViewController {
  // Header view that sits right above the worklog.
  ActuationHeaderView* _headerView;
  // Current step layout. Visible in `ActuationWorklogDisplayModeCompact`.
  ActuationWorklogCompactView* _compactView;
  // Timeline view embedded inside a scrollview to support vertical growth as
  // steps are added.
  ActuationWorklogView* _fullView;
  // Scrollable container for the `_fullView`. Visible in
  // `ActuationWorklogDisplayModeExpanded`.
  UIScrollView* _scrollView;
  // Interactive intervention UI pinned to the bottom of the worklog.
  ActuationInterventionView* _interventionView;
  // Height of `_compactView`. Adjusted dynamically when its content changes.
  NSLayoutConstraint* _compactHeightConstraint;

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
  [self updateVisibility];
}

- (CGFloat)heightForDisplayMode:(ActuationWorklogDisplayMode)mode {
  CGFloat width = self.view.bounds.size.width;
  // `_interventionView` owns its padding and collapses to zero height when no
  // intervention is active.
  CGFloat interventionHeight = [self fittingHeightForView:_interventionView
                                              targetWidth:width];
  switch (mode) {
    case ActuationWorklogDisplayModeMinimized:
      return [self headerHeight];
    case ActuationWorklogDisplayModeCompact:
      return [self headerHeight] + _compactHeightConstraint.constant +
             interventionHeight;
    case ActuationWorklogDisplayModeExpanded:
      // Height needed to show the full timeline without scrolling, including
      // the bottom inset applied to `_fullView` in its scroll view.
      return [self headerHeight] +
             [self fittingHeightForView:_fullView targetWidth:width] +
             kSpacingLarge + interventionHeight;
  }
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  [self setupSubviews];
  [self setupConstraints];
  [self updateVisibility];
}

#pragma mark - ActuationWorklogConsumer

- (void)setActuationActive:(BOOL)active {
  if (_actuationActive == active) {
    return;
  }
  _actuationActive = active;
  _headerView.actuating = active;
  [self updateVisibility];
}

- (void)setTaskTitle:(NSString*)taskTitle {
  _headerView.title = [taskTitle copy];
}

- (void)updateWorklogWithItem:(ActuationWorklogItem*)item
                         chip:(ActuationWorklogChip*)chip
                     animated:(BOOL)animated {
  if (!item) {
    return;
  }
  [_fullView addItem:item];
  [_fullView setChip:chip];
  [_compactView transitionToItem:item chip:chip animated:animated];
  [self scrollToBottomAnimated:animated];
}

- (void)setIntervention:(ActuationInterventionData*)intervention {
  [_interventionView configureWithData:intervention];
  [self notifyHeightDidChange];
}

- (void)reset {
  [_headerView reset];
  _headerView.primaryItem = [self createCloseItem];
  [_compactView reset];
  [_fullView reset];
  [_scrollView setContentOffset:CGPointZero animated:NO];
  _compactHeightConstraint.constant = 0.0;
  [self setIntervention:nil];
}

#pragma mark - ActuationWorklogCompactViewDelegate

- (void)worklogCompactView:(ActuationWorklogCompactView*)view
           didChangeHeight:(CGFloat)targetHeight {
  _compactHeightConstraint.constant = targetHeight;
  [self notifyHeightDidChange];
}

#pragma mark - Private

// Handles close button tap to request stopping the active actuation task.
- (void)stopActuationButtonTapped {
  [self.mutator stopActuation];
}

// Creates the close button accessory for `_headerView`.
- (ActuationHeaderItem*)createCloseItem {
  __weak __typeof(self) weakSelf = self;
  UIAction* closeAction = [UIAction actionWithHandler:^(UIAction*) {
    [weakSelf stopActuationButtonTapped];
  }];
  return [[ActuationHeaderItem alloc]
                 initWithIcon:SymbolWithPointSize(SymbolXMark,
                                                  kCloseButtonPointSize)
                        title:l10n_util::GetNSString(IDS_CLOSE)
      accessibilityIdentifier:kActuationHeaderCloseButtonAccessibilityIdentifier
                       action:closeAction];
}

// Creates the view hierarchy.
- (void)setupSubviews {
  _headerView = [[ActuationHeaderView alloc] initWithFrame:CGRectZero];
  _headerView.accessibilityIdentifier = kActuationHeaderAccessibilityIdentifier;
  _headerView.actuating = _actuationActive;
  _headerView.primaryItem = [self createCloseItem];
  _headerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_headerView];

  _compactView = [[ActuationWorklogCompactView alloc] init];
  _compactView.accessibilityIdentifier = kCompactWorklogAccessibilityIdentifier;
  _compactView.delegate = self;
  _compactView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_compactView];

  _scrollView = [[UIScrollView alloc] initWithFrame:CGRectZero];
  _scrollView.accessibilityIdentifier =
      kFullWorklogScrollViewAccessibilityIdentifier;
  _scrollView.showsVerticalScrollIndicator = NO;
  _scrollView.showsHorizontalScrollIndicator = NO;
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_scrollView];

  _fullView = [[ActuationWorklogView alloc] initWithFrame:CGRectZero];
  // TODO(crbug.com/550337643): Set delegate when interactions are handled.
  _fullView.translatesAutoresizingMaskIntoConstraints = NO;
  [_scrollView addSubview:_fullView];

  _interventionView = [[ActuationInterventionView alloc] init];
  _interventionView.delegate = self;
  _interventionView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_interventionView];
}

// Configures layout constraints.
- (void)setupConstraints {
  _compactHeightConstraint =
      [_compactView.heightAnchor constraintEqualToConstant:0.0];

  AddSameConstraintsToSides(_headerView, self.view,
                            LayoutSides::kTop | LayoutSides::kHorizontal);
  AddSameConstraintsToSides(_compactView, self.view, LayoutSides::kHorizontal);
  AddSameConstraintsToSides(_scrollView, self.view, LayoutSides::kHorizontal);
  AddSameConstraintsWithInsets(_fullView, _scrollView.contentLayoutGuide,
                               NSDirectionalEdgeInsets{0, 0, kSpacingLarge, 0});
  AddSameConstraintsToSides(_interventionView, self.view,
                            LayoutSides::kHorizontal | LayoutSides::kBottom);

  [NSLayoutConstraint activateConstraints:@[
    [_compactView.topAnchor constraintEqualToAnchor:_headerView.bottomAnchor],
    _compactHeightConstraint,
    [_scrollView.topAnchor constraintEqualToAnchor:_headerView.bottomAnchor],
    [_scrollView.bottomAnchor
        constraintEqualToAnchor:_interventionView.topAnchor],
    [_fullView.widthAnchor
        constraintEqualToAnchor:_scrollView.frameLayoutGuide.widthAnchor],
  ]];
}

// Updates visibility of the view and its subviews for `_displayMode`.
- (void)updateVisibility {
  self.view.hidden = !_actuationActive;
  _compactView.hidden = _displayMode != ActuationWorklogDisplayModeCompact;
  _scrollView.hidden = _displayMode != ActuationWorklogDisplayModeExpanded;
  // `_interventionView` drives its own `hidden` state from its data, so it is
  // concealed through `alpha` instead while minimized.
  // TODO(crbug.com/532204179): Handle interventions that arrive while
  // minimized.
  BOOL minimized = _displayMode == ActuationWorklogDisplayModeMinimized;
  _interventionView.alpha = minimized ? 0.0 : 1.0;
  _interventionView.accessibilityElementsHidden = minimized;
}

// Calculates current fitting height and notifies the delegate. The reported
// height always reflects the compact layout, regardless of `_displayMode`,
// since only the compact layout has a dynamic height.
- (void)notifyHeightDidChange {
  [self.delegate
      worklogViewController:self
            didChangeHeight:
                [self heightForDisplayMode:ActuationWorklogDisplayModeCompact]];
}

// Returns the height of `_headerView`, falling back to its fitting height
// before the first layout pass.
- (CGFloat)headerHeight {
  CGFloat headerHeight = _headerView.bounds.size.height;
  if (headerHeight > 0) {
    return headerHeight;
  }
  return [self fittingHeightForView:_headerView
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

// Scrolls the full worklog to the bottom to reveal newly appended items.
- (void)scrollToBottomAnimated:(BOOL)animated {
  [_scrollView layoutIfNeeded];
  CGFloat bottomOffsetY = _scrollView.contentSize.height -
                          _scrollView.bounds.size.height +
                          _scrollView.adjustedContentInset.bottom;
  if (bottomOffsetY > 0) {
    [_scrollView setContentOffset:CGPointMake(0, bottomOffsetY)
                         animated:animated];
  }
}

#pragma mark - ActuationInterventionViewDelegate

- (void)interventionView:(ActuationInterventionView*)view
        didTriggerAction:(ActuationInterventionAction)action {
  [self.mutator didTriggerInterventionAction:action];
}

@end
