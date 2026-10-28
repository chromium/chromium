// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"

#import <UIKit/UIKit.h>

#import <set>
#import <utility>
#import <vector>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "base/strings/stringprintf.h"
#import "base/strings/to_string.h"
#import "base/test/gtest_util.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/test/values_test_util.h"
#import "base/types/expected.h"
#import "components/actor/core/journal_details_builder.h"
#import "components/actor/core/task_source_info.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/background_task/features.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_enterprise_policy_checker_ios.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_task.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_task_background_worker.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_intervention_delegate.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_lifecycle_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"
#import "ios/chrome/browser/intelligence/actor/test/fake_actor_task_intervention_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_extractor_java_script_feature.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
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
#import "testing/gmock/include/gmock/gmock.h"
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
@property(nonatomic, copy) void (^onStateChange)(actor::ActorTaskState);
// Returns the new states of the received transitions, in delivery order.
- (const std::vector<actor::ActorTaskState>&)states;
@end

@implementation FakeActorServiceTaskUpdatesObserver {
  std::vector<actor::ActorTaskState> _states;
}

- (const std::vector<actor::ActorTaskState>&)states {
  return _states;
}

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
  _states.push_back(newState);
  if (_onStateChange) {
    _onStateChange(newState);
  }
}

- (void)actorTaskDidStopWithID:(actor::ActorTaskId)taskID
                    finalState:(actor::ActorTaskState)finalState {
  _stoppedCount++;
}

@end

@interface FakeActorTaskLifecycleObserver
    : NSObject <ActorTaskLifecycleObserver>
@property(nonatomic, assign) NSInteger startedCount;
@property(nonatomic, assign) NSInteger stoppedCount;
// Value of `stoppedCount` at the end of the last start notification.
@property(nonatomic, assign) NSInteger stoppedCountAfterStart;
// Value of `startedCount` when the last stop was received.
@property(nonatomic, assign) NSInteger startedCountAtStop;
@property(nonatomic, assign) actor::ActorTaskId lastTaskID;
@property(nonatomic, assign) actor::TaskSourceInfo::Client lastSourceType;
// Final state carried by the last stop notification.
@property(nonatomic, assign) actor::ActorTaskState lastFinalState;
// Called during each start notification, immediately before sampling
// `stoppedCountAfterStart`.
@property(nonatomic, copy) void (^onStart)(actor::ActorTaskId taskID);
@end

@implementation FakeActorTaskLifecycleObserver

- (void)actorServiceDidStartTaskWithID:(actor::ActorTaskId)taskID
                            sourceInfo:
                                (const actor::TaskSourceInfo&)sourceInfo {
  _startedCount++;
  _lastTaskID = taskID;
  _lastSourceType = sourceInfo.type;
  if (_onStart) {
    _onStart(taskID);
  }
  _stoppedCountAfterStart = _stoppedCount;
}

- (void)actorServiceDidStopTaskWithID:(actor::ActorTaskId)taskID
                           sourceInfo:(const actor::TaskSourceInfo&)sourceInfo
                           finalState:(actor::ActorTaskState)finalState {
  _stoppedCount++;
  _startedCountAtStop = _startedCount;
  _lastTaskID = taskID;
  _lastSourceType = sourceInfo.type;
  _lastFinalState = finalState;
}

@end

namespace actor {

namespace {

constexpr int kPolicyDisabled = 1;

// Id of a task that the tests never create.
constexpr ActorTaskId kUnknownTaskId(999);

// Returns the provenance attributed to tasks created by these tests.
TaskSourceInfo TestSource() {
  return TaskSourceInfo(TaskSourceInfo::Client::kTest, /*id=*/std::nullopt);
}

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
                base::OnceClosure on_stopped,
                bool* stop_called)
      : ActorTask(task_id,
                  title,
                  TestSource(),
                  allow_incognito_web_states,
                  journal,
                  tool_factory,
                  browser_list,
                  /*policy_checker=*/nullptr,
                  std::move(on_stopped)),
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

}  // namespace

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
    // Backgrounding is disabled so that these tests do not exercise it; the
    // worker and profile agent tests cover it.
    scoped_feature_list_.InitWithFeatures(
        {kPageActionMenu, kActorTools, kGeminiActor},
        {kEnableBackgroundContinuedProcessing});

    static_cast<web::FakeWebClient*>(web_client_.Get())
        ->SetJavaScriptFeatures({
            PageContextExtractorJavaScriptFeature::GetInstance(),
        });
  }

 protected:
  // Creates a non-incognito task attributed to `TestSource()`.
  ActorTaskId CreateTask(ActorService* service,
                         const std::string& title = "Test Task") {
    return service->CreateTask(title, TestSource(),
                               /*allow_incognito_web_states=*/false);
  }

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

  // Returns the hook `ActorService::CreateTask` gives to the task `task_id`.
  base::OnceClosure MakeOnStoppedCallback(ActorService* service,
                                          ActorTaskId task_id) {
    return base::BindOnce(&ActorService::OnTaskStopped, service->GetWeakPtr(),
                          task_id);
  }

  // Returns whether the service still owns the task, whether active or
  // pending deletion.
  bool HasTask(ActorService* service, ActorTaskId task_id) {
    return service->active_tasks_.contains(task_id) ||
           service->pending_delete_tasks_.contains(task_id);
  }

  ActorTask* GetTask(ActorService* service, ActorTaskId task_id) {
    auto it = service->active_tasks_.find(task_id);
    return it != service->active_tasks_.end() ? it->second.get() : nullptr;
  }

  AggregatedJournal* GetJournal(ActorService* service) {
    return service->journal_.get();
  }

  // Completes the page context extraction that follows an act of `task_id`
  // with one page context and no action results.
  void CompleteActObservations(ActorService* service,
                               ActorTaskId task_id,
                               PerformActionsCallback callback) {
    std::vector<std::unique_ptr<TabObservationResponse>> contexts;
    contexts.push_back(std::make_unique<TabObservationResponse>(
        web::WebStateID::NewUnique(), PageContextWrapperCallbackResponse(),
        true));
    service->OnActObservationsComplete(
        task_id,
        GetJournal(service)->CreatePendingAsyncEntry(
            GURL(), task_id, MakeBrowserTrackUUID(task_id),
            "TabObservationController", /*details=*/{}),
        std::move(callback), PerformActionsResult(), std::move(contexts));
  }

  // Delivers a successful completion of the actions of `task_id` to `service`,
  // as `ActorTask::Act()` would.
  void CompleteAct(ActorService* service,
                   ActorTaskId task_id,
                   PerformActionsCallback callback) {
    service->OnActCompleted(task_id, std::move(callback),
                            MakeActionResults(mojom::ActionResultCode::kOk));
  }

  ActorToolFactory* GetToolFactory(ActorService* service) {
    return service->tool_factory_.get();
  }

  // Returns whether a PageContext extraction is in progress.
  bool HasPendingObservation(ActorService* service) {
    return !service->pending_observations_.empty();
  }

  // Returns an extractable WebState whose PageContext extraction never
  // completes, so that it only ends when the extraction times out.
  std::unique_ptr<web::FakeWebState> CreateWebStateWithHangingExtraction() {
    // URL of a page that a PageContext extraction accepts.
    static constexpr char kExtractableURL[] = "https://example.com";

    web::test::OverrideJavaScriptFeatures(
        profile_.get(), {PageContextExtractorJavaScriptFeature::GetInstance()});

    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    web_state->SetCurrentURL(GURL(kExtractableURL));
    web_state->SetContentsMimeType("text/html");

    auto main_frame =
        web::FakeWebFrame::CreateMainWebFrame(GURL(kExtractableURL));
    main_frame->set_browser_state(profile_.get());
    main_frame->set_force_timeout(true);
    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    frames_manager->AddWebFrame(std::move(main_frame));
    web_state->SetWebFramesManager(
        PageContextExtractorJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld(),
        std::move(frames_manager));
    return web_state;
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
    ActorTaskId task_id = CreateTask(service);
    EXPECT_FALSE(task_id.is_null());
    EXPECT_TRUE(task_ids.insert(task_id).second);
  }
}

// Tests that requesting tab observation with a null WebState posts an error
// response.
TEST_F(ActorServiceTest, RequestTabObservationWithNullWebStateReturnsFailure) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);

  base::test::TestFuture<PageContextWrapperCallbackResponse> future;
  service->RequestTabObservation(task_id, nullptr, future.GetCallback());
  // Run the command, which posts the reply.
  RunAlreadyPostedTasks();
  EXPECT_FALSE(future.IsReady());
  // Run the posted reply.
  RunAlreadyPostedTasks();
  ASSERT_TRUE(future.IsReady());
  const PageContextWrapperCallbackResponse& response = future.Get();
  ASSERT_FALSE(response.has_value());
  EXPECT_EQ(PageContextWrapperError::kGenericError, response.error());
}

