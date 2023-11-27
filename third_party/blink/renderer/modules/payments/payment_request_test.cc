// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_request.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/mock_function_scope.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {

TEST(PaymentRequestTest, NoExceptionWithValidData) {
  PaymentRequestV8TestingScope scope;
  PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST(PaymentRequestTest, SupportedMethodListRequired) {
  PaymentRequestV8TestingScope scope;
  PaymentRequest::Create(
      scope.GetExecutionContext(), HeapVector<Member<PaymentMethodData>>(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
}

TEST(PaymentRequestTest, NullShippingOptionWhenNoOptionsAvailable) {
  PaymentRequestV8TestingScope scope;
  PaymentDetailsInit* details = PaymentDetailsInit::Create();
  details->setTotal(BuildPaymentItemForTest());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);

  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(), details,
      options, scope.GetExceptionState());

  EXPECT_TRUE(request->shippingOption().IsNull());
}

TEST(PaymentRequestTest, NullShippingOptionWhenMultipleOptionsAvailable) {
  PaymentRequestV8TestingScope scope;
  PaymentDetailsInit* details = PaymentDetailsInit::Create();
  details->setTotal(BuildPaymentItemForTest());
  HeapVector<Member<PaymentShippingOption>> shipping_options;
  shipping_options.push_back(BuildShippingOptionForTest());
  shipping_options.push_back(BuildShippingOptionForTest());
  details->setShippingOptions(shipping_options);
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);

  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(), details,
      options, scope.GetExceptionState());

  EXPECT_TRUE(request->shippingOption().IsNull());
}

TEST(PaymentRequestTest, DontSelectSingleAvailableShippingOptionByDefault) {
  PaymentRequestV8TestingScope scope;
  PaymentDetailsInit* details = PaymentDetailsInit::Create();
  details->setTotal(BuildPaymentItemForTest());
  details->setShippingOptions(HeapVector<Member<PaymentShippingOption>>(
      1, BuildShippingOptionForTest(kPaymentTestDataId,
                                    kPaymentTestOverwriteValue, "standard")));

  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(), details,
      scope.GetExceptionState());

  EXPECT_TRUE(request->shippingOption().IsNull());
}

TEST(PaymentRequestTest,
     DontSelectSingleAvailableShippingOptionWhenShippingNotRequested) {
  PaymentRequestV8TestingScope scope;
  PaymentDetailsInit* details = PaymentDetailsInit::Create();
  details->setTotal(BuildPaymentItemForTest());
  details->setShippingOptions(HeapVector<Member<PaymentShippingOption>>(
      1, BuildShippingOptionForTest()));
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(false);

  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(), details,
      options, scope.GetExceptionState());

  EXPECT_TRUE(request->shippingOption().IsNull());
}

TEST(PaymentRequestTest,
     DontSelectSingleUnselectedShippingOptionWhenShippingRequested) {
  PaymentRequestV8TestingScope scope;
  PaymentDetailsInit* details = PaymentDetailsInit::Create();
  details->setTotal(BuildPaymentItemForTest());
  details->setShippingOptions(HeapVector<Member<PaymentShippingOption>>(
      1, BuildShippingOptionForTest()));
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);

  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(), details,
      options, scope.GetExceptionState());

  EXPECT_TRUE(request->shippingOption().IsNull());
}

TEST(PaymentRequestTest,
     SelectSingleSelectedShippingOptionWhenShippingRequested) {
  PaymentRequestV8TestingScope scope;
  PaymentDetailsInit* details = PaymentDetailsInit::Create();
  details->setTotal(BuildPaymentItemForTest());
  HeapVector<Member<PaymentShippingOption>> shipping_options(
      1, BuildShippingOptionForTest(kPaymentTestDataId,
                                    kPaymentTestOverwriteValue, "standard"));
  shipping_options[0]->setSelected(true);
  details->setShippingOptions(shipping_options);
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);

  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(), details,
      options, scope.GetExceptionState());

  EXPECT_EQ("standard", request->shippingOption());
}

