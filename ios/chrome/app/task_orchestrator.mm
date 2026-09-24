// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_orchestrator.h"

#import <UIKit/UIKit.h>

#import <map>
#import <optional>
#import <string>
#import <string_view>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/app/application_delegate/app_init_stage.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/app/change_profile_continuation.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/task_scheduling_outcome.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_authentication_continuation.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_delegate.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/url_context.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/account_profile_mapper.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_utils.h"
#import "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace {
struct SceneInfo {
  // Current stage of a scene.
  TaskExecutionStage current_stage;
  // Whether a profile or account switch is currently in progress for this
  // scene.
  bool is_switching;
  // Last Gaia ID for which a switch was initiated in the current pending task
  // batch.
  NSString* switched_gaia_id;
  // Tasks to be executed on a scene.
  NSMutableArray<TaskRequest*>* pending_tasks;

  // Adds a task to pending_tasks.
  void AddTask(TaskRequest* task) {
    if (!pending_tasks) {
      pending_tasks = [NSMutableArray array];
    }
    [pending_tasks addObject:task];
  }
};

// Returns an identifier for `scene` that will be stable for the current
// execution of the application and is valid even during the early stage of the
// application startup (i.e. before -sceneSessionID is assigned to the
// SceneState). This uses the -persistentIdentifier of the UISceneSession.
std::string GetSceneIdentifier(UIScene* scene) {
  return base::SysNSStringToUTF8(scene.session.persistentIdentifier);
}

// Returns the active `SceneState` for `task.scene`.
SceneState* GetSceneStateForTask(TaskRequest* task) {
  SceneDelegate* scene_delegate =
      base::apple::ObjCCast<SceneDelegate>(task.scene.delegate);
  return scene_delegate.sceneState;
}

}  // namespace

@interface TaskOrchestrator () {
  // SceneInfo needed to execute tasks per scene.
  // It's okay to use persistentIdentifier here because we don't care if the ID
  // changes when the app is closed.
  // TODO(crbug.com/462018636): Add implementation to handle the case where a
  // task can be executed on any scene.
  absl::flat_hash_map<std::string, SceneInfo> _tasksPerScene;
}

@end

@implementation TaskOrchestrator

- (instancetype)init {
  if ((self = [super init])) {
    CHECK(IsEnableNewStartupFlowEnabled());
  }
  return self;
}

- (void)addTaskRequest:(TaskRequest*)task {
  // Don't add the task if it requires an account/profile change and there is
  // already a task in a queue requiring a change to a different account.
  if ([self shouldDropTaskRequest:task]) {
    return;
  }

  const std::string sceneKey = GetSceneIdentifier(task.scene);
  CHECK(!sceneKey.empty());

  SceneInfo& sceneInfo = _tasksPerScene[sceneKey];
  sceneInfo.AddTask(task);
  [self executeTasksForScene:sceneKey];
}

- (void)updateToStage:(TaskExecutionStage)stage
             forScene:(SceneState*)sceneState {
  const std::string sceneKey = GetSceneIdentifier(sceneState.scene);
  if (sceneKey.empty()) {
    return;
  }

  TaskExecutionStage previousStage = _tasksPerScene[sceneKey].current_stage;
  _tasksPerScene[sceneKey].current_stage = stage;
  if (previousStage < stage) {
    [self executeTasksForScene:sceneKey];
  }
}

- (NSString*)gaiaIDForScene:(SceneState*)sceneState {
  const std::string sceneKey = GetSceneIdentifier(sceneState.scene);
  if (sceneKey.empty()) {
    return nil;
  }

  auto it = _tasksPerScene.find(sceneKey);
  if (it == _tasksPerScene.end()) {
    return nil;
  }

  SceneInfo& sceneInfo = it->second;
  for (TaskRequest* pendingTask in sceneInfo.pending_tasks) {
    if (pendingTask.gaiaID) {
      return pendingTask.gaiaID;
    }
  }
  return nil;
}

#pragma mark - Private

// Returns whether the task should be dropped.
- (BOOL)shouldDropTaskRequest:(TaskRequest*)task {
  NSString* taskGaiaID = task.gaiaID;
  if (!taskGaiaID) {
    base::UmaHistogramEnumeration("IOS.TaskOrchestrator.TaskSchedulingOutcome",
                                  TaskSchedulingOutcome::kScheduled);
    return NO;
  }

  const std::string sceneKey = GetSceneIdentifier(task.scene);
  SceneInfo& sceneInfo = _tasksPerScene[sceneKey];
  for (TaskRequest* pendingTask in sceneInfo.pending_tasks) {
    NSString* pendingGaiaID = pendingTask.gaiaID;
    if (pendingGaiaID && ![pendingGaiaID isEqualToString:taskGaiaID]) {
      base::UmaHistogramEnumeration(
          "IOS.TaskOrchestrator.TaskSchedulingOutcome",
          TaskSchedulingOutcome::kDroppedGaiaMismatch);
      return YES;
    }
  }
  base::UmaHistogramEnumeration("IOS.TaskOrchestrator.TaskSchedulingOutcome",
                                TaskSchedulingOutcome::kScheduled);
  return NO;
}

