// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_snapshots_view.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_tab_info.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/group_tab_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
constexpr CGFloat kFaviconSize = 16;
constexpr CGFloat kFaviconPadding = 8;
constexpr CGFloat kFaviconMargin = 4;
constexpr CGFloat kFaviconCornerRadius = 7;
constexpr CGFloat kSnapshotCornerRadius = 12;
constexpr CGFloat kSnapshotOverlayAlpha = 0.14;
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
}

// TODO(crbug.com/1501837): Remove this and use GroupTabView instead.
- (instancetype)initWithSnapshot:(UIImage*)snapshot favicon:(UIImage*)favicon {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group snapshot view outside "
         "the Tab Groups experiment.";
  self = [super init];
  if (self) {
    UIView* finalView = self;
    finalView.translatesAutoresizingMaskIntoConstraints = NO;

    TopAlignedImageView* snapshotView = [[TopAlignedImageView alloc] init];
    snapshotView.image = snapshot;
    snapshotView.translatesAutoresizingMaskIntoConstraints = NO;
    snapshotView.layer.cornerRadius = kSnapshotCornerRadius;
    snapshotView.contentMode = UIViewContentModeScaleAspectFill;
    snapshotView.clipsToBounds = YES;

    GradientView* snapshotOverlay = [[GradientView alloc]
        initWithTopColor:[[UIColor blackColor] colorWithAlphaComponent:0]
             bottomColor:[[UIColor blackColor]
                             colorWithAlphaComponent:kSnapshotOverlayAlpha]];
    snapshotOverlay.translatesAutoresizingMaskIntoConstraints = NO;
    [snapshotView addSubview:snapshotOverlay];
    AddSameConstraints(snapshotOverlay, snapshotView);

    // Add a favicon only if there is one.
    // TODO(crbug.com/1501837): Condition should be removed once we garanty to
    // have at least the default favicon.
    if (favicon && !CGSizeEqualToSize(favicon.size, CGSizeZero)) {
      UIImageView* faviconImageView = [[UIImageView alloc] init];
      faviconImageView.translatesAutoresizingMaskIntoConstraints = NO;
      faviconImageView.contentMode = UIViewContentModeScaleAspectFill;
      faviconImageView.image = favicon;

      UIView* faviconBackground = [[UIView alloc] init];
      faviconBackground.translatesAutoresizingMaskIntoConstraints = NO;
      faviconBackground.backgroundColor = UIColor.whiteColor;
      faviconBackground.layer.cornerRadius = kFaviconCornerRadius;
      faviconBackground.clipsToBounds = YES;

      [faviconBackground addSubview:faviconImageView];
      AddSameCenterConstraints(faviconBackground, faviconImageView);

      [snapshotView addSubview:faviconBackground];

      [NSLayoutConstraint activateConstraints:@[
        [faviconImageView.widthAnchor constraintEqualToConstant:kFaviconSize],
        [faviconImageView.heightAnchor constraintEqualToConstant:kFaviconSize],
        [faviconBackground.widthAnchor
            constraintEqualToAnchor:faviconImageView.widthAnchor
                           constant:kFaviconPadding],
        [faviconBackground.heightAnchor
            constraintEqualToAnchor:faviconImageView.heightAnchor
                           constant:kFaviconPadding],
        [faviconBackground.trailingAnchor
            constraintEqualToAnchor:snapshotView.trailingAnchor
                           constant:-kFaviconMargin],
        [faviconBackground.bottomAnchor
            constraintEqualToAnchor:snapshotView.bottomAnchor
                           constant:-kFaviconMargin],
      ]];
    }

    [finalView addSubview:snapshotView];
    AddSameConstraints(finalView, snapshotView);
  }
  return self;
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

    if (@available(iOS 17, *)) {
      [self registerForTraitChanges:@[ UITraitVerticalSizeClass.self ]
                         withAction:@selector(updateViews)];
    }
  }
  return self;
}

#pragma mark - Private Helpers

// Returns a range computed with `start` index, `length` and the tab group's
// tabs number. To compute the range, it compute if there is element left or
// not. For example, by default we have a range of 3 element, but if there is
// only 4 element in total, then the range will take the last one, but if there
// is 5 element in total, then the range will only take 3 elements.
- (NSRange)computedRangeStartIndex:(NSUInteger)start
          lengthWithoutLastElement:(NSUInteger)length {
  if (start + length + 1 == _tabGroupTabNumber) {
    length += 1;
  }
  return NSMakeRange(start, length);
}

// Returns the list of favicons pictures from the given array of `infos`.
- (NSMutableArray<UIImage*>*)faviconsFromRange:(NSRange)range {
  NSMutableArray<UIImage*>* faviconsSubArray = [[NSMutableArray alloc] init];
  for (GroupTabInfo* info : [_tabGroupInfos subarrayWithRange:range]) {
    [faviconsSubArray addObject:info.favicon];
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

// Removes nil value and put an empty pictures instead.
// TODO(crbug.com/1501837): Remove this onces we do not have nil value anymore.
- (NSArray<GroupTabInfo*>*)prepareInfos:(NSArray<GroupTabInfo*>*)infos {
  NSMutableArray<GroupTabInfo*>* preparedInfos = [[NSMutableArray alloc] init];
  for (GroupTabInfo* info in infos) {
    GroupTabInfo* newInfo = [[GroupTabInfo alloc] init];
    if (info.snapshot) {
      newInfo.snapshot = info.snapshot;
    } else {
      newInfo.snapshot = [[UIImage alloc] init];
    }
    if (info.favicon) {
      newInfo.favicon = info.favicon;
    } else {
      newInfo.favicon = [[UIImage alloc] init];
    }
    [preparedInfos addObject:newInfo];
  }
  return preparedInfos;
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
  _tabGroupInfos = [self prepareInfos:tabGroupInfos];
  _tabGroupTabNumber = size;
  [self updateViews];
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
  for (GroupTabView* view in [_firstLine.arrangedSubviews
           arrayByAddingObjectsFromArray:_secondLine.arrangedSubviews]) {
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
