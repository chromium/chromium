// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/scoped_key_window.h"

#import "base/check_op.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

ScopedKeyWindow::ScopedKeyWindow() {
  NSSet<UIScene*>* scenes =
      ([[UIApplication sharedApplication] connectedScenes]);
  // Only one scene is supported in unittests at the moment.
  DCHECK_EQ([scenes count], 1u);
  UIScene* scene =
      [[[UIApplication sharedApplication] connectedScenes] anyObject];
  DCHECK([scene isKindOfClass:[UIWindowScene class]]);
  UIWindowScene* windowScene = static_cast<UIWindowScene*>(scene);
  for (UIWindow* window in windowScene.windows) {
    if ([window isKeyWindow]) {
      original_key_window_ = window;
    }
  }
  DCHECK(original_key_window_);
  current_key_window_ = [[UIWindow alloc] initWithWindowScene:windowScene];
  if (!current_key_window_) {
    current_key_window_ =
        [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
    original_key_window_ = GetAnyKeyWindow();
  }
  [current_key_window_ makeKeyAndVisible];
}

ScopedKeyWindow::~ScopedKeyWindow() {
  [original_key_window_ makeKeyAndVisible];
}

UIWindow* ScopedKeyWindow::Get() {
  return current_key_window_;
}
