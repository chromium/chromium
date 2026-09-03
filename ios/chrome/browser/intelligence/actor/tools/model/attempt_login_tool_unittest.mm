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
#import "base/task/sequenced_task_runner.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/types/expected.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/autofill/core/browser/actor/actor_form_filling_service.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/password_manager/core/browser/actor_login/actor_login_service.h"
#import "components/password_manager/core/browser/actor_login/actor_login_types.h"
#import "components/password_manager/ios/actor_login/actor_login_tool_delegate.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_intervention_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_task_form_filling_handler.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/fake_tool_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/tool_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/passwords/model/actor_login/ios_chrome_actor_login_delegate_client.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
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
#import "third_party/ocmock/OCMock/OCMock.h"

// A fake implementation of ActorTaskInterventionDelegate.
@interface FakeActorTaskInterventionDelegate
    : NSObject <ActorTaskInterventionDelegate>
@property(nonatomic, assign) BOOL selectFromSuggestionsCalled;
@property(nonatomic, strong) NSArray<ActorFormSuggestion*>* promptedSuggestions;
@end

@implementation FakeActorTaskInterventionDelegate {
  void (^_completionHandler)(ActorFormSuggestion*, BOOL);
}

- (void)actorTask:(actor::ActorTaskId)taskID
    selectFromSuggestions:(NSArray<ActorFormSuggestion*>*)suggestions
        completionHandler:
            (void (^)(ActorFormSuggestion* selectedSuggestion,
                      BOOL shouldStorePermission))completionHandler {
  _selectFromSuggestionsCalled = YES;
  _promptedSuggestions = suggestions;
  _completionHandler = [completionHandler copy];
}

- (void)runCompletionWithSuggestion:(ActorFormSuggestion*)selectedSuggestion
              shouldStorePermission:(BOOL)shouldStorePermission {
  if (_completionHandler) {
    void (^_completion)(ActorFormSuggestion*, BOOL) = _completionHandler;
    _completionHandler = nil;
    _completion(selectedSuggestion, shouldStorePermission);
  }
}
@end

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

    if (expected_attempt_login_results_.empty()) {
      ADD_FAILURE() << "AttemptLogin called more times than configured in "
                       "expected_attempt_login_results_.";
      std::move(done_callback)
          .Run(actor_login::LoginStatusResult::kErrorNoFillableFields);
      return;
    }

    actor_login::LoginStatusResult result =
        expected_attempt_login_results_.front();
    expected_attempt_login_results_.erase(
        expected_attempt_login_results_.begin());
    std::move(done_callback).Run(result);
  }

  // Configures the sequential results returned by `AttemptLogin`.
  void SetSequentialResults(
      std::vector<actor_login::LoginStatusResult> results) {
    expected_attempt_login_results_ = std::move(results);
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

  // Sequential results for `AttemptLogin`. Each `AttemptLogin` call pops the
  // front element. Calling `AttemptLogin` when empty triggers a test failure.
  std::vector<actor_login::LoginStatusResult> expected_attempt_login_results_;
};

}  // namespace

