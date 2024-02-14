// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_bottom_trailing_view.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_tab_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// The vertical/horizontal spacing to apply between the `GroupTabView` views.
const CGFloat kSpacing = 2;
}  // namespace

@implementation GroupGridBottomTrailingView {
  // The view to display for 1 tab configuration.
  GroupTabView* _mainSubview;
  // The views to display  if the number of tabs exceeds 1.
  GroupTabView* _topLeadingView;
  GroupTabView* _topTrailingView;
  GroupTabView* _bottomLeadingView;
  GroupTabView* _bottomTrailingView;
}

- (instancetype)init {
  self = [super initWithSpacing:kSpacing adaptForCompactSizeClass:NO];
  if (self) {
    self.applicableCornerRadius = kGroupGridBottomTrailingCellCornerRadius;
    _mainSubview = [[GroupTabView alloc] init];
    _mainSubview.hidden = YES;
    _mainSubview.layer.masksToBounds = YES;

    _topLeadingView = [self makeGroupTabView];
    _topTrailingView = [self makeGroupTabView];
    _bottomLeadingView = [self makeGroupTabView];
    _bottomTrailingView = [self makeGroupTabView];

    _mainSubview.translatesAutoresizingMaskIntoConstraints = NO;
    _topLeadingView.translatesAutoresizingMaskIntoConstraints = NO;
    _topTrailingView.translatesAutoresizingMaskIntoConstraints = NO;
    _bottomLeadingView.translatesAutoresizingMaskIntoConstraints = NO;
    _bottomTrailingView.translatesAutoresizingMaskIntoConstraints = NO;

    [self addSubview:_mainSubview];
    [self updateTopLeadingWithView:_topLeadingView];
    [self updateTopTrailingWithView:_topTrailingView];
    [self updateBottomLeadingWithView:_bottomLeadingView];
    [self updateBottomTrailingWithView:_bottomTrailingView];

    self.backgroundColor = [UIColor colorNamed:kBackgroundColor];
    AddSameConstraints(self, _mainSubview);

  }

  return self;
}

- (void)configureWithGroupTabInfo:(GroupTabInfo*)groupTabInfo {
  [self hideAllViews];
  [_mainSubview configureWithSnapshot:groupTabInfo.snapshot
                              favicon:groupTabInfo.favicon];
  _mainSubview.hidden = NO;
}

- (void)configureWithFavicons:(NSArray<UIImage*>*)favicons
           remainingTabsCount:(NSInteger)remainingTabsCount {
  // Start by hiding all the views as the number of visible views can change
  // depending on the number of object in `favicons`.
  [self hideAllViews];

  int faviconLength = [favicons count];

  if (faviconLength > 0) {
    [_topLeadingView configureWithFavicon:favicons[0]];
    _topLeadingView.hidden = NO;
  }

  if (faviconLength > 1) {
    [_topTrailingView configureWithFavicon:favicons[1]];
    _topTrailingView.hidden = NO;
  }

  if (faviconLength > 2) {
    [_bottomLeadingView configureWithFavicon:favicons[2]];
    _bottomLeadingView.hidden = NO;
  }

  if (faviconLength == 4) {
    [_bottomTrailingView configureWithFavicon:favicons[3]];
    _bottomTrailingView.hidden = NO;

  } else if (remainingTabsCount > 0) {
    [_bottomTrailingView configureWithRemainingTabsNumber:remainingTabsCount];
    _bottomTrailingView.hidden = NO;
  }
}

#pragma mark - Private

// Hides all the subview of the `GroupGridBottomTrailingView`.
- (void)hideAllViews {
  _topLeadingView.hidden = YES;
  _topTrailingView.hidden = YES;
  _bottomLeadingView.hidden = YES;
  _bottomTrailingView.hidden = YES;
  _mainSubview.hidden = YES;
}

// Returns a pre-configured `GroupTabView` with a background and a corner
// radius, the returned view is hidden.
- (GroupTabView*)makeGroupTabView {
  GroupTabView* groupTabView = [[GroupTabView alloc] init];
  groupTabView.hidden = YES;
  groupTabView.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  groupTabView.layer.cornerRadius = kGroupGridBottomTrailingCellCornerRadius;
  groupTabView.layer.masksToBounds = YES;
  // TODO(crbug.com/1501837): Add the shadows.
  return groupTabView;
}

@end
