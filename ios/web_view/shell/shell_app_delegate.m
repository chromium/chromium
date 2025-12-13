// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_app_delegate.h"

#import <ChromeWebView/ChromeWebView.h>

#import "ios/web_view/shell/shell_view_controller.h"

@implementation ShellAppDelegate

@synthesize window = _window;

- (BOOL)application:(UIApplication*)application
    willFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  [[CWVGlobalState sharedInstance] earlyInit];
  [[CWVGlobalState sharedInstance] start];

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
  [[CWVGlobalState sharedInstance] stop];
}

- (BOOL)application:(UIApplication*)application
    shouldSaveSecureApplicationState:(NSCoder*)coder {
  return YES;
}

- (BOOL)application:(UIApplication*)application
    shouldRestoreSecureApplicationState:(NSCoder*)coder {
  return YES;
}

- (void)application:(UIApplication*)application
    didDecodeRestorableStateWithCoder:(NSCoder*)coder {
}

- (void)application:(UIApplication*)application
    willEncodeRestorableStateWithCoder:(NSCoder*)coder {
}

- (UIViewController*)application:(UIApplication*)application
    viewControllerWithRestorationIdentifierPath:
        (NSArray<NSString*>*)identifierComponents
                                          coder:(NSCoder*)coder {
  const NSUInteger identifiersCount = identifierComponents.count;
  if (identifiersCount > 0) {
    NSString* identifier = identifierComponents[identifiersCount - 1];
    UIViewController* rootViewController = self.window.rootViewController;
    if ([identifier isEqualToString:rootViewController.restorationIdentifier]) {
      return rootViewController;
    }
  }
  return nil;
}

@end