// Tests that requesting tab observation of a non-extractable WebState, whose
// extraction completes synchronously, still replies asynchronously.
TEST_F(ActorServiceTest, RequestTabObservationOfNonExtractablePageIsAsync) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  // A `FakeWebState` has no HTTP(S) URL, so it cannot be extracted.
  web::FakeWebState web_state;

  base::test::TestFuture<PageContextWrapperCallbackResponse> future;
  service->RequestTabObservation(task_id, &web_state, future.GetCallback());
  // Run the command, whose extraction completes synchronously and posts the
  // reply.
  RunAlreadyPostedTasks();
  EXPECT_FALSE(future.IsReady());
  // Run the posted reply.
  RunAlreadyPostedTasks();
  ASSERT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().has_value());
}

// Tests that requesting tab observation with a valid WebState extracts APC.
TEST_F(ActorServiceTest, RequestTabObservationWithValidWebState) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);

  web::WebState::CreateParams params(profile_.get());
  auto web_state = web::WebState::Create(params);

  web_state->GetView().frame = CGRectMake(0, 0, 100, 100);
  UIViewController* root_view_controller = [[UIViewController alloc] init];
  root_view_controller.view = web_state->GetView();

  ScopedKeyWindow scoped_window;
  scoped_window.Get().rootViewController = root_view_controller;

  web_state->WasShown();

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

// Tests that requesting tab observation times out after the configured
// `ActorTabObservationPageContextTimeout` duration.
TEST_F(ActorServiceMockTimeTest, RequestTabObservation_Timeout) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kGeminiActor, {{"ActorTabObservationPageContextTimeout", "500ms"}});

  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  std::unique_ptr<web::FakeWebState> fake_web_state =
      CreateWebStateWithHangingExtraction();

  base::test::TestFuture<PageContextWrapperCallbackResponse> future;
  service->RequestTabObservation(task_id, fake_web_state.get(),
                                 future.GetCallback());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_FALSE(future.IsReady());

  task_environment_.FastForwardBy(base::Milliseconds(100));
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(base::unexpected(PageContextWrapperError::kTimeout), future.Get());
}

// Tests that an extraction still in progress when the service shuts down never
// replies, because `Shutdown()` releases its wrapper along with its completion.
TEST_F(ActorServiceMockTimeTest, ShutdownDropsInProgressTabObservation) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  std::unique_ptr<web::FakeWebState> fake_web_state =
      CreateWebStateWithHangingExtraction();

  base::test::TestFuture<PageContextWrapperCallbackResponse> future;
  service->RequestTabObservation(task_id, fake_web_state.get(),
                                 future.GetCallback());
  // Run the command, which starts the extraction.
  RunAlreadyPostedTasks();
  ASSERT_FALSE(future.IsReady());

  service->Shutdown();
  // Go past the extraction timeout, which would otherwise have replied (see
  // `RequestTabObservation_Timeout`).
  task_environment_.FastForwardBy(GetActorTabObservationPageContextTimeout());
  EXPECT_FALSE(future.IsReady());
}

// Tests that an act whose tab observation is still in progress when the service
// shuts down never replies, and that the observation's journal entries are
// destroyed with it rather than outliving the journal.
TEST_F(ActorServiceMockTimeTest, ShutdownDropsActWithInProgressObservation) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  auto test_browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list->AddBrowser(test_browser.get());

  std::unique_ptr<web::FakeWebState> fake_web_state =
      CreateWebStateWithHangingExtraction();
  web::FakeWebState* fake_web_state_ptr = fake_web_state.get();
  test_browser->GetWebStateList()->InsertWebState(std::move(fake_web_state));
  service->AddControlledWebState(task_id, fake_web_state_ptr);

  base::test::TestFuture<PerformActionsResult> future;
  service->PerformActions(task_id, {}, "Update", future.GetCallback());
  // Run the command, whose empty act completes and posts its completion.
  RunAlreadyPostedTasks();
  // Run the act completion, which starts the observation.
  RunAlreadyPostedTasks();
  ASSERT_TRUE(HasPendingObservation(service));
  ASSERT_FALSE(future.IsReady());

  service->Shutdown();
  // `Shutdown()` released the extraction, and with it the barrier holding the
  // journal entry.
  EXPECT_FALSE(HasPendingObservation(service));
  // Go past the extraction timeout, which would otherwise have replied.
  task_environment_.FastForwardBy(GetActorTabObservationPageContextTimeout());
  EXPECT_FALSE(future.IsReady());

  browser_list->RemoveBrowser(test_browser.get());
}

// Tests that GetWebStateForID returns nullptr for a tab that is not controlled
// by the task.
TEST_F(ActorServiceTest, GetWebStateForID_NotControlled) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);

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

  ActorTaskId task_id = CreateTask(service);

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

  ActorTaskId task_id = CreateTask(service);

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

// Test that `PerformActions` issued while a previous request is executing is
// rejected with `kExecutionEngineExistingAction` without adding its target
// WebState to the task.
TEST_F(ActorServiceTest, PerformActionsWhileActingDoesNotAddWebStates) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  auto test_browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list->AddBrowser(test_browser.get());

  auto fake_web_state = std::make_unique<web::FakeWebState>();
  web::WebStateID web_state_id = fake_web_state->GetUniqueIdentifier();
  test_browser->GetWebStateList()->InsertWebState(std::move(fake_web_state));

  // Untargeted, so the first request stays in flight until its tool completes.
  base::test::TestFuture<PerformActionsResult> first_future;
  service->PerformActions(task_id, {MakeSuccessfulActorAction()}, "First",
                          first_future.GetCallback());
  ASSERT_FALSE(first_future.IsReady());

  base::test::TestFuture<PerformActionsResult> second_future;
  service->PerformActions(task_id, {MakeSuccessfulActorAction(web_state_id)},
                          "Second", second_future.GetCallback());
  EXPECT_FALSE(second_future.IsReady());
  EXPECT_EQ(nullptr, service->GetWebStateForID(web_state_id, task_id));

  const PerformActionsResult& second_result = second_future.Get();
  ASSERT_EQ(1u, second_result.action_results.size());
  EXPECT_EQ(mojom::ActionResultCode::kExecutionEngineExistingAction,
            second_result.action_results[0].tool_result.code());
  EXPECT_EQ(nullptr, service->GetWebStateForID(web_state_id, task_id));
  EXPECT_TRUE(HasJournalEntryWithDetail(
      *GetJournal(service), "ActorService::PerformActions", "error",
      base::StringPrintf(
          "Act rejected with mojom::ActionResultCode[%d]",
          std::to_underlying(
              mojom::ActionResultCode::kExecutionEngineExistingAction))));

  ASSERT_TRUE(first_future.Wait());

  browser_list->RemoveBrowser(test_browser.get());
}

// Tests that GetWebStateForID does not find a tab in an incognito browser if
// the task does not allow incognito.
TEST_F(ActorServiceTest, GetWebStateForID_Incognito_NotAllowed) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);

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

  EXPECT_DEATH_IF_SUPPORTED(
      service->CreateTask("Test Task", TestSource(),
                          /*allow_incognito_web_states=*/true),
      "");
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

  CreateTask(service);
  EXPECT_EQ(ActorTaskState::kInit, service->GetActiveTaskState());
}

// Tests that PerformActions completes immediately when the WebState is not
// loading.
TEST_F(ActorServiceTest, PerformActions_NoLoading_InstantCompletion) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);

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

// Tests that PerformActions logs the actions proto to AggregatedJournal.
TEST_F(ActorServiceTest, PerformActions_LogsActionsToJournal) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId invalid_task_id = ActorTaskId(999);

  optimization_guide::proto::Action action;
  action.mutable_wait()->set_wait_time_ms(0);
  std::vector<optimization_guide::proto::Action> actions = {action};

  base::test::TestFuture<PerformActionsResult> future;
  service->PerformActions(invalid_task_id, actions, "Update",
                          future.GetCallback());
  PerformActionsResult result = future.Take();
  ASSERT_EQ(1u, result.action_results.size());
  EXPECT_EQ(mojom::ActionResultCode::kTaskWentAway,
            result.action_results[0].tool_result.code());

  EXPECT_TRUE(HasJournalEntry(*GetJournal(service), "PerformActions"));
  EXPECT_TRUE(HasJournalEntryWithDetail(
      *GetJournal(service), "PerformActions", "proto_type",
      "chrome_intelligence_proto_features.Actions"));
}

