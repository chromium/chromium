// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"

#import <UIKit/UIKit.h>

#import <set>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/test/gtest_util.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/test/values_test_util.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_task.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_extractor_java_script_feature.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/fake_snapshot_generator_delegate.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// TODO(crbug.com/556276928): Centralize fake observer across unit tests.
@interface FakeActorServiceTaskUpdatesObserver
    : NSObject <ActorTaskUpdatesObserver>
@property(nonatomic, assign) NSInteger registeredCount;
@property(nonatomic, assign) NSInteger stateChangeCount;
@property(nonatomic, assign) NSInteger stoppedCount;
@property(nonatomic, copy) NSString* taskTitle;
@end

@implementation FakeActorServiceTaskUpdatesObserver

- (void)didRegisterAsObserverForTaskID:(actor::ActorTaskId)taskID
                             taskTitle:(NSString*)taskTitle
                            taskUpdate:(NSString*)taskUpdate
                          currentState:(actor::ActorTaskState)state
                             webStates:(NSArray<NSNumber*>*)webStatesIDs {
  _registeredCount++;
  _taskTitle = [taskTitle copy];
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
         didChangeState:(actor::ActorTaskState)newState
              fromState:(actor::ActorTaskState)oldState {
  _stateChangeCount++;
}

- (void)actorTaskDidStopWithID:(actor::ActorTaskId)taskID
                    finalState:(actor::ActorTaskState)finalState {
  _stoppedCount++;
}

@end

namespace actor {

class ObservingFakeWebState : public web::FakeWebState {
 public:
  void AddObserver(web::WebStateObserver* observer) override {
    web::FakeWebState::AddObserver(observer);
    has_observer_ = true;
  }
  void RemoveObserver(web::WebStateObserver* observer) override {
    web::FakeWebState::RemoveObserver(observer);
    has_observer_ = false;
  }
  bool has_observer() const { return has_observer_; }

 private:
  bool has_observer_ = false;
};

class MockActorTask : public ActorTask {
 public:
  MockActorTask(ActorTaskId task_id,
                const std::string& title,
                bool allow_incognito_web_states,
                AggregatedJournal* journal,
                ActorToolFactory* tool_factory,
                BrowserList* browser_list,
                bool* stop_called)
      : ActorTask(task_id,
                  title,
                  allow_incognito_web_states,
                  journal,
                  tool_factory,
                  browser_list),
        stop_called_(stop_called) {}

  void Stop(ActorTaskStoppedReason stop_reason) override {
    if (stop_called_) {
      *stop_called_ = true;
    }
    ActorTask::Stop(stop_reason);
  }

 private:
  raw_ptr<bool> stop_called_;
};

class ActorServiceTest : public PlatformTest {
 public:
  explicit ActorServiceTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::DEFAULT)
      : task_environment_(time_source),
        web_client_(std::make_unique<web::FakeWebClient>()) {
    ActorServiceFactory::GetInstance();
    profile_ = TestProfileIOS::Builder().Build();
  }

  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(kActorTools);

    static_cast<web::FakeWebClient*>(web_client_.Get())
        ->SetJavaScriptFeatures({
            PageContextExtractorJavaScriptFeature::GetInstance(),
        });
  }

 protected:
  PerformActionsResult PerformActions(
      ActorService* service,
      ActorTaskId task_id,
      const std::vector<optimization_guide::proto::Action>& actions = {},
      const std::string& update = "Update") {
    base::test::TestFuture<PerformActionsResult> future;
    service->PerformActions(task_id, actions, update, future.GetCallback());
    return future.Take();
  }

  void SwapTask(ActorService* service,
                ActorTaskId task_id,
                std::unique_ptr<ActorTask> task) {
    service->active_tasks_[task_id] = std::move(task);
  }

  bool HasTask(ActorService* service, ActorTaskId task_id) {
    return service->active_tasks_.find(task_id) != service->active_tasks_.end();
  }

  AggregatedJournal* GetJournal(ActorService* service) {
    return service->journal_.get();
  }

  ActorToolFactory* GetToolFactory(ActorService* service) {
    return service->tool_factory_.get();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  web::ScopedTestingWebClient web_client_;
  std::unique_ptr<TestProfileIOS> profile_;
};

class ActorServiceMockTimeTest : public ActorServiceTest {
 public:
  ActorServiceMockTimeTest()
      : ActorServiceTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

// Tests that `ActorService` is successfully created when the `kActorTools`
// feature is enabled.
TEST_F(ActorServiceTest, ServiceCreationWithFeatureEnabled) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  EXPECT_NE(nullptr, service);
}

// Tests that `ActorService` is not created when the `kActorTools` feature is
// disabled.
TEST_F(ActorServiceTest, ServiceCreationWithFeatureDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(kActorTools);

  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  EXPECT_EQ(nullptr, service);
}

// Tests that `CreateTask` generates unique IDs for sequential tasks.
TEST_F(ActorServiceTest, CreateTaskGeneratesUniqueIds) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  std::set<ActorTaskId> task_ids;
  for (int i = 0; i < 100; ++i) {
    ActorTaskId task_id =
        service->CreateTask("Test Task",
                            /*allow_incognito_web_states=*/false);
    EXPECT_FALSE(task_id.is_null());
    EXPECT_TRUE(task_ids.insert(task_id).second);
  }
}

