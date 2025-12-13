// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_animation_utils.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

CAShapeLayer* CreateTabGridAnimationMaskWithFrame(CGRect frame) {
  CAShapeLayer* mask = [CAShapeLayer layer];
  mask.frame = frame;

  UIBezierPath* path = [UIBezierPath bezierPathWithRect:frame];
  mask.path = path.CGPath;

  return mask;
}

UIBezierPath* CreateTabGridAnimationRoundedRectPathWithInsets(
    CGRect frame,
    CGFloat bottom_inset,
    CGFloat top_inset,
    CGFloat corner_radius) {
  CGRect cropped_snapshot_frame =
      CGRectMake(frame.origin.x, frame.origin.y + top_inset, frame.size.width,
                 frame.size.height - (top_inset + bottom_inset));

  return [UIBezierPath bezierPathWithRoundedRect:cropped_snapshot_frame
                                    cornerRadius:corner_radius];
}

UIView* CreateTabGridAnimationBackgroundView(bool is_incognito) {
  UIView* backgroundView = [[UIView alloc] init];
  if (is_incognito) {
    backgroundView.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  }
  backgroundView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  return backgroundView;
}

UIView* CreateToolbarSnapshotBackgroundView(UIView* snapshot_view,
                                            UIView* animated_view,
                                            BOOL is_incognito,
                                            BOOL align_to_bottom,
                                            CGRect reference_frame) {
  if (!snapshot_view) {
    return nil;
  }

  UIView* toolbar_background =
      CreateTabGridAnimationBackgroundView(is_incognito);
  [animated_view addSubview:toolbar_background];
  [toolbar_background addSubview:snapshot_view];

  CGRect new_frame = snapshot_view.frame;
  if (align_to_bottom) {
    new_frame.origin =
        CGPointMake(0, reference_frame.size.height - new_frame.size.height);
  } else {
    new_frame.origin = CGPointMake(0, 0);
  }
  toolbar_background.frame = new_frame;

  // Reuse the same frame, but at a (0,0) origin for the snapshot in the
  // background view.
  new_frame.origin = CGPointMake(0, 0);
  snapshot_view.frame = new_frame;
  return toolbar_background;
}

void SetAnchorPointToFrameCenter(UIView* view, CGRect frame) {
  // Calculate the center of the frame.
  CGPoint center = CGPointMake(CGRectGetMidX(frame), CGRectGetMidY(frame));

  // Convert the center point to the coordinate system of view's superview.
  CGPoint center_in_grid_parent = [view.superview convertPoint:center
                                                      fromView:nil];

  // Adjust the point to be relative to view's origin.
  CGPoint center_in_grid =
      CGPointMake(center_in_grid_parent.x - view.frame.origin.x,
                  center_in_grid_parent.y - view.frame.origin.y);

  // Calculate the relative center within `view`.
  CGPoint relative_center_in_grid =
      CGPointMake(center_in_grid.x / view.frame.size.width,
                  center_in_grid.y / view.frame.size.height);

  // Set the `view`'s anchor point to the `frame`'s center.
  CGRect old_frame = view.frame;
  view.layer.anchorPoint = relative_center_in_grid;
  view.frame = old_frame;
}
