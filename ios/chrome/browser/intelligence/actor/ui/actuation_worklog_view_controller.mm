// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_controller.h"

#import "base/notreached.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_intervention_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_suggestions_presenter.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_compact_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_consumer.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_mutator.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/ui/intelligence_header_item.h"
#import "ios/chrome/browser/intelligence/ui/intelligence_header_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Point size for the header button symbols.
constexpr CGFloat kHeaderButtonPointSize = 14.0;

// Accessibility label of the header pause button.
// TODO(crbug.com/573515833): Localize the string.
NSString* const kPauseButtonTitle = @"Pause";

// Identifiers of the accessory items in `_headerView`.
NSString* const kCloseHeaderItemIdentifier = @"close";
NSString* const kPauseHeaderItemIdentifier = @"pause";

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

using intelligence::actor::kSpacingLarge;

@interface ActuationWorklogViewController () <
    ActuationInterventionViewDelegate,
    ActuationWorklogCompactViewDelegate,
    ActuationWorklogViewDelegate,
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
  // Mode-specific header content applied to `_headerView`.
  ActuationHeaderContentVariants* _headerContentVariants;
  // Offscreen header for content-independent height calculation.
  IntelligenceHeaderView* _sizingHeaderView;
  // Action of the header's primary button.
  ActuationHeaderPrimaryAction _headerPrimaryAction;
  // Intervention displayed by `_interventionView`, if any.
  ActuationInterventionData* _intervention;

  BOOL _actuationActive;
}

#pragma mark - Public

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _displayMode = ActuationWorklogDisplayModeCompact;
    _headerPrimaryAction = ActuationHeaderPrimaryAction::kStop;
    _actuationActive = NO;
  }
  return self;
}

- (void)setDisplayMode:(ActuationWorklogDisplayMode)displayMode {
  if (_displayMode == displayMode) {
    return;
  }
  _displayMode = displayMode;
  [self updateHeaderContent];
  [self updateVisibility];
}