class AttemptLoginToolTest : public PlatformTest {
 public:
  AttemptLoginToolTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());
    // Create a navigation manager.
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->SetBrowserState(profile_.get());

    auto fake_service = std::make_unique<FakeActorLoginService>();
    fake_actor_login_service_ptr_ = fake_service.get();

    auto form_filling_handler = ActorTaskFormFillingHandler::CreateForTesting(
        ActorTaskId(1), std::move(fake_service), nullptr);
    intervention_delegate_ = [[FakeActorTaskInterventionDelegate alloc] init];
    form_filling_handler->SetInterventionDelegateForTesting(
        intervention_delegate_);
    form_filling_handler_ptr_ = form_filling_handler.get();

    delegate_.set_form_filling_handler(std::move(form_filling_handler));
  }

  base::expected<std::unique_ptr<AttemptLoginTool>, ToolExecutionResult>
  CreateToolAndValidate(
      const optimization_guide::proto::AttemptLoginAction& action,
      web::WebState* web_state) {
    std::unique_ptr<AttemptLoginTool> tool =
        AttemptLoginTool::Create(web_state->GetWeakPtr(), action, &delegate_);
    CHECK(tool);
    base::test::TestFuture<ToolExecutionResult> validate_future;
    tool->Validate(validate_future.GetCallback());
    if (!validate_future.Get().IsOk()) {
      return base::unexpected(validate_future.Get());
    }
    return tool;
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
    web_state->SetWebFramesManager(
        web::ContentWorld::kIsolatedWorld,
        std::make_unique<web::FakeWebFramesManager>());
    PasswordTabHelper::CreateForWebState(web_state.get());
    IOSChromeActorLoginDelegateClient::CreateForWebState(web_state.get());
    web::FakeWebState* web_state_ptr = web_state.get();
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::AtIndex(0).Activate());
    return web_state_ptr;
  }

  FakeActorLoginService* fake_actor_login_service() {
    return fake_actor_login_service_ptr_;
  }

  ActorTaskFormFillingHandler* form_filling_handler() {
    return form_filling_handler_ptr_;
  }

  FakeActorTaskInterventionDelegate* intervention_delegate() {
    return intervention_delegate_;
  }

  void TearDown() override {
    [mock_password_controller_ stopMocking];
    mock_password_controller_ = nil;
    captured_reparse_completion_ = nil;
    PlatformTest::TearDown();
  }

  void CloseAllWebStatesHelper() {
    CloseAllWebStates(*GetWebStateList(),
                      WebStateList::ClosingReason::kDefault);
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void CancelToolAndRefocus(AttemptLoginTool* tool,
                            base::WeakPtr<web::WebState> web_state,
                            base::OnceClosure callback) {
    tool->Cancel();
    if (web_state) {
      web_state->WasShown();
    }
    std::move(callback).Run();
  }

  // Stubs SharedPasswordController on `web_state` to intercept page reparse
  // requests and capture the completion handler.
  void MockReparseForms(web::WebState* web_state) {
    SharedPasswordController* controller =
        PasswordTabHelper::FromWebState(web_state)
            ->GetSharedPasswordController();
    mock_password_controller_ = OCMPartialMock(controller);

    OCMStub([mock_password_controller_
                actorLoginToolFindsFormsInWebState:(web::WebState*)
                                                       [OCMArg anyPointer]
                                 completionHandler:[OCMArg any]])
        .andDo(^(NSInvocation* invocation) {
          reparse_call_count_++;
          __unsafe_unretained void (^handler)(BOOL) = nil;
          [invocation getArgument:&handler atIndex:3];
          captured_reparse_completion_ = [handler copy];
        });
  }

  // Completes the pending reparse request with `forms_found`. If `forms_found`
  // is true and `tool` is provided, also notifies `tool` that a password form
  // was parsed.
  [[nodiscard]] testing::AssertionResult CompleteReparse(
      BOOL forms_found,
      AttemptLoginTool* tool = nullptr) {
    if (!captured_reparse_completion_) {
      return testing::AssertionFailure()
             << "captured_reparse_completion_ is nil (no pending reparse "
                "request).";
    }
    std::exchange(captured_reparse_completion_, nil)(forms_found);
    if (forms_found && tool) {
      tool->OnPasswordFormParsed(nullptr);
    }
    return testing::AssertionSuccess();
  }

  int reparse_call_count() const { return reparse_call_count_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::NavigationItem> navigation_item_;
  std::unique_ptr<TestBrowser> browser_;
  FakeToolDelegate delegate_;
  raw_ptr<FakeActorLoginService> fake_actor_login_service_ptr_ = nullptr;
  raw_ptr<ActorTaskFormFillingHandler> form_filling_handler_ptr_ = nullptr;
  FakeActorTaskInterventionDelegate* intervention_delegate_;
  id mock_password_controller_ = nil;
  void (^captured_reparse_completion_)(BOOL) = nil;
  int reparse_call_count_ = 0;
};

// Tests that creating the tool succeeds when given a valid tab ID corresponding
// to an active WebState.
TEST_F(AttemptLoginToolTest, Create_Success) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state = CreateAndInsertWebState();
  action.set_tab_id(web_state->GetUniqueIdentifier().identifier());

  base::expected<std::unique_ptr<AttemptLoginTool>, ToolExecutionResult>
      result = CreateToolAndValidate(action, web_state);

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

  auto result = CreateToolAndValidate(action, web_state);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  // Destroy WebState.
  CloseAllWebStatesHelper();

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

  auto result = CreateToolAndValidate(action, web_state);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  fake_actor_login_service()->get_credentials_error_ =
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

  auto result = CreateToolAndValidate(action, web_state);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  fake_actor_login_service()->credentials_ = {};

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

  auto result = CreateToolAndValidate(action, web_state_ptr);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(123);
  cred.has_persistent_permission = false;
  fake_actor_login_service()->credentials_ = {cred};

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  // Trigger the user selecting the credential.
  ASSERT_TRUE(intervention_delegate().selectFromSuggestionsCalled);
  [intervention_delegate() runCompletionWithSuggestion:nil
                                 shouldStorePermission:false];

  EXPECT_EQ(future.Get().code(),
            mojom::ActionResultCode::kLoginNoCredentialsAvailable);
}

