// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_AI_SAVE_ENTITY_PROMPT_MODEL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_AI_SAVE_ENTITY_PROMPT_MODEL_H_

#import <optional>

#import "base/compiler_specific.h"
#import "base/functional/callback.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/foundations/autofill_client.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/save_entity_params.h"

// Model for the Autofill AI Save or Update Entity prompt bottom sheet.
// Owns the SaveEntityParams and the single-use prompt result callback,
// ensuring exactly one result is reported through it.
//
// Lifecycle and ownership:
// One instance exists per active save/update entity offer on a WebState.
// Staged in AutofillBottomSheetTabHelper (via AutofillPendingPromptDelegate)
// while pending user interaction across tab switches, borrowed by
// AutofillAISaveEntityCoordinator and AutofillAISaveEntityMediator while the
// prompt is visible, and consumed (taken and deallocated) upon user decision,
// committed navigation, or WebState destruction.
//
// Result reporting:
// - `OnAccepted(...)`: `kAccepted`, clears strikes.
// - `OnCanceled()`: `kCancelled`, records 1 decline strike.
// - `OnClosed()`: `kClosed`, records 1 decline strike.
// - `OnExpired()` or destruction while pending: `kUnknown`, records 0 strikes.
class AutofillAiSaveEntityPromptModel final {
 public:
  explicit AutofillAiSaveEntityPromptModel(autofill::SaveEntityParams params);
  ~AutofillAiSaveEntityPromptModel();

  AutofillAiSaveEntityPromptModel(const AutofillAiSaveEntityPromptModel&) =
      delete;
  AutofillAiSaveEntityPromptModel& operator=(
      const AutofillAiSaveEntityPromptModel&) = delete;

  // Read-only access to parameters.
  const autofill::SaveEntityParams& params() const LIFETIME_BOUND {
    return params_;
  }

  // User interactions:
  // Accepts saving the entity as-is.
  // Note: The iOS bottom sheet prompt does not support editing entities inline,
  // so this always reports `kAccepted` with `std::nullopt` for the edited
  // entity.
  void OnAccepted(
      const autofill::AutofillClient::EntityImportUIContext& ui_context = {});
  void OnCanceled();
  void OnClosed();
  void OnExpired();

 private:
  void RunResultCallback(
      autofill::AutofillClient::AutofillAiBubbleResult result,
      std::optional<autofill::EntityInstance> edited_entity,
      const autofill::AutofillClient::EntityImportUIContext& ui_context);

  autofill::SaveEntityParams params_;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOFILL_AI_SAVE_ENTITY_PROMPT_MODEL_H_
