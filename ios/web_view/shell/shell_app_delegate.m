// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_app_delegate.h"

#import "ios/web_view/shell/shell_view_controller.h"

@implementation ShellAppDelegate

@synthesize window = _window;

- (BOOL)application:(UIApplication*)application
    willFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  // Note that initialization of the window and the root view controller must be
  // done here, not in -application:didFinishLaunchingWithOptions: when state
  // restoration is supported.

  self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  self.window.backgroundColor = [UIColor whiteColor];
  self.window.tintColor = [UIColor darkGrayColor];

  ShellViewController* controller = [[ShellViewController alloc] init];
  // Gives a restoration identifier so that state restoration works.
  controller.restorationIdentifier = @"rootViewController";
  self.window.rootViewController = controller;

  return YES;
}

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  [self.window makeKeyAndVisible];
  return YES;
}

- (void)applicationWillResignActive:(UIApplication*)application {
}

- (void)applicationDidEnterBackground:(UIApplication*)application {
}

- (void)applicationWillEnterForeground:(UIApplication*)application {
}

- (void)applicationDidBecomeActive:(UIApplication*)application {
}

- (void)applicationWillTerminate:(UIApplication*)application {
}

- (BOOL)application:(UIApplication*)application
    shouldSaveSecureApplicationState:(NSCoder*)coder {
  return YES;
}

- (BOOL)application:(UIApplication*)application
    shouldRestoreSecureApplicationState:(NSCoder*)coder {
  // TODO(crbug.com/41312374): Make this value configurable in the settings.
  return YES;
}

@end
