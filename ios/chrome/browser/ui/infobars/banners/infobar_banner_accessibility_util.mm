// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_accessibility_util.h"

#import "base/check.h"

void UpdateBannerAccessibilityForPresentation(
    UIViewController* presenting_view_controller,
    UIView* banner_view) {
  if (!presenting_view_controller.view || !banner_view) {
    // Sometimes banner presentation completion block results in either being
    // null (crbug.com/1215961). Return early while keeping the DCHECK fail to
    // minimize crashes while still keeping pre-stable crash signal for further
    // investigation.
    DCHECK(banner_view);
    DCHECK(presenting_view_controller);
    return;
  }
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