// Tests that requesting tab observation with a null WebState triggers the
// callback.
TEST_F(ActorServiceTest, RequestTabObservationWithNullWebStateReturnsFailure) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id =
      service->CreateTask("Test Task",
                          /*allow_incognito_web_states=*/false);

  bool callback_called = false;
  service->RequestTabObservation(
      task_id, nullptr,
      base::BindOnce(
          [](bool* called, PageContextWrapperCallbackResponse response) {
            *called = true;
          },
          &callback_called));

  EXPECT_TRUE(callback_called);
}

// Tests that requesting tab observation with a valid WebState extracts APC.
TEST_F(ActorServiceTest, RequestTabObservationWithValidWebState) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id =
      service->CreateTask("Test Task",
                          /*allow_incognito_web_states=*/false);

  web::WebState::CreateParams params(profile_.get());
  auto web_state = web::WebState::Create(params);

  web_state->GetView().frame = CGRectMake(0, 0, 100, 100);
  UIViewController* root_view_controller = [[UIViewController alloc] init];
  root_view_controller.view = web_state->GetView();

  ScopedKeyWindow scoped_window;
  scoped_window.Get().rootViewController = root_view_controller;

  web_state->WasShown();

  SnapshotTabHelper::CreateForWebState(web_state.get());
  SnapshotSourceTabHelper::CreateForWebState(web_state.get());

  SnapshotTabHelper* snapshot_tab_helper =
      SnapshotTabHelper::FromWebState(web_state.get());
  FakeSnapshotGeneratorDelegate* snapshot_delegate =
      [[FakeSnapshotGeneratorDelegate alloc] init];
  snapshot_delegate.view = web_state->GetView();
  snapshot_tab_helper->SetDelegate(snapshot_delegate);

  web::test::LoadHtml(@"<html><body>Most basic APC content</body></html>",
                      GURL("http://dummy.url"), web_state.get());

  base::RunLoop run_loop;
  bool callback_called = false;
  bool apc_extracted = false;

  service->RequestTabObservation(
      task_id, web_state.get(),
      base::BindOnce(
          [](bool* called, bool* apc_ok, base::OnceClosure quit_closure,
             PageContextWrapperCallbackResponse response) {
            base::ScopedClosureRunner quit_runner(std::move(quit_closure));
            *called = true;
            ASSERT_TRUE(response.has_value());
            const auto& page_context = response.value();
            ASSERT_TRUE(page_context->has_annotated_page_content());
            const auto& apc = page_context->annotated_page_content();
            ASSERT_TRUE(apc.has_root_node());
            *apc_ok = true;
          },
          &callback_called, &apc_extracted, run_loop.QuitClosure()));

  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(apc_extracted);
}

// Tests that GetWebStateForID returns nullptr for a tab that is not controlled
// by the task.
TEST_F(ActorServiceTest, GetWebStateForID_NotControlled) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id =
      service->CreateTask("Test Task", /*allow_incognito_web_states=*/false);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  auto test_browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list->AddBrowser(test_browser.get());

  auto fake_web_state = std::make_unique<web::FakeWebState>();
  web::WebStateID web_state_id = fake_web_state->GetUniqueIdentifier();

  test_browser->GetWebStateList()->InsertWebState(std::move(fake_web_state));

  web::WebState* resolved_web_state =
      service->GetWebStateForID(web_state_id, task_id);
  // Should be nullptr because the tab is not controlled by the task.
  EXPECT_EQ(nullptr, resolved_web_state);

  browser_list->RemoveBrowser(test_browser.get());
}