// Tests that `PerformActionsFinished` is logged with the code of the first
// failed action once a batch of actions finishes.
TEST_F(ActorServiceTest, PerformActions_LogsPerformActionsFinished) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);
  RecordingJournalObserver observer(*GetJournal(service));

  ActorTaskId task_id = CreateTask(service);
  PerformActionsResult result =
      PerformActions(service, task_id, {MakeFailingActorAction()});
  ASSERT_EQ(1u, result.action_results.size());
  const mojom::ActionResultCode failure_code =
      result.action_results[0].tool_result.code();
  ASSERT_NE(mojom::ActionResultCode::kOk, failure_code);

  std::vector<const mojom::JournalEntry*> entries =
      observer.EntriesNamed("PerformActionsFinished");
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(mojom::JournalEntryType::kInstant, entries[0]->type);
  EXPECT_EQ(MakeBrowserTrackUUID(task_id), entries[0]->track_uuid);
  EXPECT_EQ(base::ToString(failure_code),
            FindJournalDetail(*entries[0], "result_code"));
}

// Tests that no `TabObservationController` slice is logged when a batch of
// actions leaves no WebState to observe.
TEST_F(ActorServiceTest,
       PerformActions_NoObservedWebState_SkipsTabObservationController) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);
  RecordingJournalObserver observer(*GetJournal(service));

  ActorTaskId task_id = CreateTask(service);
  PerformActions(service, task_id);

  std::vector<const mojom::JournalEntry*> finished_entries =
      observer.EntriesNamed("PerformActionsFinished");
  ASSERT_EQ(1u, finished_entries.size());
  EXPECT_EQ("kOk", FindJournalDetail(*finished_entries[0], "result_code"));
  EXPECT_TRUE(observer.EntriesNamed("TabObservationController").empty());
  EXPECT_TRUE(observer.EntriesNamed("RequestTabObservation").empty());
}

// Tests that the tab observation following a batch of actions is wrapped in a
// `TabObservationController` slice, with each `RequestTabObservation` slice
// nested under it on the task's Browser track.
TEST_F(ActorServiceTest,
       PerformActions_WrapsRequestTabObservationInTabObservationController) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);
  RecordingJournalObserver observer(*GetJournal(service));

  ActorTaskId task_id = CreateTask(service);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  auto test_browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list->AddBrowser(test_browser.get());

  auto fake_web_state = std::make_unique<web::FakeWebState>();
  auto* fake_web_state_ptr = fake_web_state.get();
  test_browser->GetWebStateList()->InsertWebState(std::move(fake_web_state));

  PerformActionsResult result = PerformActions(
      service, task_id,
      {MakeSuccessfulActorAction(fake_web_state_ptr->GetUniqueIdentifier())});
  ASSERT_EQ(1u, result.action_results.size());
  ASSERT_TRUE(result.action_results[0].tool_result.IsOk());
  ASSERT_EQ(1u, result.page_contexts.size());

  std::vector<std::pair<mojom::JournalEntryType, std::string>>
      observation_entries;
  for (const mojom::JournalEntryPtr& entry : observer.entries()) {
    if (entry->event != "TabObservationController" &&
        entry->event != "RequestTabObservation") {
      continue;
    }
    EXPECT_EQ(MakeBrowserTrackUUID(task_id), entry->track_uuid);
    observation_entries.emplace_back(entry->type, entry->event);
  }
  EXPECT_THAT(
      observation_entries,
      testing::ElementsAre(
          testing::Pair(mojom::JournalEntryType::kBegin,
                        "TabObservationController"),
          testing::Pair(mojom::JournalEntryType::kBegin,
                        "RequestTabObservation"),
          testing::Pair(mojom::JournalEntryType::kEnd, "RequestTabObservation"),
          testing::Pair(mojom::JournalEntryType::kEnd,
                        "TabObservationController")));
  EXPECT_TRUE(IsStrictlyNestedPerTrack(observer.entries()));

  std::vector<const mojom::JournalEntry*> controller_entries =
      observer.EntriesNamed("TabObservationController");
  ASSERT_FALSE(controller_entries.empty());
  EXPECT_EQ("kOk", FindJournalDetail(*controller_entries[0], "result_code"));
  EXPECT_EQ("false", FindJournalDetail(*controller_entries[0],
                                       "skip_async_observation_information"));

  browser_list->RemoveBrowser(test_browser.get());
}

// Tests that PerformActions is deferred when the WebState is loading, and only
// resolves when loading completes.
TEST_F(ActorServiceTest, PerformActions_Loading_DeferredUntilStopLoading) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);

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

  ActorTaskId task_id = CreateTask(service);

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

// Test that StopTask calls Stop() on the task, removes it from the active
// tasks, and destroys it asynchronously.
TEST_F(ActorServiceTest, StopTask) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);

  // Verify that the task exists in service's active tasks.
  ASSERT_TRUE(HasTask(service, task_id));

  // Swap the task with our MockActorTask.
  bool stop_called = false;
  SwapTask(service, task_id,
           std::make_unique<MockActorTask>(
               task_id, "Test Task",
               /*allow_incognito_web_states=*/false, GetJournal(service),
               GetToolFactory(service),
               BrowserListFactory::GetForProfile(profile_.get()),
               MakeOnStoppedCallback(service, task_id), &stop_called));

  // Stop the task.
  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  EXPECT_FALSE(stop_called);
  EXPECT_NE(nullptr, GetTask(service, task_id));

  // Run the enqueued stop only; the deletion it enqueues runs afterwards.
  RunAlreadyPostedTasks();

  // Verify that Stop() was called on our MockActorTask.
  EXPECT_TRUE(stop_called);

  // Verify the task is inactive once stopped but only destroyed once the
  // enqueued deletion runs.
  EXPECT_EQ(std::nullopt, service->GetActiveTaskState());
  EXPECT_EQ(nullptr, GetTask(service, task_id));
  EXPECT_TRUE(HasTask(service, task_id));
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !HasTask(service, task_id); }));
}

// Test lifecycle of a single task updates observer.
TEST_F(ActorServiceTest, TaskUpdatesObserverLifecycle) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  FakeActorServiceTaskUpdatesObserver* observer =
      [[FakeActorServiceTaskUpdatesObserver alloc] init];
  service->AddTaskUpdatesObserver(observer);

  ActorTaskId task_id = CreateTask(service);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer.registeredCount == 1; }));
  EXPECT_EQ(1, observer.registeredCount);
  EXPECT_NSEQ(@"Test Task", observer.taskTitle);

  PerformActions(service, task_id);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer.stateChangeCount > 0; }));
  EXPECT_GT(observer.stateChangeCount, 0);

  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer.stoppedCount == 1; }));
  EXPECT_EQ(1, observer.stoppedCount);

  service->RemoveTaskUpdatesObserver(observer);
}

// Test that adding an observer when a task is already running immediately
// attaches the observer to the active task.
TEST_F(ActorServiceTest, LateAddedTaskUpdatesObserverAttachesToActiveTasks) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service, "Active Task");

  FakeActorServiceTaskUpdatesObserver* observer =
      [[FakeActorServiceTaskUpdatesObserver alloc] init];
  service->AddTaskUpdatesObserver(observer);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer.registeredCount == 1; }));
  EXPECT_EQ(1, observer.registeredCount);
  EXPECT_NSEQ(@"Active Task", observer.taskTitle);

  PerformActions(service, task_id);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer.stateChangeCount > 0; }));
  EXPECT_GT(observer.stateChangeCount, 0);

  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer.stoppedCount == 1; }));
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
  ActorTaskId task_id = CreateTask(service, "Shared Task");
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer1.registeredCount == 1 && observer2.registeredCount == 1;
  }));
  EXPECT_EQ(1, observer1.registeredCount);
  EXPECT_NSEQ(@"Shared Task", observer1.taskTitle);
  EXPECT_EQ(1, observer2.registeredCount);
  EXPECT_NSEQ(@"Shared Task", observer2.taskTitle);

  // Performing actions transitions task state and broadcasts to all observers.
  PerformActions(service, task_id);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer1.stateChangeCount > 0; }));
  EXPECT_GT(observer1.stateChangeCount, 0);
  EXPECT_EQ(observer1.stateChangeCount, observer2.stateChangeCount);

  // Stopping the task should only notify the remaining active observer.
  service->RemoveTaskUpdatesObserver(observer1);
  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer2.stoppedCount == 1; }));
  EXPECT_EQ(0, observer1.stoppedCount);
  EXPECT_EQ(1, observer2.stoppedCount);

  // Creating a new task should only notify observer2.
  CreateTask(service, "Next Task");
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer2.registeredCount == 2; }));
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

  CreateTask(service);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer.registeredCount == 1; }));
  EXPECT_EQ(1, observer.registeredCount);

  service->RemoveTaskUpdatesObserver(observer);
}