// Tests that execution automatically selects and logs in using a credential
// that has persistent permission, without prompting the user.
TEST_F(AttemptLoginToolTest, Execute_PersistentCredentialDirectSelect_Success) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state = CreateAndInsertWebState();
  action.set_tab_id(web_state->GetUniqueIdentifier().identifier());

  auto result = CreateToolAndValidate(action, web_state);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(123);
  cred.has_persistent_permission = true;

  fake_actor_login_service()->credentials_ = {cred};
  fake_actor_login_service()->SetSequentialResults(
      {actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled});

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());
  EXPECT_EQ(future.Get().code(), mojom::ActionResultCode::kOk);
  EXPECT_TRUE(fake_actor_login_service()->attempt_login_called_);
  EXPECT_EQ(fake_actor_login_service()->attempted_credentials_.front().id,
            cred.id);
  EXPECT_TRUE(fake_actor_login_service()->attempted_should_store_permission_);
}

// Tests that when login requires device re-authentication, the tool waits for
// the WebState to become active (visible) before retrying login, and succeeds
// when the WebState is shown.
TEST_F(AttemptLoginToolTest, Execute_DeviceReauthRequired_Shown_Retry_Success) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state_ptr = CreateAndInsertWebState();
  action.set_tab_id(web_state_ptr->GetUniqueIdentifier().identifier());

  auto result = CreateToolAndValidate(action, web_state_ptr);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(123);
  cred.has_persistent_permission = false;

  fake_actor_login_service()->credentials_ = {cred};
  fake_actor_login_service()->SetSequentialResults(
      {actor_login::LoginStatusResult::kErrorDeviceReauthRequired,
       actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled});

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  // Execute shouldn't have completed yet because we are waiting on focus.
  EXPECT_FALSE(future.IsReady());

  // Trigger the user selecting the credential.
  ASSERT_TRUE(intervention_delegate().selectFromSuggestionsCalled);
  ASSERT_EQ(1u, intervention_delegate().promptedSuggestions.count);
  [intervention_delegate()
      runCompletionWithSuggestion:intervention_delegate().promptedSuggestions[0]
            shouldStorePermission:false];

  // Still not completed because the first attempt returned
  // kErrorDeviceReauthRequired and we are waiting on focus.
  EXPECT_FALSE(future.IsReady());

  // Simulate tab focus/visible again.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&web::WebState::WasShown, web_state_ptr->GetWeakPtr()));

  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(future.Get().code(), mojom::ActionResultCode::kOk);
  EXPECT_EQ(fake_actor_login_service()->attempted_credentials_.size(), 2u);
}

