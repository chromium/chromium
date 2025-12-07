// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_snapshots_view.h"

#import "base/check_op.h"
#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/group_tab_view.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_snapshot_and_favicon.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

constexpr CGFloat kSpacing = 4;
constexpr CGFloat kFinalViewCornerRadius = 16;

// Total number of GroupTabView in this view.
constexpr NSInteger kGroupTabViewCount = 4;
// Number of GroupTabView displayed per line.
constexpr NSInteger kGroupTabViewLineCount = 2;
// Maximum number of favicons visible in the summary tile when some tabs are
// hidden behind "+N" additional tabs.
constexpr NSInteger kMaxSummaryFaviconVisible = 3;

}  // namespace

@implementation TabGroupSnapshotsView {
  BOOL _isLight;
  BOOL _isCell;
  NSMutableArray<TabSnapshotAndFavicon*>* _tabSnapshotsAndFavicons;
  UIStackView* _firstLine;
  UIStackView* _secondLine;
  GroupTabView* _singleView;
}

- (instancetype)initWithLightInterface:(BOOL)isLight cell:(BOOL)isCell {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.translatesAutoresizingMaskIntoConstraints = NO;
    _isLight = isLight;
    _isCell = isCell;

    NSMutableArray<GroupTabView*>* finalViews =
        [[NSMutableArray alloc] initWithCapacity:kGroupTabViewCount];
    for (NSInteger i = 0; i < kGroupTabViewCount; ++i) {
      [finalViews addObject:[[GroupTabView alloc] initWithIsCell:isCell]];
    }

    UIStackView* snapshotsView = [self squaredViews:finalViews];

    [self addSubview:snapshotsView];
    AddSameConstraints(snapshotsView, self);

    if (!_isCell) {
      _singleView = [[GroupTabView alloc] initWithIsCell:_isCell];
      _singleView.translatesAutoresizingMaskIntoConstraints = NO;
      [self addSubview:_singleView];
      AddSameConstraints(_singleView, self);
    }

    [self registerForTraitChanges:@[ UITraitVerticalSizeClass.class ]
                       withAction:@selector(updateViews)];
  }
  return self;
}

- (NSArray<GroupTabView*>*)allGroupTabViews {
  return [_firstLine.arrangedSubviews
      arrayByAddingObjectsFromArray:_secondLine.arrangedSubviews];
}

#pragma mark - Public

- (void)configureTabSnapshotAndFavicon:
            (TabSnapshotAndFavicon*)tabSnapshotAndFavicon
                              tabIndex:(NSInteger)tabIndex {
  _tabSnapshotsAndFavicons[tabIndex] = tabSnapshotAndFavicon;
  [self configureTabSnapshotAndFaviconForTabIndex:tabIndex];
}

#pragma mark - Setters

- (void)setTabsCount:(NSInteger)tabsCount {
  _tabsCount = tabsCount;

  // Reinitialize `_tabSnapshotsAndFavicons`.
  _tabSnapshotsAndFavicons =
      [[NSMutableArray alloc] initWithCapacity:_tabsCount];
  for (NSInteger i = 0; i < _tabsCount; i++) {
    [_tabSnapshotsAndFavicons addObject:[[TabSnapshotAndFavicon alloc] init]];
  }
  [self updateViews];
}

#pragma mark - Private Helpers

