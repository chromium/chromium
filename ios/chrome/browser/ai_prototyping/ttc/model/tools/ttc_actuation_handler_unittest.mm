// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_handler.h"

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_handler+Testing.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_browser_agent.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

class TTCActuationHandlerTest : public PlatformTest {
 public:
  TTCActuationHandlerTest() {
    scoped_feature_list_.InitAndEnableFeature(kActorTools);
    profile_ = TestProfileIOS::Builder().Build();
    actor::ActorServiceFactory::GetForProfile(profile_.get());

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());
    ActorBrowserAgent::CreateForBrowser(browser_.get());

    browser_->GetWebStateList()->InsertWebState(
        CreateFakeWebState(123, profile_.get()),
        WebStateList::InsertionParams::AtIndex(0).Activate());
  }

 protected:
  std::unique_ptr<web::FakeWebState> CreateFakeWebState(
      int web_state_id,
      web::BrowserState* browser_state) {
    auto fake_web_state = std::make_unique<web::FakeWebState>(
        web::WebStateID::FromSerializedValue(web_state_id));
    fake_web_state->SetBrowserState(browser_state);
    return fake_web_state;
  }

  TTCActuationHandler* CreateHandler() {
    SessionID browserID =
        ActorBrowserAgent::FromBrowser(browser_.get())->browser_id();
    return
        [[TTCActuationHandler alloc] initWithProfile:profile_.get()
                                        webStateList:browser_->GetWebStateList()
                                           browserID:browserID];
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

// Tests initialization.
TEST_F(TTCActuationHandlerTest, TestInitialization) {
  TTCActuationHandler* handler = CreateHandler();
  EXPECT_NE(handler, nil);
  [handler disconnect];

  SessionID browserID =
      ActorBrowserAgent::FromBrowser(browser_.get())->browser_id();
  TTCActuationHandler* nilProfileHandler =
      [[TTCActuationHandler alloc] initWithProfile:nullptr
                                      webStateList:browser_->GetWebStateList()
                                         browserID:browserID];
  EXPECT_EQ(nilProfileHandler, nil);

  TTCActuationHandler* nilWebStateListHandler =
      [[TTCActuationHandler alloc] initWithProfile:profile_.get()
                                      webStateList:nullptr
                                         browserID:browserID];
  EXPECT_EQ(nilWebStateListHandler, nil);

  TTCActuationHandler* invalidBrowserIDHandler =
      [[TTCActuationHandler alloc] initWithProfile:profile_.get()
                                      webStateList:browser_->GetWebStateList()
                                         browserID:SessionID::InvalidValue()];
  EXPECT_EQ(invalidBrowserIDHandler, nil);
}

// Tests setting actor service for testing.
TEST_F(TTCActuationHandlerTest, TestSetActorServiceForTesting) {
  actor::ActorService* actor_service =
      actor::ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(actor_service);

  TTCActuationHandler* handler = CreateHandler();
  [handler setActorServiceForTesting:actor_service];
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];
  EXPECT_FALSE(task_id.is_null());
  [handler disconnect];
}

// Tests task creation with an active WebState.
TEST_F(TTCActuationHandlerTest, TestCreateTask) {
  TTCActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"TTC Task"];
  EXPECT_FALSE(task_id.is_null());
  [handler disconnect];
}

// Tests task creation when no active WebState exists.
TEST_F(TTCActuationHandlerTest, TestCreateTaskWithNoActiveWebState) {
  browser_->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::ClosingReason::kDefault);
  TTCActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"TTC Task"];
  EXPECT_TRUE(task_id.is_null());
  [handler disconnect];
}

// Tests that task creation returns a null ActorTaskId if the active WebState is
// off-the-record (incognito).
TEST_F(TTCActuationHandlerTest, TestCreateTaskWithIncognitoWebState) {
  browser_->GetWebStateList()->InsertWebState(
      CreateFakeWebState(456, profile_->GetOffTheRecordProfile()),
      WebStateList::InsertionParams::AtIndex(1).Activate());

  TTCActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Incognito Task"];
  EXPECT_TRUE(task_id.is_null());
  [handler disconnect];
}

// Tests that disconnecting cleans up state safely and stops active tasks.
TEST_F(TTCActuationHandlerTest, TestDisconnectCancelsActiveTasks) {
  actor::ActorService* actor_service =
      actor::ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(actor_service);

  TTCActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"TTC Task"];
  ASSERT_FALSE(task_id.is_null());
  EXPECT_TRUE(actor_service->GetActiveTaskState().has_value());

  [handler disconnect];

  // Verify task is stopped in ActorService.
  EXPECT_FALSE(actor_service->GetActiveTaskState().has_value());

  // Post-disconnect task creation fails since WebStateList is cleared.
  actor::ActorTaskId post_disconnect_id =
      [handler createTaskWithTitle:@"Post-disconnect Task"];
  EXPECT_TRUE(post_disconnect_id.is_null());
}

// Tests that multiple disconnect calls are idempotent and safe.
TEST_F(TTCActuationHandlerTest, TestMultipleDisconnectCallsAreIdempotent) {
  TTCActuationHandler* handler = CreateHandler();
  [handler disconnect];
  EXPECT_NO_FATAL_FAILURE([handler disconnect]);
}

}  // namespace