// Test that a task-scoped observer only receives updates for its task and
// stops receiving them once removed.
TEST_F(ActorServiceTest, TaskScopedUpdatesObserver) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId observed_task_id = CreateTask(service, "Observed Task");
  ActorTaskId other_task_id = CreateTask(service, "Other Task");

  FakeActorServiceTaskUpdatesObserver* observer =
      [[FakeActorServiceTaskUpdatesObserver alloc] init];
  EXPECT_TRUE(service->AddTaskUpdatesObserver(observed_task_id, observer));
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer.registeredCount == 1; }));
  EXPECT_NSEQ(@"Observed Task", observer.taskTitle);

  // Neither other tasks nor newly created tasks notify the observer.
  service->StopTask(other_task_id, ActorTaskStoppedReason::kStoppedByUser);
  CreateTask(service, "New Task");
  FlushCurrentSequence();
  EXPECT_EQ(1, observer.registeredCount);
  EXPECT_EQ(0, observer.stoppedCount);

  service->RemoveTaskUpdatesObserver(observed_task_id, observer);
  service->StopTask(observed_task_id, ActorTaskStoppedReason::kStoppedByUser);
  FlushCurrentSequence();
  EXPECT_EQ(0, observer.stoppedCount);
}

// Test that adding a task-scoped observer fails for an unknown task.
TEST_F(ActorServiceTest, AddTaskScopedUpdatesObserverFailures) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !HasTask(service, task_id); }));

  FakeActorServiceTaskUpdatesObserver* observer =
      [[FakeActorServiceTaskUpdatesObserver alloc] init];
  EXPECT_FALSE(service->AddTaskUpdatesObserver(task_id, observer));
  EXPECT_EQ(0, observer.registeredCount);
}

// Tests that a task lifecycle observer is notified asynchronously, with the
// task source, when a task is started and stopped, and that the stop
// notification is delivered after the task has left the active tasks.
TEST_F(ActorServiceTest, TaskLifecycleObserverNotifiedOnStartAndStop) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  FakeActorTaskLifecycleObserver* observer =
      [[FakeActorTaskLifecycleObserver alloc] init];
  service->AddTaskLifecycleObserver(observer);
  // Complete the registration, so that the check below covers the start
  // notification itself.
  FlushCurrentSequence();

  ActorTaskId task_id = CreateTask(service);
  EXPECT_EQ(0, observer.startedCount);
  FlushCurrentSequence();
  EXPECT_EQ(1, observer.startedCount);
  EXPECT_EQ(task_id, observer.lastTaskID);
  EXPECT_EQ(TaskSourceInfo::Client::kTest, observer.lastSourceType);

  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  RunAlreadyPostedTasks();
  EXPECT_EQ(nullptr, GetTask(service, task_id));
  EXPECT_EQ(0, observer.stoppedCount);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer.stoppedCount == 1 && !HasTask(service, task_id);
  }));
  EXPECT_EQ(task_id, observer.lastTaskID);
  EXPECT_EQ(ActorTaskState::kCancelled, observer.lastFinalState);

  service->RemoveTaskLifecycleObserver(observer);
}

// Tests that adding a task lifecycle observer when a task is already active
// does not replay its start, even if the start was not delivered yet, but still
// notifies when that task stops.
TEST_F(ActorServiceTest, LateAddedTaskLifecycleObserverNotReplayed) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service, "Active Task");

  FakeActorTaskLifecycleObserver* observer =
      [[FakeActorTaskLifecycleObserver alloc] init];
  service->AddTaskLifecycleObserver(observer);
  FlushCurrentSequence();
  EXPECT_EQ(0, observer.startedCount);

  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  FlushCurrentSequence();
  EXPECT_EQ(1, observer.stoppedCount);
  EXPECT_EQ(task_id, observer.lastTaskID);
  EXPECT_EQ(0, observer.startedCount);

  service->RemoveTaskLifecycleObserver(observer);
}

// Tests broadcasting to multiple task lifecycle observers and selective
// removal.
TEST_F(ActorServiceTest, MultipleTaskLifecycleObserversBroadcastAndRemoval) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  FakeActorTaskLifecycleObserver* observer1 =
      [[FakeActorTaskLifecycleObserver alloc] init];
  FakeActorTaskLifecycleObserver* observer2 =
      [[FakeActorTaskLifecycleObserver alloc] init];
  service->AddTaskLifecycleObserver(observer1);
  service->AddTaskLifecycleObserver(observer2);

  ActorTaskId task_id = CreateTask(service, "Shared Task");
  FlushCurrentSequence();
  EXPECT_EQ(1, observer1.startedCount);
  EXPECT_EQ(1, observer2.startedCount);

  // Stopping the task should only notify the remaining observer.
  service->RemoveTaskLifecycleObserver(observer1);
  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  FlushCurrentSequence();
  EXPECT_EQ(0, observer1.stoppedCount);
  EXPECT_EQ(1, observer2.stoppedCount);

  // Creating a new task should only notify the remaining observer.
  CreateTask(service, "Next Task");
  FlushCurrentSequence();
  EXPECT_EQ(1, observer1.startedCount);
  EXPECT_EQ(2, observer2.startedCount);

  service->RemoveTaskLifecycleObserver(observer2);
}

// Tests that adding a duplicate task lifecycle observer is ignored.
TEST_F(ActorServiceTest, DuplicateTaskLifecycleObserverIgnored) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  FakeActorTaskLifecycleObserver* observer =
      [[FakeActorTaskLifecycleObserver alloc] init];
  service->AddTaskLifecycleObserver(observer);
  service->AddTaskLifecycleObserver(observer);

  CreateTask(service);
  FlushCurrentSequence();
  EXPECT_EQ(1, observer.startedCount);

  service->RemoveTaskLifecycleObserver(observer);
}

// Tests that a task lifecycle observer removed before a pending notification is
// delivered does not receive it.
TEST_F(ActorServiceTest,
       TaskLifecycleObserverRemovedBeforeDeliveryNotNotified) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  FakeActorTaskLifecycleObserver* observer =
      [[FakeActorTaskLifecycleObserver alloc] init];
  service->AddTaskLifecycleObserver(observer);
  // Complete the registration, so that the removal below cancels a pending
  // notification rather than a pending registration.
  FlushCurrentSequence();

  CreateTask(service);
  service->RemoveTaskLifecycleObserver(observer);
  FlushCurrentSequence();
  EXPECT_EQ(0, observer.startedCount);
}

// Tests that a task lifecycle observer stopping a task from its start
// notification runs outside `CreateTask()`, and receives the stop on a later
// turn.
TEST_F(ActorServiceTest, TaskLifecycleObserverStopsTaskFromStart) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  FakeActorTaskLifecycleObserver* observer =
      [[FakeActorTaskLifecycleObserver alloc] init];
  observer.onStart = ^(ActorTaskId task_id) {
    service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  };
  service->AddTaskLifecycleObserver(observer);
  // Complete the registration, so that the check below covers the start
  // notification itself.
  FlushCurrentSequence();

  ActorTaskId task_id = CreateTask(service);
  EXPECT_EQ(0, observer.startedCount);

  // Delivers the start, from which the observer stops the task, posting the
  // stop.
  RunAlreadyPostedTasks();
  EXPECT_EQ(1, observer.startedCount);
  EXPECT_EQ(0, observer.stoppedCount);

  // Delivers the stop.
  FlushCurrentSequence();
  EXPECT_EQ(1, observer.stoppedCount);
  EXPECT_EQ(0, observer.stoppedCountAfterStart);
  EXPECT_EQ(1, observer.startedCountAtStop);
  EXPECT_EQ(task_id, observer.lastTaskID);

  service->RemoveTaskLifecycleObserver(observer);
}

// Tests that a task stopped right after its creation is reported as started
// before it is reported as stopped, and that its start notification runs ahead
// of the queued stop, so it can still add task-scoped update observers.
TEST_F(ActorServiceTest, TaskLifecycleStartDeliveredBeforeStop) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  FakeActorTaskLifecycleObserver* observer =
      [[FakeActorTaskLifecycleObserver alloc] init];
  service->AddTaskLifecycleObserver(observer);

  FakeActorServiceTaskUpdatesObserver* updates_observer =
      [[FakeActorServiceTaskUpdatesObserver alloc] init];
  __block bool add_updates_observer_result = false;
  observer.onStart = ^(ActorTaskId started_task_id) {
    add_updates_observer_result =
        service->AddTaskUpdatesObserver(started_task_id, updates_observer);
  };

  ActorTaskId task_id = CreateTask(service);
  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  FlushCurrentSequence();
  EXPECT_EQ(1, observer.stoppedCount);
  EXPECT_EQ(1, observer.startedCountAtStop);
  EXPECT_EQ(1, observer.startedCount);
  EXPECT_TRUE(add_updates_observer_result);

  service->RemoveTaskLifecycleObserver(observer);
}

