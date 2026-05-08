// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/autofill_data_extraction_utils.h"

#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/test_autofill_client_ios.h"
#import "components/autofill/ios/browser/test_autofill_manager_injector.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using optimization_guide::proto::FormControlData;
using optimization_guide::proto::RedactionDecision;

// Test fixture.
using AutofillDataExtractionUtilsTest = PlatformTest;

// Tests that credit card fields are correctly identified as needing redaction.
TEST_F(AutofillDataExtractionUtilsTest, GetRedactionReason_CreditCardFields) {
  autofill::FieldTypeSet field_types;
  field_types.insert(autofill::CREDIT_CARD_NUMBER);

  EXPECT_EQ(GetRedactionReason(field_types),
            AutofillFieldRedactionReason::kShouldRedactForPayments);

  field_types.clear();
  field_types.insert(autofill::CREDIT_CARD_VERIFICATION_CODE);

  EXPECT_EQ(GetRedactionReason(field_types),
            AutofillFieldRedactionReason::kShouldRedactForPayments);

  // Redaction should apply even if mixed with a safe field like name.
  field_types.clear();
  field_types.insert(autofill::NAME_FIRST);
  field_types.insert(autofill::CREDIT_CARD_NUMBER);

  EXPECT_EQ(GetRedactionReason(field_types),
            AutofillFieldRedactionReason::kShouldRedactForPayments);
}

// Tests that normal fields don't require redaction.
TEST_F(AutofillDataExtractionUtilsTest, GetRedactionReason_NormalFields) {
  autofill::FieldTypeSet field_types;
  field_types.insert(autofill::NAME_FULL);
  field_types.insert(autofill::ADDRESS_HOME_LINE1);
  field_types.insert(autofill::EMAIL_ADDRESS);
  field_types.insert(autofill::FLIGHT_RESERVATION_FLIGHT_NUMBER);
  field_types.insert(autofill::ORDER_ID);

  EXPECT_EQ(GetRedactionReason(field_types),
            AutofillFieldRedactionReason::kNoRedactionNeeded);
}

// Tests the conversion from Autofill's redaction reason to APC proto's.
TEST_F(AutofillDataExtractionUtilsTest, ConvertAutofillFieldRedactionReason) {
  FormControlData empty_form_data;
  FormControlData populated_form_data;
  populated_form_data.set_field_value("some value");

  // No redaction.
  EXPECT_EQ(
      ConvertAutofillFieldRedactionReason(
          empty_form_data, AutofillFieldRedactionReason::kNoRedactionNeeded),
      optimization_guide::proto::REDACTION_DECISION_NO_REDACTION_NECESSARY);

  // Redaction on an empty field.
  EXPECT_EQ(ConvertAutofillFieldRedactionReason(
                empty_form_data,
                AutofillFieldRedactionReason::kShouldRedactForPayments),
            optimization_guide::proto::
                REDACTION_DECISION_UNREDACTED_EMPTY_PAYMENT_FIELD);

  // Redaction on a filled field.
  EXPECT_EQ(ConvertAutofillFieldRedactionReason(
                populated_form_data,
                AutofillFieldRedactionReason::kShouldRedactForPayments),
            optimization_guide::proto::
                REDACTION_DECISION_REDACTED_IS_SENSITIVE_PAYMENT_FIELD);
}

// Tests that ShouldRedactContent matches the expected conditions.
TEST_F(AutofillDataExtractionUtilsTest, ShouldRedactContent) {
  AutofillExtractionContext context;
  context.extract_autofill_credit_card_redactions = true;

  // Test that redaction is correctly applied when enabled.
  // Where no redaction is needed.
  EXPECT_FALSE(ShouldRedactContent(
      optimization_guide::proto::REDACTION_DECISION_NO_REDACTION_NECESSARY,
      context));
  EXPECT_FALSE(ShouldRedactContent(
      optimization_guide::proto::REDACTION_DECISION_UNREDACTED_EMPTY_PASSWORD,
      context));
  EXPECT_FALSE(
      ShouldRedactContent(optimization_guide::proto::
                              REDACTION_DECISION_UNREDACTED_EMPTY_PAYMENT_FIELD,
                          context));
  // Where redaction is needed.
  EXPECT_TRUE(ShouldRedactContent(
      optimization_guide::proto::REDACTION_DECISION_REDACTED_HAS_BEEN_PASSWORD,
      context));
  EXPECT_TRUE(ShouldRedactContent(
      optimization_guide::proto::
          REDACTION_DECISION_REDACTED_IS_SENSITIVE_PAYMENT_FIELD,
      context));

  // Test that redaction is not applied when disabled.
  context.extract_autofill_credit_card_redactions = false;
  EXPECT_FALSE(ShouldRedactContent(
      optimization_guide::proto::
          REDACTION_DECISION_REDACTED_IS_SENSITIVE_PAYMENT_FIELD,
      context));
}

