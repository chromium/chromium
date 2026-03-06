// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/uikit_test_util.h"

#import "base/apple/foundation_util.h"

namespace chrome_test_util {

UIWindowScene* GetAnyWindowScene() {
  UIWindowScene* scene = nil;
  for (UIScene* connectedScene in UIApplication.sharedApplication
           .connectedScenes) {
    scene = base::apple::ObjCCast<UIWindowScene>(connectedScene);
    if (scene) {
      return scene;
    }
  }

  return nil;
}

}  // namespace chrome_test_util
