// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

namespace {

void PopulateUIWindow(UIWindow* window) {
  window.backgroundColor = UIColor.whiteColor;
  [window makeKeyAndVisible];
  CGRect bounds = window.windowScene.screen.bounds;
  // Add a label with the app name.
  UILabel* label = [[UILabel alloc] initWithFrame:bounds];
  label.text = NSProcessInfo.processInfo.processName;
  label.textAlignment = NSTextAlignmentCenter;
  [window addSubview:label];

  // An NSInternalInconsistencyException is thrown if the app doesn't have a
  // root view controller. Set an empty one here.
  window.rootViewController = [[UIViewController alloc] init];
}

}  // namespace

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
  PopulateUIWindow(_window);
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
