// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@end

@implementation AppDelegate
- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
  return YES;
}
@end

@interface ChromeUnitTestSceneDelegate : NSObject <UIWindowSceneDelegate>
@property(nonatomic, strong) UIWindow* window;
@end

@implementation ChromeUnitTestSceneDelegate

- (void)scene:(UIScene*)scene
    willConnectToSession:(UISceneSession*)session
                 options:(UISceneConnectionOptions*)connectionOptions {
  _window =
      [[UIWindow alloc] initWithWindowScene:static_cast<UIWindowScene*>(scene)];

  [_window setRootViewController:[[UIViewController alloc] init]];
}

- (void)sceneDidDisconnect:(UIScene*)scene {
  _window = nil;
}

@end

int main(int argc, char* argv[]) {
  NSString* appDelegateClassName;
  @autoreleasepool {
    // Setup code that might create autoreleased objects goes here.
    appDelegateClassName = NSStringFromClass([AppDelegate class]);
  }
  return UIApplicationMain(argc, argv, nil, appDelegateClassName);
}