// Tests GetAutofillFieldData returns nullopt when context is missing web_state
// or frame_token.
TEST_F(AutofillDataExtractionUtilsTest, GetAutofillFieldData_MissingContext) {
  AutofillExtractionContext context;
  EXPECT_FALSE(GetAutofillFieldData(1, context));

  web::FakeWebState web_state;
  context.web_state = web_state.GetWeakPtr();
  EXPECT_FALSE(GetAutofillFieldData(1, context));

  context.web_state = nullptr;
  context.frame_token = autofill::LocalFrameToken();
  EXPECT_FALSE(GetAutofillFieldData(1, context));
}

// Tests GetAutofillFieldData returns nullopt when no AutofillDriver exists.
TEST_F(AutofillDataExtractionUtilsTest, GetAutofillFieldData_NoDriver) {
  web::FakeWebState web_state;
  auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
  web_state.SetWebFramesManager(
      autofill::AutofillJavaScriptFeature::GetInstance()
          ->GetSupportedContentWorld(),
      std::move(frames_manager));

  AutofillExtractionContext context;
  context.web_state = web_state.GetWeakPtr();
  context.frame_token = autofill::LocalFrameToken();
  EXPECT_FALSE(GetAutofillFieldData(1, context));
}

// Tests GetAutofillFieldData returns valid data when a form is cached.
TEST_F(AutofillDataExtractionUtilsTest, GetAutofillFieldData_Valid) {
  web::WebTaskEnvironment task_environment;
  web::FakeWebState web_state;
  auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
  auto* frames_manager_ptr = frames_manager.get();
  web_state.SetWebFramesManager(
      autofill::AutofillJavaScriptFeature::GetInstance()
          ->GetSupportedContentWorld(),
      std::move(frames_manager));

  autofill::TestAutofillClientIOS autofill_client(&web_state, nil);

  autofill::TestAutofillManagerInjector<autofill::TestBrowserAutofillManager>
      injector(&web_state);

  autofill::LocalFrameToken frame_token;
  auto frame = web::FakeWebFrame::Create(frame_token.ToString(), true,
                                         GURL("https://example.com"));
  auto* frame_ptr = frame.get();
  frames_manager_ptr->AddWebFrame(std::move(frame));

  autofill::AutofillDriverIOS* driver =
      autofill_client.GetAutofillDriverFactory().DriverForFrame(frame_ptr);
  autofill::TestBrowserAutofillManager* manager =
      static_cast<autofill::TestBrowserAutofillManager*>(
          &driver->GetAutofillManager());

  // Setup form and field.
  autofill::FormData form_data;
  form_data.set_host_frame(frame_token);
  autofill::FormFieldData field_data;
  field_data.set_host_frame(frame_token);
  field_data.set_renderer_id(autofill::FieldRendererId(10));
  form_data.set_fields({field_data});

  manager->AddSeenForm(form_data, {autofill::CREDIT_CARD_NUMBER});

  base::flat_map<std::string, uint32_t> section_numbers;
  AutofillExtractionContext context(
      web_state.GetWeakPtr(), frame_token,
      /*extract_autofill_credit_card_redactions=*/false, &section_numbers);

  std::optional<AutofillFieldMetadata> metadata =
      GetAutofillFieldData(10, context);
  ASSERT_TRUE(metadata);
  EXPECT_EQ(metadata->coarse_field_type,
            optimization_guide::proto::COARSE_AUTOFILL_FIELD_TYPE_CREDIT_CARD);
  EXPECT_EQ(metadata->redaction_reason,
            AutofillFieldRedactionReason::kShouldRedactForPayments);
}