// Tests that a task-scoped observer added after a stop was enqueued, but before
// it ran, receives the registration snapshot and then the stop.
TEST_F(ActorServiceTest, TaskScopedObserverAddedAfterStopReceivesStop) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);

  FakeActorServiceTaskUpdatesObserver* observer =
      [[FakeActorServiceTaskUpdatesObserver alloc] init];
  EXPECT_TRUE(service->AddTaskUpdatesObserver(task_id, observer));
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer.stoppedCount == 1; }));
  EXPECT_EQ(1, observer.registeredCount);
  EXPECT_FALSE(HasTask(service, task_id));
}

// Tests that `StopAllTasks` only stops the tasks active when it is called.
TEST_F(ActorServiceTest, StopAllTasksDoesNotStopTasksCreatedAfter) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId stopped_task_id = CreateTask(service);
  service->StopAllTasks();
  ActorTaskId new_task_id = CreateTask(service);

  // `StopAllTasks` enqueues its stops in a single turn, so a stray stop of the
  // new task would have run by the time the first task is gone.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !HasTask(service, stopped_task_id); }));
  EXPECT_TRUE(HasTask(service, new_task_id));
}

// Tests that actions enqueued after a stop are rejected with `kTaskWentAway`.
TEST_F(ActorServiceTest, PerformActionsAfterStopTaskRepliesTaskWentAway) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);

  PerformActionsResult result = PerformActions(service, task_id);
  ASSERT_EQ(1u, result.action_results.size());
  EXPECT_EQ(mojom::ActionResultCode::kTaskWentAway,
            result.action_results[0].tool_result.code());
}

// Tests that a tab observation requested after a stop replies with a generic
// error instead of extracting.
TEST_F(ActorServiceTest, RequestTabObservationAfterStopTaskRepliesError) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  // A `FakeWebState` cannot be extracted, so an extraction would reply with
  // `kPageNotExtractableError` rather than `kGenericError`.
  web::FakeWebState web_state;
  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);

  base::test::TestFuture<PageContextWrapperCallbackResponse> future;
  service->RequestTabObservation(task_id, &web_state, future.GetCallback());
  ASSERT_TRUE(future.Wait());
  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(PageContextWrapperError::kGenericError, future.Get().error());
}

// Tests that a tab observation whose WebState is destroyed before the command
// runs replies with a generic error instead of extracting.
TEST_F(ActorServiceTest, RequestTabObservationOfWebStateDestroyedBeforeRun) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  // A `FakeWebState` cannot be extracted, so an extraction would reply with
  // `kPageNotExtractableError` rather than `kGenericError`.
  auto web_state = std::make_unique<web::FakeWebState>();

  base::test::TestFuture<PageContextWrapperCallbackResponse> future;
  service->RequestTabObservation(task_id, web_state.get(),
                                 future.GetCallback());
  web_state.reset();

  ASSERT_TRUE(future.Wait());
  ASSERT_FALSE(future.Get().has_value());
  EXPECT_EQ(PageContextWrapperError::kGenericError, future.Get().error());
}

// Tests that commands run in call order: the act runs before the stop.
TEST_F(ActorServiceTest, CommandsRunInCallOrder) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  FakeActorServiceTaskUpdatesObserver* observer =
      [[FakeActorServiceTaskUpdatesObserver alloc] init];
  ASSERT_TRUE(service->AddTaskUpdatesObserver(task_id, observer));
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer.registeredCount == 1; }));
  const size_t base_count = observer.states.size();

  base::test::TestFuture<PerformActionsResult> future;
  service->PerformActions(task_id, {}, "Update", future.GetCallback());
  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  ASSERT_TRUE(future.Wait());
  // The act's completion runs after the stop, so it replies `kTaskWentAway`.
  ASSERT_EQ(1u, future.Get().action_results.size());
  EXPECT_EQ(mojom::ActionResultCode::kTaskWentAway,
            future.Get().action_results[0].tool_result.code());
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer.stoppedCount == 1; }));

  // An empty act completes within its command, so it transitions to `kActing`
  // then `kReflecting` before the stop transitions to `kCancelled`. A stop
  // running first would transition to `kCancelled` only.
  const std::vector<ActorTaskState> expected_states = {
      ActorTaskState::kActing, ActorTaskState::kReflecting,
      ActorTaskState::kCancelled};
  EXPECT_EQ(expected_states,
            std::vector<ActorTaskState>(observer.states.begin() + base_count,
                                        observer.states.end()));
}

// Tests that a stop issued while an interrupt is still queued ends the task,
// reports a single stop, and never asks the delegate for confirmation.
TEST_F(ActorServiceTest, StopQueuedAfterInterruptStopsTask) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  FakeActorTaskLifecycleObserver* observer =
      [[FakeActorTaskLifecycleObserver alloc] init];
  service->AddTaskLifecycleObserver(observer);
  ActorTaskId task_id = CreateTask(service);
  FakeActorTaskInterventionDelegate* delegate =
      [[FakeActorTaskInterventionDelegate alloc] init];
  service->SetTaskInterventionDelegate(task_id, delegate);

  service->InterruptTask(task_id,
                         ActorTaskInterruptReason::kWaitingUserConfirmation,
                         "Please confirm");
  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer.stoppedCount == 1; }));
  EXPECT_FALSE(HasTask(service, task_id));
  EXPECT_EQ(ActorTaskState::kCancelled, observer.lastFinalState);
  // The stop notification is posted after any confirmation request the
  // interrupt posted, so that request already ran or was dropped.
  EXPECT_FALSE(delegate.requestConfirmationCalled);
  EXPECT_EQ(1, observer.stoppedCount);

  service->RemoveTaskLifecycleObserver(observer);
}

// Tests that commands issued from a reply run after the commands they follow.
TEST_F(ActorServiceTest, CommandsIssuedFromReplyRunInCallOrder) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  base::test::TestFuture<PerformActionsResult> future;
  service->PerformActions(
      task_id, {}, "Update",
      base::BindOnce(
          [](ActorService* service, ActorTaskId task_id,
             PerformActionsCallback callback, PerformActionsResult result) {
            service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
            service->PerformActions(task_id, {}, "Update", std::move(callback));
          },
          base::Unretained(service), task_id, future.GetCallback()));

  ASSERT_TRUE(future.Wait());
  ASSERT_EQ(1u, future.Get().action_results.size());
  EXPECT_EQ(mojom::ActionResultCode::kTaskWentAway,
            future.Get().action_results[0].tool_result.code());
}

// Tests that a command for an unknown task replies after two hops: the queued
// command, then its posted reply. This is internal structure, not the public
// contract, that the `ShutdownDropsPosted*` tests rely on to call `Shutdown()`
// between the two.
TEST_F(ActorServiceTest, CommandForUnknownTaskRepliesAfterTwoHops) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  base::test::TestFuture<PerformActionsResult> future;
  service->PerformActions(kUnknownTaskId, {}, "Update", future.GetCallback());
  RunAlreadyPostedTasks();
  EXPECT_FALSE(future.IsReady());
  RunAlreadyPostedTasks();

  ASSERT_TRUE(future.IsReady());
  ASSERT_EQ(1u, future.Get().action_results.size());
  EXPECT_EQ(mojom::ActionResultCode::kTaskWentAway,
            future.Get().action_results[0].tool_result.code());
}

// Tests that a command still queued when the service shuts down never replies.
TEST_F(ActorServiceTest, ShutdownDropsQueuedCommands) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  base::test::TestFuture<PerformActionsResult> future;
  service->PerformActions(kUnknownTaskId, {}, "Update", future.GetCallback());
  service->Shutdown();
  // Drain the sequence: dropped work has no observable effect to wait on.
  FlushCurrentSequence();

  EXPECT_FALSE(future.IsReady());
}

// Tests that a stop still queued when the service shuts down never runs.
TEST_F(ActorServiceTest, ShutdownDropsQueuedStop) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  bool stop_called = false;
  SwapTask(service, task_id,
           std::make_unique<MockActorTask>(
               task_id, "Test Task",
               /*allow_incognito_web_states=*/false, GetJournal(service),
               GetToolFactory(service),
               BrowserListFactory::GetForProfile(profile_.get()),
               MakeOnStoppedCallback(service, task_id), &stop_called));

  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  service->Shutdown();
  // Drain the sequence: dropped work has no observable effect to wait on.
  FlushCurrentSequence();

  EXPECT_FALSE(stop_called);
}

