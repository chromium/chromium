// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/uikit_test_util.h"

#import "base/apple/foundation_util.h"
#import "testing/gtest/include/gtest/gtest.h"

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

UILabel* FindLabelWithText(UIView* view, NSString* text) {
  EXPECT_NE(view, nil);
  EXPECT_NE(text, nil);
  return FindViewWithPredicate<UILabel>(view, ^BOOL(UIView* candidate) {
    UILabel* label = base::apple::ObjCCast<UILabel>(candidate);
    return [label.text isEqualToString:text];
  });
}

}  // namespace chrome_test_util
