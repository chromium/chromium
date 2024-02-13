// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_snapshots_view.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
constexpr CGFloat kFaviconSize = 16;
constexpr CGFloat kFaviconPadding = 8;
constexpr CGFloat kFaviconMargin = 4;
constexpr CGFloat kFaviconCornerRadius = 7;
constexpr CGFloat kSnapshotCornerRadius = 12;
constexpr CGFloat kSnapshotOverlayAlpha = 0.14;
}  // namespace

@implementation TabGroupSnapshotsView

- (instancetype)initWithSnapshot:(UIImage*)snapshot favicon:(UIImage*)favicon {
  CHECK(base::FeatureList::IsEnabled(kTabGroupsInGrid))
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

    UIView* snapshotOverlay = [[UIView alloc] init];
    snapshotOverlay.translatesAutoresizingMaskIntoConstraints = NO;
    snapshotOverlay.backgroundColor =
        [UIColor colorWithRed:0 green:0 blue:0 alpha:kSnapshotOverlayAlpha];
    [snapshotView addSubview:snapshotOverlay];
    AddSameConstraints(snapshotOverlay, snapshotView);

    // Add a favicon only if there is one.
    if (favicon) {
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

@end
