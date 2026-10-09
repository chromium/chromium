// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/actor_profile_agent.h"

#import <memory>
#import <optional>
#import <string>
#import <vector>

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/actor/core/task_source_info.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/background_task/background_continued_processing_app_agent.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_configuration.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_context.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_request.h"
#import "ios/chrome/app/background_task/features.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_task_background_worker.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_intervention_delegate.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Intervention delegate that lets a task be interrupted without resolving the
// confirmation, so that it stays waiting on the user.
@interface FakeActorProfileAgentInterventionDelegate
    : NSObject <ActorTaskInterventionDelegate>
@end

@implementation FakeActorProfileAgentInterventionDelegate

- (void)actorTask:(actor::ActorTaskId)taskID
    requestUserInterventionWithTitle:(NSString*)title
                            subtitle:(NSString*)subtitle
                          buttonText:(NSString*)buttonText
                   completionHandler:(void (^)(void))completionHandler {
}

@end

namespace {

// Returns the request identifiers in `requests`.
NSArray<NSString*>* Identifiers(
    NSArray<BackgroundContinuedProcessingTaskRequest*>* requests) {
  NSMutableArray<NSString*>* identifiers = [NSMutableArray array];
  for (BackgroundContinuedProcessingTaskRequest* request in requests) {
    [identifiers addObject:request.identifier];
  }
  return identifiers;
}

// Returns the identifier the agent uses for `task_id`.
NSString* IdentifierForTaskId(actor::ActorTaskId task_id) {
  return base::SysUTF8ToNSString(base::NumberToString(task_id.value()));
}

// Returns a background task context with a no-op expiration handler.
BackgroundContinuedProcessingTaskContext* CreateContext() {
  BackgroundContinuedProcessingTaskConfiguration* config =
      [[BackgroundContinuedProcessingTaskConfiguration alloc]
              initWithTitle:@"Test Task"
                   subtitle:@""
          expirationHandler:^{
          }];
  return [[BackgroundContinuedProcessingTaskContext alloc]
      initWithTaskIdentifier:@"org.chromium.test.task"
               configuration:config
               finishHandler:nil];
}

}  // namespace

// Tests `ActorProfileAgent` against a real actor service and a mocked app
// agent.
class ActorProfileAgentTest : public PlatformTest {
 protected:
  ActorProfileAgentTest()
      : web_client_(std::make_unique<web::FakeWebClient>()) {
    scoped_feature_list_.InitWithFeatures(
        {kPageActionMenu, kActorTools, kGeminiClientMigration, kGeminiActor,
         kEnableBackgroundContinuedProcessing},
        {});
    actor::ActorServiceFactory::GetInstance();
    profile_ = TestProfileIOS::Builder().Build();

    mock_app_state_ = OCMClassMock([AppState class]);
    mock_app_agent_ =
        OCMClassMock([BackgroundContinuedProcessingAppAgent class]);
    OCMStub([mock_app_agent_ agentFromApp:mock_app_state_])
        .andReturn(mock_app_agent_);

    profile_state_ = [[ProfileState alloc] initWithAppState:mock_app_state_];
    profile_state_.profile = profile_.get();
    agent_ = [[ActorProfileAgent alloc] init];
  }

  ~ActorProfileAgentTest() override {
    [mock_app_agent_ stopMocking];
    [mock_app_state_ stopMocking];
  }

  // Returns the profile's actor service, creating it if needed.
  actor::ActorService* service() {
    return actor::ActorServiceFactory::GetForProfile(profile_.get());
  }

