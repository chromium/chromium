// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/attempt_login_tool.h"

#import <memory>
#import <optional>
#import <utility>
#import <vector>

#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/types/expected.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/password_manager/core/browser/actor_login/actor_login_service.h"
#import "components/password_manager/core/browser/actor_login/actor_login_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/fake_tool_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/tool_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/profile_context_resolver.h"
#import "ios/chrome/browser/passwords/model/actor_login/ios_chrome_actor_login_delegate_client.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

namespace {

// A fake implementation of actor_login::ActorLoginService used to mock
// credentials retrieval and login attempts in unit tests.
class FakeActorLoginService : public actor_login::ActorLoginService {
 public:
  FakeActorLoginService() = default;
  ~FakeActorLoginService() override = default;

  void GetCredentials(
      actor_login::ActorLoginDelegateClient* client,
      bool has_sign_in_with_google_button,
      base::WeakPtr<actor_login::ActorLoginQualityLoggerInterface> mqls_logger,
      actor_login::CredentialsOrErrorReply callback) override {
    if (get_credentials_error_.has_value()) {
      std::move(callback).Run(base::unexpected(*get_credentials_error_));
    } else {
      std::move(callback).Run(credentials_);
    }
  }

  void AttemptLogin(
      actor_login::ActorLoginDelegateClient* client,
      const actor_login::Credential& credential,
      bool should_store_permission,
      base::WeakPtr<actor_login::ActorLoginQualityLoggerInterface> mqls_logger,
      base::TimeTicks attempt_login_tool_start_time,
      actor_login::FrameFillingStartedCallback frame_filling_started_cb,
      actor_login::LoginStatusResultOrErrorReply done_callback,
      base::WeakPtr<actor_login::ActionSequenceDelegate>
          action_sequence_delegate) override {
    attempt_login_called_ = true;
    attempted_credentials_.push_back(credential);
    attempted_should_store_permission_ = should_store_permission;
    if (attempt_login_error_.has_value()) {
      std::move(done_callback).Run(base::unexpected(*attempt_login_error_));
    } else {
      if (attempted_should_require_reauth_) {
        attempted_should_require_reauth_ = false;
        std::move(done_callback)
            .Run(actor_login::LoginStatusResult::kErrorDeviceReauthRequired);
      } else {
        std::move(done_callback)
            .Run(actor_login::LoginStatusResult::
                     kSuccessUsernameAndPasswordFilled);
      }
    }
  }

  // The mock list of credentials returned by GetCredentials.
  std::vector<actor_login::Credential> credentials_;

  // The mock error returned by GetCredentials, if set.
  std::optional<actor_login::ActorLoginError> get_credentials_error_;

  // Tracks whether `AttemptLogin` has been called during execution.
  bool attempt_login_called_ = false;

  // Stores credentials passed to AttemptLogin in chronological order.
  std::vector<actor_login::Credential> attempted_credentials_;

  // Stores whether the last AttemptLogin call requested storing the permission.
  bool attempted_should_store_permission_;

  // Overrides the need for device reauthentication. If a test wishes to verify
  // correct behavior handling `LoginStatusResult::kErrorDeviceReauthRequired`,
  // set this to `true`.
  bool attempted_should_require_reauth_ = false;

  // The mock error returned by `AttemptLogin`, if set.
  std::optional<actor_login::ActorLoginError> attempt_login_error_;
};

}  // namespace

class AttemptLoginToolTest : public PlatformTest {
 public:
  AttemptLoginToolTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());
    // Create a navigation manager.
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->SetBrowserState(profile_.get());

    delegate_.set_actor_login_service(&fake_actor_login_service_);
  }

  base::expected<std::unique_ptr<AttemptLoginTool>, ToolExecutionResult>
  CreateTool(const optimization_guide::proto::AttemptLoginAction& action) {
    return AttemptLoginTool::Create(action, &delegate_,
                                    ProfileContextResolver(profile_.get()));
  }

  // Retrieves the web state list for the current test browser.
  WebStateList* GetWebStateList() { return browser_->GetWebStateList(); }

  // Helper method to add a new tab the tool will actuate upon.
  web::FakeWebState* CreateAndInsertWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->SetBrowserState(profile_.get());

    // Create navigation item and hold it to avoid dangling pointer.
    navigation_item_ = web::NavigationItem::Create();
    navigation_manager->SetVisibleItem(navigation_item_.get());

    web_state->SetNavigationManager(std::move(navigation_manager));
    auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_state->SetWebFramesManager(web::ContentWorld::kPageContentWorld,
                                   std::move(web_frames_manager));
    IOSChromeActorLoginDelegateClient::CreateForWebState(web_state.get());
    web::FakeWebState* web_state_ptr = web_state.get();
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::AtIndex(0).Activate());
    return web_state_ptr;
  }

  FakeToolDelegate& delegate() { return delegate_; }

  FakeActorLoginService& fake_actor_login_service() {
    return fake_actor_login_service_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  FakeActorLoginService fake_actor_login_service_;
  FakeToolDelegate delegate_;
  std::unique_ptr<web::NavigationItem> navigation_item_;
};

