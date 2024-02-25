// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_grid_configurable_view.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_tab_info.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_tab_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

const CGFloat kSpacing = 4.0;
const CGFloat kTabGroupOverflowViewSpacing = 2.0;

}  // namespace

@interface GroupGridConfigurableView ()

// The current vertical trait is compact.
@property(nonatomic, assign, readonly) BOOL compact;

@end

@implementation GroupGridConfigurableView {
  BOOL _isMainGroupView;
  GroupTabView* _topLeadingView;
  GroupTabView* _topTrailingView;
  GroupTabView* _bottomLeadingView;
  GroupTabView* _bottomTrailingView;

  // The view that acts as the `_bottomTrailingView` when the
  // GroupGridConfigurableView is in a group view configuration.
  GroupGridConfigurableView* _tabGroupOverflowView;

  CGFloat _applicableCornerRadius;

  NSArray<NSLayoutConstraint*>* _compactConstraints;
  NSArray<NSLayoutConstraint*>* _nonCompactConstraints;

  // The last used parameters to configure the group cell views, used to update
  // the view when the trait collection changes.
  NSArray<GroupTabInfo*>* _groupTabInfos;
  NSInteger _totalTabsCount;
}

- (instancetype)initWithIsMainGroupView:(BOOL)isMainGroupView {
  self = [super initWithFrame:CGRectZero];

  if (self) {
    _isMainGroupView = isMainGroupView;
    CGFloat spacing;
    if (_isMainGroupView) {
      spacing = kSpacing;
      _applicableCornerRadius = kGroupGridCellCornerRadius;
      _tabGroupOverflowView =
          [[GroupGridConfigurableView alloc] initWithIsMainGroupView:NO];
      _tabGroupOverflowView.translatesAutoresizingMaskIntoConstraints = NO;
      _tabGroupOverflowView.layer.masksToBounds = YES;
      [self addSubview:_tabGroupOverflowView];

    } else {
      spacing = kTabGroupOverflowViewSpacing;
      _applicableCornerRadius = kGroupGridBottomTrailingCellCornerRadius;
    }
    _topLeadingView = [self buildGroupTabView];
    _topTrailingView = [self buildGroupTabView];
    _bottomLeadingView = [self buildGroupTabView];
    _bottomTrailingView = [self buildGroupTabView];

    [self addSubview:_topLeadingView];
    [self addSubview:_topTrailingView];
    [self addSubview:_bottomLeadingView];
    [self addSubview:_bottomTrailingView];

    _nonCompactConstraints = @[
      [_topLeadingView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],
      [_topLeadingView.topAnchor constraintEqualToAnchor:self.topAnchor],

      [_topTrailingView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [_topTrailingView.topAnchor constraintEqualToAnchor:self.topAnchor],

      [_bottomLeadingView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],
      [_bottomLeadingView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor],

      [_bottomTrailingView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor],
      [_bottomTrailingView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],

      [_topTrailingView.leadingAnchor
          constraintEqualToAnchor:_topLeadingView.trailingAnchor
                         constant:spacing],
      [_topLeadingView.widthAnchor
          constraintEqualToAnchor:_topTrailingView.widthAnchor],

      [_bottomTrailingView.leadingAnchor
          constraintEqualToAnchor:_bottomLeadingView.trailingAnchor
                         constant:spacing],
      [_bottomLeadingView.widthAnchor
          constraintEqualToAnchor:_bottomTrailingView.widthAnchor],

      [_bottomLeadingView.topAnchor
          constraintEqualToAnchor:_topLeadingView.bottomAnchor
                         constant:spacing],
      [_bottomLeadingView.heightAnchor
          constraintEqualToAnchor:_topLeadingView.heightAnchor],

      [_bottomTrailingView.topAnchor
          constraintEqualToAnchor:_topTrailingView.bottomAnchor
                         constant:spacing],
      [_bottomTrailingView.heightAnchor
          constraintEqualToAnchor:_topTrailingView.heightAnchor],
    ];

    _compactConstraints = @[
      [_topLeadingView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],
      [_topLeadingView.topAnchor constraintEqualToAnchor:self.topAnchor],

      [_bottomTrailingView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [_bottomTrailingView.topAnchor constraintEqualToAnchor:self.topAnchor],

      [_bottomTrailingView.leadingAnchor
          constraintEqualToAnchor:_topLeadingView.trailingAnchor
                         constant:spacing],
      [_bottomTrailingView.widthAnchor
          constraintEqualToAnchor:_topLeadingView.widthAnchor],

      [_topLeadingView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      [_bottomTrailingView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor],
    ];

    [self updateAndActivateConstraints];
    [self prepareSubviewsToBeReused];

    if (@available(iOS 17, *)) {
      [self registerForTraitChanges:@[ UITraitVerticalSizeClass.self ]
                         withAction:@selector(updateAndReconfigureGroup)];
    }
  }

  return self;
}

- (void)configureWithGroupTabInfos:(NSArray<GroupTabInfo*>*)groupTabInfos
                    totalTabsCount:(NSInteger)totalTabsCount {
  [self prepareSubviewsToBeReused];

  _groupTabInfos = groupTabInfos;
  _totalTabsCount = totalTabsCount;

  NSRange range;

  int groupTabInfosLength = [groupTabInfos count];
  if (groupTabInfosLength > 0) {
    [self configureGroupTabView:_topLeadingView withInfo:groupTabInfos[0]];
  }
  if (self.compact && _isMainGroupView) {
    if (groupTabInfosLength == 2) {
      [self configureGroupTabView:_bottomTrailingView
                         withInfo:groupTabInfos[1]];
    } else if (groupTabInfosLength > 2) {
      range.location = 1;
      range.length = groupTabInfosLength - 1;
      [_tabGroupOverflowView
          configureWithGroupTabInfos:[groupTabInfos subarrayWithRange:range]
                      totalTabsCount:totalTabsCount - 1];
      _bottomTrailingView.hidden = YES;
      _tabGroupOverflowView.hidden = NO;
    }

  } else {
    if (groupTabInfosLength > 1) {
      [self configureGroupTabView:_topTrailingView withInfo:groupTabInfos[1]];
    }
    if (groupTabInfosLength > 2) {
      [self configureGroupTabView:_bottomLeadingView withInfo:groupTabInfos[2]];
    }

    if (groupTabInfosLength == 4) {
      [self configureGroupTabView:_bottomTrailingView
                         withInfo:groupTabInfos[3]];
    } else if (groupTabInfosLength > 4) {
      if (_isMainGroupView) {
        range.location = 3;
        range.length = groupTabInfosLength - 3;
        [_tabGroupOverflowView
            configureWithGroupTabInfos:[groupTabInfos subarrayWithRange:range]
                        totalTabsCount:totalTabsCount - 3];
        _bottomTrailingView.hidden = YES;
        _tabGroupOverflowView.hidden = NO;
      } else {
        [_bottomTrailingView
            configureWithRemainingTabsNumber:totalTabsCount - 3];
        _bottomTrailingView.hidden = NO;
      }
    }
  }
}

- (void)setApplicableCornerRadius:(CGFloat)applicableCornerRadius {
  _applicableCornerRadius = applicableCornerRadius;
  _topLeadingView.layer.cornerRadius = _applicableCornerRadius;
  _topTrailingView.layer.cornerRadius = _applicableCornerRadius;
  _bottomLeadingView.layer.cornerRadius = _applicableCornerRadius;
  _bottomTrailingView.layer.cornerRadius = _applicableCornerRadius;
  _tabGroupOverflowView.layer.cornerRadius = _applicableCornerRadius;
}

- (BOOL)compact {
  return self.traitCollection.verticalSizeClass ==
         UIUserInterfaceSizeClassCompact;
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }
  if (self.traitCollection.verticalSizeClass !=
      previousTraitCollection.verticalSizeClass) {
    [self updateAndReconfigureGroup];
  }
}

#pragma mark - Private

// Updates the constraints and reconfigures reconfigures the views.
- (void)updateAndReconfigureGroup {
  if (!_isMainGroupView) {
    return;
  }
  [self updateAndActivateConstraints];
  [self configureWithGroupTabInfos:_groupTabInfos
                    totalTabsCount:_totalTabsCount];
}

// Applies the constraint to use depending the current vertical trait.
- (void)updateAndActivateConstraints {
  if (_isMainGroupView) {
    if (self.compact) {
      [NSLayoutConstraint deactivateConstraints:_nonCompactConstraints];
      [NSLayoutConstraint activateConstraints:_compactConstraints];
    } else {
      [NSLayoutConstraint deactivateConstraints:_compactConstraints];
      [NSLayoutConstraint activateConstraints:_nonCompactConstraints];
    }

    AddSameConstraints(_tabGroupOverflowView, _bottomTrailingView);
  } else {
    [NSLayoutConstraint activateConstraints:_nonCompactConstraints];
  }
}

// Hides all the views and their attributes.
- (void)prepareSubviewsToBeReused {
  [_topLeadingView hideAllAttributes];
  [_topTrailingView hideAllAttributes];
  [_bottomLeadingView hideAllAttributes];
  [_bottomTrailingView hideAllAttributes];
  [_tabGroupOverflowView configureWithGroupTabInfos:nil totalTabsCount:0];
  _topLeadingView.hidden = YES;
  _topTrailingView.hidden = YES;
  _bottomLeadingView.hidden = YES;
  _bottomTrailingView.hidden = YES;
  _tabGroupOverflowView.hidden = YES;
}

// Returns a configured `GroupTabView` with `_applicableCornerRadius`.
- (GroupTabView*)buildGroupTabView {
  GroupTabView* groupTabView = [[GroupTabView alloc] init];
  groupTabView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  groupTabView.layer.cornerRadius = _applicableCornerRadius;
  groupTabView.layer.masksToBounds = YES;
  groupTabView.translatesAutoresizingMaskIntoConstraints = NO;
  return groupTabView;
}

// Configures a given `GroupTabView` with a given `GroupTabInfo`.
- (void)configureGroupTabView:(GroupTabView*)groupTabView
                     withInfo:(GroupTabInfo*)tabInfo {
  if (_isMainGroupView) {
    [groupTabView configureWithSnapshot:tabInfo.snapshot
                                favicon:tabInfo.favicon];
  } else {
    [groupTabView configureWithFavicon:tabInfo.favicon];
  }
  groupTabView.hidden = NO;
}

@end