TEST(PaymentRequestTest,
     SelectOnlySelectedShippingOptionWhenShippingRequested) {
  PaymentRequestV8TestingScope scope;
  PaymentDetailsInit* details = PaymentDetailsInit::Create();
  details->setTotal(BuildPaymentItemForTest());
  HeapVector<Member<PaymentShippingOption>> shipping_options(2);
  shipping_options[0] = BuildShippingOptionForTest(
      kPaymentTestDataId, kPaymentTestOverwriteValue, "standard");
  shipping_options[0]->setSelected(true);
  shipping_options[1] = BuildShippingOptionForTest(
      kPaymentTestDataId, kPaymentTestOverwriteValue, "express");
  details->setShippingOptions(shipping_options);
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);

  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(), details,
      options, scope.GetExceptionState());

  EXPECT_EQ("standard", request->shippingOption());
}

TEST(PaymentRequestTest,
     SelectLastSelectedShippingOptionWhenShippingRequested) {
  PaymentRequestV8TestingScope scope;
  PaymentDetailsInit* details = PaymentDetailsInit::Create();
  details->setTotal(BuildPaymentItemForTest());
  HeapVector<Member<PaymentShippingOption>> shipping_options(2);
  shipping_options[0] = BuildShippingOptionForTest(
      kPaymentTestDataId, kPaymentTestOverwriteValue, "standard");
  shipping_options[0]->setSelected(true);
  shipping_options[1] = BuildShippingOptionForTest(
      kPaymentTestDataId, kPaymentTestOverwriteValue, "express");
  shipping_options[1]->setSelected(true);
  details->setShippingOptions(shipping_options);
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);

  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(), details,
      options, scope.GetExceptionState());

  EXPECT_EQ("express", request->shippingOption());
}

TEST(PaymentRequestTest, NullShippingTypeWhenRequestShippingIsFalse) {
  PaymentRequestV8TestingScope scope;
  PaymentDetailsInit* details = PaymentDetailsInit::Create();
  details->setTotal(BuildPaymentItemForTest());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(false);

  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(), details,
      options, scope.GetExceptionState());

  EXPECT_TRUE(request->shippingType().IsNull());
}

TEST(PaymentRequestTest,
     DefaultShippingTypeWhenRequestShippingIsTrueWithNoSpecificType) {
  PaymentRequestV8TestingScope scope;
  PaymentDetailsInit* details = PaymentDetailsInit::Create();
  details->setTotal(BuildPaymentItemForTest());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);

  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(), details,
      options, scope.GetExceptionState());

  EXPECT_EQ("shipping", request->shippingType());
}

TEST(PaymentRequestTest, DeliveryShippingTypeWhenShippingTypeIsDelivery) {
  PaymentRequestV8TestingScope scope;
  PaymentDetailsInit* details = PaymentDetailsInit::Create();
  details->setTotal(BuildPaymentItemForTest());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);
  options->setShippingType("delivery");

  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(), details,
      options, scope.GetExceptionState());

  EXPECT_EQ("delivery", request->shippingType());
}

TEST(PaymentRequestTest, PickupShippingTypeWhenShippingTypeIsPickup) {
  PaymentRequestV8TestingScope scope;
  PaymentDetailsInit* details = PaymentDetailsInit::Create();
  details->setTotal(BuildPaymentItemForTest());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);
  options->setShippingType("pickup");

  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(), details,
      options, scope.GetExceptionState());

  EXPECT_EQ("pickup", request->shippingType());
}

TEST(PaymentRequestTest, RejectShowPromiseOnInvalidShippingAddress) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnShippingAddressChange(payments::mojom::blink::PaymentAddress::New());
}

TEST(PaymentRequestTest, OnShippingOptionChange) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectNoCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnShippingOptionChange("standardShipping");
}

TEST(PaymentRequestTest, CannotCallShowTwice) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);
  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION);

  // The second show() call will be rejected before user activation is checked,
  // so there is no need to re-trigger user-activation here.
  request->show(scope.GetScriptState(), scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().Code(),
            ToExceptionCode(DOMExceptionCode::kInvalidStateError));
}

TEST(PaymentRequestTest, CannotShowAfterAborted) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  request->abort(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)->OnAbort(
      true);

  // The second show() call will be rejected before user activation is checked,
  // so there is no need to re-trigger user-activation here.
  request->show(scope.GetScriptState(), scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().Code(),
            ToExceptionCode(DOMExceptionCode::kInvalidStateError));
  ;
}