// Tests that an interrupt still queued when the service shuts down never asks
// the delegate for confirmation.
TEST_F(ActorServiceTest, ShutdownDropsQueuedInterrupt) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  FakeActorTaskInterventionDelegate* delegate =
      [[FakeActorTaskInterventionDelegate alloc] init];
  service->SetTaskInterventionDelegate(task_id, delegate);

  service->InterruptTask(task_id,
                         ActorTaskInterruptReason::kWaitingUserConfirmation,
                         "Please confirm");
  service->Shutdown();
  // Drain the sequence: dropped work has no observable effect to wait on.
  FlushCurrentSequence();

  EXPECT_FALSE(delegate.requestConfirmationCalled);
}

// Tests that a reply already posted when the service shuts down is not run.
TEST_F(ActorServiceTest, ShutdownDropsPostedReplies) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  base::test::TestFuture<PerformActionsResult> future;
  service->PerformActions(kUnknownTaskId, {}, "Update", future.GetCallback());
  // Run the command, which posts its reply.
  RunAlreadyPostedTasks();
  service->Shutdown();
  // Drain the sequence: dropped work has no observable effect to wait on.
  FlushCurrentSequence();

  EXPECT_FALSE(future.IsReady());
}

// Tests that a tab observation reply already posted when the service shuts
// down is not run.
TEST_F(ActorServiceTest, ShutdownDropsPostedTabObservationReply) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  base::test::TestFuture<PageContextWrapperCallbackResponse> future;
  service->RequestTabObservation(task_id, nullptr, future.GetCallback());
  // Run the command, which posts its reply.
  RunAlreadyPostedTasks();
  service->Shutdown();
  // Drain the sequence: dropped work has no observable effect to wait on.
  FlushCurrentSequence();

  EXPECT_FALSE(future.IsReady());
}

// Tests that an extraction reply already posted when the service shuts down is
// not run.
TEST_F(ActorServiceTest, ShutdownDropsPostedExtractionReply) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  // A `FakeWebState` cannot be extracted, so its extraction completes
  // synchronously.
  web::FakeWebState web_state;
  base::test::TestFuture<PageContextWrapperCallbackResponse> future;
  service->RequestTabObservation(task_id, &web_state, future.GetCallback());
  // Run the command, whose extraction completes and posts its reply.
  RunAlreadyPostedTasks();
  service->Shutdown();
  // Drain the sequence: dropped work has no observable effect to wait on.
  FlushCurrentSequence();

  EXPECT_FALSE(future.IsReady());
}

// Tests that an act completion already posted when the service shuts down
// never replies.
TEST_F(ActorServiceTest, ShutdownDropsPostedActCompletion) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  base::test::TestFuture<PerformActionsResult> future;
  service->PerformActions(task_id, {}, "Update", future.GetCallback());
  // Run the command, whose empty act completes and posts its completion.
  RunAlreadyPostedTasks();
  ASSERT_FALSE(future.IsReady());

  service->Shutdown();
  // Drain the sequence: dropped work has no observable effect to wait on.
  FlushCurrentSequence();

  EXPECT_FALSE(future.IsReady());
}

// Tests that an act whose tab observation completes synchronously replies from
// the act completion. Internal: a separately posted reply would hold the
// journal entry and could run after the journal is destroyed.
TEST_F(ActorServiceTest, ActObservationCompletingSynchronouslyRepliesDirectly) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  auto test_browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list->AddBrowser(test_browser.get());

  // A `FakeWebState` cannot be extracted, so its extraction completes
  // synchronously.
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  web::WebState* fake_web_state_ptr = fake_web_state.get();
  test_browser->GetWebStateList()->InsertWebState(std::move(fake_web_state));
  service->AddControlledWebState(task_id, fake_web_state_ptr);

  base::test::TestFuture<PerformActionsResult> future;
  service->PerformActions(task_id, {}, "Update", future.GetCallback());
  // Run the command, whose empty act completes and posts its completion.
  RunAlreadyPostedTasks();
  EXPECT_FALSE(future.IsReady());
  // Run the act completion, whose observation completes and replies.
  RunAlreadyPostedTasks();
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(1u, future.Get().page_contexts.size());

  browser_list->RemoveBrowser(test_browser.get());
}

// Tests that a command issued after the service shut down never replies.
TEST_F(ActorServiceTest, ShutdownDropsCommandsIssuedAfterwards) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  service->Shutdown();
  base::test::TestFuture<PerformActionsResult> future;
  service->PerformActions(kUnknownTaskId, {}, "Update", future.GetCallback());
  // Drain the sequence: dropped work has no observable effect to wait on.
  FlushCurrentSequence();

  EXPECT_FALSE(future.IsReady());
}

// Tests that calling `CreateTask()` after `Shutdown()` crashes.
TEST_F(ActorServiceTest, CreateTaskAfterShutdownCrashes) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  service->Shutdown();
  EXPECT_DEATH_IF_SUPPORTED(CreateTask(service), "");
}

// Tests that `GetWeakPtr()` is null after `Shutdown()`, for both pointers
// obtained before and after it, so that no work bound to it runs afterwards.
TEST_F(ActorServiceTest, GetWeakPtrNullAfterShutdown) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  base::WeakPtr<ActorService> weak_service = service->GetWeakPtr();
  ASSERT_TRUE(weak_service);
  service->Shutdown();

  EXPECT_FALSE(weak_service);
  EXPECT_FALSE(service->GetWeakPtr());
}

// Tests that lifecycle notifications still pending when the service shuts down
// are not delivered.
TEST_F(ActorServiceTest, ShutdownDropsPendingLifecycleNotifications) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  FakeActorTaskLifecycleObserver* observer =
      [[FakeActorTaskLifecycleObserver alloc] init];
  service->AddTaskLifecycleObserver(observer);
  FlushCurrentSequence();

  ActorTaskId task_id = CreateTask(service);
  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  service->Shutdown();
  FlushCurrentSequence();

  EXPECT_EQ(0, observer.startedCount);
  EXPECT_EQ(0, observer.stoppedCount);
}

// Tests that stopping all tasks posts one lifecycle stop per task, none of
// which is delivered synchronously.
TEST_F(ActorServiceTest, StopAllTasksPostsOneStopPerTask) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  FakeActorTaskLifecycleObserver* observer =
      [[FakeActorTaskLifecycleObserver alloc] init];
  service->AddTaskLifecycleObserver(observer);
  CreateTask(service, "First Task");
  ActorTaskId last_task_id = CreateTask(service, "Second Task");
  FlushCurrentSequence();

  service->StopAllTasks();
  EXPECT_EQ(0, observer.stoppedCount);
  FlushCurrentSequence();
  EXPECT_EQ(2, observer.stoppedCount);
  EXPECT_EQ(last_task_id, observer.lastTaskID);

  service->RemoveTaskLifecycleObserver(observer);
}

// Test that `GetWeakTaskBackgroundWorkers()` returns one weak pointer per
// active task, in task ID order, that the list shrinks when a task is stopped,
// and that a stopped task's weak pointer is invalidated.
TEST_F(ActorServiceTest, GetWeakTaskBackgroundWorkers) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);
  EXPECT_TRUE(service->GetWeakTaskBackgroundWorkers().empty());

  ActorTaskId first_id = CreateTask(service);
  ActorTaskId second_id = CreateTask(service);

  std::vector<base::WeakPtr<ActorTaskBackgroundWorker>> workers =
      service->GetWeakTaskBackgroundWorkers();
  ASSERT_EQ(2u, workers.size());
  ASSERT_TRUE(workers[0]);
  ASSERT_TRUE(workers[1]);
  EXPECT_EQ(first_id, workers[0]->task_id());
  EXPECT_EQ(second_id, workers[1]->task_id());
  EXPECT_EQ(GetTask(service, first_id)->background_worker(), workers[0].get());
  base::WeakPtr<ActorTaskBackgroundWorker> first_worker = workers[0];

  service->StopTask(first_id, ActorTaskStoppedReason::kStoppedByUser);

  ASSERT_TRUE(base::test::RunUntil([&]() { return !first_worker; }));
  workers = service->GetWeakTaskBackgroundWorkers();
  ASSERT_EQ(1u, workers.size());
  ASSERT_TRUE(workers[0]);
  EXPECT_EQ(second_id, workers[0]->task_id());
}

// Test that `GetForProfileIfExists()` returns null until the service is
// created, and the same instance afterwards.
TEST_F(ActorServiceTest, GetForProfileIfExistsDoesNotCreateService) {
  EXPECT_EQ(nullptr,
            ActorServiceFactory::GetForProfileIfExists(profile_.get()));

  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);
  EXPECT_EQ(service,
            ActorServiceFactory::GetForProfileIfExists(profile_.get()));
}

