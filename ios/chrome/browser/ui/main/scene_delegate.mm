// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/scene_delegate.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/app/chrome_overlay_window.h"
#import "ios/chrome/app/main_application_delegate.h"
#import "ios/chrome/browser/crash_report/main_thread_freeze_detector.h"
#import "ios/chrome/browser/ui/appearance/appearance_customization.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kOriginDetectedKey = @"OriginDetectedKey";

@implementation SceneDelegate

- (SceneState*)sceneState {
  if (!_sceneState) {
    MainApplicationDelegate* appDelegate =
        base::mac::ObjCCastStrict<MainApplicationDelegate>(
            UIApplication.sharedApplication.delegate);
    _sceneState = [[SceneState alloc] initWithAppState:appDelegate.appState];
    _sceneController = [[SceneController alloc] initWithSceneState:_sceneState];
    _sceneState.controller = _sceneController;
  }
  return _sceneState;
}

#pragma mark - UIWindowSceneDelegate

// This getter is called when the SceneDelegate is created. Returning a
// ChromeOverlayWindow allows UIKit to use that as the main window for this
// scene.
- (UIWindow*)window {
  if (!_window) {
    // With iOS15 pre-warming, this appears to be the first callback after the
    // app is restored.  This is a no-op in non-prewarming.
    [[MainThreadFreezeDetector sharedInstance] start];

    // Sizing of the window is handled by UIKit.
    _window = [[ChromeOverlayWindow alloc] init];
    CustomizeUIWindowAppearance(_window);

    // Assign an a11y identifier for using in EGTest.
    // See comment for [ChromeMatchersAppInterface windowWithNumber:] matcher
    // for context.
    _window.accessibilityIdentifier = [NSString
        stringWithFormat:@"%ld",
                         UIApplication.sharedApplication.connectedScenes.count -
                             1];
  }
  return _window;
}

#pragma mark Connecting and Disconnecting the Scene

- (void)scene:(UIScene*)scene
    willConnectToSession:(UISceneSession*)session
                 options:(UISceneConnectionOptions*)connectionOptions {
  self.sceneState.scene = base::mac::ObjCCastStrict<UIWindowScene>(scene);
  self.sceneState.currentOrigin = [self originFromSession:session
                                                  options:connectionOptions];
  self.sceneState.activationLevel = SceneActivationLevelBackground;
  self.sceneState.connectionOptions = connectionOptions;
  if (connectionOptions.URLContexts || connectionOptions.shortcutItem) {
    self.sceneState.startupHadExternalIntent = YES;
  }
}

- (WindowActivityOrigin)originFromSession:(UISceneSession*)session
                                  options:(UISceneConnectionOptions*)options {
  WindowActivityOrigin origin = WindowActivityUnknownOrigin;

  // When restoring the session, the origin is set to restore to avoid
  // observers treating this as a new request. Also the only time the origin
  // can be correctly detected is on the first observation, because subsequent
  // view are restored, and do not contain the user activities. The key
  // kOriginDetectedKey is set in the session uerInfo to track just that.
  if (session.userInfo[kOriginDetectedKey]) {
    origin = WindowActivityRestoredOrigin;
  } else {
    NSMutableDictionary* userInfo =
        [NSMutableDictionary dictionaryWithDictionary:session.userInfo];
    userInfo[kOriginDetectedKey] = kOriginDetectedKey;
    session.userInfo = userInfo;
    origin = WindowActivityExternalOrigin;
    for (NSUserActivity* activity in options.userActivities) {
      WindowActivityOrigin activityOrigin = OriginOfActivity(activity);
      if (activityOrigin != WindowActivityUnknownOrigin) {
        origin = activityOrigin;
        break;
      }
    }
  }

  return origin;
}

- (void)sceneDidDisconnect:(UIScene*)scene {
  self.sceneState.activationLevel = SceneActivationLevelUnattached;
}

#pragma mark Transitioning to the Foreground

- (void)sceneWillEnterForeground:(UIScene*)scene {
  self.sceneState.currentOrigin = WindowActivityRestoredOrigin;
  self.sceneState.activationLevel = SceneActivationLevelForegroundInactive;
}

- (void)sceneDidBecomeActive:(UIScene*)scene {
  self.sceneState.currentOrigin = WindowActivityRestoredOrigin;
  self.sceneState.activationLevel = SceneActivationLevelForegroundActive;
}

#pragma mark Transitioning to the Background

- (void)sceneWillResignActive:(UIScene*)scene {
  self.sceneState.activationLevel = SceneActivationLevelForegroundInactive;
}

- (void)sceneDidEnterBackground:(UIScene*)scene {
  self.sceneState.activationLevel = SceneActivationLevelBackground;
}

- (void)scene:(UIScene*)scene
    openURLContexts:(NSSet<UIOpenURLContext*>*)URLContexts {
  DCHECK(!self.sceneState.URLContextsToOpen);
  self.sceneState.startupHadExternalIntent = YES;
  self.sceneState.URLContextsToOpen = URLContexts;
}

- (void)windowScene:(UIWindowScene*)windowScene
    performActionForShortcutItem:(UIApplicationShortcutItem*)shortcutItem
               completionHandler:(void (^)(BOOL succeeded))completionHandler {
  [_sceneController performActionForShortcutItem:shortcutItem
                               completionHandler:completionHandler];
}

- (void)scene:(UIScene*)scene
    continueUserActivity:(NSUserActivity*)userActivity {
  self.sceneState.pendingUserActivity = userActivity;
}

@end
