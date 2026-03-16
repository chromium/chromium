// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/scoped_key_window.h"

#import "base/check_op.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/test/app/uikit_test_util.h"

ScopedKeyWindow::ScopedKeyWindow() {
  NSSet<UIScene*>* scenes =
      ([[UIApplication sharedApplication] connectedScenes]);
  // Only one scene is supported in unittests at the moment.
  DCHECK_EQ([scenes count], 1u);

  UIWindowScene* window_scene = chrome_test_util::GetAnyWindowScene();
  original_key_window_ = window_scene.keyWindow;
  DCHECK(original_key_window_);

  current_key_window_ = [[UIWindow alloc] initWithWindowScene:window_scene];
  DCHECK(current_key_window_);

  [current_key_window_ makeKeyAndVisible];
}

ScopedKeyWindow::~ScopedKeyWindow() {
  [original_key_window_ makeKeyAndVisible];
}

UIWindow* ScopedKeyWindow::Get() {
  return current_key_window_;
}