// Tests that GetWebStateForID finds a tab that is controlled by the task.
TEST_F(ActorServiceTest, GetWebStateForID_Controlled) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id =
      service->CreateTask("Test Task", /*allow_incognito_web_states=*/false);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  auto test_browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list->AddBrowser(test_browser.get());

  auto fake_web_state = std::make_unique<web::FakeWebState>();
  web::WebStateID web_state_id = fake_web_state->GetUniqueIdentifier();
  web::WebState* fake_web_state_ptr = fake_web_state.get();

  test_browser->GetWebStateList()->InsertWebState(std::move(fake_web_state));

  // Make the tab controlled by the task by performing an action targeting it.
  std::vector<optimization_guide::proto::Action> actions;
  actions.push_back(MakeSuccessfulActorAction(web_state_id));
  PerformActions(service, task_id, actions);

  EXPECT_EQ(fake_web_state_ptr,
            service->GetWebStateForID(web_state_id, task_id));

  web::WebState* resolved_web_state =
      service->GetWebStateForID(web_state_id, task_id);
  EXPECT_NE(nullptr, resolved_web_state);
  EXPECT_EQ(web_state_id, resolved_web_state->GetUniqueIdentifier());
  EXPECT_EQ(fake_web_state_ptr, resolved_web_state);

  browser_list->RemoveBrowser(test_browser.get());
}

// Tests that AddControlledWebState correctly adds a WebState so that
// GetWebStateForID finds it immediately before any actions are performed.
TEST_F(ActorServiceTest, AddControlledWebState) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id =
      service->CreateTask("Test Task", /*allow_incognito_web_states=*/false);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  auto test_browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list->AddBrowser(test_browser.get());

  auto fake_web_state = std::make_unique<web::FakeWebState>();
  web::WebStateID web_state_id = fake_web_state->GetUniqueIdentifier();
  web::WebState* fake_web_state_ptr = fake_web_state.get();

  test_browser->GetWebStateList()->InsertWebState(std::move(fake_web_state));

  EXPECT_EQ(nullptr, service->GetWebStateForID(web_state_id, task_id));

  service->AddControlledWebState(task_id, fake_web_state_ptr);

  EXPECT_EQ(fake_web_state_ptr,
            service->GetWebStateForID(web_state_id, task_id));

  browser_list->RemoveBrowser(test_browser.get());
}

// Tests that GetWebStateForID does not find a tab in an incognito browser if
// the task does not allow incognito.
TEST_F(ActorServiceTest, GetWebStateForID_Incognito_NotAllowed) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id =
      service->CreateTask("Test Task", /*allow_incognito_web_states=*/false);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());

  ProfileIOS* incognito_profile = profile_->GetOffTheRecordProfile();
  auto incognito_browser = std::make_unique<TestBrowser>(incognito_profile);
  browser_list->AddBrowser(incognito_browser.get());

  auto fake_web_state = std::make_unique<web::FakeWebState>();
  web::WebStateID web_state_id = fake_web_state->GetUniqueIdentifier();

  incognito_browser->GetWebStateList()->InsertWebState(
      std::move(fake_web_state));

  web::WebState* resolved_web_state =
      service->GetWebStateForID(web_state_id, task_id);
  EXPECT_EQ(nullptr, resolved_web_state);

  browser_list->RemoveBrowser(incognito_browser.get());
}

// Tests that CreateTask crashes when trying to allow incognito web states,
// as it is not yet supported.
TEST_F(ActorServiceTest, CreateTask_Incognito_Crashes) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  EXPECT_DEATH_IF_SUPPORTED(service->CreateTask("Test Task", true), "");
}

// Tests that GetWebStateForID returns nullptr when the task is not found.
TEST_F(ActorServiceTest, GetWebStateForID_TaskNotFound) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  // Use a task ID that doesn't exist.
  ActorTaskId invalid_task_id = ActorTaskId(999);

  auto fake_web_state = std::make_unique<web::FakeWebState>();
  web::WebStateID web_state_id = fake_web_state->GetUniqueIdentifier();

  web::WebState* resolved_web_state =
      service->GetWebStateForID(web_state_id, invalid_task_id);
  EXPECT_EQ(nullptr, resolved_web_state);
}

// Tests that GetActiveTaskState returns the state of the active task, or
// nullopt if there are no active tasks.
TEST_F(ActorServiceTest, GetActiveTaskState) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  // No active tasks returns nullopt.
  EXPECT_EQ(std::nullopt, service->GetActiveTaskState());

  service->CreateTask("Test Task", /*allow_incognito_web_states=*/false);
  EXPECT_EQ(ActorTaskState::kInit, service->GetActiveTaskState());
}

