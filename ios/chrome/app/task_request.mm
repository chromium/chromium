// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/ios/block_types.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/url_opener.h"
#import "ios/chrome/app/application_delegate/url_opener_params.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/task_request+testing.h"
#import "ios/chrome/browser/intents/model/user_activity_browser_agent.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_delegate.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"

@interface TaskRequestForTesting : TaskRequest
@end

@implementation TaskRequestForTesting {
  ProceduralBlock _executeBlock;
}

- (instancetype)initWithSceneID:(std::string_view)sceneID
                   executeBlock:(ProceduralBlock)executeBlock {
  if ((self = [super initWithSceneID:sceneID])) {
    CHECK(executeBlock);
    _executeBlock = executeBlock;
  }
  return self;
}

- (void)execute {
  _executeBlock();
}

@end

@interface TaskRequest () {
  std::string _sceneSessionID;
}

@property(nonatomic, assign) TaskSource source;

// Properties needed to handle a shortcut item.
@property(nonatomic, copy) ShortcutCompletionHandler shortcutHandler;
@property(nonatomic, strong) UIApplicationShortcutItem* shortcutItem;

// Properties needed to handle a NSUserActivity item.
@property(nonatomic, strong) NSUserActivity* userActivity;

// Property needed to handle a URL context.
@property(nonatomic, strong) UIOpenURLContext* URLContext;

@end

@implementation TaskRequest

@synthesize minimumStage = _minimumStage;

- (std::string_view)sceneSessionID {
  return _sceneSessionID;
}

- (instancetype)initWithURLContext:(UIOpenURLContext*)URLContext
                        sceneState:(SceneState*)sceneState
                        taskSource:(TaskSource)taskSource {
  self = [super init];
  if (self) {
    // TODO(crbug.com/462018636): Minimum stage can be different in this case,
    // handle all scenarios based on the received options (check bookmarks
    // feature).
    CHECK(IsEnableNewStartupFlowEnabled());
    _source = taskSource;
    _minimumStage = TaskExecutionStage::TaskExecutionUIReady;
    _sceneSessionID = sceneState.sceneSessionID;
    _URLContext = URLContext;
  }
  return self;
}

- (instancetype)initWithUserActivity:(NSUserActivity*)userActivity
                          sceneState:(SceneState*)sceneState
                          taskSource:(TaskSource)taskSource {
  self = [super init];
  if (self) {
    CHECK(IsEnableNewStartupFlowEnabled());
    _source = taskSource;
    _minimumStage = TaskExecutionStage::TaskExecutionUIReady;
    _sceneSessionID = sceneState.sceneSessionID;
    _userActivity = userActivity;
  }
  return self;
}

- (instancetype)initWithShortcutItem:(UIApplicationShortcutItem*)shortcutItem
                          sceneState:(SceneState*)sceneState
                          taskSource:(TaskSource)taskSource
                             handler:(ShortcutCompletionHandler)handler {
  self = [super init];
  if (self) {
    CHECK(IsEnableNewStartupFlowEnabled());
    _source = taskSource;
    _minimumStage = TaskExecutionStage::TaskExecutionUIReady;
    _sceneSessionID = sceneState.sceneSessionID;
    _shortcutItem = shortcutItem;
    _shortcutHandler = [handler copy];
  }
  return self;
}

// Initializer used for tests.
- (instancetype)initWithSceneID:(std::string_view)sceneID {
  if ((self = [super init])) {
    _sceneSessionID = sceneID;
  }
  return self;
}

// Factory used for tests.
+ (instancetype)taskForTestingWithSceneID:(std::string_view)sceneID
                             executeBlock:(ProceduralBlock)block {
  return [[TaskRequestForTesting alloc] initWithSceneID:sceneID
                                           executeBlock:block];
}

- (void)execute {
  switch (_source) {
    case TaskSource::TaskSourceColdStart:
      // TODO(crbug.com/462018636): Handle cold start logic.
      break;
    case TaskSource::TaskSourceContextURL:
      [self executeContextURL];
      break;
    case TaskSource::TaskSourceUserActivity:
      [self executeUserActivity];
      break;
    case TaskSource::TaskSourceQuickAction:
      [self executeShortcutItem];
      break;
  }
}

#pragma mark - Private

// TODO(crbug.com/462018636): Find a better solution to get the SceneState from
// the sceneSessionID.
- (SceneState*)sceneStateFromSessionID {
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    SceneDelegate* sceneDelegate =
        base::apple::ObjCCast<SceneDelegate>(scene.delegate);
    if (sceneDelegate &&
        sceneDelegate.sceneState.sceneSessionID == _sceneSessionID) {
      return sceneDelegate.sceneState;
    }
  }
  return nil;
}

- (void)executeShortcutItem {
  SceneState* sceneState = [self sceneStateFromSessionID];
  CHECK(sceneState);
  Browser* browser =
      sceneState.browserProviderInterface.currentBrowserProvider.browser;
  CHECK(browser);

  UserActivityBrowserAgent* userActivityBrowserAgent =
      UserActivityBrowserAgent::FromBrowser(browser);
  BOOL handledShortcutItem =
      userActivityBrowserAgent->Handle3DTouchApplicationShortcuts(
          self.shortcutItem);
  if (_shortcutHandler) {
    _shortcutHandler(handledShortcutItem);
  }
}

- (void)executeUserActivity {
  SceneState* sceneState = [self sceneStateFromSessionID];
  CHECK(sceneState);
  Browser* browser =
      sceneState.browserProviderInterface.currentBrowserProvider.browser;
  CHECK(browser);

  PrefService* prefs = sceneState.profileState.profile->GetPrefs();
  UserActivityBrowserAgent* userActivityBrowserAgent =
      UserActivityBrowserAgent::FromBrowser(browser);
  if (IsIncognitoPolicyApplied(prefs) &&
      !userActivityBrowserAgent->ProceedWithUserActivity(self.userActivity)) {
    // TODO(crbug.com/462018636): Find a centralized solution to handle toasts
    // for all intent types.
    userActivityBrowserAgent->ShowToastWhenOpenExternalIntentInUnexpectedMode();
  } else {
    userActivityBrowserAgent->ContinueUserActivity(self.userActivity, YES);
  }
}

- (void)executeContextURL {
  SceneState* sceneState = [self sceneStateFromSessionID];
  CHECK(sceneState);

  NSSet* URLContextSet = [NSSet setWithObject:self.URLContext];
  // If the SystemIdentityManager handles the URL context, return early to avoid
  // opening the URL twice.
  if (GetApplicationContext()
          ->GetSystemIdentityManager()
          ->HandleSessionOpenURLContexts(sceneState.scene, URLContextSet)) {
    return;
  }
  ProfileState* profileState = sceneState.profileState;
  URLOpenerParams* options =
      [[URLOpenerParams alloc] initWithUIOpenURLContext:self.URLContext];
  [URLOpener openURL:options
          applicationActive:YES
                  tabOpener:sceneState.controller
      connectionInformation:sceneState.controller
         startupInformation:profileState.startupInformation
                prefService:profileState.profile->GetPrefs()
                  initStage:profileState.initStage];
}

@end
