// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/captured_surface_control_permission_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/mock_permission_controller.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace content {
namespace {

using ::blink::mojom::PermissionStatus;
using PermissionManager = ::content::CapturedSurfaceControlPermissionManager;
using CallbackType = ::base::OnceCallback<void(PermissionStatus)>;
using CallbackActionType = ::base::OnceCallback<void(CallbackType)>;

// Extends MockPermissionController and allows test suites to conveniently
// determine whether a prompt was shown, as well as simulate when and how the
// user interacts with the prompts.
class CscMockPermissionController : public MockPermissionController {
 public:
  ~CscMockPermissionController() override = default;

  PermissionStatus GetPermissionStatusForCurrentDocument(
      blink::PermissionType permission,
      RenderFrameHost* render_frame_host) override {
    return permission_status_;
  }

  void RequestPermissionFromCurrentDocument(
      RenderFrameHost* render_frame_host,
      PermissionRequestDescription request_description,
      base::OnceCallback<void(PermissionStatus)> callback) override {
    CHECK(callback_action_);
    std::move(callback_action_).Run(std::move(callback));
  }

  void SetPermissionStatus(PermissionStatus permission_status) {
    permission_status_ = permission_status;
  }

  void SetCallbackAction(CallbackActionType callback_action) {
    callback_action_ = std::move(callback_action);
  }

 private:
  CallbackActionType callback_action_;
  PermissionStatus permission_status_ = PermissionStatus::ASK;
};

// Encapsulates the state and logic of an invocation of
// CapturedSurfaceControlPermissionManager::CheckPermission(),
// thereby allowing us to conveniently test the interaction
// between multiple calls.
class PermissionCheckState final {
 public:
  void WaitForUserPromptToBeShown() { user_prompt_shown_run_loop.Run(); }

  void WaitForCheckPermissionCallbackResult() {
    check_permission_run_loop.Run();
  }

  void SimulateUserPromptResponse(bool allow) {
    CHECK(callback_for_pending_prompt_);
    std::move(callback_for_pending_prompt_)
        .Run(allow ? PermissionStatus::GRANTED : PermissionStatus::DENIED);
  }

  void SetUserPrompted(base::OnceCallback<void(PermissionStatus)> callback) {
    CHECK(!user_prompted_);
    CHECK(!callback_for_pending_prompt_);

    user_prompted_ = true;
    callback_for_pending_prompt_ = std::move(callback);

    user_prompt_shown_run_loop.Quit();
  }

  bool user_prompted() const { return user_prompted_; }

  void SetResult(PermissionManager::PermissionResult result) {
    CHECK(!result_.has_value());
    result_ = result;
    check_permission_run_loop.Quit();
  }

  std::optional<PermissionManager::PermissionResult> result() const {
    return result_;
  }

 private:
  bool user_prompted_ = false;
  base::RunLoop user_prompt_shown_run_loop;

  std::optional<PermissionManager::PermissionResult> result_;
  base::RunLoop check_permission_run_loop;