// Tests that if the WebState is destroyed while waiting for device
// re-authentication, the tool correctly exits with a kTabWentAway code.
TEST_F(AttemptLoginToolTest, Execute_DeviceReauthRequired_WebStateDestroyed) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state = CreateAndInsertWebState();
  action.set_tab_id(web_state->GetUniqueIdentifier().identifier());

  auto result = CreateToolAndValidate(action, web_state);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(123);
  cred.has_persistent_permission = false;

  fake_actor_login_service()->credentials_ = {cred};
  fake_actor_login_service()->SetSequentialResults(
      {actor_login::LoginStatusResult::kErrorDeviceReauthRequired});

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  // Trigger the user selecting the credential.
  ASSERT_TRUE(intervention_delegate().selectFromSuggestionsCalled);
  ASSERT_EQ(1u, intervention_delegate().promptedSuggestions.count);
  [intervention_delegate()
      runCompletionWithSuggestion:intervention_delegate().promptedSuggestions[0]
            shouldStorePermission:false];

  EXPECT_FALSE(future.IsReady());

  // Simulate web state destroyed.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AttemptLoginToolTest::CloseAllWebStatesHelper,
                                base::Unretained(this)));
  EXPECT_EQ(future.Get().code(), mojom::ActionResultCode::kTabWentAway);
}

// Tests that cancelling the tool while it is waiting for device
// re-authentication correctly tears down observations and stops future login
// attempts.
TEST_F(AttemptLoginToolTest, Execute_DeviceReauthRequired_Cancel) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state_ptr = CreateAndInsertWebState();
  action.set_tab_id(web_state_ptr->GetUniqueIdentifier().identifier());

  auto result = CreateToolAndValidate(action, web_state_ptr);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(123);
  cred.has_persistent_permission = false;

  fake_actor_login_service()->credentials_ = {cred};
  fake_actor_login_service()->SetSequentialResults(
      {actor_login::LoginStatusResult::kErrorDeviceReauthRequired});

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  // Trigger the user selecting the credential.
  ASSERT_TRUE(intervention_delegate().selectFromSuggestionsCalled);
  ASSERT_EQ(1u, intervention_delegate().promptedSuggestions.count);
  [intervention_delegate()
      runCompletionWithSuggestion:intervention_delegate().promptedSuggestions[0]
            shouldStorePermission:false];

  EXPECT_FALSE(future.IsReady());

  // Cancel execution, and simulate re-focusing.
  base::test::TestFuture<void> cancel_future;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&AttemptLoginToolTest::CancelToolAndRefocus,
                     base::Unretained(this), base::Unretained(tool.get()),
                     web_state_ptr->GetWeakPtr(), cancel_future.GetCallback()));
  EXPECT_TRUE(cancel_future.Wait());

  // Since we cancelled, AttemptLogin should not have been called a second time.
  EXPECT_EQ(fake_actor_login_service()->attempted_credentials_.size(), 1u);
}

// Tests that if the page changes (e.g. user navigates away) while the
// credential selection prompt is showing, the tool completes with
// kLoginPageChangedDuringSelection.
TEST_F(AttemptLoginToolTest, Execute_PageChangedDuringSelection) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state_ptr = CreateAndInsertWebState();
  action.set_tab_id(web_state_ptr->GetUniqueIdentifier().identifier());

  auto result = CreateToolAndValidate(action, web_state_ptr);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(123);
  cred.has_persistent_permission = false;

  fake_actor_login_service()->credentials_ = {cred};

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
  ASSERT_TRUE(intervention_delegate().selectFromSuggestionsCalled);
  ASSERT_EQ(1u, intervention_delegate().promptedSuggestions.count);
  [intervention_delegate()
      runCompletionWithSuggestion:intervention_delegate().promptedSuggestions[0]
            shouldStorePermission:false];
  EXPECT_EQ(future.Get().code(),
            mojom::ActionResultCode::kLoginPageChangedDuringSelection);
}