  // Creates a task titled `title` in the profile's actor service.
  actor::ActorTaskId CreateTask(const std::string& title) {
    return service()->CreateTask(
        title,
        actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kTest,
                              /*id=*/std::nullopt),
        /*allow_incognito_web_states=*/false);
  }

  // Interrupts the task identified by `task_id` so that it waits on the user.
  void InterruptTask(actor::ActorTaskId task_id) {
    service()->SetTaskInterventionDelegate(task_id, intervention_delegate_);
    service()->InterruptTask(
        task_id, actor::ActorTaskInterruptReason::kWaitingUserConfirmation,
        "Please confirm");
  }

  // Returns the background worker of the task identified by `task_id`, or
  // null if the task is gone.
  actor::ActorTaskBackgroundWorker* WorkerForTask(actor::ActorTaskId task_id) {
    for (const base::WeakPtr<actor::ActorTaskBackgroundWorker>& worker :
         service()->GetWeakTaskBackgroundWorkers()) {
      if (worker && worker->task_id() == task_id) {
        return worker.get();
      }
    }
    return nullptr;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  web::ScopedTestingWebClient web_client_;
  std::unique_ptr<TestProfileIOS> profile_;
  id mock_app_state_;
  id mock_app_agent_;
  ProfileState* profile_state_;
  ActorProfileAgent* agent_;
  FakeActorProfileAgentInterventionDelegate* intervention_delegate_ =
      [[FakeActorProfileAgentInterventionDelegate alloc] init];
};

// Fixture for tests that need background continued processing to be available;
// they are skipped in configurations where it is not.
class ActorProfileAgentBackgroundingTest : public ActorProfileAgentTest {
 protected:
  void SetUp() override {
    ActorProfileAgentTest::SetUp();
    if (!IsGeminiActorBackgroundingEnabled()) {
      GTEST_SKIP() << "Backgrounding is unavailable in this configuration.";
    }
  }
};

// Tests that the agent registers as a task provider when added to a profile
// state and unregisters when removed.
TEST_F(ActorProfileAgentTest, RegistersAndUnregistersWithAppAgent) {
  OCMExpect([mock_app_agent_ addTaskProvider:agent_]);
  [profile_state_ addAgent:agent_];
  EXPECT_OCMOCK_VERIFY(mock_app_agent_);

  OCMExpect([mock_app_agent_ removeTaskProvider:agent_]);
  [profile_state_ removeAgent:agent_];
  EXPECT_OCMOCK_VERIFY(mock_app_agent_);
}

// Tests that no request is returned without a profile, even if the service
// has tasks.
TEST_F(ActorProfileAgentTest, NoRequestsWithoutProfile) {
  [profile_state_ addAgent:agent_];
  CreateTask("Task");
  profile_state_.profile = nullptr;

  EXPECT_NSEQ(@[], agent_.continuedProcessingTaskRequests);
}

// Tests that no request is returned without an actor service, and that asking
// does not create the service.
TEST_F(ActorProfileAgentTest, NoRequestsWithoutServiceAndNoServiceCreation) {
  [profile_state_ addAgent:agent_];

  EXPECT_NSEQ(@[], agent_.continuedProcessingTaskRequests);
  EXPECT_EQ(nullptr,
            actor::ActorServiceFactory::GetForProfileIfExists(profile_.get()));
}

// Tests that a service created after an empty ask is picked up by later asks,
// i.e. the agent does not remember the service's absence.
TEST_F(ActorProfileAgentBackgroundingTest,
       RequestsTasksOfServiceCreatedAfterFirstAsk) {
  [profile_state_ addAgent:agent_];
  ASSERT_NSEQ(@[], agent_.continuedProcessingTaskRequests);

  actor::ActorTaskId task_id = CreateTask("Task");
  NSArray<BackgroundContinuedProcessingTaskRequest*>* requests =
      agent_.continuedProcessingTaskRequests;

  EXPECT_NSEQ(@[ IdentifierForTaskId(task_id) ], Identifiers(requests));
}

