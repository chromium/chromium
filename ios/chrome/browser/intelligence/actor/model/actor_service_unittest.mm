// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"

#import <set>

#import "base/functional/bind.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/gtest_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
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

 private:
  base::WeakPtr<web::WebState> web_state_;
};

class ActorServiceTest : public PlatformTest {
 public:
  ActorServiceTest() {
    ActorServiceFactory::GetInstance();
    profile_ = TestProfileIOS::Builder().Build();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
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

// Tests that requesting tab observation with a valid WebState triggers the
// callback.
TEST_F(ActorServiceTest, RequestTabObservationWithValidWebState) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

  ActorService* service = ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_NE(nullptr, service);

  ActorTaskId task_id =
      service->CreateTask("Test Task",
                          /*allow_incognito_web_states=*/false);

  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetWebFramesManager(
      web::ContentWorld::kPageContentWorld,
      std::make_unique<web::FakeWebFramesManager>());
  fake_web_state->SetWebFramesManager(
      web::ContentWorld::kIsolatedWorld,
      std::make_unique<web::FakeWebFramesManager>());

  base::RunLoop run_loop;
  bool callback_called = false;
  service->RequestTabObservation(
      task_id, fake_web_state.get(),
      base::BindOnce(
          [](bool* called, base::OnceClosure quit_closure,
             PageContextWrapperCallbackResponse response) {
            *called = true;
            std::move(quit_closure).Run();
          },
          &callback_called, run_loop.QuitClosure()));

  run_loop.Run();

  EXPECT_TRUE(callback_called);
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

}  // namespace actor
