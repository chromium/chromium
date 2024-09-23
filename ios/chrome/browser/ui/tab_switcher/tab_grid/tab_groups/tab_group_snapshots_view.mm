// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_snapshots_view.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_tab_info.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/group_tab_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
constexpr CGFloat kSpacing = 4;
constexpr CGFloat kFinalViewCornerRadius = 16;
}  // namespace

@implementation TabGroupSnapshotsView {
  BOOL _isLight;
  BOOL _isCell;
  NSArray<GroupTabInfo*>* _tabGroupInfos;
  NSUInteger _tabGroupTabNumber;
  UIStackView* _firstLine;
  UIStackView* _secondLine;
  GroupTabView* _singleView;
}

- (instancetype)initWithTabGroupInfos:(NSArray<GroupTabInfo*>*)tabGroupInfos
                                 size:(NSUInteger)size
                                light:(BOOL)isLight
                                 cell:(BOOL)isCell {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group snapshot view outside "
         "the Tab Groups experiment.";
  self = [super initWithFrame:CGRectZero];
  if (self) {
    CHECK_LE([tabGroupInfos count], size);
    self.translatesAutoresizingMaskIntoConstraints = NO;
    _isLight = isLight;
    _isCell = isCell;

    NSMutableArray<GroupTabView*>* finalViews =
        [[NSMutableArray alloc] initWithArray:@[
          [[GroupTabView alloc] initWithIsCell:isCell],
          [[GroupTabView alloc] initWithIsCell:isCell],
          [[GroupTabView alloc] initWithIsCell:isCell],
          [[GroupTabView alloc] initWithIsCell:isCell]
        ]];

    UIStackView* snapshotsView = [self squaredViews:finalViews];

    [self addSubview:snapshotsView];
    AddSameConstraints(snapshotsView, self);

    [self configureTabGroupSnapshotsViewWithTabGroupInfos:tabGroupInfos
                                                     size:size];

    if (!_isCell) {
      _singleView = [[GroupTabView alloc] initWithIsCell:_isCell];
      _singleView.translatesAutoresizingMaskIntoConstraints = NO;
      [_singleView configureWithSnapshot:tabGroupInfos.firstObject.snapshot
                                 favicon:tabGroupInfos.firstObject.favicon];
      [self addSubview:_singleView];
      AddSameConstraints(_singleView, self);
      _singleView.hidden = size > 1;
      _firstLine.hidden = size == 1;
      _secondLine.hidden = size == 1;
    }

    if (@available(iOS 17, *)) {
      [self registerForTraitChanges:@[ UITraitVerticalSizeClass.self ]
                         withAction:@selector(updateViews)];
    }
  }
  return self;
}

- (NSArray<GroupTabView*>*)allGroupTabViews {
  return [_firstLine.arrangedSubviews
      arrayByAddingObjectsFromArray:_secondLine.arrangedSubviews];
}

#pragma mark - Private Helpers

// Returns a range computed with `start` index, `length` and the tab group's
// tabs number. To compute the range, it compute if there is element left or
// not. For example, by default we have a range of 3 element, but if there is
// only 4 element in total, then the range will take the last one, but if there
// is 5 element in total, then the range will only take 3 elements.
- (NSRange)computedRangeStartIndex:(NSUInteger)start
          lengthWithoutLastElement:(NSUInteger)length {
  NSUInteger computedNumberOfElement = start + length + 1;
  if (computedNumberOfElement == _tabGroupTabNumber &&
      computedNumberOfElement <= [_tabGroupInfos count]) {
    length += 1;
  }
  return NSMakeRange(start, length);
}

// Returns the list of favicons pictures from the given array of `infos`.
- (NSMutableArray<UIImage*>*)faviconsFromRange:(NSRange)range {
  NSMutableArray<UIImage*>* faviconsSubArray = [[NSMutableArray alloc] init];
  for (GroupTabInfo* info : [_tabGroupInfos subarrayWithRange:range]) {
    if (info.favicon) {
      [faviconsSubArray addObject:info.favicon];
    } else {
      [faviconsSubArray addObject:[[UIImage alloc] init]];
    }
  }
  return faviconsSubArray;
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
  CHECK_EQ([views count], 4u);
  _firstLine = [self lineViews];
  _secondLine = [self lineViews];

  for (NSUInteger i = 0; i < 4; i++) {
    if (i < 2) {
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

- (void)configureTabGroupSnapshotsViewWithTabGroupInfos:
            (NSArray<GroupTabInfo*>*)tabGroupInfos
                                                   size:(NSUInteger)size {
  _tabGroupInfos = tabGroupInfos;
  _tabGroupTabNumber = size;
  if (!_isCell && (size == 1)) {
    _singleView.hidden = NO;
    _firstLine.hidden = YES;
    _secondLine.hidden = YES;
    [_singleView configureWithSnapshot:_tabGroupInfos.firstObject.snapshot
                               favicon:_tabGroupInfos.firstObject.favicon];
  } else {
    _singleView.hidden = YES;
    _firstLine.hidden = NO;
    _secondLine.hidden = NO;
    [self updateViews];
  }
}

- (void)updateViews {
  NSRange snapshotsViewRange =
      [self computedRangeStartIndex:0
           lengthWithoutLastElement:([self compactHeight]
                                         ? MIN(1, [_tabGroupInfos count])
                                         : MIN(3, [_tabGroupInfos count]))];
  NSRange faviconsViewRange =
      [self computedRangeStartIndex:NSMaxRange(snapshotsViewRange)
           lengthWithoutLastElement:MIN(3, [_tabGroupInfos count] -
                                               NSMaxRange(snapshotsViewRange))];

  _secondLine.hidden = [self compactHeight];

  NSUInteger index = snapshotsViewRange.location;
  for (GroupTabView* view in [self allGroupTabViews]) {
    if (index >= [_tabGroupInfos count]) {
      [view hideAllAttributes];
      continue;
    }

    GroupTabInfo* tabGroupInfo = _tabGroupInfos[index];
    if (index < NSMaxRange(snapshotsViewRange)) {
      [view configureWithSnapshot:tabGroupInfo.snapshot
                          favicon:tabGroupInfo.favicon];
    } else if (index < _tabGroupTabNumber) {
      NSMutableArray<UIImage*>* faviconImages =
          [self faviconsFromRange:faviconsViewRange];
      if (NSMaxRange(faviconsViewRange) < _tabGroupTabNumber) {
        [view configureWithFavicons:faviconImages
                remainingTabsNumber:(_tabGroupTabNumber -
                                     NSMaxRange(faviconsViewRange))];
      } else {
        [view configureWithFavicons:faviconImages];
      }
    }
    ++index;
  }
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }
  if (self.traitCollection.verticalSizeClass !=
      previousTraitCollection.verticalSizeClass) {
    [self updateViews];
  }
}

@end