// Test that SetTaskInterventionDelegate sets the intervention delegate on an
// active task, and that InterruptTask successfully interrupts the task and
// invokes the delegate.
TEST_F(ActorServiceTest, SetTaskInterventionDelegateAndInterruptTask) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  ASSERT_TRUE(HasTask(service, task_id));

  FakeActorTaskInterventionDelegate* delegate =
      [[FakeActorTaskInterventionDelegate alloc] init];
  service->SetTaskInterventionDelegate(task_id, delegate);

  // Transition task to kReflecting by executing an empty action sequence.
  PerformActions(service, task_id);

  ActorTask* task = GetTask(service, task_id);
  ASSERT_NE(nullptr, task);
  EXPECT_EQ(task->GetState(), ActorTaskState::kReflecting);

  // Interrupt the task for user confirmation.
  service->InterruptTask(task_id,
                         ActorTaskInterruptReason::kWaitingUserConfirmation,
                         "Please confirm");
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return delegate.requestConfirmationCalled; }));

  EXPECT_EQ(task->GetState(), ActorTaskState::kWaitingOnUser);
  EXPECT_TRUE(delegate.requestConfirmationCalled);
  EXPECT_NSEQ(@"Input needed", delegate.confirmationTitle);
  EXPECT_NSEQ(@"Please confirm", delegate.confirmationSubtitle);
  EXPECT_NSEQ(@"Continue", delegate.confirmationButtonText);
  ASSERT_TRUE(delegate.hasPendingUserIntervention);

  // Resolving the confirmation resumes the task to kReflecting once the posted
  // completion runs.
  [delegate runUserInterventionCompletion];
  EXPECT_EQ(task->GetState(), ActorTaskState::kWaitingOnUser);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return task->GetState() == ActorTaskState::kReflecting; }));
}

// Test that a task can be interrupted for user confirmation before it performs
// any action, i.e. that it yields on start, and that resolving the
// confirmation resumes it so it can act.
TEST_F(ActorServiceTest, InterruptTaskBeforeFirstAct) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  ASSERT_TRUE(HasTask(service, task_id));

  FakeActorTaskInterventionDelegate* delegate =
      [[FakeActorTaskInterventionDelegate alloc] init];
  service->SetTaskInterventionDelegate(task_id, delegate);

  ActorTask* task = GetTask(service, task_id);
  ASSERT_NE(nullptr, task);
  // No actions were performed, so the task is still in its initial state.
  ASSERT_EQ(task->GetState(), ActorTaskState::kInit);

  service->InterruptTask(task_id,
                         ActorTaskInterruptReason::kWaitingUserConfirmation,
                         "Please confirm before starting");
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return delegate.requestConfirmationCalled; }));

  EXPECT_EQ(task->GetState(), ActorTaskState::kWaitingOnUser);
  EXPECT_TRUE(delegate.requestConfirmationCalled);
  EXPECT_NSEQ(@"Input needed", delegate.confirmationTitle);
  EXPECT_NSEQ(@"Please confirm before starting", delegate.confirmationSubtitle);
  EXPECT_NSEQ(@"Continue", delegate.confirmationButtonText);
  ASSERT_TRUE(delegate.hasPendingUserIntervention);

  // Resolving the confirmation resumes the task to kReflecting once the posted
  // completion runs, from which it can perform its first actions.
  [delegate runUserInterventionCompletion];
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return task->GetState() == ActorTaskState::kReflecting; }));

  PerformActions(service, task_id);
  EXPECT_EQ(task->GetState(), ActorTaskState::kReflecting);
}

// Tests that an intervention delegate set from the lifecycle start
// notification receives an interrupt issued in the same turn as `CreateTask()`.
TEST_F(ActorServiceTest,
       InterventionDelegateSetFromStartReachesSameTurnInterrupt) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  FakeActorTaskLifecycleObserver* observer =
      [[FakeActorTaskLifecycleObserver alloc] init];
  service->AddTaskLifecycleObserver(observer);
  FlushCurrentSequence();

  FakeActorTaskInterventionDelegate* delegate =
      [[FakeActorTaskInterventionDelegate alloc] init];
  observer.onStart = ^(ActorTaskId started_task_id) {
    service->SetTaskInterventionDelegate(started_task_id, delegate);
  };

  ActorTaskId task_id = CreateTask(service);
  service->InterruptTask(task_id,
                         ActorTaskInterruptReason::kWaitingUserConfirmation,
                         "Please confirm");
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return delegate.requestConfirmationCalled; }));

  EXPECT_TRUE(HasTask(service, task_id));
  EXPECT_EQ(0, observer.stoppedCount);

  service->RemoveTaskLifecycleObserver(observer);
}

// Test that SetTaskInterventionDelegate and InterruptTask safely handle unknown
// task IDs without crashing.
TEST_F(ActorServiceTest, InterventionAndInterruptSafelyHandleUnknownTaskId) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId unknown_task_id = ActorTaskId(9999);
  FakeActorTaskInterventionDelegate* delegate =
      [[FakeActorTaskInterventionDelegate alloc] init];

  service->SetTaskInterventionDelegate(unknown_task_id, delegate);
  service->InterruptTask(unknown_task_id,
                         ActorTaskInterruptReason::kWaitingUserConfirmation,
                         "Unknown");
  FlushCurrentSequence();

  EXPECT_FALSE(delegate.requestConfirmationCalled);
}

// Test that a task stopped by a failed interrupt is removed from the active
// tasks once the posted interrupt runs, notifies lifecycle observers, and is
// destroyed asynchronously.
TEST_F(ActorServiceTest, InterruptTaskRemovesStoppedTaskOnFailure) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  ASSERT_TRUE(HasTask(service, task_id));

  FakeActorTaskLifecycleObserver* observer =
      [[FakeActorTaskLifecycleObserver alloc] init];
  service->AddTaskLifecycleObserver(observer);

  // Interrupting for confirmation without setting an intervention delegate
  // stops the task.
  service->InterruptTask(task_id,
                         ActorTaskInterruptReason::kWaitingUserConfirmation,
                         "Please confirm");
  EXPECT_NE(nullptr, GetTask(service, task_id));
  // Run the interrupt command.
  RunAlreadyPostedTasks();

  EXPECT_EQ(std::nullopt, service->GetActiveTaskState());
  EXPECT_EQ(nullptr, GetTask(service, task_id));
  EXPECT_TRUE(HasTask(service, task_id));
  EXPECT_EQ(0, observer.stoppedCount);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer.stoppedCount == 1 && !HasTask(service, task_id);
  }));
  EXPECT_EQ(task_id, observer.lastTaskID);

  service->RemoveTaskLifecycleObserver(observer);
}

// Test that calling StopTask from within the intervention delegate callback
// does not cause iterator invalidation or Use-After-Free.
TEST_F(ActorServiceTest, InterruptTaskHandlesReentrantStopTask) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  ASSERT_TRUE(HasTask(service, task_id));

  FakeActorTaskInterventionDelegate* delegate =
      [[FakeActorTaskInterventionDelegate alloc] init];
  delegate.onUserInterventionRequested = ^{
    service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  };
  service->SetTaskInterventionDelegate(task_id, delegate);

  service->InterruptTask(task_id,
                         ActorTaskInterruptReason::kWaitingUserConfirmation,
                         "Please confirm");
  EXPECT_FALSE(delegate.requestConfirmationCalled);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return delegate.requestConfirmationCalled; }));

  EXPECT_TRUE(delegate.requestConfirmationCalled);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !HasTask(service, task_id); }));
}

// Test that calling StopTask from a state change observer notification while an
// action is in flight replies `kTaskWentAway` and destroys the task from a
// later posted task.
TEST_F(ActorServiceTest, StopTaskWhileActingRepliesTaskWentAway) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  FakeActorServiceTaskUpdatesObserver* observer =
      [[FakeActorServiceTaskUpdatesObserver alloc] init];
  observer.onStateChange = ^(ActorTaskState new_state) {
    service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  };
  service->AddTaskUpdatesObserver(observer);

  base::test::TestFuture<PerformActionsResult> future;
  service->PerformActions(task_id, {MakeSuccessfulActorAction()}, "Update",
                          future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  const PerformActionsResult& result = future.Get();
  ASSERT_EQ(1u, result.action_results.size());
  EXPECT_EQ(mojom::ActionResultCode::kTaskWentAway,
            result.action_results[0].tool_result.code());
  EXPECT_EQ(std::nullopt, service->GetActiveTaskState());
  EXPECT_TRUE(HasTask(service, task_id));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer.stoppedCount == 1 && !HasTask(service, task_id);
  }));

  service->RemoveTaskUpdatesObserver(observer);
}