// Configures the GroupTabView for the tab at `tabIndex`.
- (void)configureTabSnapshotAndFaviconForTabIndex:(NSInteger)tabIndex {
  CHECK(tabIndex < _tabsCount);

  TabSnapshotAndFavicon* tabSnapshotAndFavicon =
      _tabSnapshotsAndFavicons[tabIndex];

  // If `_singleView` is visible, configure it.
  if (_singleView && !_singleView.hidden) {
    CHECK_EQ(_tabsCount, 1);
    CHECK_EQ(tabIndex, 0);
    [_singleView configureWithSnapshot:tabSnapshotAndFavicon.snapshot
                               favicon:tabSnapshotAndFavicon.favicon];
    return;
  }

  NSArray<GroupTabView*>* groupViews = [self allGroupTabViews];
  BOOL compactHeight = [self compactHeight];
  bool isOnSummaryTile =
      IsTabOnSummaryTile(tabIndex, _tabsCount, compactHeight);

  // Configure the view with the snapshot and the favicon.
  if (!isOnSummaryTile) {
    GroupTabView* view = groupViews[tabIndex];
    [view configureWithSnapshot:tabSnapshotAndFavicon.snapshot
                        favicon:tabSnapshotAndFavicon.favicon];
    return;
  }

  NSInteger faviconIndex =
      SummaryFaviconSlotForTabIndex(tabIndex, _tabsCount, compactHeight);

  // This favicon is skipped if its slot is reserved for the '+N' indicator
  // when the total number of tabs exceeds the maximum individual visuals.
  // For instance, a group with 7 tabs shows all favicons, but with 8 tabs,
  // the last two favicons are hidden in favor of the '+N' indicator.
  if (_tabsCount > MaxIndividualTabVisuals(compactHeight) &&
      faviconIndex >= kMaxSummaryFaviconVisible) {
    return;
  }

  GroupTabView* summaryView = compactHeight
                                  ? groupViews[kGroupTabViewLineCount - 1]
                                  : groupViews[kGroupTabViewCount - 1];
  NSInteger hiddenTabsCount = SummaryHiddenTabsCount(_tabsCount, compactHeight);

  // Configure the view with only the favicon if all favions can be displayed
  // in the remaning view.
  if (hiddenTabsCount == 0) {
    [summaryView configureWithFavicon:tabSnapshotAndFavicon.favicon
                         faviconIndex:faviconIndex];
    return;
  }

  // Otherwise configure the view with the favicon and the remaining tabs
  // number.
  [summaryView configureWithFavicon:tabSnapshotAndFavicon.favicon
                       faviconIndex:faviconIndex
                remainingTabsNumber:hiddenTabsCount];
}

// Updates view properties and reconfigures sub views.
- (void)updateViews {
  if (!_isCell && (_tabsCount == 1)) {
    _singleView.hidden = NO;
    _firstLine.hidden = YES;
    _secondLine.hidden = YES;
  } else {
    _singleView.hidden = YES;
    _firstLine.hidden = NO;
    _secondLine.hidden = [self compactHeight];
  }
  // Reinitialize group views attributes.
  for (GroupTabView* view in [self allGroupTabViews]) {
    [view hideAllAttributes];
  }

  // Reconfigure group views for each tabs.
  for (NSInteger index = 0; index < _tabsCount; index++) {
    [self configureTabSnapshotAndFaviconForTabIndex:index];
  }
}

// Returns a configured stack view that correspond to a line in the final
// squared view.
- (UIStackView*)lineViews {
  UIStackView* line = [[UIStackView alloc] init];
  line.translatesAutoresizingMaskIntoConstraints = NO;
  line.distribution = UIStackViewDistributionFillEqually;
  line.contentMode = UIViewContentModeScaleAspectFill;
  line.spacing = kSpacing;
  return line;
}

// Returns a stack view that put the views, given in parameters, aligned in
// square.
- (UIStackView*)squaredViews:(NSMutableArray<GroupTabView*>*)views {
  _firstLine = [self lineViews];
  _secondLine = [self lineViews];

  for (NSInteger i = 0; i < kGroupTabViewCount; i++) {
    if (i < kGroupTabViewLineCount) {
      [_firstLine addArrangedSubview:views[i]];
    } else {
      [_secondLine addArrangedSubview:views[i]];
    }
  }

  UIStackView* completeView = [[UIStackView alloc] init];
  completeView.translatesAutoresizingMaskIntoConstraints = NO;
  completeView.layer.cornerRadius = kFinalViewCornerRadius;
  completeView.spacing = kSpacing;
  completeView.axis = UILayoutConstraintAxisVertical;
  completeView.distribution = UIStackViewDistributionFillEqually;
  completeView.contentMode = UIViewContentModeScaleAspectFill;
  completeView.layer.masksToBounds = YES;
  [completeView addArrangedSubview:_firstLine];
  [completeView addArrangedSubview:_secondLine];

  _secondLine.hidden = [self compactHeight];

  return completeView;
}

// YES if the view is compact.
- (BOOL)compactHeight {
  return self.traitCollection.verticalSizeClass ==
             UIUserInterfaceSizeClassCompact &&
         _isCell;
}

@end