// Tests that when the initial login attempt returns kErrorNoSigninForm, the
// tool requests a page reparse via ActorLoginToolDelegate and retries login
// upon discovering forms.
TEST_F(AttemptLoginToolTest, Execute_NoSigninForm_ReparseSuccess_RetriesLogin) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state = CreateAndInsertWebState();
  action.set_tab_id(web_state->GetUniqueIdentifier().identifier());

  auto result = CreateToolAndValidate(action, web_state);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(123);
  cred.has_persistent_permission = true;
  fake_actor_login_service()->credentials_ = {cred};

  // Configure first attempt to return kErrorNoSigninForm, and retry to succeed.
  fake_actor_login_service()->SetSequentialResults(
      {actor_login::LoginStatusResult::kErrorNoSigninForm,
       actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled});

  MockReparseForms(web_state);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Verifies that WasShown called during an active reparse is ignored and does
  // not trigger a premature login attempt.
  tool->WasShown(web_state);
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(fake_actor_login_service()->attempted_credentials_.size(), 1u);

  // Complete reparse with forms found and parsed.
  ASSERT_TRUE(CompleteReparse(YES, tool.get()));

  EXPECT_EQ(future.Get().code(), mojom::ActionResultCode::kOk);
  EXPECT_EQ(fake_actor_login_service()->attempted_credentials_.size(), 2u);
  EXPECT_EQ(reparse_call_count(), 1);
}

// Tests that when a reparse returns NO (no forms found), the tool exits with
// `kLoginNotLoginPage` without retrying login.
TEST_F(AttemptLoginToolTest, Execute_NoSigninForm_ReparseNoFormsFound) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state = CreateAndInsertWebState();
  action.set_tab_id(web_state->GetUniqueIdentifier().identifier());

  auto result = CreateToolAndValidate(action, web_state);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(123);
  cred.has_persistent_permission = true;
  fake_actor_login_service()->credentials_ = {cred};

  fake_actor_login_service()->SetSequentialResults(
      {actor_login::LoginStatusResult::kErrorNoSigninForm});

  MockReparseForms(web_state);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Complete reparse with NO forms found.
  ASSERT_TRUE(CompleteReparse(NO));

  EXPECT_EQ(future.Get().code(), mojom::ActionResultCode::kLoginNotLoginPage);
  EXPECT_EQ(fake_actor_login_service()->attempted_credentials_.size(), 1u);
  EXPECT_EQ(reparse_call_count(), 1);
}

// Tests that when form reparse does not complete within the timeout, the timer
// triggers and finishes execution with kLoginNotLoginPage.
TEST_F(AttemptLoginToolTest, Execute_NoSigninForm_ReparseTimeout) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state = CreateAndInsertWebState();
  action.set_tab_id(web_state->GetUniqueIdentifier().identifier());

  auto result = CreateToolAndValidate(action, web_state);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(123);
  cred.has_persistent_permission = true;
  fake_actor_login_service()->credentials_ = {cred};

  fake_actor_login_service()->SetSequentialResults(
      {actor_login::LoginStatusResult::kErrorNoSigninForm});

  MockReparseForms(web_state);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Fast forward past kReparseTimeout (1 second).
  FastForwardBy(base::Seconds(1));

  EXPECT_EQ(future.Get().code(), mojom::ActionResultCode::kLoginNotLoginPage);
  EXPECT_EQ(fake_actor_login_service()->attempted_credentials_.size(), 1u);
  EXPECT_EQ(reparse_call_count(), 1);

  // Verifies that a late reparse completion callback arriving after the timeout
  // has fired is safely dropped and does not trigger another login attempt.
  ASSERT_TRUE(CompleteReparse(YES, tool.get()));
  EXPECT_EQ(fake_actor_login_service()->attempted_credentials_.size(), 1u);
}

// Tests that the tool only attempts reparse once and does not loop if the retry
// attempt also returns kErrorNoSigninForm.
TEST_F(AttemptLoginToolTest, Execute_NoSigninForm_OnlyReparsesOnce) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state = CreateAndInsertWebState();
  action.set_tab_id(web_state->GetUniqueIdentifier().identifier());

  auto result = CreateToolAndValidate(action, web_state);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(123);
  cred.has_persistent_permission = true;
  fake_actor_login_service()->credentials_ = {cred};

  // Both attempts return kErrorNoSigninForm.
  fake_actor_login_service()->SetSequentialResults(
      {actor_login::LoginStatusResult::kErrorNoSigninForm,
       actor_login::LoginStatusResult::kErrorNoSigninForm});

  MockReparseForms(web_state);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ASSERT_TRUE(CompleteReparse(YES, tool.get()));

  EXPECT_EQ(future.Get().code(), mojom::ActionResultCode::kLoginNotLoginPage);
  EXPECT_EQ(reparse_call_count(), 1);
  EXPECT_EQ(fake_actor_login_service()->attempted_credentials_.size(), 2u);
}

