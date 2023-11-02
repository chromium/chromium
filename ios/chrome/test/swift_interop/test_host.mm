// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
                 options:(UISceneConnectionOptions*)connectionOptions
    API_AVAILABLE(ios(13)) {
  _window =
      [[UIWindow alloc] initWithWindowScene:static_cast<UIWindowScene*>(scene)];

  [_window setRootViewController:[[UIViewController alloc] init]];
}

- (void)sceneDidDisconnect:(UIScene*)scene API_AVAILABLE(ios(13)) {
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
