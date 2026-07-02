// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_app_delegate.h"

#import <ChromeWebView/ChromeWebView.h>

#import "ios/web_view/shell/shell_view_controller.h"

@interface ShellSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(strong, nonatomic) UIWindow* window;
@end

@implementation ShellSceneDelegate

- (void)scene:(UIScene*)scene
    willConnectToSession:(UISceneSession*)session
                 options:(UISceneConnectionOptions*)connectionOptions {
  if (![scene isKindOfClass:[UIWindowScene class]]) {
    return;
  }
  UIWindowScene* windowScene = (UIWindowScene*)scene;
  self.window = [[UIWindow alloc] initWithWindowScene:windowScene];
  self.window.frame = [[UIScreen mainScreen] bounds];
  self.window.backgroundColor = [UIColor whiteColor];
  self.window.tintColor = [UIColor darkGrayColor];

  ShellViewController* controller = [[ShellViewController alloc] init];
  controller.restorationIdentifier = @"rootViewController";
  self.window.rootViewController = controller;
  [self.window makeKeyAndVisible];
}

@end

@implementation ShellAppDelegate

@synthesize window = _window;

- (BOOL)application:(UIApplication*)application
    willFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  [[CWVGlobalState sharedInstance] earlyInit];
  [[CWVGlobalState sharedInstance] start];
  return YES;
}

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
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