TEST(PaymentRequestTest, CannotShowWithoutUserActivation) {
  ScopedPaymentRequestAllowOneActivationlessShowForTest
      scoped_activationless_show_enabled(false);
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  EXPECT_FALSE(scope.GetDocument().IsUseCounted(
      WebFeature::kPaymentRequestShowWithoutGestureOrToken));
  request->show(scope.GetScriptState(), scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().Code(),
            ToExceptionCode(DOMExceptionCode::kSecurityError));
  EXPECT_TRUE(scope.GetDocument().IsUseCounted(
      WebFeature::kPaymentRequestShowWithoutGestureOrToken));
  EXPECT_FALSE(scope.GetDocument().IsUseCounted(
      WebFeature::kPaymentRequestActivationlessShow));
}

TEST(PaymentRequestTest, ShowConsumesUserActivation) {
  ScopedPaymentRequestAllowOneActivationlessShowForTest
      scoped_activationless_show_enabled(false);
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &(scope.GetFrame()), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectNoCall());
  EXPECT_FALSE(LocalFrame::HasTransientUserActivation(&(scope.GetFrame())));
  EXPECT_FALSE(scope.GetDocument().IsUseCounted(
      WebFeature::kPaymentRequestShowWithoutGestureOrToken));
}

TEST(PaymentRequestTest, PaymentRequestActivationlessShowEnabled) {
  ScopedPaymentRequestAllowOneActivationlessShowForTest
      scoped_activationless_show_enabled(true);
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  EXPECT_FALSE(LocalFrame::HasTransientUserActivation(&(scope.GetFrame())));
  EXPECT_FALSE(scope.GetDocument().IsUseCounted(
      WebFeature::kPaymentRequestActivationlessShow));
  EXPECT_FALSE(scope.GetDocument().IsUseCounted(
      WebFeature::kPaymentRequestShowWithoutGestureOrToken));

  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectNoCall());
  EXPECT_TRUE(scope.GetDocument().IsUseCounted(
      WebFeature::kPaymentRequestActivationlessShow));
  EXPECT_TRUE(scope.GetDocument().IsUseCounted(
      WebFeature::kPaymentRequestShowWithoutGestureOrToken));
}

TEST(PaymentRequestTest, RejectShowPromiseOnErrorPaymentMethodNotSupported) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  String error_message;
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall(&error_message));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)->OnError(
      payments::mojom::blink::PaymentErrorReason::NOT_SUPPORTED,
      "The payment method \"foo\" is not supported");

  scope.PerformMicrotaskCheckpoint();
  EXPECT_EQ("NotSupportedError: The payment method \"foo\" is not supported",
            error_message);
}

TEST(PaymentRequestTest, RejectShowPromiseOnErrorCancelled) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  String error_message;
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall(&error_message));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)->OnError(
      payments::mojom::blink::PaymentErrorReason::USER_CANCEL,
      "Request cancelled");

  scope.PerformMicrotaskCheckpoint();
  EXPECT_EQ("AbortError: Request cancelled", error_message);
}

TEST(PaymentRequestTest, RejectShowPromiseOnUpdateDetailsFailure) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  String error_message;
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall(&error_message));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnShippingAddressChange(BuildPaymentAddressForTest());
  request->OnUpdatePaymentDetailsFailure("oops");

  scope.PerformMicrotaskCheckpoint();
  EXPECT_EQ("AbortError: oops", error_message);
}

TEST(PaymentRequestTest, IgnoreUpdatePaymentDetailsAfterShowPromiseResolved) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectCall(), funcs.ExpectNoCall());
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentResponse(BuildPaymentResponseForTest());

  request->OnUpdatePaymentDetails(
      ScriptValue::From(scope.GetScriptState(), "foo"));
}

TEST(PaymentRequestTest, RejectShowPromiseOnNonPaymentDetailsUpdate) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnShippingAddressChange(BuildPaymentAddressForTest());
  request->OnUpdatePaymentDetails(
      ScriptValue::From(scope.GetScriptState(), "NotPaymentDetails"));
}

