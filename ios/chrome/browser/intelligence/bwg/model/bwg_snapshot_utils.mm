// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_snapshot_utils.h"

#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"

namespace {

// Gets a fullscreen snapshot of the `window`.
UIImage* GetFullscreenSnapshot(UIWindow* window) {
  if (!window) {
    return nil;
  }

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:window.frame.size];

  return [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
    [window drawViewHierarchyInRect:window.bounds afterScreenUpdates:NO];
  }];
}

// Crops a snapshot to the window's safe areas for the left, bottom and right,
// and at the top of the content area for the top of the crop. `view` is used to
// fetch a UIWindow and to find the ContentArea layout guide (needs to be in the
// same view hierarchy).
UIImage* CropSnapshot(UIImage* snapshot, UIView* view) {
  if (!snapshot || !view) {
    return nil;
  }

  NamedGuide* content_area_guide = [NamedGuide guideWithName:kContentAreaGuide
                                                        view:view];
  if (!content_area_guide) {
    return nil;
  }

  CGRect content_area_guide_frame = content_area_guide.layoutFrame;
  CGRect safe_area_frame = view.window.safeAreaLayoutGuide.layoutFrame;

  // Create the cropping rect in points at the safe area insets for all sides
  // except for top which aligns with the `content_area_guide_frame`'s top.
  const CGFloat left = safe_area_frame.origin.x;
  const CGFloat top = content_area_guide_frame.origin.y;
  const CGFloat width = safe_area_frame.size.width;
  const CGFloat height = CGRectGetMaxY(safe_area_frame) - top;
  CGRect crop_rect_in_points = CGRectMake(left, top, width, height);

  // Convert the points-based crop CGRect into pixels (since that's what CG
  // operates on).
  CGRect crop_rect_in_pixels = CGRectApplyAffineTransform(
      crop_rect_in_points,
      CGAffineTransformMakeScale(snapshot.scale, snapshot.scale));

  CGImageRef image_ref =
      CGImageCreateWithImageInRect(snapshot.CGImage, crop_rect_in_pixels);
  UIImage* cropped_snapshot = [[UIImage alloc] initWithCGImage:image_ref];

  // Release the `CGImageRef` after being consumed.
  CGImageRelease(image_ref);

  return cropped_snapshot;
}

}  // namespace

namespace bwg_snapshot_utils {

UIImage* GetCroppedFullscreenSnapshot(UIView* view) {
  UIImage* fullscreen_snapshot = GetFullscreenSnapshot(view.window);
  if (!fullscreen_snapshot) {
    return nil;
  }

  return CropSnapshot(fullscreen_snapshot, view);
}

}  // namespace bwg_snapshot_utils
