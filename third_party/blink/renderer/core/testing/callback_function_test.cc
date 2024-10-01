// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/callback_function_test.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_internal_enum.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_enum_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_receiver_object_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_sequence_callback.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"

namespace blink {

String CallbackFunctionTest::testCallback(V8TestCallback* callback,
                                          const String& message1,
                                          const String& message2,
                                          ExceptionState& exception_state) {
  String return_value;

  v8::TryCatch try_catch(callback->GetIsolate());
  try_catch.SetVerbose(true);

  if (!callback->Invoke(nullptr, message1, message2).To(&return_value)) {
    return String("Error!");
  }

  return String("SUCCESS: ") + return_value;
}

String CallbackFunctionTest::testNullableCallback(
    V8TestCallback* callback,
    const String& message1,
    const String& message2,
    ExceptionState& exception_state) {
  if (!callback)
    return String("Empty callback");
  return testCallback(callback, message1, message2, exception_state);
}

void CallbackFunctionTest::testInterfaceCallback(
    V8TestInterfaceCallback* callback,
    HTMLDivElement* div_element,
    ExceptionState& exception_state) {
  callback->InvokeAndReportException(nullptr, div_element);
}

void CallbackFunctionTest::testReceiverObjectCallback(
    V8TestReceiverObjectCallback* callback,
    ExceptionState& exception_state) {
  callback->InvokeAndReportException(this);
}

Vector<String> CallbackFunctionTest::testSequenceCallback(
    V8TestSequenceCallback* callback,
    const Vector<int>& numbers,
    ExceptionState& exception_state) {
  Vector<String> return_value;

  v8::TryCatch try_catch(callback->GetIsolate());
  try_catch.SetVerbose(true);

  if (!callback->Invoke(nullptr, numbers).To(&return_value)) {
    return Vector<String>();
  }

  return return_value;
}

void CallbackFunctionTest::testEnumCallback(V8TestEnumCallback* callback,
                                            const V8InternalEnum& enum_value,
                                            ExceptionState& exception_state) {
  callback->InvokeAndReportException(
      nullptr, V8InternalEnum::Create((String)enum_value).value());
}

}  // namespace blink