- (CGFloat)heightForDisplayMode:(ActuationWorklogDisplayMode)mode {
  CGFloat width = self.view.bounds.size.width;
  // `_interventionView` owns its padding and collapses to zero height when no
  // intervention is active.
  CGFloat interventionHeight = [self fittingHeightForView:_interventionView
                                              targetWidth:width];
  CGFloat headerHeight = [self headerHeightForDisplayMode:mode];
  switch (mode) {
    case ActuationWorklogDisplayModeMinimized:
      return headerHeight;
    case ActuationWorklogDisplayModeCompact:
      return headerHeight + _compactHeightConstraint.constant +
             interventionHeight;
    case ActuationWorklogDisplayModeExpanded:
      // Height needed to show the full timeline without scrolling, including
      // the bottom inset applied to `_fullView` in its scroll view.
      return headerHeight +
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
  [self updateHeaderContent];
  [self updateVisibility];
  [self updateIntervention];
}

#pragma mark - ActuationWorklogConsumer

- (void)setActuationActive:(BOOL)active {
  if (_actuationActive == active) {
    return;
  }
  _actuationActive = active;
  _headerView.loading = active;
  _fullView.actuating = active;
  [self updateVisibility];
}

- (void)setHeaderPrimaryAction:(ActuationHeaderPrimaryAction)action {
  if (_headerPrimaryAction == action) {
    return;
  }
  _headerPrimaryAction = action;
  _headerView.primaryItem = [self createPrimaryItem];
}

- (void)setHeaderContentVariants:(ActuationHeaderContentVariants*)variants {
  _headerContentVariants = variants;
  [self updateHeaderContent];
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

- (void)setLastItem:(ActuationWorklogItem*)item
               chip:(ActuationWorklogChip*)chip {
  [_fullView setLastItem:item];
  [_fullView setChip:chip];
  [_compactView transitionToItem:item chip:chip animated:NO];
  [self notifyHeightDidChange];
}

- (void)setIntervention:(ActuationInterventionData*)intervention {
  // Configuring `_interventionView` again would redraw its buttons.
  if (_intervention == intervention) {
    return;
  }
  _intervention = intervention;
  [self updateIntervention];
  [self notifyHeightDidChange];
}

- (void)reset {
  _headerContentVariants = nil;
  [_headerView reset];
  _headerPrimaryAction = ActuationHeaderPrimaryAction::kStop;
  _headerView.primaryItem = [self createPrimaryItem];
  [_compactView reset];
  [_fullView reset];
  [_scrollView setContentOffset:CGPointZero animated:NO];
  _compactHeightConstraint.constant = 0.0;
  // The height must be reported even without an intervention update.
  _intervention = nil;
  [self updateIntervention];
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

#pragma mark - ActuationWorklogViewDelegate

- (void)worklogView:(ActuationWorklogView*)worklogView
    didTapAccessoryItem:(ActuationWorklogAccessoryItem*)accessoryItem {
  [self.suggestionsPresenter showSuggestionsPickerForAccessory:accessoryItem];
}

#pragma mark - Private

// Creates the primary button accessory of `_headerView` for
// `_headerPrimaryAction`.
- (IntelligenceHeaderItem*)createPrimaryItem {
  switch (_headerPrimaryAction) {
    case ActuationHeaderPrimaryAction::kPause:
      return [self createPauseItem];
    case ActuationHeaderPrimaryAction::kStop:
      return [self createCloseItem];
  }
}

// Creates the pause button accessory for `_headerView`.
- (IntelligenceHeaderItem*)createPauseItem {
  // The mocks use the stop symbol for the pause button.
  return [IntelligenceHeaderItem
           itemWithIdentifier:kPauseHeaderItemIdentifier
                         icon:SymbolWithPointSize(SymbolStopFill,
                                                  kHeaderButtonPointSize)
                        title:kPauseButtonTitle
      accessibilityIdentifier:
          kActuationHeaderPauseButtonAccessibilityIdentifier];
}

// Creates the close button accessory for `_headerView`.
- (IntelligenceHeaderItem*)createCloseItem {
  return [IntelligenceHeaderItem
           itemWithIdentifier:kCloseHeaderItemIdentifier
                         icon:SymbolWithPointSize(SymbolXMark,
                                                  kHeaderButtonPointSize)
                        title:l10n_util::GetNSString(IDS_CLOSE)
      accessibilityIdentifier:
          kActuationHeaderCloseButtonAccessibilityIdentifier];
}

// Creates the view hierarchy.
- (void)setupSubviews {
  _headerView = [[IntelligenceHeaderView alloc] initWithFrame:CGRectZero];
  _headerView.accessibilityIdentifier = kActuationHeaderAccessibilityIdentifier;
  _headerView.delegate = self;
  _headerView.loading = _actuationActive;
  _headerView.primaryItem = [self createPrimaryItem];
  _headerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_headerView];

  _compactView = [[ActuationWorklogCompactView alloc] init];
  _compactView.accessibilityIdentifier = kCompactWorklogAccessibilityIdentifier;
  _compactView.delegate = self;
  _compactView.itemDelegate = self;
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
  _fullView.delegate = self;
  _fullView.actuating = _actuationActive;
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

// Updates `_headerView` content and density for `_displayMode`.
- (void)updateHeaderContent {
  ActuationHeaderContent* content =
      [_headerContentVariants contentForDisplayMode:_displayMode];
  _headerView.title = content.title;
  _headerView.subtitle = content.subtitle;
  _headerView.density = HeaderDensityForDisplayMode(_displayMode);
}

// Updates visibility of the view and its subviews for `_displayMode`.
- (void)updateVisibility {
  self.view.hidden = !_actuationActive;
  _compactView.hidden = _displayMode != ActuationWorklogDisplayModeCompact;
  _scrollView.hidden = _displayMode != ActuationWorklogDisplayModeExpanded;
  // `_interventionView` drives its own `hidden` state from its data, so it is
  // concealed through `alpha` instead while minimized.
  BOOL minimized = _displayMode == ActuationWorklogDisplayModeMinimized;
  _interventionView.alpha = minimized ? 0.0 : 1.0;
  _interventionView.accessibilityElementsHidden = minimized;
}

// Updates `_interventionView` for `_intervention`.
- (void)updateIntervention {
  [_interventionView configureWithData:_intervention];
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

// Returns the header height for any given `mode`. A sizing view ensures we do
// not mutate `_headerView`. The minimized height fits both title and subtitle
// without clippping in the minimized display mode.
- (CGFloat)headerHeightForDisplayMode:(ActuationWorklogDisplayMode)mode {
  if (!_sizingHeaderView) {
    _sizingHeaderView =
        [[IntelligenceHeaderView alloc] initWithFrame:CGRectZero];
  }
  ActuationHeaderContent* content =
      [_headerContentVariants contentForDisplayMode:mode];
  BOOL minimized = mode == ActuationWorklogDisplayModeMinimized;
  _sizingHeaderView.title =
      minimized ? kHeaderSizingPlaceholderText : content.title;
  _sizingHeaderView.subtitle =
      minimized ? kHeaderSizingPlaceholderText : content.subtitle;
  _sizingHeaderView.density = HeaderDensityForDisplayMode(mode);
  _sizingHeaderView.primaryItem = _headerView.primaryItem;
  _sizingHeaderView.secondaryItems = _headerView.secondaryItems;
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
        didTriggerAction:(ActuationInterventionAction*)action {
  [self.mutator didTriggerInterventionAction:action];
}

#pragma mark - IntelligenceHeaderViewDelegate

- (void)headerView:(IntelligenceHeaderView*)headerView
    didTriggerItemWithIdentifier:(NSString*)identifier {
  if ([identifier isEqualToString:kCloseHeaderItemIdentifier]) {
    [self.mutator stopActuation];
  } else if ([identifier isEqualToString:kPauseHeaderItemIdentifier]) {
    [self.mutator pauseActuation];
  } else {
    NOTREACHED();
  }
}

@end