// Internal logic to filter and execute tasks based on the current stage.
- (void)executeTasksForScene:(std::string_view)sceneKey {
  SceneInfo& sceneInfo = _tasksPerScene[sceneKey];
  if (sceneInfo.is_switching) {
    return;
  }

  // Initiate any required profile or account switch as early as
  // `TaskExecutionProfileLoaded` (before `TaskExecutionUIReady` and before
  // task execution).
  if (sceneInfo.current_stage >=
          TaskExecutionStage::TaskExecutionProfileLoaded &&
      [self switchProfileOrAccountIfNeededForScene:sceneKey]) {
    return;
  }

  NSMutableArray<TaskRequest*>* pendingTasks =
      std::exchange(sceneInfo.pending_tasks, [NSMutableArray new]);
  bool hasPendingGaiaTask = false;
  for (TaskRequest* task in pendingTasks) {
    if (task.minimumStage <= sceneInfo.current_stage) {
      [task execute];
    } else {
      if (task.gaiaID.length > 0) {
        hasPendingGaiaTask = true;
      }
      sceneInfo.AddTask(task);
    }
  }
  if (!hasPendingGaiaTask) {
    sceneInfo.switched_gaia_id = nil;
  }
}

// Initiates a profile or account switch via `ChangeProfileCommands` if a
// pending task for `sceneKey` requires a different profile or identity.
// Returns YES if an asynchronous switch was started.
- (BOOL)switchProfileOrAccountIfNeededForScene:(std::string_view)sceneKey {
  SceneInfo& sceneInfo = _tasksPerScene[sceneKey];
  TaskRequest* gaiaTask = nil;
  for (TaskRequest* task in sceneInfo.pending_tasks) {
    if (task.gaiaID.length > 0) {
      gaiaTask = task;
      break;
    }
  }
  if (!gaiaTask ||
      [sceneInfo.switched_gaia_id isEqualToString:gaiaTask.gaiaID]) {
    return NO;
  }

  SceneState* sceneState = GetSceneStateForTask(gaiaTask);
  ProfileIOS* profile = sceneState.profileState.profile;
  if (!sceneState || !profile ||
      sceneState.profileState.appState.initStage < AppInitStage::kFinal) {
    return NO;
  }

  BOOL shouldSignOut = [gaiaTask.gaiaID isEqualToString:app_group::kNoAccount];
  std::optional<std::string> targetProfileName;
  if (shouldSignOut) {
    targetProfileName = GetApplicationContext()
                            ->GetProfileManager()
                            ->GetProfileAttributesStorage()
                            ->GetPersonalProfileName();
  } else {
    targetProfileName = GetApplicationContext()
                            ->GetAccountProfileMapper()
                            ->FindProfileNameForGaiaID(GaiaId(gaiaTask.gaiaID));
  }
  if (!targetProfileName.has_value()) {
    return NO;
  }

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile->GetOriginalProfile());
  CoreAccountInfo primaryAccount =
      identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  const bool needsProfileSwitch =
      (*targetProfileName != profile->GetProfileName());
  const bool needsAccountSwitch =
      shouldSignOut ? !primaryAccount.gaia.empty()
                    : (primaryAccount.gaia != GaiaId(gaiaTask.gaiaID));
  if (!needsProfileSwitch && !needsAccountSwitch) {
    return NO;
  }

  id<ChangeProfileCommands> changeProfileHandler =
      HandlerForProtocol(sceneState.profileState.appState.appCommandDispatcher,
                         ChangeProfileCommands);
  if (!changeProfileHandler) {
    return NO;
  }

  ChangeProfileReason reason =
      app_group::IsShareExtensionCommandURL(gaiaTask.URL)
          ? ChangeProfileReason::kSwitchAccountsFromShareExtension
          : ChangeProfileReason::kSwitchAccountsFromWidget;

  AccountSwitchType switchType =
      shouldSignOut ? AccountSwitchType::kSignOut : AccountSwitchType::kSignIn;
  URLContext* urlContext =
      [[URLContext alloc] initWithContext:nil
                                   gaiaID:GaiaId(gaiaTask.gaiaID)
                                     type:switchType];

  __weak __typeof(self) weakSelf = self;
  ChangeProfileContinuation completionContinuation = base::BindOnce(
      [](__weak TaskOrchestrator* orchestrator, std::string scene_key,
         SceneState* scene_state, base::OnceClosure closure) {
        [orchestrator
            onProfileOrAccountSwitchCompletedForScene:scene_key
                                           completion:std::move(closure)];
      },
      weakSelf, std::string(sceneKey));

  ChangeProfileContinuation continuation = ChainChangeProfileContinuations(
      CreateChangeProfileAuthenticationContinuation(urlContext,
                                                    /*contexts=*/nil),
      std::move(completionContinuation));

  sceneInfo.is_switching = true;
  sceneInfo.switched_gaia_id = [gaiaTask.gaiaID copy];

  [changeProfileHandler changeProfile:*targetProfileName
                             forScene:sceneState
                               reason:reason
                         continuation:std::move(continuation)];
  return YES;
}

// Called when a profile or account switch initiated by `ChangeProfileCommands`
// has completed its authentication continuation.
- (void)onProfileOrAccountSwitchCompletedForScene:(std::string_view)sceneKey
                                       completion:(base::OnceClosure)closure {
  SceneInfo& sceneInfo = _tasksPerScene[sceneKey];
  sceneInfo.is_switching = false;
  [self executeTasksForScene:sceneKey];
  std::move(closure).Run();
}

@end
