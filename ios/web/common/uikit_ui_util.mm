// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/uikit_ui_util.h"

#import <UIKit/UIKit.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UIWindow* GetAnyKeyWindow() {
#if !defined(__IPHONE_13_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_13_0
  return [UIApplication sharedApplication].keyWindow;
#else
  // In iOS 15 and later key windows are a deprecated concept. Window state
  // should be determined at the scene rather than the application level.
  if (@available(iOS 15, *)) {
    NSSet<UIScene*>* windowScenes =
        [UIApplication sharedApplication].connectedScenes;
    for (UIScene* scene : windowScenes) {
      if ([scene.delegate
              conformsToProtocol:@protocol(UIWindowSceneDelegate)]) {
        return [(id<UIWindowSceneDelegate>)scene.delegate window];
      }
    }
  } else {
    NSArray<UIWindow*>* windows = [UIApplication sharedApplication].windows;
    // Find a key window if it exists.
    for (UIWindow* window in windows) {
      if (window.isKeyWindow)
        return window;
    }
  }
  return nil;
#endif
}

UIInterfaceOrientation GetInterfaceOrientation() {
#if !defined(__IPHONE_13_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_13_0
  return [[UIApplication sharedApplication] statusBarOrientation];
#else
  return GetAnyKeyWindow().windowScene.interfaceOrientation;
#endif
}
