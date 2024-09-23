// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/scene_delegate.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "components/breadcrumbs/core/breadcrumb_persistent_storage_util.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/app/chrome_overlay_window.h"
#import "ios/chrome/app/main_application_delegate.h"
#import "ios/chrome/browser/appearance/ui_bundled/appearance_customization.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"

namespace {

NSString* const kOriginDetectedKey = @"OriginDetectedKey";

// Set the breadcrumbs log in PreviousSessionInfo.
void SyncBreadcrumbsLog() {
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    base::FilePath storage_dir;
    bool result = base::PathService::Get(ios::DIR_USER_DATA, &storage_dir);
    DCHECK(result);
    const base::FilePath breadcrumbs_file_path =
        breadcrumbs::GetBreadcrumbPersistentStorageFilePath(storage_dir);
    dispatch_async(
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
          NSString* breadcrumbs = [NSString
              stringWithContentsOfFile:base::SysUTF8ToNSString(
                                           breadcrumbs_file_path.value())
                              encoding:NSUTF8StringEncoding
                                 error:NULL];
          [[PreviousSessionInfo sharedInstance] setBreadcrumbsLog:breadcrumbs];
        });
  });
}
}  // namespace

@implementation SceneDelegate

@synthesize sceneState = _sceneState;
@synthesize sceneController = _sceneController;

- (SceneState*)sceneState {
  if (!_sceneState) {
    MainApplicationDelegate* appDelegate =
        base::apple::ObjCCastStrict<MainApplicationDelegate>(
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
    // app is restored. Sync the breadcrumbs log as early as possible, before
    // any MetricKit crash reports may come in.
    SyncBreadcrumbsLog();

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

#pragma mark - UISceneDelegate

- (void)scene:(UIScene*)scene
    willConnectToSession:(UISceneSession*)session
                 options:(UISceneConnectionOptions*)connectionOptions {
  SceneState* sceneState = self.sceneState;
  sceneState.scene = base::apple::ObjCCastStrict<UIWindowScene>(scene);
  sceneState.currentOrigin = [self originFromSession:session
                                             options:connectionOptions];
  sceneState.activationLevel = SceneActivationLevelBackground;
  sceneState.connectionOptions = connectionOptions;
  if (connectionOptions.shortcutItem != nil ||
      connectionOptions.URLContexts.count != 0 ||
      connectionOptions.userActivities.count != 0) {
    sceneState.startupHadExternalIntent = YES;
  }
}

- (void)sceneDidDisconnect:(UIScene*)scene {
  CHECK(_sceneState);
  self.sceneState.activationLevel = SceneActivationLevelDisconnected;
  _sceneState = nil;
  // Setting the level to Disconnected had the side effect of tearing down the
  // controller’s UI.
  _sceneController = nil;
}

#pragma mark - private

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
