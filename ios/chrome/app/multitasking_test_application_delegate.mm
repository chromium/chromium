// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/multitasking_test_application_delegate.h"

#import "ios/chrome/app/multitasking_test_scene_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation MultitaskingTestApplicationDelegate

// Override the scene delegate class.
- (UISceneConfiguration*)application:(UIApplication*)application
    configurationForConnectingSceneSession:
        (UISceneSession*)connectingSceneSession
                                   options:(UISceneConnectionOptions*)options {
  UISceneConfiguration* sceneConfiguration = [UISceneConfiguration
      configurationWithName:@"TestResizedWindowConfiguration"
                sessionRole:UIWindowSceneSessionRoleApplication];
  sceneConfiguration.delegateClass = [MultitaskingTestSceneDelegate class];
  return sceneConfiguration;
}

@end
