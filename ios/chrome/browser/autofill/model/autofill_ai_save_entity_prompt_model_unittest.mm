// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autofill_ai_save_entity_prompt_model.h"

#import <memory>
#import <optional>
#import <vector>

#import "base/test/bind.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using autofill::AutofillClient;
using autofill::EntityInstance;
using autofill::SaveEntityParams;
using AutofillAiBubbleResult = autofill::AutofillClient::AutofillAiBubbleResult;

class AutofillAiSaveEntityPromptModelTest : public PlatformTest {
 protected:
  std::unique_ptr<AutofillAiSaveEntityPromptModel> CreateModel() {
    return std::make_unique<AutofillAiSaveEntityPromptModel>(SaveEntityParams(
        autofill::test::GetPassportEntityInstance(),
        /*old_entity=*/std::nullopt,
        /*user_email=*/u"test@example.com",
        /*save_is_synchronous=*/true,
        base::BindLambdaForTesting(
            [this](AutofillAiBubbleResult result,
                   std::optional<EntityInstance> edited_entity,
                   const AutofillClient::EntityImportUIContext& ui_context) {
              results_.push_back(result);
              last_edited_entity_ = std::move(edited_entity);
            }),
        /*public_passes_notice=*/{},
        /*show_explicit_decline_button=*/false));
  }

  std::vector<AutofillAiBubbleResult> results_;
  std::optional<EntityInstance> last_edited_entity_;
};

// Tests that accepting reports `kAccepted` and subsequent calls are no-ops.
TEST_F(AutofillAiSaveEntityPromptModelTest, AcceptReportsAccepted) {
  auto model = CreateModel();

  model->OnAccepted();

  // Subsequent events should be no-ops.
  model->OnCanceled();
  model->OnExpired();
  model.reset();

  EXPECT_EQ(std::vector{AutofillAiBubbleResult::kAccepted}, results_);
  EXPECT_FALSE(last_edited_entity_.has_value());
}

// Tests that cancelling reports `kCancelled`.
TEST_F(AutofillAiSaveEntityPromptModelTest, CancelReportsCancelled) {
  auto model = CreateModel();

  model->OnCanceled();

  EXPECT_EQ(std::vector{AutofillAiBubbleResult::kCancelled}, results_);
}

// Tests that closing reports `kClosed`.
TEST_F(AutofillAiSaveEntityPromptModelTest, CloseReportsClosed) {
  auto model = CreateModel();

  model->OnClosed();

  EXPECT_EQ(std::vector{AutofillAiBubbleResult::kClosed}, results_);
}

// Tests that calling OnExpired reports `kUnknown` (0 strikes).
TEST_F(AutofillAiSaveEntityPromptModelTest, OnExpiredReportsUnknown) {
  auto model = CreateModel();

  model->OnExpired();

  EXPECT_EQ(std::vector{AutofillAiBubbleResult::kUnknown}, results_);
}

// Tests that destroying a pending model without a user decision reports
// `kUnknown` (0 strikes).
TEST_F(AutofillAiSaveEntityPromptModelTest, DestroyReportsUnknown) {
  CreateModel().reset();

  EXPECT_EQ(std::vector{AutofillAiBubbleResult::kUnknown}, results_);
}

// Tests that accepting forwards the provided UI context.
TEST_F(AutofillAiSaveEntityPromptModelTest, AcceptForwardsUIContext) {
  AutofillClient::EntityImportUIContext captured_context;
  auto model =
      std::make_unique<AutofillAiSaveEntityPromptModel>(SaveEntityParams(
          autofill::test::GetPassportEntityInstance(),
          /*old_entity=*/std::nullopt,
          /*user_email=*/u"test@example.com",
          /*save_is_synchronous=*/true,
          base::BindLambdaForTesting(
              [&](AutofillAiBubbleResult result,
                  std::optional<EntityInstance> edited_entity,
                  const AutofillClient::EntityImportUIContext& ui_context) {
                results_.push_back(result);
                captured_context = ui_context;
              }),
          /*public_passes_notice=*/{},
          /*show_explicit_decline_button=*/false));

  AutofillClient::EntityImportUIContext test_context;
  test_context.accepted_consent_string_ids = {10};
  test_context.accept_button_string_id = 20;
  model->OnAccepted(test_context);

  EXPECT_EQ(std::vector{AutofillAiBubbleResult::kAccepted}, results_);
  EXPECT_EQ(test_context.accepted_consent_string_ids,
            captured_context.accepted_consent_string_ids);
  EXPECT_EQ(test_context.accept_button_string_id,
            captured_context.accept_button_string_id);
}
