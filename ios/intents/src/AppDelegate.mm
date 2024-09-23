// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/intents/src/AppDelegate.h"

namespace {
using LaunchOptions = NSDictionary<UIApplicationLaunchOptionsKey, id>;
}

@implementation AppDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(LaunchOptions*)launchOptions {
  return YES;
}

- (UISceneConfiguration*)application:(UIApplication*)application
    configurationForConnectingSceneSession:(UISceneSession*)sceneSession
                                   options:(UISceneConnectionOptions*)options {
  return [[UISceneConfiguration alloc] initWithName:@"Default Configuration"
                                        sessionRole:sceneSession.role];
}

- (void)application:(UIApplication*)application
    didDiscardSceneSessions:(NSSet<UISceneSession*>*)sceneSessions {
}

@end