TEST(PaymentRequestTest, RejectShowPromiseOnInvalidPaymentDetailsUpdate) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnShippingAddressChange(BuildPaymentAddressForTest());
  request->OnUpdatePaymentDetails(ScriptValue::From(
      scope.GetScriptState(),
      FromJSONString(scope.GetScriptState()->GetIsolate(),
                     scope.GetScriptState()->GetContext(), "{\"total\": {}}",
                     ASSERT_NO_EXCEPTION)));
}

TEST(PaymentRequestTest,
     ClearShippingOptionOnPaymentDetailsUpdateWithoutShippingOptions) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentDetailsInit* details = PaymentDetailsInit::Create();
  details->setTotal(BuildPaymentItemForTest());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(), details,
      options, ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(request->shippingOption().IsNull());

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectNoCall());

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnShippingAddressChange(BuildPaymentAddressForTest());
  String detail_with_shipping_options =
      "{\"total\": {\"label\": \"Total\", \"amount\": {\"currency\": \"USD\", "
      "\"value\": \"5.00\"}},"
      "\"shippingOptions\": [{\"id\": \"standardShippingOption\", \"label\": "
      "\"Standard shipping\", \"amount\": {\"currency\": \"USD\", \"value\": "
      "\"5.00\"}, \"selected\": true}]}";
  request->OnUpdatePaymentDetails(ScriptValue::From(
      scope.GetScriptState(),
      FromJSONString(scope.GetScriptState()->GetIsolate(),
                     scope.GetScriptState()->GetContext(),
                     detail_with_shipping_options, ASSERT_NO_EXCEPTION)));
  EXPECT_EQ("standardShippingOption", request->shippingOption());
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnShippingAddressChange(BuildPaymentAddressForTest());
  String detail_without_shipping_options =
      "{\"total\": {\"label\": \"Total\", \"amount\": {\"currency\": \"USD\", "
      "\"value\": \"5.00\"}}}";

  request->OnUpdatePaymentDetails(ScriptValue::From(
      scope.GetScriptState(),
      FromJSONString(scope.GetScriptState()->GetIsolate(),
                     scope.GetScriptState()->GetContext(),
                     detail_without_shipping_options, ASSERT_NO_EXCEPTION)));

  EXPECT_TRUE(request->shippingOption().IsNull());
}

TEST(
    PaymentRequestTest,
    ClearShippingOptionOnPaymentDetailsUpdateWithMultipleUnselectedShippingOptions) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectNoCall());

  String detail =
      "{\"total\": {\"label\": \"Total\", \"amount\": {\"currency\": \"USD\", "
      "\"value\": \"5.00\"}},"
      "\"shippingOptions\": [{\"id\": \"slow\", \"label\": \"Slow\", "
      "\"amount\": {\"currency\": \"USD\", \"value\": \"5.00\"}},"
      "{\"id\": \"fast\", \"label\": \"Fast\", \"amount\": {\"currency\": "
      "\"USD\", \"value\": \"50.00\"}}]}";

  request->OnUpdatePaymentDetails(
      ScriptValue::From(scope.GetScriptState(),
                        FromJSONString(scope.GetScriptState()->GetIsolate(),
                                       scope.GetScriptState()->GetContext(),
                                       detail, ASSERT_NO_EXCEPTION)));

  EXPECT_TRUE(request->shippingOption().IsNull());
}

TEST(PaymentRequestTest, UseTheSelectedShippingOptionFromPaymentDetailsUpdate) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), options, ASSERT_NO_EXCEPTION);
  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectNoCall());
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnShippingAddressChange(BuildPaymentAddressForTest());

  String detail =
      "{\"total\": {\"label\": \"Total\", \"amount\": {\"currency\": \"USD\", "
      "\"value\": \"5.00\"}},"
      "\"shippingOptions\": [{\"id\": \"slow\", \"label\": \"Slow\", "
      "\"amount\": {\"currency\": \"USD\", \"value\": \"5.00\"}},"
      "{\"id\": \"fast\", \"label\": \"Fast\", \"amount\": {\"currency\": "
      "\"USD\", \"value\": \"50.00\"}, \"selected\": true}]}";

  request->OnUpdatePaymentDetails(
      ScriptValue::From(scope.GetScriptState(),
                        FromJSONString(scope.GetScriptState()->GetIsolate(),
                                       scope.GetScriptState()->GetContext(),
                                       detail, ASSERT_NO_EXCEPTION)));

  EXPECT_EQ("fast", request->shippingOption());
}