// Tests that the agent builds one request per eligible task, identified by the
// task ID and titled after the task, skipping tasks that wait on the user.
TEST_F(ActorProfileAgentBackgroundingTest, OneRequestPerEligibleTask) {
  [profile_state_ addAgent:agent_];
  actor::ActorTaskId first_id = CreateTask("First");
  actor::ActorTaskId waiting_id = CreateTask("Waiting");
  actor::ActorTaskId second_id = CreateTask("Second");
  InterruptTask(waiting_id);
  ASSERT_FALSE(WorkerForTask(waiting_id)->ShouldRequestBackgroundTask());

  NSArray<BackgroundContinuedProcessingTaskRequest*>* requests =
      agent_.continuedProcessingTaskRequests;

  EXPECT_NSEQ(
      (@[ IdentifierForTaskId(first_id), IdentifierForTaskId(second_id) ]),
      Identifiers(requests));
  ASSERT_EQ(2u, requests.count);
  EXPECT_NSEQ(@"First", requests[0].configuration.title);
  EXPECT_NSEQ(@"Second", requests[1].configuration.title);
}

// Tests that a request's configuration fails rather than queues the task, and
// that its expiration handler pauses the task.
TEST_F(ActorProfileAgentBackgroundingTest,
       ConfigurationStrategyAndExpirationPausesTask) {
  [profile_state_ addAgent:agent_];
  actor::ActorTaskId task_id = CreateTask("Task");

  NSArray<BackgroundContinuedProcessingTaskRequest*>* requests =
      agent_.continuedProcessingTaskRequests;
  ASSERT_EQ(1u, requests.count);
  BackgroundContinuedProcessingTaskConfiguration* config =
      requests[0].configuration;
  EXPECT_EQ(BackgroundContinuedProcessingSubmissionStrategy::kFail,
            config.submissionStrategy);

  config.expirationHandler();

  EXPECT_NE(nullptr, WorkerForTask(task_id));
  EXPECT_EQ(1u, service()->GetWeakTaskBackgroundWorkers().size());
}

// Tests that a request's started handler hands the context to its own task's
// worker: that task stops being eligible while the other stays eligible,
// and completing the task completes the context with success.
TEST_F(ActorProfileAgentBackgroundingTest,
       StartedHandlerDeliversContextToTaskWorker) {
  [profile_state_ addAgent:agent_];
  actor::ActorTaskId other_id = CreateTask("Other");
  actor::ActorTaskId task_id = CreateTask("Task");
  NSArray<BackgroundContinuedProcessingTaskRequest*>* requests =
      agent_.continuedProcessingTaskRequests;
  ASSERT_EQ(2u, requests.count);
  ASSERT_NSEQ(IdentifierForTaskId(task_id), requests[1].identifier);
  BackgroundContinuedProcessingTaskContext* context = CreateContext();

  requests[1].startedHandler(context);

  EXPECT_FALSE(context.completed);
  EXPECT_FALSE(WorkerForTask(task_id)->ShouldRequestBackgroundTask());
  EXPECT_TRUE(WorkerForTask(other_id)->ShouldRequestBackgroundTask());

  service()->StopTask(task_id, actor::ActorTaskStoppedReason::kTaskComplete);
  EXPECT_TRUE(context.completed);
  EXPECT_DOUBLE_EQ(1.0, context.fractionCompleted);
}

// Tests that a started handler run after its task was stopped fails the
// context, so the system task does not linger until expiration.
TEST_F(ActorProfileAgentBackgroundingTest,
       StartedHandlerAfterTaskStoppedFailsContext) {
  [profile_state_ addAgent:agent_];
  actor::ActorTaskId task_id = CreateTask("Task");
  NSArray<BackgroundContinuedProcessingTaskRequest*>* requests =
      agent_.continuedProcessingTaskRequests;
  ASSERT_EQ(1u, requests.count);
  service()->StopTask(task_id, actor::ActorTaskStoppedReason::kStoppedByUser);
  BackgroundContinuedProcessingTaskContext* context = CreateContext();

  requests[0].startedHandler(context);

  EXPECT_TRUE(context.completed);
  EXPECT_DOUBLE_EQ(0.0, context.fractionCompleted);
}

// Tests that the agent does not implement the optional foreground hook, so it
// can never end background tasks when the app returns to the foreground.
TEST_F(ActorProfileAgentTest, DoesNotObserveForeground) {
  EXPECT_FALSE([agent_
      respondsToSelector:@selector(backgroundProcessingBecameUnnecessary)]);
}