// Tests that creating the tool fails if the Action payload is missing the tab
// ID field.
TEST_F(AttemptLoginToolTest, Create_MissingTabId) {
  optimization_guide::proto::AttemptLoginAction action;
  base::expected<std::unique_ptr<AttemptLoginTool>, ToolExecutionResult>
      result = CreateTool(action);

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kArgumentsInvalid);
  EXPECT_EQ(result.error().internal_code(),
            InternalToolErrorCode::kCreationMissingRequiredFields);
}

// Tests that creating the tool fails if the requested tab ID does not
// correspond to any active WebState.
TEST_F(AttemptLoginToolTest, Create_NoWebStateForTabId) {
  optimization_guide::proto::AttemptLoginAction action;
  action.set_tab_id(1);

  base::expected<std::unique_ptr<AttemptLoginTool>, ToolExecutionResult>
      result = CreateTool(action);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kTabWentAway);
  EXPECT_FALSE(result.error().internal_code().has_value());
}

// Tests that creating the tool succeeds when given a valid tab ID corresponding
// to an active WebState.
TEST_F(AttemptLoginToolTest, Create_Success) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state = CreateAndInsertWebState();
  action.set_tab_id(web_state->GetUniqueIdentifier().identifier());

  base::expected<std::unique_ptr<AttemptLoginTool>, ToolExecutionResult>
      result = CreateTool(action);

  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value()->GetToolType(), ToolType::kAttemptLogin);
  EXPECT_EQ(result.value()->GetTargetWebState().get(),
            GetWebStateList()->GetWebStateAt(0));
}

// Tests that execution fails and returns kTabWentAway if the target WebState is
// destroyed before execution starts.
TEST_F(AttemptLoginToolTest, Execute_NoWebState) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state = CreateAndInsertWebState();
  action.set_tab_id(web_state->GetUniqueIdentifier().identifier());

  auto result = CreateTool(action);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  // Destroy WebState.
  CloseAllWebStates(*GetWebStateList(), WebStateList::ClosingReason::kDefault);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());
  EXPECT_EQ(future.Get().code(), mojom::ActionResultCode::kTabWentAway);
}

// Tests that execution propagates appropriate ActionResults when the login
// service fails to fetch matching credentials (e.g. kLoginTooManyRequests).
TEST_F(AttemptLoginToolTest, Execute_GetCredentialsError) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state = CreateAndInsertWebState();
  action.set_tab_id(web_state->GetUniqueIdentifier().identifier());

  auto result = CreateTool(action);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  fake_actor_login_service().get_credentials_error_ =
      actor_login::ActorLoginError::kServiceBusy;

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());
  EXPECT_EQ(future.Get().code(),
            mojom::ActionResultCode::kLoginTooManyRequests);
}

// Tests that execution returns kLoginNoCredentialsAvailable if there are no
// stored credentials for the page origin in password manager.
TEST_F(AttemptLoginToolTest, Execute_GetCredentialsEmpty) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state = CreateAndInsertWebState();
  action.set_tab_id(web_state->GetUniqueIdentifier().identifier());

  auto result = CreateTool(action);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  fake_actor_login_service().credentials_ = {};

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());
  EXPECT_EQ(future.Get().code(),
            mojom::ActionResultCode::kLoginNoCredentialsAvailable);
}

// Tests that execution returns kLoginNoCredentialsAvailable if the user
// declines to select a credential.
TEST_F(AttemptLoginToolTest, Execute_UserDeclinesCredential) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state_ptr = CreateAndInsertWebState();
  action.set_tab_id(web_state_ptr->GetUniqueIdentifier().identifier());

  auto result = CreateTool(action);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(123);
  cred.has_persistent_permission = false;
  fake_actor_login_service().credentials_ = {cred};

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  // Trigger the user selecting the credential.
  ASSERT_TRUE(delegate().prompt_to_select_called());
  delegate().RunPromptCallback(/*selected_credential=*/{},
                               /*should_store_permission=*/false);

  EXPECT_EQ(future.Get().code(),
            mojom::ActionResultCode::kLoginNoCredentialsAvailable);
}

// Tests that execution automatically selects and logs in using a credential
// that has persistent permission, without prompting the user.
TEST_F(AttemptLoginToolTest, Execute_PersistentCredentialDirectSelect_Success) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state = CreateAndInsertWebState();
  action.set_tab_id(web_state->GetUniqueIdentifier().identifier());

  auto result = CreateTool(action);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(123);
  cred.has_persistent_permission = true;

  fake_actor_login_service().credentials_ = {cred};

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());
  EXPECT_EQ(future.Get().code(), mojom::ActionResultCode::kOk);
  EXPECT_TRUE(fake_actor_login_service().attempt_login_called_);
  EXPECT_EQ(fake_actor_login_service().attempted_credentials_.front().id,
            cred.id);
  EXPECT_TRUE(fake_actor_login_service().attempted_should_store_permission_);
}

