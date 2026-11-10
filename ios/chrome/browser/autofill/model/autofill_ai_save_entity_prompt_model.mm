// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_ai_save_entity_prompt_model.h"

#import <utility>

#import "base/check.h"

using autofill::AutofillClient;
using autofill::EntityInstance;
using autofill::SaveEntityParams;
using AutofillAiBubbleResult = autofill::AutofillClient::AutofillAiBubbleResult;

AutofillAiSaveEntityPromptModel::AutofillAiSaveEntityPromptModel(
    SaveEntityParams params)
    : params_(std::move(params)) {
  CHECK(!params_.callback.is_null());
}

AutofillAiSaveEntityPromptModel::~AutofillAiSaveEntityPromptModel() {
  // If destroyed without a decision, report kUnknown (0 strikes).
  OnExpired();
}

void AutofillAiSaveEntityPromptModel::OnAccepted(
    const AutofillClient::EntityImportUIContext& ui_context) {
  RunResultCallback(AutofillAiBubbleResult::kAccepted, std::nullopt,
                    ui_context);
}

void AutofillAiSaveEntityPromptModel::OnCanceled() {
  RunResultCallback(AutofillAiBubbleResult::kCancelled, std::nullopt, {});
}

void AutofillAiSaveEntityPromptModel::OnClosed() {
  RunResultCallback(AutofillAiBubbleResult::kClosed, std::nullopt, {});
}

void AutofillAiSaveEntityPromptModel::OnExpired() {
  RunResultCallback(AutofillAiBubbleResult::kUnknown, std::nullopt, {});
}

void AutofillAiSaveEntityPromptModel::RunResultCallback(
    AutofillAiBubbleResult result,
    std::optional<EntityInstance> edited_entity,
    const AutofillClient::EntityImportUIContext& ui_context) {
  if (params_.callback.is_null()) {
    return;
  }
  std::move(params_.callback).Run(result, std::move(edited_entity), ui_context);
}
