// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_pending_prompt_manager.h"

#import <memory>
#import <optional>
#import <vector>

#import "base/test/bind.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/save_entity_params.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_save_entity_prompt_model.h"
#import "ios/chrome/browser/autofill/model/features.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/page_transition_types.h"

namespace {

using AutofillAiBubbleResult = autofill::AutofillClient::AutofillAiBubbleResult;
using autofill::AutofillClient;
using autofill::EntityInstance;
using autofill::SaveEntityParams;
namespace test = autofill::test;

}  // namespace

// Fake implementation of `AutofillCommands` to capture save entity prompt
// commands in unit tests.
@interface FakeAutofillCommandsForPromptManager : NSObject
@property(nonatomic, assign) int showSaveEntityDialogCount;
@property(nonatomic, assign) int dismissSaveEntityDialogCount;
@end

@implementation FakeAutofillCommandsForPromptManager
- (void)showSaveEntityDialogOnWebState:(web::WebState*)webState {
  _showSaveEntityDialogCount++;
}
- (void)dismissSaveEntityDialog {
  _dismissSaveEntityDialogCount++;
}
@end

namespace {

class AutofillPendingPromptManagerTest : public PlatformTest {
 protected:
  AutofillPendingPromptManagerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kAutofillReshowSavePromptOnTabReturn);
    autofill_handler_ = [[FakeAutofillCommandsForPromptManager alloc] init];
    manager_.emplace(&web_state_, (id<AutofillCommands>)autofill_handler_);
  }

  void TearDown() override {
    manager_.reset();
    PlatformTest::TearDown();
  }

  std::unique_ptr<AutofillAiSaveEntityPromptModel> CreateEntityModel() {
    return std::make_unique<AutofillAiSaveEntityPromptModel>(SaveEntityParams(
        test::GetPassportEntityInstance(), /*old_entity=*/std::nullopt,
        /*user_email=*/u"test@example.com",
        /*save_is_synchronous=*/true,
        base::BindLambdaForTesting(
            [this](AutofillAiBubbleResult result,
                   std::optional<EntityInstance> edited_entity,
                   const AutofillClient::EntityImportUIContext& ui_context) {
              results_.push_back(result);
            }),
        /*public_passes_notice=*/{},
        /*show_explicit_decline_button=*/false));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  FakeAutofillCommandsForPromptManager* autofill_handler_;
  std::vector<AutofillAiBubbleResult> results_;
  web::FakeWebState web_state_;
  std::optional<AutofillPendingPromptManager> manager_;
};

// Tests that `WasShown` re-triggers `showSaveEntityDialogOnWebState:`
// when a model is pending.
TEST_F(AutofillPendingPromptManagerTest, WasShownReTriggersPromptWhenPending) {
  manager_->ShowSaveEntityPrompt(CreateEntityModel());
  manager_->WasShown(&web_state_);
  EXPECT_EQ(autofill_handler_.showSaveEntityDialogCount, 2);
}

// Tests that WasShown does nothing if the feature flag is disabled.
TEST_F(AutofillPendingPromptManagerTest,
       WasShownDoesNothingWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kAutofillReshowSavePromptOnTabReturn);

  manager_->ShowSaveEntityPrompt(CreateEntityModel());
  EXPECT_EQ(autofill_handler_.showSaveEntityDialogCount, 1);

  manager_->WasShown(&web_state_);
  EXPECT_EQ(autofill_handler_.showSaveEntityDialogCount, 1);
}

// Tests that WasShown does nothing if no model is pending.
TEST_F(AutofillPendingPromptManagerTest, WasShownDoesNothingWhenNotPending) {
  EXPECT_EQ(autofill_handler_.showSaveEntityDialogCount, 0);
  manager_->WasShown(&web_state_);
  EXPECT_EQ(autofill_handler_.showSaveEntityDialogCount, 0);
}

// Tests that `ShowSaveEntityPrompt` notifies the commands handler.
TEST_F(AutofillPendingPromptManagerTest, ShowSaveEntityPromptNotifiesHandler) {
  manager_->ShowSaveEntityPrompt(CreateEntityModel());

  EXPECT_EQ(autofill_handler_.showSaveEntityDialogCount, 1);
  EXPECT_NE(manager_->GetSaveEntityPromptModel(), nullptr);
}

// Tests that calling `ShowSaveEntityPrompt` while a prompt is already pending
// expires the previous prompt with 0 strikes before replacing it.
TEST_F(AutofillPendingPromptManagerTest,
       ShowSaveEntityPromptExpiresPreviousPrompt) {
  manager_->ShowSaveEntityPrompt(CreateEntityModel());
  EXPECT_TRUE(results_.empty());

  // Show a second prompt; the first one should be expired neutrally (kUnknown).
  manager_->ShowSaveEntityPrompt(CreateEntityModel());
  EXPECT_EQ(std::vector{AutofillAiBubbleResult::kUnknown}, results_);
  EXPECT_NE(manager_->GetSaveEntityPromptModel(), nullptr);
}

