// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_AUTOFILL_PENDING_PROMPT_MANAGER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_AUTOFILL_PENDING_PROMPT_MANAGER_H_

#import <Foundation/Foundation.h>

#import <memory>

#import "base/memory/raw_ptr.h"

class AutofillAiSaveEntityPromptModel;
@protocol AutofillCommands;

namespace web {
class NavigationContext;
class WebState;
}  // namespace web

// Coordinates the lifecycle of staged, pending autofill prompts (such as
// Autofill AI save entity) for a specific WebState.
//
// Lifecycle and ownership:
// - Exactly one instance per WebState.
// - Owned and instantiated by `AutofillBottomSheetTabHelper` during its
//   creation for a WebState.
// - Receives forwarded WebState lifecycle events (`WasShown`,
//   `DidFinishNavigation`, `WebStateDestroyed`) from
//   `AutofillBottomSheetTabHelper`.
//
// Ensures prompts survive tab switches, and are neutrally expired (0 strikes)
// when cross-document user-initiated navigations commit or when the WebState is
// destroyed.
class AutofillPendingPromptManager {
 public:
  AutofillPendingPromptManager(web::WebState* web_state,
                               id<AutofillCommands> autofill_handler);
  AutofillPendingPromptManager(const AutofillPendingPromptManager&) = delete;
  AutofillPendingPromptManager& operator=(const AutofillPendingPromptManager&) =
      delete;
  ~AutofillPendingPromptManager();

  // Tab lifecycle event forwarding:
  void WasShown(web::WebState* web_state);
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context);
  void WebStateDestroyed(web::WebState* web_state);

  // Autofill AI Save Entity prompt management:
  void ShowSaveEntityPrompt(
      std::unique_ptr<AutofillAiSaveEntityPromptModel> model);
  const AutofillAiSaveEntityPromptModel* GetSaveEntityPromptModel() const;
  AutofillAiSaveEntityPromptModel* GetSaveEntityPromptModel();
  std::unique_ptr<AutofillAiSaveEntityPromptModel> TakeSaveEntityPromptModel();

  void SetAutofillBottomSheetHandler(id<AutofillCommands> autofill_handler);

 private:
  // Neutrally expires any active prompt offer (0 strikes) and commands the UI
  // layer to dismiss the active bottom sheet dialog.
  void ExpirePendingPromptAndDismissDialog();

  // The WebState this prompt manager is attached to. Managed by the tab helper
  // and expected to outlive this instance until `WebStateDestroyed`.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // Handler for dispatching autofill commands. Held weakly to avoid retain
  // cycles; may be nil if unconfigured or after teardown.
  __weak id<AutofillCommands> autofill_handler_;

  // Owned prompt model for the active Autofill AI save entity offer. Held while
  // the prompt is staged across tab switches, and reset when accepted, expired,
  // or cleared.
  std::unique_ptr<AutofillAiSaveEntityPromptModel> save_entity_model_;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_AUTOFILL_PENDING_PROMPT_MANAGER_H_