// Tests that when DOM extraction succeeds (discovers forms) but form parsing
// does not complete within the timeout, the timer triggers and exits with
// kLoginNotLoginPage.
TEST_F(AttemptLoginToolTest,
       Execute_NoSigninForm_ReparseDomFoundForms_ParsingTimesOut) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state = CreateAndInsertWebState();
  action.set_tab_id(web_state->GetUniqueIdentifier().identifier());

  auto result = CreateToolAndValidate(action, web_state);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(123);
  cred.has_persistent_permission = true;
  fake_actor_login_service()->credentials_ = {cred};

  fake_actor_login_service()->SetSequentialResults(
      {actor_login::LoginStatusResult::kErrorNoSigninForm});

  MockReparseForms(web_state);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Complete DOM extraction with forms found, but do not emit
  // OnPasswordFormParsed.
  ASSERT_TRUE(CompleteReparse(YES));

  // Still waiting for OnPasswordFormParsed.
  EXPECT_FALSE(future.IsReady());

  // Fast forward past kReparseTimeout (1 second).
  FastForwardBy(base::Seconds(1));

  EXPECT_EQ(future.Get().code(), mojom::ActionResultCode::kLoginNotLoginPage);
  EXPECT_EQ(fake_actor_login_service()->attempted_credentials_.size(), 1u);
  EXPECT_EQ(reparse_call_count(), 1);
}

// Tests that cancelling the tool while waiting for reparse cleans up state and
// prevents subsequent callbacks from retrying.
TEST_F(AttemptLoginToolTest, Execute_NoSigninForm_CancelDuringReparse) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state = CreateAndInsertWebState();
  action.set_tab_id(web_state->GetUniqueIdentifier().identifier());

  auto result = CreateToolAndValidate(action, web_state);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(123);
  cred.has_persistent_permission = true;
  fake_actor_login_service()->credentials_ = {cred};

  fake_actor_login_service()->SetSequentialResults(
      {actor_login::LoginStatusResult::kErrorNoSigninForm});

  MockReparseForms(web_state);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  tool->Cancel();

  // Invoke completion after cancellation.
  ASSERT_TRUE(CompleteReparse(YES, tool.get()));

  // Advance timer as well.
  FastForwardBy(base::Seconds(1));

  // Verify only the 1 initial attempt happened and no crash occurred.
  EXPECT_EQ(fake_actor_login_service()->attempted_credentials_.size(), 1u);
  EXPECT_EQ(reparse_call_count(), 1);
}

// Tests that if the WebState is destroyed while waiting for reparse, execution
// terminates with kTabWentAway.
TEST_F(AttemptLoginToolTest,
       Execute_NoSigninForm_WebStateDestroyedDuringReparse) {
  optimization_guide::proto::AttemptLoginAction action;
  web::FakeWebState* web_state = CreateAndInsertWebState();
  action.set_tab_id(web_state->GetUniqueIdentifier().identifier());

  auto result = CreateToolAndValidate(action, web_state);
  ASSERT_TRUE(result.has_value());
  std::unique_ptr<AttemptLoginTool> tool = std::move(result.value());

  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(123);
  cred.has_persistent_permission = true;
  fake_actor_login_service()->credentials_ = {cred};

  fake_actor_login_service()->SetSequentialResults(
      {actor_login::LoginStatusResult::kErrorNoSigninForm});

  MockReparseForms(web_state);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Destroy WebState.
  CloseAllWebStatesHelper();

  EXPECT_EQ(future.Get().code(), mojom::ActionResultCode::kTabWentAway);
  EXPECT_EQ(reparse_call_count(), 1);
}

}  // namespace actor
