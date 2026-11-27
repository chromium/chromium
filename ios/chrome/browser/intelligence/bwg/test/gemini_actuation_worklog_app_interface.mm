// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/test/gemini_actuation_worklog_app_interface.h"

#import <vector>

#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/test/ios/wait_util.h"
#import "components/actor/core/task_source_info.h"
#import "components/password_manager/core/browser/actor_login/actor_login_types.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_coordinator.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_intervention_delegate.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_controller.h"
#import "ios/chrome/browser/intelligence/bwg/test/gemini_actuation_worklog_test_helper.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"

namespace {

// Returns the list of task IDs created by this app interface.
std::vector<actor::ActorTaskId>& GetCreatedTaskIds() {
  static base::NoDestructor<std::vector<actor::ActorTaskId>> task_ids;
  return *task_ids;
}

}  // namespace

@implementation GeminiActuationWorklogAppInterface

+ (void)startActuationWorklogCoordinatorOnMinimalUI {
  Browser* browser = chrome_test_util::GetMainBrowser();
  CHECK(browser);

  UIViewController* rootViewController = GetAnyKeyWindow().rootViewController;
  CHECK(rootViewController);

  ActuationWorklogCoordinator* coordinator =
      [[ActuationWorklogCoordinator alloc]
          initWithBaseViewController:rootViewController
                             browser:browser];
  [GeminiActuationWorklogAppInterface sharedWorklogHelper].coordinator =
      coordinator;
  [coordinator start];

  ActuationWorklogViewController* viewController = coordinator.viewController;
  // Hosts the worklog and its intervention like the Gemini floaty does.
  GeminiContainerViewController* containerViewController =
      [[GeminiContainerViewController alloc]
          initWithWorklogViewController:viewController
             interventionViewController:coordinator.interventionViewController];
  [containerViewController
      setWorklogDisplayMode:ActuationWorklogDisplayModeCompact];
  [containerViewController setActuationActive:YES];
  containerViewController.modalPresentationStyle =
      UIModalPresentationFullScreen;
  [coordinator.baseViewController presentViewController:containerViewController
                                               animated:NO
                                             completion:nil];
}

+ (void)stopActuationWorklogCoordinatorOnMinimalUI {
  GeminiActuationWorklogTestHelper* helper =
      [GeminiActuationWorklogAppInterface sharedWorklogHelper];
  [helper stop];

  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  if (!profile) {
    return;
  }

  if (actor::ActorService* service =
          actor::ActorServiceFactory::GetForProfile(profile)) {
    for (actor::ActorTaskId task_id : GetCreatedTaskIds()) {
      service->StopTask(task_id, actor::ActorTaskStoppedReason::kStoppedByUser);
    }
  }
  GetCreatedTaskIds().clear();
}

+ (void)stopObservingWorklogTask {
  [[GeminiActuationWorklogAppInterface sharedWorklogHelper] stopObservingTask];
}

+ (BOOL)hasActiveTask {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  actor::ActorService* actorService =
      actor::ActorServiceFactory::GetForProfile(profile);
  CHECK(actorService);
  return actorService->GetActiveTaskState().has_value();
}

+ (void)stopActiveTask {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  if (!profile) {
    return;
  }

  actor::ActorTaskId currentTaskId =
      [GeminiActuationWorklogAppInterface sharedWorklogHelper].currentTaskId;
  if (currentTaskId.is_null()) {
    return;
  }

  if (actor::ActorService* service =
          actor::ActorServiceFactory::GetForProfile(profile)) {
    service->StopTask(currentTaskId,
                      actor::ActorTaskStoppedReason::kStoppedByUser);
  }
}

+ (void)startWorklogTaskWithCredentialSuggestions:(NSArray<NSString*>*)usernames
                                completionHandler:(void (^)(NSString* username))
                                                      completionHandler {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  actor::ActorService* actorService =
      actor::ActorServiceFactory::GetForProfile(profile);
  CHECK(actorService);

  actor::ActorTaskId taskID = actorService->CreateTask(
      "Test Task",
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kTest,
                            /*id=*/std::nullopt),
      /*allow_incognito_web_states=*/false);
  GetCreatedTaskIds().push_back(taskID);

  GeminiActuationWorklogTestHelper* worklogHelper =
      [GeminiActuationWorklogAppInterface sharedWorklogHelper];
  [worklogHelper startObservingTaskWithID:taskID actorService:actorService];

  // Wait for the asynchronous task observer registration snapshot (which pushes
  // the initial "Task started" item) to be delivered on this sequence before
  // requesting the suggestion intervention.
  __block BOOL snapshotDelivered = NO;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^{
        snapshotDelivered = YES;
      }));
  CHECK(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForActionTimeout, ^bool {
        return snapshotDelivered;
      }));

  NSMutableArray<ActorFormSuggestion*>* suggestions = [NSMutableArray array];
  int64_t IDCounter = 1;
  for (NSString* username in usernames) {
    actor_login::Credential credential;
    credential.id = actor_login::Credential::Id(IDCounter++);
    credential.type = actor_login::CredentialType::kPassword;
    credential.username = base::SysNSStringToUTF16(username);
    credential.display_origin = u"example.com";

    [suggestions
        addObject:[[ActorFormSuggestion alloc] initWithCredential:credential]];
  }

  [worklogHelper.interventionDelegate
                  actorTask:taskID
      selectFromSuggestions:suggestions
                   callback:^(actor::InterventionDelegateResult<
                              actor::SuggestionSelection> result) {
                     [GeminiActuationWorklogAppInterface
                         handleSelectedSuggestion:result.value().suggestion
                                completionHandler:completionHandler];
                   }];
}

#pragma mark - Private

// A singleton instance used by the test that establishes the actuation
// suggestions worklog.
+ (GeminiActuationWorklogTestHelper*)sharedWorklogHelper {
  static GeminiActuationWorklogTestHelper* worklogHelper = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    worklogHelper = [[GeminiActuationWorklogTestHelper alloc] init];
  });
  return worklogHelper;
}

// Helper method that invokes `completion` with the username from
// `selectedSuggestion` (or `nil` if no credential was selected).
+ (void)handleSelectedSuggestion:(ActorFormSuggestion*)selectedSuggestion
               completionHandler:(void (^)(NSString* username))completion {
  NSString* username = nil;
  if (selectedSuggestion && selectedSuggestion.credential) {
    username =
        base::SysUTF16ToNSString(selectedSuggestion.credential->username);
  }
  if (completion) {
    completion(username);
  }
}

@end
