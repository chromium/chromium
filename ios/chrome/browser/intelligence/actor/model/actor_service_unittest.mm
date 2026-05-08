// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"

#import <UIKit/UIKit.h>

#import <set>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/gtest_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/values_test_util.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
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
#import "testing/platform_test.h"

namespace actor {

class TestTool : public ActorTool {
 public:
  TestTool(base::WeakPtr<web::WebState> web_state) : web_state_(web_state) {}
  ~TestTool() override = default;

  void Execute(ToolExecutionCallback callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), ToolExecutionResult::Ok()));
  }

  base::WeakPtr<web::WebState> GetTargetWebState() const override {
    return web_state_;
  }

  optimization_guide::proto::Action::ActionCase GetActionCase() const override {
    return optimization_guide::proto::Action::ACTION_NOT_SET;
  }

 private:
  base::WeakPtr<web::WebState> web_state_;
};

class ActorServiceTest : public PlatformTest {
 public:
  ActorServiceTest() : web_client_(std::make_unique<web::FakeWebClient>()) {
    ActorServiceFactory::GetInstance();
    profile_ = TestProfileIOS::Builder().Build();
  }

  void SetUp() override {
    PlatformTest::SetUp();

    static_cast<web::FakeWebClient*>(web_client_.Get())
        ->SetJavaScriptFeatures({
            PageContextExtractorJavaScriptFeature::GetInstance(),
        });
  }

 protected:
  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that `ActorService` is successfully created when the `kActorTools`
// feature is enabled.
TEST_F(ActorServiceTest, ServiceCreationWithFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  EXPECT_NE(nullptr, service);
}

// Tests that `ActorService` is not created when the `kActorTools` feature is
// disabled.
TEST_F(ActorServiceTest, ServiceCreationWithFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kActorTools);

  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  EXPECT_EQ(nullptr, service);
}

// Tests that `CreateTask` generates unique IDs for sequential tasks.
TEST_F(ActorServiceTest, CreateTaskGeneratesUniqueIds) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id =
      service->CreateTask("Test Task", /*allow_incognito_web_states=*/false);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  auto test_browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list->AddBrowser(test_browser.get());

  auto fake_web_state = std::make_unique<web::FakeWebState>();
  web::WebStateID web_state_id = fake_web_state->GetUniqueIdentifier();
  auto* fake_web_state_ptr = fake_web_state.get();

  test_browser->GetWebStateList()->InsertWebState(std::move(fake_web_state));

  // Make the tab controlled by the task by performing an action targeting it.
  std::vector<std::unique_ptr<ActorTool>> actions;
  actions.push_back(
      std::make_unique<TestTool>(fake_web_state_ptr->GetWeakPtr()));

  service->PerformActions(task_id, std::move(actions), "Update",
                          base::BindOnce(^(PerformActionsResult result){
                              // Do nothing.
                          }));

  web::WebState* resolved_web_state =
      service->GetWebStateForID(web_state_id, task_id);
  EXPECT_NE(nullptr, resolved_web_state);
  EXPECT_EQ(web_state_id, resolved_web_state->GetUniqueIdentifier());

  browser_list->RemoveBrowser(test_browser.get());
}

// Tests that GetWebStateForID does not find a tab in an incognito browser if
// the task does not allow incognito.
TEST_F(ActorServiceTest, GetWebStateForID_Incognito_NotAllowed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  EXPECT_DEATH_IF_SUPPORTED(service->CreateTask("Test Task", true), "");
}

// Tests that GetWebStateForID returns nullptr when the task is not found.
TEST_F(ActorServiceTest, GetWebStateForID_TaskNotFound) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

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

// Tests that PerformActions completes immediately when the WebState is not
// loading.
TEST_F(ActorServiceTest, PerformActions_NoLoading_InstantCompletion) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

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

  std::vector<std::unique_ptr<ActorTool>> actions;
  actions.push_back(
      std::make_unique<TestTool>(fake_web_state_ptr->GetWeakPtr()));

  bool callback_called = false;
  service->PerformActions(
      task_id, std::move(actions), "Update",
      base::BindOnce(
          [](bool* called, PerformActionsResult result) { *called = true; },
          base::Unretained(&callback_called)));

  // Run the queued tasks (tool execution and completion) deterministically.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(callback_called);

  browser_list->RemoveBrowser(test_browser.get());
}

// Tests that PerformActions is deferred when the WebState is loading, and only
// resolves when loading completes.
TEST_F(ActorServiceTest, PerformActions_Loading_DeferredUntilStopLoading) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

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

  // Set the WebState to a loading state.
  fake_web_state_ptr->SetLoading(true);

  std::vector<std::unique_ptr<ActorTool>> actions;
  actions.push_back(
      std::make_unique<TestTool>(fake_web_state_ptr->GetWeakPtr()));

  bool callback_called = false;
  service->PerformActions(
      task_id, std::move(actions), "Update",
      base::BindOnce(
          [](bool* called, PerformActionsResult result) { *called = true; },
          base::Unretained(&callback_called)));

  // Run the queued execution tasks.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // Gating: The callback should not be executed yet because the page is
  // loading.
  EXPECT_FALSE(callback_called);

  // Stop the load.
  fake_web_state_ptr->SetLoading(false);

  // Now the callback should execute successfully.
  EXPECT_TRUE(callback_called);

  browser_list->RemoveBrowser(test_browser.get());
}

// Tests that a loading WebState times out after 5 seconds, forcing the
// deferred PerformActions callback to run to prevent hanging.
TEST_F(ActorServiceTest, PerformActions_Loading_TimeoutResolvesCallback) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

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

  // Set the WebState to a loading state.
  fake_web_state_ptr->SetLoading(true);

  std::vector<std::unique_ptr<ActorTool>> actions;
  actions.push_back(
      std::make_unique<TestTool>(fake_web_state_ptr->GetWeakPtr()));

  bool callback_called = false;
  service->PerformActions(
      task_id, std::move(actions), "Update",
      base::BindOnce(
          [](bool* called, PerformActionsResult result) { *called = true; },
          base::Unretained(&callback_called)));

  // Run the queued execution tasks.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // Callback should be deferred.
  EXPECT_FALSE(callback_called);

  // Fast forward the environment by 7 seconds to trigger the load timeout.
  task_environment_.FastForwardBy(base::Seconds(7));

  // The callback must be resolved now due to the timeout.
  EXPECT_TRUE(callback_called);

  browser_list->RemoveBrowser(test_browser.get());
}

}  // namespace actor
