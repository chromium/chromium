// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/app/web_shell_test_util.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/web/shell/view_controller.h"

namespace web {
namespace shell_test_util {

UIWindow* GetAnyKeyWindow() {
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    UIWindowScene* windowScene =
        base::apple::ObjCCastStrict<UIWindowScene>(scene);
    // Find a key window if it exists.
    for (UIWindow* window in windowScene.windows) {
      if (window.isKeyWindow) {
        return window;
      }
    }
  }

  return nil;
}

web::WebState* GetCurrentWebState() {
  ViewController* view_controller =
      static_cast<ViewController*>([GetAnyKeyWindow() rootViewController]);
  return view_controller.webState;
}

}  // namespace shell_test_util
}  // namespace web
