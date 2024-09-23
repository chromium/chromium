// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/modules/payments/payment_request.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {
namespace {

class MockPaymentProvider : public payments::mojom::blink::PaymentRequest {
 public:
  // mojom::PaymentRequest
  void Init(
      mojo::PendingRemote<payments::mojom::blink::PaymentRequestClient> client,
      WTF::Vector<payments::mojom::blink::PaymentMethodDataPtr> method_data,
      payments::mojom::blink::PaymentDetailsPtr details,
      payments::mojom::blink::PaymentOptionsPtr options) override {
    details_ = std::move(details);
  }

  void Show(bool wait_for_updated_details, bool had_user_activation) override {
    NOTREACHED_IN_MIGRATION();
  }
  void Retry(
      payments::mojom::blink::PaymentValidationErrorsPtr errors) override {
    NOTREACHED_IN_MIGRATION();
  }
  void UpdateWith(
      payments::mojom::blink::PaymentDetailsPtr update_with_details) override {
    NOTREACHED_IN_MIGRATION();
  }
  void OnPaymentDetailsNotUpdated() override { NOTREACHED_IN_MIGRATION(); }
  void Abort() override { NOTREACHED_IN_MIGRATION(); }
  void Complete(payments::mojom::PaymentComplete result) override {
    NOTREACHED_IN_MIGRATION();
  }
  void CanMakePayment() override { NOTREACHED_IN_MIGRATION(); }
  void HasEnrolledInstrument() override { NOTREACHED_IN_MIGRATION(); }