// Tests that same-document navigation does not expire the prompt.
TEST_F(AutofillPendingPromptManagerTest,
       SameDocumentNavigationDoesNotExpirePrompt) {
  manager_->ShowSaveEntityPrompt(CreateEntityModel());

  web::FakeNavigationContext same_doc_context;
  same_doc_context.SetIsSameDocument(true);
  same_doc_context.SetHasCommitted(true);
  manager_->DidFinishNavigation(&web_state_, &same_doc_context);

  EXPECT_TRUE(results_.empty());
  EXPECT_EQ(autofill_handler_.dismissSaveEntityDialogCount, 0);
  EXPECT_NE(manager_->GetSaveEntityPromptModel(), nullptr);
}

// Tests that uncommitted navigation does not expire the prompt.
TEST_F(AutofillPendingPromptManagerTest,
       UncommittedNavigationDoesNotExpirePrompt) {
  manager_->ShowSaveEntityPrompt(CreateEntityModel());

  web::FakeNavigationContext uncommitted_context;
  uncommitted_context.SetIsSameDocument(false);
  uncommitted_context.SetHasCommitted(false);
  manager_->DidFinishNavigation(&web_state_, &uncommitted_context);

  EXPECT_TRUE(results_.empty());
  EXPECT_EQ(autofill_handler_.dismissSaveEntityDialogCount, 0);
  EXPECT_NE(manager_->GetSaveEntityPromptModel(), nullptr);
}

// Tests that non-gesture navigation does not expire the prompt.
TEST_F(AutofillPendingPromptManagerTest,
       NonGestureNavigationDoesNotExpirePrompt) {
  manager_->ShowSaveEntityPrompt(CreateEntityModel());

  web::FakeNavigationContext non_gesture_context;
  non_gesture_context.SetIsSameDocument(false);
  non_gesture_context.SetHasCommitted(true);
  non_gesture_context.SetHasUserGesture(false);
  manager_->DidFinishNavigation(&web_state_, &non_gesture_context);

  EXPECT_TRUE(results_.empty());
  EXPECT_EQ(autofill_handler_.dismissSaveEntityDialogCount, 0);
  EXPECT_NE(manager_->GetSaveEntityPromptModel(), nullptr);
}

// Tests that POST navigation does not expire the prompt.
TEST_F(AutofillPendingPromptManagerTest, PostNavigationDoesNotExpirePrompt) {
  manager_->ShowSaveEntityPrompt(CreateEntityModel());

  web::FakeNavigationContext post_context;
  post_context.SetIsSameDocument(false);
  post_context.SetHasCommitted(true);
  post_context.SetHasUserGesture(true);
  post_context.SetIsPost(true);
  manager_->DidFinishNavigation(&web_state_, &post_context);

  EXPECT_TRUE(results_.empty());
  EXPECT_EQ(autofill_handler_.dismissSaveEntityDialogCount, 0);
  EXPECT_NE(manager_->GetSaveEntityPromptModel(), nullptr);
}

// Tests that form submit transition navigation does not expire the prompt.
TEST_F(AutofillPendingPromptManagerTest,
       FormSubmitNavigationDoesNotExpirePrompt) {
  manager_->ShowSaveEntityPrompt(CreateEntityModel());

  web::FakeNavigationContext form_submit_context;
  form_submit_context.SetIsSameDocument(false);
  form_submit_context.SetHasCommitted(true);
  form_submit_context.SetHasUserGesture(true);
  form_submit_context.SetIsPost(false);
  form_submit_context.SetPageTransition(ui::PAGE_TRANSITION_FORM_SUBMIT);
  manager_->DidFinishNavigation(&web_state_, &form_submit_context);

  EXPECT_TRUE(results_.empty());
  EXPECT_EQ(autofill_handler_.dismissSaveEntityDialogCount, 0);
  EXPECT_NE(manager_->GetSaveEntityPromptModel(), nullptr);
}

// Tests user-initiated committed cross-document navigation expires the prompt
// with 0 strikes and calls dismiss on commands handler.
TEST_F(AutofillPendingPromptManagerTest, UserNavigationExpiresPromptNeutrally) {
  manager_->ShowSaveEntityPrompt(CreateEntityModel());

  web::FakeNavigationContext user_nav_context;
  user_nav_context.SetIsSameDocument(false);
  user_nav_context.SetHasCommitted(true);
  user_nav_context.SetHasUserGesture(true);
  user_nav_context.SetPageTransition(ui::PAGE_TRANSITION_TYPED);

  manager_->DidFinishNavigation(&web_state_, &user_nav_context);

  EXPECT_EQ(std::vector{AutofillAiBubbleResult::kUnknown}, results_);
  EXPECT_EQ(autofill_handler_.dismissSaveEntityDialogCount, 1);
  EXPECT_EQ(manager_->GetSaveEntityPromptModel(), nullptr);
}

