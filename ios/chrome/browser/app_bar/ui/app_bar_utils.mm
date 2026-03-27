// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_utils.h"

AppBarPosition AppBarPositionForView(UIView* view) {
  if (!view.window || !view.window.windowScene) {
    return AppBarPosition::kNone;
  }

  UIInterfaceOrientation orientation =
      view.window.windowScene.effectiveGeometry.interfaceOrientation;

  switch (orientation) {
    case UIInterfaceOrientationPortrait:
      return AppBarPosition::kBottom;
    case UIInterfaceOrientationLandscapeLeft:
      return AppBarPosition::kLeft;
    case UIInterfaceOrientationLandscapeRight:
      return AppBarPosition::kRight;
    default:
      return AppBarPosition::kNone;
  }
}