// Tests that PerformActions completes immediately when the WebState is not
// loading.
TEST_F(ActorServiceTest, PerformActions_NoLoading_InstantCompletion) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id =
      service->CreateTask("Test Task", /*allow_incognito_web_states=*/false);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  auto test_browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list->AddBrowser(test_browser.get());

  auto fake_web_state = std::make_unique<web::FakeWebState>();
  auto* fake_web_state_ptr = fake_web_state.get();
  test_browser->GetWebStateList()->InsertWebState(std::move(fake_web_state));

  std::vector<optimization_guide::proto::Action> actions;
  actions.push_back(
      MakeSuccessfulActorAction(fake_web_state_ptr->GetUniqueIdentifier()));

  PerformActionsResult result = PerformActions(service, task_id, actions);
  ASSERT_EQ(1u, result.action_results.size());
  EXPECT_TRUE(result.action_results[0].tool_result.IsOk());

  browser_list->RemoveBrowser(test_browser.get());
}

// Tests that PerformActions is deferred when the WebState is loading, and only
// resolves when loading completes.
TEST_F(ActorServiceTest, PerformActions_Loading_DeferredUntilStopLoading) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id =
      service->CreateTask("Test Task", /*allow_incognito_web_states=*/false);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  auto test_browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list->AddBrowser(test_browser.get());

  auto fake_web_state = std::make_unique<ObservingFakeWebState>();
  auto* fake_web_state_ptr = fake_web_state.get();
  test_browser->GetWebStateList()->InsertWebState(std::move(fake_web_state));

  // Set the WebState to a loading state.
  fake_web_state_ptr->SetLoading(true);

  std::vector<optimization_guide::proto::Action> actions;
  actions.push_back(
      MakeSuccessfulActorAction(fake_web_state_ptr->GetUniqueIdentifier()));

  base::test::TestFuture<PerformActionsResult> future;
  service->PerformActions(task_id, actions, "Update", future.GetCallback());

  // Wait until the task has deferred the completion callback (i.e. started
  // observing the loading web state).
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_web_state_ptr->has_observer(); }));

  // Gating: The callback should not be executed yet because the page is
  // loading.
  EXPECT_FALSE(future.IsReady());

  // Stop the load.
  fake_web_state_ptr->SetLoading(false);

  // Now the callback should execute successfully.
  const PerformActionsResult& result = future.Get();
  ASSERT_EQ(1u, result.action_results.size());
  EXPECT_TRUE(result.action_results[0].tool_result.IsOk());

  browser_list->RemoveBrowser(test_browser.get());
}

// Tests that a loading WebState times out after 5 seconds, forcing the
// deferred PerformActions callback to run to prevent hanging.
TEST_F(ActorServiceMockTimeTest,
       PerformActions_Loading_TimeoutResolvesCallback) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id =
      service->CreateTask("Test Task", /*allow_incognito_web_states=*/false);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  auto test_browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list->AddBrowser(test_browser.get());

  auto fake_web_state = std::make_unique<ObservingFakeWebState>();
  auto* fake_web_state_ptr = fake_web_state.get();
  test_browser->GetWebStateList()->InsertWebState(std::move(fake_web_state));

  // Set the WebState to a loading state.
  fake_web_state_ptr->SetLoading(true);

  std::vector<optimization_guide::proto::Action> actions;
  actions.push_back(
      MakeSuccessfulActorAction(fake_web_state_ptr->GetUniqueIdentifier()));

  base::test::TestFuture<PerformActionsResult> future;
  service->PerformActions(task_id, actions, "Update", future.GetCallback());

  task_environment_.FastForwardBy(base::Seconds(0));

  // Callback should be deferred.
  EXPECT_FALSE(future.IsReady());

  // Fast forward the environment by 7 seconds to trigger the load timeout.
  task_environment_.FastForwardBy(base::Seconds(7));

  // The callback must be resolved now due to the timeout.
  const PerformActionsResult& result = future.Get();
  ASSERT_EQ(1u, result.action_results.size());
  EXPECT_TRUE(result.action_results[0].tool_result.IsOk());

  browser_list->RemoveBrowser(test_browser.get());
}

