// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/uikit_ui_util.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"

UIWindow* GetAnyKeyWindow() {
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    UIWindowScene* windowScene =
        base::apple::ObjCCastStrict<UIWindowScene>(scene);
    // Find a key window if it exists.
    for (UIWindow* window in windowScene.windows) {
      if (window.isKeyWindow)
        return window;
    }
  }

  return nil;
}

UIInterfaceOrientation GetInterfaceOrientation() {
  return GetAnyKeyWindow().windowScene.interfaceOrientation;
}
