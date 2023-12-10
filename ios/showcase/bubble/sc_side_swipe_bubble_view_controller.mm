// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/bubble/sc_side_swipe_bubble_view_controller.h"

#import "ios/chrome/browser/ui/bubble/bubble_constants.h"
#import "ios/chrome/browser/ui/bubble/side_swipe_bubble/side_swipe_bubble_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/showcase/bubble/constants.h"

@implementation SCSideSwipeBubbleViewController

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  UILayoutGuide* guide = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:guide];
  AddSameConstraintsToSides(guide, self.view.safeAreaLayoutGuide,
                            LayoutSides::kTop);
  AddSameConstraintsToSides(
      guide, self.view,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom);
  SideSwipeBubbleView* sideSwipeBubbleView =
      [[SideSwipeBubbleView alloc] initWithText:@"Lorem ipsum dolor"
                             bubbleBoundingSize:guide.layoutFrame.size
                                 arrowDirection:BubbleArrowDirectionUp];
  [sideSwipeBubbleView setTranslatesAutoresizingMaskIntoConstraints:NO];

  __weak SCSideSwipeBubbleViewController* weakSelf = self;
  sideSwipeBubbleView.dismissCallback =
      ^(IPHDismissalReasonType reason,
        feature_engagement::Tracker::SnoozeAction _) {
        UILabel* label = [[UILabel alloc] init];
        switch (reason) {
          case IPHDismissalReasonType::kTimedOut:
            label.text = kSideSwipeBubbleViewTimeoutText;
            break;
          case IPHDismissalReasonType::kTappedClose:
            label.text = kSideSwipeBubbleViewDismissedByTapText;
            break;
          default:
            break;
        }
        label.textColor = UIColor.whiteColor;
        [weakSelf.view addSubview:label];
      };
  [self.view addSubview:sideSwipeBubbleView];
  AddSameConstraints(sideSwipeBubbleView, guide);
  [sideSwipeBubbleView startAnimation];
}

@end