  mojo::PendingRemote<payments::mojom::blink::PaymentRequest>
  CreatePendingRemoteAndBind() {
    mojo::PendingRemote<payments::mojom::blink::PaymentRequest> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  payments::mojom::blink::PaymentDetailsPtr& GetDetails() { return details_; }

 private:
  mojo::Receiver<payments::mojom::blink::PaymentRequest> receiver_{this};
  payments::mojom::blink::PaymentDetailsPtr details_;
};

// This test suite is about the optional total parameter of the PaymentRequest
// constructor.
class PaymentRequestOptionalTotalTest : public testing::Test {
 public:
  void SetUp() override {
    payment_provider_ = std::make_unique<MockPaymentProvider>();
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<MockPaymentProvider> payment_provider_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
};

// This test requests a mix of app-store billing methods and normal payment
// methods. Total is required in this scenario.
TEST_F(PaymentRequestOptionalTotalTest,
       AppStoreBillingFlagEnabledTotalIsRequiredWhenMixMethods) {
  ScopedDigitalGoodsForTest digital_goods(true);

  PaymentRequestV8TestingScope scope;
  // Intentionally leaves the total of details unset.
  PaymentDetailsInit* details = PaymentDetailsInit::Create();

  HeapVector<Member<PaymentMethodData>> method_data(2);
  method_data[0] = PaymentMethodData::Create();
  method_data[0]->setSupportedMethod("foo");
  method_data[1] = PaymentMethodData::Create();
  method_data[1]->setSupportedMethod("https://play.google.com/billing");

  PaymentRequest::Create(scope.GetExecutionContext(), method_data, details,
                         scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ("required member details is undefined.",
            scope.GetExceptionState().Message());
  EXPECT_FALSE(payment_provider_->GetDetails());
}

// When the DigitalGoods flag is disabled: although this test requests a
// app-store billing methods, total is required.
TEST_F(PaymentRequestOptionalTotalTest,
       AppStoreBillingFlagDisabledTotalIsRequiredWhenMixMethods) {
  ScopedDigitalGoodsForTest digital_goods(false);

  PaymentRequestV8TestingScope scope;
  // Intentionally leaves the total of details unset.
  PaymentDetailsInit* details = PaymentDetailsInit::Create();

  HeapVector<Member<PaymentMethodData>> method_data(1);
  method_data[0] = PaymentMethodData::Create();
  method_data[0]->setSupportedMethod("https://play.google.com/billing");

  PaymentRequest::Create(scope.GetExecutionContext(), method_data, details,
                         scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ("required member details is undefined.",
            scope.GetExceptionState().Message());
  EXPECT_FALSE(payment_provider_->GetDetails());
}

// When the DigitalGoods flag is enabled: undefined total gets a place holder
// when only requesting app-store billing methods.
TEST_F(PaymentRequestOptionalTotalTest,
       AppStoreBillingFlagEnabledTotalGetPlaceHolder) {
  ScopedDigitalGoodsForTest digital_goods(true);

  PaymentRequestV8TestingScope scope;
  // Intentionally leaves the total of details unset.
  PaymentDetailsInit* details = PaymentDetailsInit::Create();

  HeapVector<Member<PaymentMethodData>> method_data(
      1, PaymentMethodData::Create());
  method_data[0]->setSupportedMethod("https://play.google.com/billing");

  MakeGarbageCollected<PaymentRequest>(
      scope.GetExecutionContext(), method_data, details,
      PaymentOptions::Create(), payment_provider_->CreatePendingRemoteAndBind(),
      ASSERT_NO_EXCEPTION);
  platform_->RunUntilIdle();
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ("0", payment_provider_->GetDetails()->total->amount->value);
  EXPECT_EQ("ZZZ", payment_provider_->GetDetails()->total->amount->currency);
}

// When the DigitalGoods flag is disabled: undefined total is rejected.
TEST_F(PaymentRequestOptionalTotalTest,
       AppStoreBillingFlagDisabledTotalGetRejected) {
  ScopedDigitalGoodsForTest digital_goods(false);

  PaymentRequestV8TestingScope scope;
  // Intentionally leaves the total of details unset.
  PaymentDetailsInit* details = PaymentDetailsInit::Create();

  HeapVector<Member<PaymentMethodData>> method_data(
      1, PaymentMethodData::Create());
  method_data[0]->setSupportedMethod("https://play.google.com/billing");

  MakeGarbageCollected<PaymentRequest>(
      scope.GetExecutionContext(), method_data, details,
      PaymentOptions::Create(), payment_provider_->CreatePendingRemoteAndBind(),
      scope.GetExceptionState());
  platform_->RunUntilIdle();
  // Verify that total is required.
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ("required member details is undefined.",
            scope.GetExceptionState().Message());
  EXPECT_FALSE(payment_provider_->GetDetails());
}

// When the DigitalGoods flag is enabled: total get overridden when only
// requesting app-store billing methods.
TEST_F(PaymentRequestOptionalTotalTest,
       AppStoreBillingFlagEnabledTotalGetOverridden) {
  ScopedDigitalGoodsForTest digital_goods(true);

  PaymentRequestV8TestingScope scope;
  PaymentDetailsInit* details = PaymentDetailsInit::Create();
  // Set a non-empty total.
  details->setTotal((BuildPaymentItemForTest()));

  HeapVector<Member<PaymentMethodData>> method_data(
      1, PaymentMethodData::Create());
  method_data[0]->setSupportedMethod("https://play.google.com/billing");

  MakeGarbageCollected<PaymentRequest>(
      scope.GetExecutionContext(), method_data, details,
      PaymentOptions::Create(), payment_provider_->CreatePendingRemoteAndBind(),
      ASSERT_NO_EXCEPTION);
  platform_->RunUntilIdle();
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  // Verify that the total get overridden.
  EXPECT_EQ("0", payment_provider_->GetDetails()->total->amount->value);
  EXPECT_EQ("ZZZ", payment_provider_->GetDetails()->total->amount->currency);
}

// When the DigitalGoods flag is disabled: total does not get overridden when
// only requesting app-store billing methods.
TEST_F(PaymentRequestOptionalTotalTest,
       AppStoreBillingFlagDisabledTotalNotGetOverridden) {
  ScopedDigitalGoodsForTest digital_goods(false);

  PaymentRequestV8TestingScope scope;
  PaymentDetailsInit* details = PaymentDetailsInit::Create();
  // Set a non-empty total.
  details->setTotal(BuildPaymentItemForTest());

  HeapVector<Member<PaymentMethodData>> method_data(
      1, PaymentMethodData::Create());
  method_data[0]->setSupportedMethod("https://play.google.com/billing");

  MakeGarbageCollected<PaymentRequest>(
      scope.GetExecutionContext(), method_data, details,
      PaymentOptions::Create(), payment_provider_->CreatePendingRemoteAndBind(),
      ASSERT_NO_EXCEPTION);
  platform_->RunUntilIdle();
  // Verify that the total is set.
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(payment_provider_->GetDetails()->total);
}
}  // namespace
}  // namespace blink