  base::OnceCallback<void(PermissionStatus)> callback_for_pending_prompt_;
};

class CapturedSurfaceControlPermissionManagerTest
    : public RenderViewHostTestHarness,
      public ::testing::WithParamInterface<bool> {
 public:
  CapturedSurfaceControlPermissionManagerTest()
      : sticky_permissions_(GetParam()) {
    scoped_feature_list_.InitWithFeatureState(
        features::kCapturedSurfaceControlStickyPermissions,
        sticky_permissions_);
  }
  ~CapturedSurfaceControlPermissionManagerTest() override = default;

  std::unique_ptr<TestWebContents> MakeTestWebContents() {
    scoped_refptr<SiteInstance> instance =
        SiteInstance::Create(GetBrowserContext());
    instance->GetProcess()->Init();
    return TestWebContents::Create(GetBrowserContext(), std::move(instance));
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    capturing_wc_ = MakeTestWebContents();
    FocusCapturer();

    auto mock_permission_controller =
        std::make_unique<CscMockPermissionController>();
    mock_permission_controller_ = mock_permission_controller.get();
    GetBrowserContext()->SetPermissionControllerForTesting(
        std::move(mock_permission_controller));

    permission_manager_ =
        std::make_unique<CapturedSurfaceControlPermissionManager>(
            capturing_wc_->GetPrimaryMainFrame()->GetGlobalId());
  }

  void TearDown() override {
    permission_manager_.reset();
    capturing_wc_.reset();
    mock_permission_controller_ = nullptr;

    RenderViewHostTestHarness::TearDown();
  }

  void FocusCapturer() {
    capturing_wc_->GetPrimaryMainFrame()->GetRenderWidgetHost()->Focus();
    FrameTree& frame_tree = capturing_wc_->GetPrimaryFrameTree();
    FrameTreeNode* const root = frame_tree.root();
    frame_tree.SetFocusedFrame(
        root, root->current_frame_host()->GetSiteInstance()->group());
  }

  void UnFocusCapturer() {
    capturing_wc_->GetPrimaryMainFrame()->GetRenderWidgetHost()->Blur();
  }

  // Run CheckPermission() on the unit-under-test
  // (CapturedSurfaceControlPermissionManager).
  // Returns a state object that can be used to test the results
  // of this invocation of CheckPermission().
  std::unique_ptr<PermissionCheckState> CheckPermission() {
    auto state = std::make_unique<PermissionCheckState>();

    mock_permission_controller_->SetCallbackAction(base::BindOnce(
        [](PermissionCheckState* state,
           base::OnceCallback<void(PermissionStatus)> callback) {
          state->SetUserPrompted(std::move(callback));
        },
        base::Unretained(state.get())));

    permission_manager_->CheckPermission(base::BindOnce(
        [](PermissionCheckState* state, bool sticky_permissions,
           CscMockPermissionController* mock_permission_controller,
           PermissionManager::PermissionResult result) {
          if (sticky_permissions) {
            mock_permission_controller->SetPermissionStatus(
                result == PermissionManager::PermissionResult::kGranted
                    ? PermissionStatus::GRANTED
                    : PermissionStatus::DENIED);
          }
          state->SetResult(result);
        },
        base::Unretained(state.get()), sticky_permissions_,
        base::Unretained(mock_permission_controller_.get())));

    return state;
  }

  void SetTransientActivation(bool has_activation) {
    TestRenderFrameHost* const rfh = capturing_wc_->GetPrimaryMainFrame();
    if (has_activation) {
      rfh->SimulateUserActivation();
    } else {
      rfh->frame_tree_node()->UpdateUserActivationState(
          blink::mojom::UserActivationUpdateType::kConsumeTransientActivation,
          blink::mojom::UserActivationNotificationType::kTest);
    }
    CHECK_EQ(rfh->HasTransientUserActivation(), has_activation);
  }

 protected:
  const bool sticky_permissions_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestWebContents> capturing_wc_;
  raw_ptr<CscMockPermissionController> mock_permission_controller_ = nullptr;
  std::unique_ptr<CapturedSurfaceControlPermissionManager> permission_manager_;
};

INSTANTIATE_TEST_SUITE_P(,
                         CapturedSurfaceControlPermissionManagerTest,
                         ::testing::Bool());

TEST_P(CapturedSurfaceControlPermissionManagerTest,
       UserNotPromptedOnFirstCheckAndHasNoTransientActivation) {
  SetTransientActivation(false);

  std::unique_ptr<PermissionCheckState> state = CheckPermission();
  state->WaitForCheckPermissionCallbackResult();

  EXPECT_FALSE(state->user_prompted());
  EXPECT_EQ(state->result(), PermissionManager::PermissionResult::kDenied);
}

TEST_P(CapturedSurfaceControlPermissionManagerTest,
       UserPromptedOnFirstCheckAndHasTransientActivation) {
  SetTransientActivation(true);

  std::unique_ptr<PermissionCheckState> state = CheckPermission();
  state->WaitForUserPromptToBeShown();

  EXPECT_TRUE(state->user_prompted());
}

TEST_P(CapturedSurfaceControlPermissionManagerTest,
       DeniedReportedIfUserDeniesPrompt) {
  SetTransientActivation(true);

  std::unique_ptr<PermissionCheckState> state = CheckPermission();
  state->WaitForUserPromptToBeShown();
  state->SimulateUserPromptResponse(/*allow=*/false);
  state->WaitForCheckPermissionCallbackResult();

  EXPECT_TRUE(state->user_prompted());
  EXPECT_EQ(state->result(), PermissionManager::PermissionResult::kDenied);
}

TEST_P(CapturedSurfaceControlPermissionManagerTest,
       GrantedReportedIfUserApprovesPrompt) {
  SetTransientActivation(true);

  std::unique_ptr<PermissionCheckState> state = CheckPermission();
  state->WaitForUserPromptToBeShown();
  state->SimulateUserPromptResponse(/*allow=*/true);
  state->WaitForCheckPermissionCallbackResult();

  EXPECT_TRUE(state->user_prompted());
  EXPECT_EQ(state->result(), PermissionManager::PermissionResult::kGranted);
}

TEST_P(CapturedSurfaceControlPermissionManagerTest,
       AfterFirstGrantNoPromptAndNoActivationRequirement) {
  // This block repeats `GrantedReportedIfUserApprovesPrompt`.
  {
    SetTransientActivation(true);

    std::unique_ptr<PermissionCheckState> init_state = CheckPermission();
    init_state->WaitForUserPromptToBeShown();
    init_state->SimulateUserPromptResponse(/*allow=*/true);
    init_state->WaitForCheckPermissionCallbackResult();

    ASSERT_TRUE(init_state->user_prompted());
    ASSERT_EQ(init_state->result(),
              PermissionManager::PermissionResult::kGranted);
  }

  SetTransientActivation(false);
  std::unique_ptr<PermissionCheckState> state = CheckPermission();
  state->WaitForCheckPermissionCallbackResult();

  EXPECT_FALSE(state->user_prompted());
  EXPECT_EQ(state->result(), PermissionManager::PermissionResult::kGranted);
}

TEST_P(CapturedSurfaceControlPermissionManagerTest, CanGrantAfterDenying) {
  if (sticky_permissions_) {
    GTEST_SKIP() << "With sticky permissions, denials are persisted.";
  }

  // This block repeats `DeniedReportedIfUserDeniesPrompt`.
  {
    SetTransientActivation(true);

    std::unique_ptr<PermissionCheckState> init_state = CheckPermission();
    init_state->WaitForUserPromptToBeShown();
    init_state->SimulateUserPromptResponse(/*allow=*/false);
    init_state->WaitForCheckPermissionCallbackResult();

    EXPECT_TRUE(init_state->user_prompted());
    EXPECT_EQ(init_state->result(),
              PermissionManager::PermissionResult::kDenied);
  }

  // Approve this time.
  std::unique_ptr<PermissionCheckState> state = CheckPermission();
  state->WaitForUserPromptToBeShown();
  state->SimulateUserPromptResponse(/*allow=*/true);
  state->WaitForCheckPermissionCallbackResult();

  EXPECT_TRUE(state->user_prompted());
  EXPECT_EQ(state->result(), PermissionManager::PermissionResult::kGranted);
}

TEST_P(CapturedSurfaceControlPermissionManagerTest, EmbargoObserved) {
  if (sticky_permissions_) {
    GTEST_SKIP() << "With sticky permissions, embargo is managed by the "
                    "PermissionController.";
  }

  // Prompt and deny `kMaxPromptAttempts` times.
  for (int i = 0; i < PermissionManager::kMaxPromptAttempts; ++i) {
    SetTransientActivation(true);

    std::unique_ptr<PermissionCheckState> denied_state = CheckPermission();
    denied_state->WaitForUserPromptToBeShown();
    denied_state->SimulateUserPromptResponse(/*allow=*/false);
    denied_state->WaitForCheckPermissionCallbackResult();

    ASSERT_TRUE(denied_state->user_prompted());
    ASSERT_EQ(denied_state->result(),
              PermissionManager::PermissionResult::kDenied);
  }

  // Ensure that no more prompts are permitted.
  SetTransientActivation(true);
  std::unique_ptr<PermissionCheckState> state = CheckPermission();
  state->WaitForCheckPermissionCallbackResult();
  EXPECT_FALSE(state->user_prompted());
  EXPECT_EQ(state->result(), PermissionManager::PermissionResult::kDenied);
}

TEST_P(CapturedSurfaceControlPermissionManagerTest,
       AdditionalPermissionChecksDeniedWhilePromptPending) {
  if (sticky_permissions_) {
    GTEST_SKIP() << "With sticky permissions, the PermissionController manages "
                    "limitations on concurrent prompting.";
  }

  // User prompted but does not yet respond.
  SetTransientActivation(true);
  std::unique_ptr<PermissionCheckState> state_1 = CheckPermission();
  state_1->WaitForUserPromptToBeShown();
  ASSERT_TRUE(state_1->user_prompted());

  // Additional checks immediately denied while prior prompt pending.
  SetTransientActivation(true);
  std::unique_ptr<PermissionCheckState> state_2 = CheckPermission();
  state_2->WaitForCheckPermissionCallbackResult();
  EXPECT_FALSE(state_2->user_prompted());
  EXPECT_EQ(state_2->result(), PermissionManager::PermissionResult::kDenied);
}

TEST_P(CapturedSurfaceControlPermissionManagerTest,
       SubsequentPermissionChecksAllowedAfterPromptAcceptedByUser) {
  if (sticky_permissions_) {
    GTEST_SKIP() << "State managed by PermissionController.";
  }

  // User prompted but does not yet respond.
  SetTransientActivation(true);
  std::unique_ptr<PermissionCheckState> state_1 = CheckPermission();
  state_1->WaitForUserPromptToBeShown();
  ASSERT_TRUE(state_1->user_prompted());

  // Additional checks immediately denied while prior prompt pending.
  // Further, embargo cannot be triggered by excessive checks at this stage,
  // because they do not trigger a prompt.
  for (int i = 0; i < PermissionManager::kMaxPromptAttempts; ++i) {
    SetTransientActivation(true);

    std::unique_ptr<PermissionCheckState> denied_state = CheckPermission();
    denied_state->WaitForCheckPermissionCallbackResult();

    ASSERT_FALSE(denied_state->user_prompted());
    ASSERT_EQ(denied_state->result(),
              PermissionManager::PermissionResult::kDenied);
  }

  // If the user accepts the original prompt, that permission-check's callback
  // is handled correctly, despite the intervening denied calls.
  state_1->SimulateUserPromptResponse(/*allow=*/true);
  state_1->WaitForCheckPermissionCallbackResult();
  EXPECT_EQ(state_1->result(), PermissionManager::PermissionResult::kGranted);

  // Further checks are now granted without a prompt.
  SetTransientActivation(true);
  std::unique_ptr<PermissionCheckState> state_2 = CheckPermission();
  state_2->WaitForCheckPermissionCallbackResult();
  EXPECT_FALSE(state_2->user_prompted());
  EXPECT_EQ(state_2->result(), PermissionManager::PermissionResult::kGranted);
}

TEST_P(CapturedSurfaceControlPermissionManagerTest,
       SubsequentPermissionChecksCauseNewPromptAfterPromptRejecdByUser) {
  if (sticky_permissions_) {
    GTEST_SKIP() << "With sticky permissions, denials are persisted.";
  }

  // User prompted but does not yet respond.
  SetTransientActivation(true);
  std::unique_ptr<PermissionCheckState> state_1 = CheckPermission();
  state_1->WaitForUserPromptToBeShown();
  ASSERT_TRUE(state_1->user_prompted());

  // Additional checks immediately denied while prior prompt pending.
  // Further, embargo cannot be triggered by excessive checks at this stage,
  // because they do not trigger a prompt.
  for (int i = 0; i < PermissionManager::kMaxPromptAttempts; ++i) {
    SetTransientActivation(true);

    std::unique_ptr<PermissionCheckState> denied_state = CheckPermission();
    denied_state->WaitForCheckPermissionCallbackResult();

    ASSERT_FALSE(denied_state->user_prompted());
    ASSERT_EQ(denied_state->result(),
              PermissionManager::PermissionResult::kDenied);
  }

  // If the user rejects the original prompt, that permission-check's callback
  // is handled correctly, despite the intervening denied calls.
  state_1->SimulateUserPromptResponse(/*allow=*/false);
  state_1->WaitForCheckPermissionCallbackResult();
  EXPECT_EQ(state_1->result(), PermissionManager::PermissionResult::kDenied);

  // Further checks cause a new prompt.
  SetTransientActivation(true);
  std::unique_ptr<PermissionCheckState> state_2 = CheckPermission();
  state_2->WaitForUserPromptToBeShown();
  EXPECT_TRUE(state_2->user_prompted());
  EXPECT_EQ(state_2->result(), std::nullopt);
}

TEST_P(CapturedSurfaceControlPermissionManagerTest,
       CallFailsIfCapturerUnfocused) {
  UnFocusCapturer();
  SetTransientActivation(true);

  std::unique_ptr<PermissionCheckState> state = CheckPermission();
  state->WaitForCheckPermissionCallbackResult();

  EXPECT_FALSE(state->user_prompted());
  EXPECT_EQ(state->result(), PermissionManager::PermissionResult::kError);
}

TEST_P(CapturedSurfaceControlPermissionManagerTest,
       UserPromptedIfCapturerFocused) {
  // Capturer focused in SetUp()
  SetTransientActivation(true);

  std::unique_ptr<PermissionCheckState> state = CheckPermission();
  state->WaitForUserPromptToBeShown();

  EXPECT_TRUE(state->user_prompted());
}

}  // namespace
}  // namespace content