// Test that StopTask stops and erases the task, and verifies that Stop() is
// called on the ActorTask.
TEST_F(ActorServiceTest, StopTask) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id =
      service->CreateTask("Test Task", /*allow_incognito_web_states=*/false);

  // Verify that the task exists in service's active tasks.
  ASSERT_TRUE(HasTask(service, task_id));

  // Swap the task with our MockActorTask.
  bool stop_called = false;
  SwapTask(
      service, task_id,
      std::make_unique<MockActorTask>(
          task_id, "Test Task",
          /*allow_incognito_web_states=*/false, GetJournal(service),
          GetToolFactory(service),
          BrowserListFactory::GetForProfile(profile_.get()), &stop_called));

  // Stop the task.
  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);

  // Verify that Stop() was called on our MockActorTask.
  EXPECT_TRUE(stop_called);

  // Verify the task is also erased from ActorService's active tasks.
  EXPECT_FALSE(HasTask(service, task_id));
}

// Test lifecycle of a single task updates observer.
TEST_F(ActorServiceTest, TaskUpdatesObserverLifecycle) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  FakeActorServiceTaskUpdatesObserver* observer =
      [[FakeActorServiceTaskUpdatesObserver alloc] init];
  service->AddTaskUpdatesObserver(observer);

  ActorTaskId task_id =
      service->CreateTask("Test Task", /*allow_incognito_web_states=*/false);
  EXPECT_EQ(1, observer.registeredCount);
  EXPECT_NSEQ(@"Test Task", observer.taskTitle);

  PerformActions(service, task_id);
  EXPECT_GT(observer.stateChangeCount, 0);

  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  EXPECT_EQ(1, observer.stoppedCount);

  service->RemoveTaskUpdatesObserver(observer);
}

// Test that adding an observer when a task is already running immediately
// attaches the observer to the active task.
TEST_F(ActorServiceTest, LateAddedTaskUpdatesObserverAttachesToActiveTasks) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id =
      service->CreateTask("Active Task", /*allow_incognito_web_states=*/false);

  FakeActorServiceTaskUpdatesObserver* observer =
      [[FakeActorServiceTaskUpdatesObserver alloc] init];
  service->AddTaskUpdatesObserver(observer);
  EXPECT_EQ(1, observer.registeredCount);
  EXPECT_NSEQ(@"Active Task", observer.taskTitle);

  PerformActions(service, task_id);
  EXPECT_GT(observer.stateChangeCount, 0);

  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  EXPECT_EQ(1, observer.stoppedCount);

  service->RemoveTaskUpdatesObserver(observer);
}

// Test broadcasting to multiple task updates observers and selective removal.
TEST_F(ActorServiceTest, MultipleTaskUpdatesObserversBroadcastAndRemoval) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  FakeActorServiceTaskUpdatesObserver* observer1 =
      [[FakeActorServiceTaskUpdatesObserver alloc] init];
  FakeActorServiceTaskUpdatesObserver* observer2 =
      [[FakeActorServiceTaskUpdatesObserver alloc] init];

  service->AddTaskUpdatesObserver(observer1);
  service->AddTaskUpdatesObserver(observer2);

  // Both observers should receive the registration callback on task creation.
  ActorTaskId task_id =
      service->CreateTask("Shared Task", /*allow_incognito_web_states=*/false);
  EXPECT_EQ(1, observer1.registeredCount);
  EXPECT_NSEQ(@"Shared Task", observer1.taskTitle);
  EXPECT_EQ(1, observer2.registeredCount);
  EXPECT_NSEQ(@"Shared Task", observer2.taskTitle);

  // Performing actions transitions task state and broadcasts to all observers.
  PerformActions(service, task_id);
  EXPECT_GT(observer1.stateChangeCount, 0);
  EXPECT_EQ(observer1.stateChangeCount, observer2.stateChangeCount);

  // Stopping the task should only notify the remaining active observer.
  service->RemoveTaskUpdatesObserver(observer1);
  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  EXPECT_EQ(0, observer1.stoppedCount);
  EXPECT_EQ(1, observer2.stoppedCount);

  // Creating a new task should only notify observer2.
  service->CreateTask("Next Task", /*allow_incognito_web_states=*/false);
  EXPECT_EQ(1, observer1.registeredCount);
  EXPECT_EQ(2, observer2.registeredCount);

  service->RemoveTaskUpdatesObserver(observer2);
}

// Test that adding a duplicate task updates observer is ignored.
TEST_F(ActorServiceTest, DuplicateTaskUpdatesObserverIgnored) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  FakeActorServiceTaskUpdatesObserver* observer =
      [[FakeActorServiceTaskUpdatesObserver alloc] init];
  service->AddTaskUpdatesObserver(observer);
  service->AddTaskUpdatesObserver(observer);

  service->CreateTask("Test Task", /*allow_incognito_web_states=*/false);
  EXPECT_EQ(1, observer.registeredCount);

  service->RemoveTaskUpdatesObserver(observer);
}

}  // namespace actor