// Test that a stopped task that has not been destroyed yet rejects new actions
// with a single `kTaskWentAway` result.
TEST_F(ActorServiceTest, PerformActionsOnStoppedTaskReturnsTaskWentAway) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  RunAlreadyPostedTasks();
  ASSERT_EQ(nullptr, GetTask(service, task_id));
  ASSERT_TRUE(HasTask(service, task_id));

  PerformActionsResult result = PerformActions(service, task_id);
  ASSERT_EQ(1u, result.action_results.size());
  EXPECT_EQ(mojom::ActionResultCode::kTaskWentAway,
            result.action_results[0].tool_result.code());
  EXPECT_TRUE(result.page_contexts.empty());
}

// Test that performing actions on an unknown task replies with a single
// `kTaskWentAway` result.
TEST_F(ActorServiceTest, PerformActionsOnUnknownTaskReturnsTaskWentAway) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  PerformActionsResult result = PerformActions(service, ActorTaskId(999));
  ASSERT_EQ(1u, result.action_results.size());
  EXPECT_EQ(mojom::ActionResultCode::kTaskWentAway,
            result.action_results[0].tool_result.code());
  EXPECT_TRUE(result.page_contexts.empty());
}

// Test that StopTask on a task that is already stopped is a no-op.
TEST_F(ActorServiceTest, StopTaskOnStoppedTaskIsNoOp) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId first_task_id = CreateTask(service, "First Task");
  ActorTaskId second_task_id = CreateTask(service, "Second Task");
  FakeActorServiceTaskUpdatesObserver* observer =
      [[FakeActorServiceTaskUpdatesObserver alloc] init];
  observer.onStateChange = ^(ActorTaskState new_state) {
    service->StopTask(second_task_id, ActorTaskStoppedReason::kStoppedByUser);
  };
  service->AddTaskUpdatesObserver(observer);

  service->StopAllTasks();
  RunAlreadyPostedTasks();
  EXPECT_EQ(std::nullopt, service->GetActiveTaskState());

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return observer.stoppedCount == 2 && !HasTask(service, first_task_id) &&
           !HasTask(service, second_task_id);
  }));

  service->RemoveTaskUpdatesObserver(observer);
}

// Test that shutting down the service while a stopped task awaits deletion is
// safe.
TEST_F(ActorServiceTest, ShutdownWithPendingDelete) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  RunAlreadyPostedTasks();
  ASSERT_TRUE(HasTask(service, task_id));

  service->Shutdown();
  EXPECT_FALSE(HasTask(service, task_id));

  FlushCurrentSequence();
  EXPECT_FALSE(HasTask(service, task_id));
}

// Test that a task stopped while the page contexts that follow its actions are
// extracted replies with a single `kTaskWentAway` result and no page contexts.
TEST_F(ActorServiceTest, ActObservationsAfterStopReplyTaskWentAway) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !GetTask(service, task_id); }));

  base::test::TestFuture<PerformActionsResult> future;
  CompleteActObservations(service, task_id, future.GetCallback());

  const PerformActionsResult& result = future.Get();
  ASSERT_EQ(1u, result.action_results.size());
  EXPECT_EQ(mojom::ActionResultCode::kTaskWentAway,
            result.action_results[0].tool_result.code());
  EXPECT_TRUE(result.page_contexts.empty());
}

// Test that a task stopped after its actions completed, but before their
// completion is delivered, replies with a single `kTaskWentAway` result instead
// of the actions' results.
TEST_F(ActorServiceTest, ActCompletionAfterStopRepliesTaskWentAway) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);
  RecordingJournalObserver journal_observer(*GetJournal(service));

  ActorTaskId task_id = CreateTask(service);
  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !GetTask(service, task_id); }));

  base::test::TestFuture<PerformActionsResult> future;
  CompleteAct(service, task_id, future.GetCallback());

  const PerformActionsResult& result = future.Get();
  ASSERT_EQ(1u, result.action_results.size());
  EXPECT_EQ(mojom::ActionResultCode::kTaskWentAway,
            result.action_results[0].tool_result.code());
  EXPECT_TRUE(result.page_contexts.empty());

  std::vector<const mojom::JournalEntry*> finished_entries =
      journal_observer.EntriesNamed("PerformActionsFinished");
  ASSERT_EQ(1u, finished_entries.size());
  EXPECT_EQ(base::ToString(mojom::ActionResultCode::kTaskWentAway),
            FindJournalDetail(*finished_entries[0], "result_code"));
}

// Test that the task update notifications of a stop still pending when the
// service shuts down are not delivered.
TEST_F(ActorServiceTest, ShutdownDropsPendingTaskUpdateNotifications) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  FakeActorServiceTaskUpdatesObserver* observer =
      [[FakeActorServiceTaskUpdatesObserver alloc] init];
  service->AddTaskUpdatesObserver(observer);
  FlushCurrentSequence();
  ASSERT_EQ(1, observer.registeredCount);

  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  RunAlreadyPostedTasks();
  service->Shutdown();
  FlushCurrentSequence();

  EXPECT_EQ(0, observer.stateChangeCount);
  EXPECT_EQ(0, observer.stoppedCount);
}

// Test that observers removed after a task stopped, but before its stop was
// delivered, are not notified of the stop.
TEST_F(ActorServiceTest, ObserversRemovedBeforeStopDeliveryAreNotNotified) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  FakeActorServiceTaskUpdatesObserver* scoped_observer =
      [[FakeActorServiceTaskUpdatesObserver alloc] init];
  FakeActorServiceTaskUpdatesObserver* global_observer =
      [[FakeActorServiceTaskUpdatesObserver alloc] init];
  ASSERT_TRUE(service->AddTaskUpdatesObserver(task_id, scoped_observer));
  service->AddTaskUpdatesObserver(global_observer);
  FlushCurrentSequence();

  service->StopTask(task_id, ActorTaskStoppedReason::kStoppedByUser);
  RunAlreadyPostedTasks();
  ASSERT_EQ(nullptr, GetTask(service, task_id));
  ASSERT_TRUE(HasTask(service, task_id));

  service->RemoveTaskUpdatesObserver(task_id, scoped_observer);
  service->RemoveTaskUpdatesObserver(global_observer);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !HasTask(service, task_id); }));

  EXPECT_EQ(0, scoped_observer.stoppedCount);
  EXPECT_EQ(0, global_observer.stoppedCount);
}

// Test that `CreateTask()` returns a null ID and creates no task when web
// actuation is disabled by enterprise policy.
TEST_F(ActorServiceTest, Create_BlockedByEnterprisePolicy) {
  profile_->GetPrefs()->SetInteger(prefs::kGeminiActOnWebSettings,
                                   kPolicyDisabled);

  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  EXPECT_TRUE(task_id.is_null());
  EXPECT_EQ(std::nullopt, service->GetActiveTaskState());
}

// Test that disabling web actuation via enterprise policy stops all active
// tasks.
TEST_F(ActorServiceTest, CanActOnWebChanged_StopsActiveTasksWhenDisabled) {
  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id = CreateTask(service);
  ASSERT_FALSE(task_id.is_null());
  ASSERT_TRUE(HasTask(service, task_id));

  profile_->GetPrefs()->SetInteger(prefs::kGeminiActOnWebSettings,
                                   kPolicyDisabled);

  EXPECT_TRUE(
      base::test::RunUntil([&]() { return !HasTask(service, task_id); }));
}

// Test that `PerformActions()` rejects with `kActionsBlockedByEnterprisePolicy`
// when web actuation is disabled by enterprise policy.
TEST_F(ActorServiceTest, PerformActions_RejectedWhenBlockedByEnterprisePolicy) {
  profile_->GetPrefs()->SetInteger(prefs::kGeminiActOnWebSettings,
                                   kPolicyDisabled);

  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  const ActorTaskId task_id(1);
  SwapTask(
      service, task_id,
      std::make_unique<ActorTask>(
          task_id, "Test Task", TestSource(),
          /*allow_incognito_web_states=*/false, GetJournal(service),
          GetToolFactory(service),
          BrowserListFactory::GetForProfile(profile_.get()),
          /*policy_checker=*/nullptr, MakeOnStoppedCallback(service, task_id)));

  PerformActionsResult result = PerformActions(service, task_id);
  ASSERT_EQ(1u, result.action_results.size());
  EXPECT_EQ(mojom::ActionResultCode::kActionsBlockedByEnterprisePolicy,
            result.action_results[0].tool_result.code());
}

}  // namespace actor