// Tests that when login requires device re-authentication, the tool waits for
// the WebState to become active (visible) before retrying login, and succeeds
// when the WebState is shown.
TEST_F(AttemptLoginToolTest, Execute_DeviceReauthRequired_Shown_Retry_Success) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state_ptr = CreateAndInsertWebState();
  action.set_tab_id(web_state_ptr->GetUniqueIdentifier().identifier());

  auto result = CreateTool(action);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(123);
  cred.has_persistent_permission = false;

  fake_actor_login_service().credentials_ = {cred};
  fake_actor_login_service().attempted_should_require_reauth_ = true;

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  // Execute shouldn't have completed yet because we are waiting on focus.
  EXPECT_FALSE(future.IsReady());

  // Trigger the user selecting the credential.
  ASSERT_TRUE(delegate().prompt_to_select_called());
  delegate().RunPromptCallback(cred, /*should_store_permission=*/false);

  // Still not completed because the first attempt returned
  // kErrorDeviceReauthRequired and we are waiting on focus.
  EXPECT_FALSE(future.IsReady());

  // Simulate tab focus/visible again.
  web_state_ptr->WasShown();

  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(future.Get().code(), mojom::ActionResultCode::kOk);
  EXPECT_EQ(fake_actor_login_service().attempted_credentials_.size(), 2u);
}

// Tests that if the WebState is destroyed while waiting for device
// re-authentication, the tool correctly exits with a kTabWentAway code.
TEST_F(AttemptLoginToolTest, Execute_DeviceReauthRequired_WebStateDestroyed) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state = CreateAndInsertWebState();
  action.set_tab_id(web_state->GetUniqueIdentifier().identifier());

  auto result = CreateTool(action);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(123);
  cred.has_persistent_permission = false;

  fake_actor_login_service().credentials_ = {cred};
  fake_actor_login_service().attempted_should_require_reauth_ = true;

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  // Trigger the user selecting the credential.
  ASSERT_TRUE(delegate().prompt_to_select_called());
  delegate().RunPromptCallback(cred, /*should_store_permission=*/false);

  EXPECT_FALSE(future.IsReady());

  // Simulate web state destroyed.
  CloseAllWebStates(*GetWebStateList(), WebStateList::ClosingReason::kDefault);
  EXPECT_EQ(future.Get().code(), mojom::ActionResultCode::kTabWentAway);
}

// Tests that cancelling the tool while it is waiting for device
// re-authentication correctly tears down observations and stops future login
// attempts.
TEST_F(AttemptLoginToolTest, Execute_DeviceReauthRequired_Cancel) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state_ptr = CreateAndInsertWebState();
  action.set_tab_id(web_state_ptr->GetUniqueIdentifier().identifier());

  auto result = CreateTool(action);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(123);
  cred.has_persistent_permission = false;

  fake_actor_login_service().credentials_ = {cred};
  fake_actor_login_service().attempted_should_require_reauth_ = true;

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  // Trigger the user selecting the credential.
  ASSERT_TRUE(delegate().prompt_to_select_called());
  delegate().RunPromptCallback(cred, /*should_store_permission=*/false);

  EXPECT_FALSE(future.IsReady());

  // Cancel execution, and simulate re-focusing.
  tool->Cancel();
  web_state_ptr->WasShown();

  // Since we cancelled, AttemptLogin should not have been called a second time.
  EXPECT_EQ(fake_actor_login_service().attempted_credentials_.size(), 1u);
}

// Tests that if the page changes (e.g. user navigates away) while the
// credential selection prompt is showing, the tool completes with
// kLoginPageChangedDuringSelection.
TEST_F(AttemptLoginToolTest, Execute_PageChangedDuringSelection) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state_ptr = CreateAndInsertWebState();
  action.set_tab_id(web_state_ptr->GetUniqueIdentifier().identifier());

  auto result = CreateTool(action);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(123);
  cred.has_persistent_permission = false;

  fake_actor_login_service().credentials_ = {cred};

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  // Simulate a page change by updating the visible navigation item on the
  // manager.
  auto new_navigation_item = web::NavigationItem::Create();
  new_navigation_item->SetURL(GURL("https://different-page.com"));
  auto* fake_nav_manager = static_cast<web::FakeNavigationManager*>(
      web_state_ptr->GetNavigationManager());
  fake_nav_manager->SetVisibleItem(new_navigation_item.get());

  // Trigger the user selecting the credential.
  ASSERT_TRUE(delegate().prompt_to_select_called());
  delegate().RunPromptCallback(cred, /*should_store_permission=*/false);
  EXPECT_EQ(future.Get().code(),
            mojom::ActionResultCode::kLoginPageChangedDuringSelection);
}

}  // namespace actor
