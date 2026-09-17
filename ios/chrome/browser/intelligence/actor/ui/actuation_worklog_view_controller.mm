// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_header_view.h"
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
    ActuationWorklogCompactViewDelegate>
@end

@implementation ActuationWorklogViewController {
  // Header view that sits right above the worklog.
  ActuationHeaderView* _headerView;
  // Primary accessory close button displayed in `_headerView`.
  UIButton* _closeButton;
  // Current step layout. Visible when `_compact` is true.
  ActuationWorklogCompactView* _compactView;
  // Timeline view embedded inside a scrollview to support vertical growth as
  // steps are added.
  ActuationWorklogView* _fullView;
  // Scrollable container for the `_fullView`. Visible when `_compact` is false.
  UIScrollView* _scrollView;
  // Height of `_compactView`. Adjusted dynamically when its content changes.
  NSLayoutConstraint* _compactHeightConstraint;

  BOOL _compact;
  BOOL _actuationActive;
}

#pragma mark - Public

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _compact = YES;
    _actuationActive = NO;
  }
  return self;
}

- (void)setCompact:(BOOL)compact {
  if (_compact == compact) {
    return;
  }
  _compact = compact;
  [self updateVisibility];
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

- (void)reset {
  [_headerView reset];
  _headerView.primaryAccessoryButton = _closeButton;
  [_compactView reset];
  [_fullView reset];
  [_scrollView setContentOffset:CGPointZero animated:NO];
  _compactHeightConstraint.constant = 0.0;
}

#pragma mark - ActuationWorklogCompactViewDelegate

- (void)worklogCompactView:(ActuationWorklogCompactView*)view
           didChangeHeight:(CGFloat)targetHeight {
  _compactHeightConstraint.constant = targetHeight;
  CGFloat headerHeight = _headerView.bounds.size.height;
  if (headerHeight == 0) {
    headerHeight =
        [_headerView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
            .height;
  }
  CGFloat totalHeight = headerHeight + targetHeight;
  [self.delegate worklogViewController:self didChangeHeight:totalHeight];
}

#pragma mark - Private

// Handles close button tap to request stopping the active actuation task.
- (void)stopActuationButtonTapped {
  [self.mutator stopActuation];
}

// Creates the close button accessory for `_headerView`.
- (UIButton*)createCloseButton {
  __weak __typeof(self) weakSelf = self;
  UIAction* closeAction = [UIAction actionWithHandler:^(UIAction*) {
    [weakSelf stopActuationButtonTapped];
  }];
  UIButton* button = [ActuationHeaderView
      createCircularIconButtonWithIcon:SymbolWithPointSize(
                                           SymbolXMark, kCloseButtonPointSize)
                                action:closeAction];
  button.accessibilityIdentifier =
      kActuationHeaderCloseButtonAccessibilityIdentifier;
  button.accessibilityLabel = l10n_util::GetNSString(IDS_CLOSE);
  return button;
}

// Creates the view hierarchy.
- (void)setupSubviews {
  _headerView = [[ActuationHeaderView alloc] initWithFrame:CGRectZero];
  _headerView.accessibilityIdentifier = kActuationHeaderAccessibilityIdentifier;
  _headerView.actuating = _actuationActive;
  _closeButton = [self createCloseButton];
  _headerView.primaryAccessoryButton = _closeButton;
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
}

// Configures layout constraints.
- (void)setupConstraints {
  _compactHeightConstraint =
      [_compactView.heightAnchor constraintEqualToConstant:0.0];

  AddSameConstraintsToSides(_headerView, self.view,
                            LayoutSides::kTop | LayoutSides::kHorizontal);
  AddSameConstraintsToSides(_compactView, self.view, LayoutSides::kHorizontal);
  AddSameConstraintsToSides(_scrollView, self.view,
                            LayoutSides::kHorizontal | LayoutSides::kBottom);
  AddSameConstraintsWithInsets(_fullView, _scrollView.contentLayoutGuide,
                               NSDirectionalEdgeInsets{0, 0, kSpacingLarge, 0});

  [NSLayoutConstraint activateConstraints:@[
    [_compactView.topAnchor constraintEqualToAnchor:_headerView.bottomAnchor],
    _compactHeightConstraint,
    [_scrollView.topAnchor constraintEqualToAnchor:_headerView.bottomAnchor],
    [_fullView.widthAnchor
        constraintEqualToAnchor:_scrollView.frameLayoutGuide.widthAnchor],
  ]];
}

// Updates visibility of the view and switches between compact and full mode.
- (void)updateVisibility {
  self.view.hidden = !_actuationActive;
  _compactView.hidden = !_compact;
  _scrollView.hidden = _compact;
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

@end
