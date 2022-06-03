// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/merchant_validation_event.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_merchant_validation_event_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {
namespace {

const char kValidPaymentMethod[] = "basic-card";
const char kValidURL[] = "https://example.test";

TEST(MerchantValidationEventTest, ValidInitializer) {
  V8TestingScope scope;
  MerchantValidationEventInit initializer;
  initializer.setMethodName(kValidPaymentMethod);
  initializer.setValidationURL(kValidURL);
  MerchantValidationEvent* event = MerchantValidationEvent::Create(
      scope.GetScriptState(), "merchantvalidation", &initializer,
      scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_EQ(kValidPaymentMethod, event->methodName());
  EXPECT_EQ(KURL(kValidURL), event->validationURL());
}

TEST(MerchantValidationEventTest, EmptyPaymentMethodIsValid) {
  V8TestingScope scope;
  MerchantValidationEventInit initializer;
  initializer.setMethodName("");
  initializer.setValidationURL(kValidURL);
  MerchantValidationEvent* event = MerchantValidationEvent::Create(
      scope.GetScriptState(), "merchantvalidation", &initializer,
      scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  EXPECT_TRUE(event->methodName().IsEmpty());
  EXPECT_EQ(KURL(kValidURL), event->validationURL());
}

TEST(MerchantValidationEventTest, InvalidPaymentMethod) {
  V8TestingScope scope;
  MerchantValidationEventInit initializer;
  initializer.setMethodName("-123");
  initializer.setValidationURL(kValidURL);
  MerchantValidationEvent* event = MerchantValidationEvent::Create(
      scope.GetScriptState(), "merchantvalidation", &initializer,
      scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kRangeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
  EXPECT_TRUE(event);
}

TEST(MerchantValidationEventTest, InvalidValidationURL) {
  V8TestingScope scope;
  MerchantValidationEventInit initializer;
  initializer.setMethodName("");
  initializer.setValidationURL("not a URL");
  MerchantValidationEvent* event = MerchantValidationEvent::Create(
      scope.GetScriptState(), "merchantvalidation", &initializer,
      scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(ESErrorType::kTypeError,
            scope.GetExceptionState().CodeAs<ESErrorType>());
  EXPECT_TRUE(event);
}

TEST(MerchantValidationEventTest, EventMustBeTrusted) {
  V8TestingScope scope;
  MerchantValidationEventInit initializer;
  initializer.setMethodName("");
  initializer.setValidationURL(kValidURL);
  MerchantValidationEvent* event = MerchantValidationEvent::Create(
      scope.GetScriptState(), "merchantvalidation", &initializer,
      scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  ASSERT_FALSE(event->isTrusted());

  ScriptPromise dummy_promise;
  event->complete(scope.GetScriptState(), dummy_promise,
                  scope.GetExceptionState());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            scope.GetExceptionState().CodeAs<DOMExceptionCode>());
}

}  // namespace
}  // namespace blink
