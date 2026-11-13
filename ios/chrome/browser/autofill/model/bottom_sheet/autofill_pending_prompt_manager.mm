// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_pending_prompt_manager.h"

#import <utility>

#import "base/check.h"
#import "base/check_op.h"
#import "base/feature_list.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_save_entity_prompt_model.h"
#import "ios/chrome/browser/autofill/model/features.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "ui/base/page_transition_types.h"

AutofillPendingPromptManager::AutofillPendingPromptManager(
    web::WebState* web_state,
    id<AutofillCommands> autofill_handler)
    : web_state_(web_state), autofill_handler_(autofill_handler) {
  CHECK(web_state_);
}

AutofillPendingPromptManager::~AutofillPendingPromptManager() {
  ExpirePendingPromptAndDismissDialog();
}

void AutofillPendingPromptManager::WasShown(web::WebState* web_state) {
  CHECK_EQ(web_state, web_state_);
  if (!base::FeatureList::IsEnabled(kAutofillReshowSavePromptOnTabReturn)) {
    return;
  }
  if (autofill_handler_ && save_entity_model_) {
    [autofill_handler_ showSaveEntityDialogOnWebState:web_state_];
  }
}

void AutofillPendingPromptManager::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  CHECK_EQ(web_state, web_state_);
  CHECK(navigation_context);

  // 1. Same-document navigations and uncommitted navigations do not expire the
  // offer.
  if (navigation_context->IsSameDocument() ||
      !navigation_context->HasCommitted()) {
    return;
  }

  // 2. Do not expire if the navigation was triggered by the form submission
  // itself (POST or form submit transition) or occurred without an explicit
  // user gesture (e.g., in-flight post-submit redirects to /order-received
  // committing while the user reads legal disclosures).
  if (navigation_context->IsPost() ||
      ui::PageTransitionCoreTypeIs(navigation_context->GetPageTransition(),
                                   ui::PAGE_TRANSITION_FORM_SUBMIT) ||
      !navigation_context->HasUserGesture()) {
    return;
  }

  // 3. User explicitly navigated away (e.g., Omnibox input, bookmark, link
  // click). In fact, when the bottom sheet is shown, it is hard to navigate the
  // page directly as modal interactions are blocked, but the user can still
  // navigate via browser chrome (e.g., Omnibox, bookmarks). Expire pending
  // offers neutrally with 0 strikes and dismiss active prompt UI.
  ExpirePendingPromptAndDismissDialog();
}

void AutofillPendingPromptManager::WebStateDestroyed(web::WebState* web_state) {
  CHECK_EQ(web_state, web_state_);
  ExpirePendingPromptAndDismissDialog();
  web_state_ = nullptr;
}

void AutofillPendingPromptManager::ShowSaveEntityPrompt(
    std::unique_ptr<AutofillAiSaveEntityPromptModel> model) {
  CHECK(model);
  if (save_entity_model_) {
    save_entity_model_->OnExpired();
  }
  save_entity_model_ = std::move(model);
  if (autofill_handler_) {
    [autofill_handler_ showSaveEntityDialogOnWebState:web_state_];
  }
}

const AutofillAiSaveEntityPromptModel*
AutofillPendingPromptManager::GetSaveEntityPromptModel() const {
  return save_entity_model_.get();
}

AutofillAiSaveEntityPromptModel*
AutofillPendingPromptManager::GetSaveEntityPromptModel() {
  return save_entity_model_.get();
}

std::unique_ptr<AutofillAiSaveEntityPromptModel>
AutofillPendingPromptManager::TakeSaveEntityPromptModel() {
  return std::move(save_entity_model_);
}

void AutofillPendingPromptManager::SetAutofillBottomSheetHandler(
    id<AutofillCommands> autofill_handler) {
  if (autofill_handler_ && autofill_handler_ != autofill_handler &&
      save_entity_model_) {
    [autofill_handler_ dismissSaveEntityDialog];
  }
  autofill_handler_ = autofill_handler;
}

void AutofillPendingPromptManager::ExpirePendingPromptAndDismissDialog() {
  std::unique_ptr<AutofillAiSaveEntityPromptModel> model =
      std::move(save_entity_model_);
  if (model) {
    model->OnExpired();
    if (autofill_handler_) {
      [autofill_handler_ dismissSaveEntityDialog];
    }
  }
}
