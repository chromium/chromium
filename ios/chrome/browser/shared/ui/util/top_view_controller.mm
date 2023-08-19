// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/top_view_controller.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

namespace top_view_controller {

UIViewController* TopPresentedViewController() {
  return TopPresentedViewControllerFrom(GetAnyKeyWindow().rootViewController);
}
UIViewController* TopPresentedViewControllerFrom(
    UIViewController* base_view_controller) {
  UIViewController* topController = base_view_controller;
  for (UIViewController* controller = [topController presentedViewController];
       controller && ![controller isBeingDismissed];
       controller = [controller presentedViewController]) {
    topController = controller;
  }
  return topController;
}

}  // namespace top_view_controller