TEST(PaymentRequestTest, NoExceptionWithErrorMessageInUpdate) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectNoCall());
  String detail_with_error_msg =
      "{\"total\": {\"label\": \"Total\", \"amount\": {\"currency\": \"USD\", "
      "\"value\": \"5.00\"}},"
      "\"error\": \"This is an error message.\"}";

  request->OnUpdatePaymentDetails(ScriptValue::From(
      scope.GetScriptState(),
      FromJSONString(scope.GetScriptState()->GetIsolate(),
                     scope.GetScriptState()->GetContext(),
                     detail_with_error_msg, ASSERT_NO_EXCEPTION)));
}

TEST(PaymentRequestTest,
     ShouldResolveWithExceptionIfIDsOfShippingOptionsAreDuplicated) {
  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentDetailsInit* details = PaymentDetailsInit::Create();
  details->setTotal(BuildPaymentItemForTest());
  HeapVector<Member<PaymentShippingOption>> shipping_options(2);
  shipping_options[0] = BuildShippingOptionForTest(
      kPaymentTestDataId, kPaymentTestOverwriteValue, "standard");
  shipping_options[0]->setSelected(true);
  shipping_options[1] = BuildShippingOptionForTest(
      kPaymentTestDataId, kPaymentTestOverwriteValue, "standard");
  details->setShippingOptions(shipping_options);
  PaymentOptions* options = PaymentOptions::Create();
  options->setRequestShipping(true);
  PaymentRequest::Create(scope.GetExecutionContext(),
                         BuildPaymentMethodDataForTest(), details, options,
                         scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST(PaymentRequestTest, DetailsIdIsSet) {
  PaymentRequestV8TestingScope scope;
  PaymentDetailsInit* details = PaymentDetailsInit::Create();
  details->setTotal(BuildPaymentItemForTest());
  details->setId("my_payment_id");

  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(), details,
      scope.GetExceptionState());

  EXPECT_EQ("my_payment_id", request->id());
}

// An event listener that owns a page and destroys it when the event is invoked.
class PageDeleter final : public NativeEventListener {
 public:
  PageDeleter()
      : holder_(DummyPageHolder::CreateAndCommitNavigation(
            KURL("https://www.example.com"))) {}
  ~PageDeleter() override = default;

  // NativeEventListener:
  void Invoke(ExecutionContext*, Event*) override { holder_.reset(); }

  DummyPageHolder* page() { return holder_.get(); }

 private:
  std::unique_ptr<DummyPageHolder> holder_;
};

TEST(PaymentRequestTest, NoCrashWhenPaymentMethodChangeEventDestroysContext) {
  PageDeleter* page_deleter = MakeGarbageCollected<PageDeleter>();
  LocalFrame& frame = page_deleter->page()->GetFrame();
  auto* isolate = ToIsolate(&frame);
  v8::HandleScope handle_scope(isolate);
  ScriptState* script_state = ScriptState::From(
      ToV8ContextEvenIfDetached(&frame, DOMWrapperWorld::MainWorld(isolate)));
  v8::Local<v8::Context> context(script_state->GetContext());
  v8::Context::Scope context_scope(context);
  MockFunctionScope funcs(script_state);

  HeapVector<Member<PaymentMethodData>> method_data =
      BuildPaymentMethodDataForTest();
  PaymentRequest* request = PaymentRequest::Create(
      ExecutionContext::From(script_state), method_data,
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);
  request->setOnpaymentmethodchange(page_deleter);
  LocalFrame::NotifyUserActivation(
      &frame, mojom::UserActivationNotificationType::kTest);
  request->show(script_state, ASSERT_NO_EXCEPTION)
      .Then(funcs.ExpectNoCall(), funcs.ExpectNoCall());

  // Trigger the event listener that deletes the execution context.
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnPaymentMethodChange(method_data.front()->supportedMethod(),
                              /*stringified_details=*/"{}");
}

TEST(PaymentRequestTest, SPCActivationlessShowEnabled) {
  ScopedSecurePaymentConfirmationAllowOneActivationlessShowForTest
      scoped_activationless_show_enabled(true);

  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());

  {
    PaymentRequest* request = PaymentRequest::Create(
        ExecutionContext::From(scope.GetScriptState()),
        BuildSecurePaymentConfirmationMethodDataForTest(scope),
        BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

    EXPECT_FALSE(scope.GetDocument().IsUseCounted(
        WebFeature::kSecurePaymentConfirmationActivationlessShow));
    EXPECT_FALSE(scope.GetDocument().IsUseCounted(
        WebFeature::kPaymentRequestShowWithoutGestureOrToken));
    request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
        .Then(funcs.ExpectNoCall(), funcs.ExpectNoCall());
    EXPECT_FALSE(LocalFrame::HasTransientUserActivation(&(scope.GetFrame())));
    EXPECT_TRUE(scope.GetDocument().IsUseCounted(
        WebFeature::kSecurePaymentConfirmationActivationlessShow));
    EXPECT_TRUE(scope.GetDocument().IsUseCounted(
        WebFeature::kPaymentRequestShowWithoutGestureOrToken));
  }
}

TEST(PaymentRequestTest, SPCActivationlessShowDisabled) {
  ScopedSecurePaymentConfirmationAllowOneActivationlessShowForTest
      scoped_activationless_show_enabled(false);

  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      ExecutionContext::From(scope.GetScriptState()),
      BuildSecurePaymentConfirmationMethodDataForTest(scope),
      BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

  EXPECT_FALSE(scope.GetDocument().IsUseCounted(
      WebFeature::kSecurePaymentConfirmationActivationlessShow));
  EXPECT_FALSE(scope.GetDocument().IsUseCounted(
      WebFeature::kPaymentRequestShowWithoutGestureOrToken));
  request->show(scope.GetScriptState(), scope.GetExceptionState());
  EXPECT_EQ(scope.GetExceptionState().Code(),
            ToExceptionCode(DOMExceptionCode::kSecurityError));
  EXPECT_FALSE(scope.GetDocument().IsUseCounted(
      WebFeature::kSecurePaymentConfirmationActivationlessShow));
  EXPECT_TRUE(scope.GetDocument().IsUseCounted(
      WebFeature::kPaymentRequestShowWithoutGestureOrToken));
}

TEST(PaymentRequestTest, SPCActivationlessNotConsumedWithActivation) {
  ScopedSecurePaymentConfirmationAllowOneActivationlessShowForTest
      scoped_activationless_show_enabled(true);

  PaymentRequestV8TestingScope scope;
  MockFunctionScope funcs(scope.GetScriptState());

  // The first show call has an activation, so activationless SPC shouldn't be
  // recorded or consumed.
  {
    PaymentRequest* request = PaymentRequest::Create(
        ExecutionContext::From(scope.GetScriptState()),
        BuildSecurePaymentConfirmationMethodDataForTest(scope),
        BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

    LocalFrame::NotifyUserActivation(
        &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
    request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
        .Then(funcs.ExpectNoCall(), funcs.ExpectNoCall());
    EXPECT_FALSE(scope.GetDocument().IsUseCounted(
        WebFeature::kSecurePaymentConfirmationActivationlessShow));
    EXPECT_FALSE(scope.GetDocument().IsUseCounted(
        WebFeature::kPaymentRequestShowWithoutGestureOrToken));
  }

  // A following activationless SPC show call should be allowed, since the first
  // did not consume the one allowed activationless call.
  {
    PaymentRequest* request = PaymentRequest::Create(
        ExecutionContext::From(scope.GetScriptState()),
        BuildSecurePaymentConfirmationMethodDataForTest(scope),
        BuildPaymentDetailsInitForTest(), ASSERT_NO_EXCEPTION);

    request->show(scope.GetScriptState(), ASSERT_NO_EXCEPTION)
        .Then(funcs.ExpectNoCall(), funcs.ExpectNoCall());
    EXPECT_TRUE(scope.GetDocument().IsUseCounted(
        WebFeature::kSecurePaymentConfirmationActivationlessShow));
    EXPECT_TRUE(scope.GetDocument().IsUseCounted(
        WebFeature::kPaymentRequestShowWithoutGestureOrToken));
  }
}

}  // namespace
}  // namespace blink
