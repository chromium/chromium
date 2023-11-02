// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_accessibility_util.h"

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void UpdateBannerAccessibilityForPresentation(
    UIViewController* presenting_view_controller,
    UIView* banner_view) {
  DCHECK(presenting_view_controller);
  DCHECK(banner_view);
  // Set the banner's superview accessibilityViewIsModal property to NO. This
  // will allow the selection of the banner sibling views (e.g. the
  // presentingViewController views).
  banner_view.superview.accessibilityViewIsModal = NO;

  // Make sure the banner is an accessibility element of
  // `presenting_view_controller`.
  presenting_view_controller.accessibilityElements =
      @[ banner_view, presenting_view_controller.view ];

  // Finally, focus the banner.
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  banner_view);
}

void UpdateBannerAccessibilityForDismissal(
    UIViewController* presenting_view_controller) {
  DCHECK(presenting_view_controller);
  // Remove the Banner as an accessibility element.
  presenting_view_controller.accessibilityElements =
      @[ presenting_view_controller.view ];
}