// Tests that a user-initiated navigation with a server redirect expires the
// prompt.
TEST_F(AutofillPendingPromptManagerTest,
       UserNavigationWithRedirectExpiresPrompt) {
  manager_->ShowSaveEntityPrompt(CreateEntityModel());

  web::FakeNavigationContext redirect_context;
  redirect_context.SetIsSameDocument(false);
  redirect_context.SetHasCommitted(true);
  redirect_context.SetHasUserGesture(true);
  redirect_context.SetPageTransition(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_SERVER_REDIRECT));

  manager_->DidFinishNavigation(&web_state_, &redirect_context);

  EXPECT_EQ(std::vector{AutofillAiBubbleResult::kUnknown}, results_);
  EXPECT_EQ(autofill_handler_.dismissSaveEntityDialogCount, 1);
  EXPECT_EQ(manager_->GetSaveEntityPromptModel(), nullptr);
}

// Tests that `WebStateDestroyed` expires pending offers neutrally.
TEST_F(AutofillPendingPromptManagerTest, WebStateDestroyedExpiresNeutrally) {
  manager_->ShowSaveEntityPrompt(CreateEntityModel());

  manager_->WebStateDestroyed(&web_state_);

  EXPECT_EQ(std::vector{AutofillAiBubbleResult::kUnknown}, results_);
  EXPECT_EQ(autofill_handler_.dismissSaveEntityDialogCount, 1);
  EXPECT_EQ(manager_->GetSaveEntityPromptModel(), nullptr);
}

// Tests that clearing the commands handler dismisses an active dialog.
TEST_F(AutofillPendingPromptManagerTest, ClearingHandlerDismissesActiveDialog) {
  manager_->ShowSaveEntityPrompt(CreateEntityModel());
  EXPECT_EQ(autofill_handler_.dismissSaveEntityDialogCount, 0);

  manager_->SetAutofillBottomSheetHandler(nil);
  EXPECT_EQ(autofill_handler_.dismissSaveEntityDialogCount, 1);
}

// Tests that replacing the commands handler with a different handler dismisses
// the active dialog on the previous handler.
TEST_F(AutofillPendingPromptManagerTest,
       ReplacingHandlerDismissesActiveDialogOnPreviousHandler) {
  manager_->ShowSaveEntityPrompt(CreateEntityModel());
  EXPECT_EQ(autofill_handler_.dismissSaveEntityDialogCount, 0);

  FakeAutofillCommandsForPromptManager* new_handler =
      [[FakeAutofillCommandsForPromptManager alloc] init];
  manager_->SetAutofillBottomSheetHandler((id<AutofillCommands>)new_handler);
  EXPECT_EQ(autofill_handler_.dismissSaveEntityDialogCount, 1);
  EXPECT_EQ(new_handler.dismissSaveEntityDialogCount, 0);
}

// Tests that destroying the manager directly dismisses an active dialog.
TEST_F(AutofillPendingPromptManagerTest, DestructorDismissesActiveDialog) {
  manager_->ShowSaveEntityPrompt(CreateEntityModel());
  EXPECT_EQ(autofill_handler_.dismissSaveEntityDialogCount, 0);

  manager_.reset();
  EXPECT_EQ(std::vector{AutofillAiBubbleResult::kUnknown}, results_);
  EXPECT_EQ(autofill_handler_.dismissSaveEntityDialogCount, 1);
}

// Tests that borrowing inspects the pending prompt model without consuming it,
// whereas taking transfers ownership and clears the pending prompt.
TEST_F(AutofillPendingPromptManagerTest,
       BorrowingRetainsModelWhileTakingConsumesModel) {
  manager_->ShowSaveEntityPrompt(CreateEntityModel());

  // Borrowing does not consume.
  EXPECT_NE(manager_->GetSaveEntityPromptModel(), nullptr);
  EXPECT_NE(manager_->GetSaveEntityPromptModel(), nullptr);

  // Taking consumes.
  std::unique_ptr<AutofillAiSaveEntityPromptModel> taken =
      manager_->TakeSaveEntityPromptModel();
  EXPECT_NE(taken, nullptr);
  EXPECT_EQ(manager_->GetSaveEntityPromptModel(), nullptr);
}

}  // namespace
