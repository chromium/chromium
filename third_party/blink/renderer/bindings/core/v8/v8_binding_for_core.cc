/*
 * Copyright (C) 2017 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"

#include "third_party/blink/renderer/bindings/core/v8/custom/v8_custom_xpath_ns_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_target.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_link_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_worker_global_scope.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_worklet_global_scope.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_xpath_ns_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/typed_arrays/flexible_array_buffer_view.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/xml/xpath_ns_resolver.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/cstring.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

bool ToBooleanSlow(v8::Isolate* isolate,
                   v8::Local<v8::Value> value,
                   ExceptionState& exception_state) {
  DCHECK(!value->IsBoolean());
  v8::TryCatch block(isolate);
  bool result = false;
  if (!value->BooleanValue(isolate->GetCurrentContext()).To(&result))
    exception_state.RethrowV8Exception(block.Exception());
  return result;
}

const int32_t kMaxInt32 = 0x7fffffff;
const int32_t kMinInt32 = -kMaxInt32 - 1;
const uint32_t kMaxUInt32 = 0xffffffff;
const int64_t kJSMaxInteger =
    0x20000000000000LL -
    1;  // 2^53 - 1, maximum uniquely representable integer in ECMAScript.

static double EnforceRange(double x,
                           double minimum,
                           double maximum,
                           const char* type_name,
                           ExceptionState& exception_state) {
  if (std::isnan(x) || std::isinf(x)) {
    exception_state.ThrowTypeError(
        "Value is" + String(std::isinf(x) ? " infinite and" : "") +
        " not of type '" + String(type_name) + "'.");
    return 0;
  }
  x = trunc(x);
  if (x < minimum || x > maximum) {
    exception_state.ThrowTypeError("Value is outside the '" +
                                   String(type_name) + "' value range.");
    return 0;
  }
  return x;
}

template <typename T>
struct IntTypeLimits {};

template <>
struct IntTypeLimits<int8_t> {
  static const int8_t kMinValue = -128;
  static const int8_t kMaxValue = 127;
  static const unsigned kNumberOfValues = 256;  // 2^8
};

template <>
struct IntTypeLimits<uint8_t> {
  static const uint8_t kMaxValue = 255;
  static const unsigned kNumberOfValues = 256;  // 2^8
};

template <>
struct IntTypeLimits<int16_t> {
  static const short kMinValue = -32768;
  static const short kMaxValue = 32767;
  static const unsigned kNumberOfValues = 65536;  // 2^16
};

template <>
struct IntTypeLimits<uint16_t> {
  static const unsigned short kMaxValue = 65535;
  static const unsigned kNumberOfValues = 65536;  // 2^16
};

template <typename T>
static inline T ToSmallerInt(v8::Isolate* isolate,
                             v8::Local<v8::Value> value,
                             IntegerConversionConfiguration configuration,
                             const char* type_name,
                             ExceptionState& exception_state) {
  typedef IntTypeLimits<T> LimitsTrait;

  // Fast case. The value is already a 32-bit integer in the right range.
  if (value->IsInt32()) {
    int32_t result = value.As<v8::Int32>()->Value();
    if (result >= LimitsTrait::kMinValue && result <= LimitsTrait::kMaxValue)
      return static_cast<T>(result);
    if (configuration == kEnforceRange) {
      exception_state.ThrowTypeError("Value is outside the '" +
                                     String(type_name) + "' value range.");
      return 0;
    }
    if (configuration == kClamp)
      return clampTo<T>(result);
    result %= LimitsTrait::kNumberOfValues;
    return static_cast<T>(result > LimitsTrait::kMaxValue
                              ? result - LimitsTrait::kNumberOfValues
                              : result);
  }

  v8::Local<v8::Number> number_object;
  if (value->IsNumber()) {
    number_object = value.As<v8::Number>();
  } else {
    // Can the value be converted to a number?
    v8::TryCatch block(isolate);
    if (!value->ToNumber(isolate->GetCurrentContext())
             .ToLocal(&number_object)) {
      exception_state.RethrowV8Exception(block.Exception());
      return 0;
    }
  }
  DCHECK(!number_object.IsEmpty());

  if (configuration == kEnforceRange) {
    return EnforceRange(number_object->Value(), LimitsTrait::kMinValue,
                        LimitsTrait::kMaxValue, type_name, exception_state);
  }

  double number_value = number_object->Value();
  if (std::isnan(number_value) || !number_value)
    return 0;

  if (configuration == kClamp)
    return clampTo<T>(number_value);

  if (std::isinf(number_value))
    return 0;

  number_value =
      number_value < 0 ? -floor(fabs(number_value)) : floor(fabs(number_value));
  number_value = fmod(number_value, LimitsTrait::kNumberOfValues);

  return static_cast<T>(number_value > LimitsTrait::kMaxValue
                            ? number_value - LimitsTrait::kNumberOfValues
                            : number_value);
}

template <typename T>
static inline T ToSmallerUInt(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              IntegerConversionConfiguration configuration,
                              const char* type_name,
                              ExceptionState& exception_state) {
  typedef IntTypeLimits<T> LimitsTrait;

  // Fast case. The value is a 32-bit signed integer - possibly positive?
  if (value->IsInt32()) {
    int32_t result = value.As<v8::Int32>()->Value();
    if (result >= 0 && result <= LimitsTrait::kMaxValue)
      return static_cast<T>(result);
    if (configuration == kEnforceRange) {
      exception_state.ThrowTypeError("Value is outside the '" +
                                     String(type_name) + "' value range.");
      return 0;
    }
    if (configuration == kClamp)
      return clampTo<T>(result);
    return static_cast<T>(result);
  }

  v8::Local<v8::Number> number_object;
  if (value->IsNumber()) {
    number_object = value.As<v8::Number>();
  } else {
    // Can the value be converted to a number?
    v8::TryCatch block(isolate);
    if (!value->ToNumber(isolate->GetCurrentContext())
             .ToLocal(&number_object)) {
      exception_state.RethrowV8Exception(block.Exception());
      return 0;
    }
  }
  DCHECK(!number_object.IsEmpty());

  if (configuration == kEnforceRange) {
    return EnforceRange(number_object->Value(), 0, LimitsTrait::kMaxValue,
                        type_name, exception_state);
  }

  double number_value = number_object->Value();

  if (std::isnan(number_value) || !number_value)
    return 0;

  if (configuration == kClamp)
    return clampTo<T>(number_value);

  if (std::isinf(number_value))
    return 0;

  // Confine number to (-kNumberOfValues, kNumberOfValues).
  double number = fmod(trunc(number_value), LimitsTrait::kNumberOfValues);

  // Adjust range to [0, kNumberOfValues).
  if (number < 0)
    number += LimitsTrait::kNumberOfValues;

  return static_cast<T>(number);
}

int8_t ToInt8(v8::Isolate* isolate,
              v8::Local<v8::Value> value,
              IntegerConversionConfiguration configuration,
              ExceptionState& exception_state) {
  return ToSmallerInt<int8_t>(isolate, value, configuration, "byte",
                              exception_state);
}

uint8_t ToUInt8(v8::Isolate* isolate,
                v8::Local<v8::Value> value,
                IntegerConversionConfiguration configuration,
                ExceptionState& exception_state) {
  return ToSmallerUInt<uint8_t>(isolate, value, configuration, "octet",
                                exception_state);
}

int16_t ToInt16(v8::Isolate* isolate,
                v8::Local<v8::Value> value,
                IntegerConversionConfiguration configuration,
                ExceptionState& exception_state) {
  return ToSmallerInt<int16_t>(isolate, value, configuration, "short",
                               exception_state);
}

uint16_t ToUInt16(v8::Isolate* isolate,
                  v8::Local<v8::Value> value,
                  IntegerConversionConfiguration configuration,
                  ExceptionState& exception_state) {
  return ToSmallerUInt<uint16_t>(isolate, value, configuration,
                                 "unsigned short", exception_state);
}

int32_t ToInt32Slow(v8::Isolate* isolate,
                    v8::Local<v8::Value> value,
                    IntegerConversionConfiguration configuration,
                    ExceptionState& exception_state) {
  DCHECK(!value->IsInt32());
  // Can the value be converted to a number?
  v8::TryCatch block(isolate);
  v8::Local<v8::Number> number_object;
  if (!value->ToNumber(isolate->GetCurrentContext()).ToLocal(&number_object)) {
    exception_state.RethrowV8Exception(block.Exception());
    return 0;
  }

  DCHECK(!number_object.IsEmpty());

  double number_value = number_object->Value();
  if (configuration == kEnforceRange) {
    return EnforceRange(number_value, kMinInt32, kMaxInt32, "long",
                        exception_state);
  }

  if (std::isnan(number_value))
    return 0;

  if (configuration == kClamp)
    return clampTo<int32_t>(number_value);

  if (std::isinf(number_value))
    return 0;

  int32_t result;
  if (!number_object->Int32Value(isolate->GetCurrentContext()).To(&result)) {
    exception_state.RethrowV8Exception(block.Exception());
    return 0;
  }
  return result;
}

uint32_t ToUInt32Slow(v8::Isolate* isolate,
                      v8::Local<v8::Value> value,
                      IntegerConversionConfiguration configuration,
                      ExceptionState& exception_state) {
  DCHECK(!value->IsUint32());
  if (value->IsInt32()) {
    DCHECK_NE(configuration, kNormalConversion);
    int32_t result = value.As<v8::Int32>()->Value();
    if (result >= 0)
      return result;
    if (configuration == kEnforceRange) {
      exception_state.ThrowTypeError(
          "Value is outside the 'unsigned long' value range.");
      return 0;
    }
    DCHECK_EQ(configuration, kClamp);
    return clampTo<uint32_t>(result);
  }

  // Can the value be converted to a number?
  v8::TryCatch block(isolate);
  v8::Local<v8::Number> number_object;
  if (!value->ToNumber(isolate->GetCurrentContext()).ToLocal(&number_object)) {
    exception_state.RethrowV8Exception(block.Exception());
    return 0;
  }
  DCHECK(!number_object.IsEmpty());

  if (configuration == kEnforceRange) {
    return EnforceRange(number_object->Value(), 0, kMaxUInt32, "unsigned long",
                        exception_state);
  }

  double number_value = number_object->Value();

  if (std::isnan(number_value))
    return 0;

  if (configuration == kClamp)
    return clampTo<uint32_t>(number_value);

  if (std::isinf(number_value))
    return 0;

  uint32_t result;
  if (!number_object->Uint32Value(isolate->GetCurrentContext()).To(&result)) {
    exception_state.RethrowV8Exception(block.Exception());
    return 0;
  }
  return result;
}

int64_t ToInt64Slow(v8::Isolate* isolate,
                    v8::Local<v8::Value> value,
                    IntegerConversionConfiguration configuration,
                    ExceptionState& exception_state) {
  DCHECK(!value->IsInt32());

  v8::Local<v8::Number> number_object;
  // Can the value be converted to a number?
  v8::TryCatch block(isolate);
  if (!value->ToNumber(isolate->GetCurrentContext()).ToLocal(&number_object)) {
    exception_state.RethrowV8Exception(block.Exception());
    return 0;
  }
  DCHECK(!number_object.IsEmpty());

  double number_value = number_object->Value();

  if (configuration == kEnforceRange) {
    return EnforceRange(number_value, -kJSMaxInteger, kJSMaxInteger,
                        "long long", exception_state);
  }

  return DoubleToInteger(number_value);
}

uint64_t ToUInt64Slow(v8::Isolate* isolate,
                      v8::Local<v8::Value> value,
                      IntegerConversionConfiguration configuration,
                      ExceptionState& exception_state) {
  DCHECK(!value->IsUint32());
  if (value->IsInt32()) {
    DCHECK(configuration != kNormalConversion);
    int32_t result = value.As<v8::Int32>()->Value();
    if (result >= 0)
      return result;
    if (configuration == kEnforceRange) {
      exception_state.ThrowTypeError(
          "Value is outside the 'unsigned long long' value range.");
      return 0;
    }
    DCHECK_EQ(configuration, kClamp);
    return clampTo<uint64_t>(result);
  }

  v8::Local<v8::Number> number_object;
  // Can the value be converted to a number?
  v8::TryCatch block(isolate);
  if (!value->ToNumber(isolate->GetCurrentContext()).ToLocal(&number_object)) {
    exception_state.RethrowV8Exception(block.Exception());
    return 0;
  }
  DCHECK(!number_object.IsEmpty());

  double number_value = number_object->Value();

  if (configuration == kEnforceRange) {
    return EnforceRange(number_value, 0, kJSMaxInteger, "unsigned long long",
                        exception_state);
  }

  if (std::isnan(number_value))
    return 0;

  if (configuration == kClamp)
    return clampTo<uint64_t>(number_value);

  return DoubleToInteger(number_value);
}

float ToRestrictedFloat(v8::Isolate* isolate,
                        v8::Local<v8::Value> value,
                        ExceptionState& exception_state) {
  float number_value = ToFloat(isolate, value, exception_state);
  if (exception_state.HadException())
    return 0;
  if (!std::isfinite(number_value)) {
    exception_state.ThrowTypeError("The provided float value is non-finite.");
    return 0;
  }
  return number_value;
}

double ToDoubleSlow(v8::Isolate* isolate,
                    v8::Local<v8::Value> value,
                    ExceptionState& exception_state) {
  DCHECK(!value->IsNumber());
  v8::TryCatch block(isolate);
  v8::Local<v8::Number> number_value;
  if (!value->ToNumber(isolate->GetCurrentContext()).ToLocal(&number_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return 0;
  }
  return number_value->Value();
}

double ToRestrictedDouble(v8::Isolate* isolate,
                          v8::Local<v8::Value> value,
                          ExceptionState& exception_state) {
  double number_value = ToDouble(isolate, value, exception_state);
  if (exception_state.HadException())
    return 0;
  if (!std::isfinite(number_value)) {
    exception_state.ThrowTypeError("The provided double value is non-finite.");
    return 0;
  }
  return number_value;
}

static bool HasUnmatchedSurrogates(const String& string) {
  // By definition, 8-bit strings are confined to the Latin-1 code page and
  // have no surrogates, matched or otherwise.
  if (string.IsEmpty() || string.Is8Bit())
    return false;

  const UChar* characters = string.Characters16();
  const unsigned length = string.length();

  for (unsigned i = 0; i < length; ++i) {
    UChar c = characters[i];
    if (U16_IS_SINGLE(c))
      continue;
    if (U16_IS_TRAIL(c))
      return true;
    DCHECK(U16_IS_LEAD(c));
    if (i == length - 1)
      return true;
    UChar d = characters[i + 1];
    if (!U16_IS_TRAIL(d))
      return true;
    ++i;
  }
  return false;
}

// Replace unmatched surrogates with REPLACEMENT CHARACTER U+FFFD.
String ReplaceUnmatchedSurrogates(const String& string) {
  // This roughly implements http://heycam.github.io/webidl/#dfn-obtain-unicode
  // but since Blink strings are 16-bits internally, the output is simply
  // re-encoded to UTF-16.

  // The concept of surrogate pairs is explained at:
  // http://www.unicode.org/versions/Unicode6.2.0/ch03.pdf#G2630

  // Blink-specific optimization to avoid making an unnecessary copy.
  if (!HasUnmatchedSurrogates(string))
    return string;
  DCHECK(!string.Is8Bit());

  // 1. Let S be the DOMString value.
  const UChar* s = string.Characters16();

  // 2. Let n be the length of S.
  const unsigned n = string.length();

  // 3. Initialize i to 0.
  unsigned i = 0;

  // 4. Initialize U to be an empty sequence of Unicode characters.
  StringBuilder u;
  u.ReserveCapacity(n);

  // 5. While i < n:
  while (i < n) {
    // 1. Let c be the code unit in S at index i.
    UChar c = s[i];
    // 2. Depending on the value of c:
    if (U16_IS_SINGLE(c)) {
      // c < 0xD800 or c > 0xDFFF
      // Append to U the Unicode character with code point c.
      u.Append(c);
    } else if (U16_IS_TRAIL(c)) {
      // 0xDC00 <= c <= 0xDFFF
      // Append to U a U+FFFD REPLACEMENT CHARACTER.
      u.Append(kReplacementCharacter);
    } else {
      // 0xD800 <= c <= 0xDBFF
      DCHECK(U16_IS_LEAD(c));
      if (i == n - 1) {
        // 1. If i = n-1, then append to U a U+FFFD REPLACEMENT CHARACTER.
        u.Append(kReplacementCharacter);
      } else {
        // 2. Otherwise, i < n-1:
        DCHECK_LT(i, n - 1);
        // ....1. Let d be the code unit in S at index i+1.
        UChar d = s[i + 1];
        if (U16_IS_TRAIL(d)) {
          // 2. If 0xDC00 <= d <= 0xDFFF, then:
          // ..1. Let a be c & 0x3FF.
          // ..2. Let b be d & 0x3FF.
          // ..3. Append to U the Unicode character with code point
          //      2^16+2^10*a+b.
          u.Append(U16_GET_SUPPLEMENTARY(c, d));
          // Blink: This is equivalent to u.append(c); u.append(d);
          ++i;
        } else {
          // 3. Otherwise, d < 0xDC00 or d > 0xDFFF. Append to U a U+FFFD
          //    REPLACEMENT CHARACTER.
          u.Append(kReplacementCharacter);
        }
      }
    }
    // 3. Set i to i+1.
    ++i;
  }

  // 6. Return U.
  DCHECK_EQ(u.length(), string.length());
  return u.ToString();
}

XPathNSResolver* ToXPathNSResolver(ScriptState* script_state,
                                   v8::Local<v8::Value> value) {
  XPathNSResolver* resolver = nullptr;
  if (V8XPathNSResolver::hasInstance(value, script_state->GetIsolate())) {
    resolver = V8XPathNSResolver::ToImpl(v8::Local<v8::Object>::Cast(value));
  } else if (value->IsObject()) {
    resolver =
        V8CustomXPathNSResolver::Create(script_state, value.As<v8::Object>());
  }
  return resolver;
}

DOMWindow* ToDOMWindow(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  if (value.IsEmpty() || !value->IsObject())
    return nullptr;

  v8::Local<v8::Object> window_wrapper = V8Window::findInstanceInPrototypeChain(
      v8::Local<v8::Object>::Cast(value), isolate);
  if (!window_wrapper.IsEmpty())
    return V8Window::ToImpl(window_wrapper);
  return nullptr;
}

LocalDOMWindow* ToLocalDOMWindow(v8::Local<v8::Context> context) {
  if (context.IsEmpty())
    return nullptr;
  return ToLocalDOMWindow(
      ToDOMWindow(context->GetIsolate(), context->Global()));
}

LocalDOMWindow* EnteredDOMWindow(v8::Isolate* isolate) {
  LocalDOMWindow* window =
      ToLocalDOMWindow(isolate->GetEnteredOrMicrotaskContext());
  DCHECK(window);
  return window;
}

LocalDOMWindow* CurrentDOMWindow(v8::Isolate* isolate) {
  return ToLocalDOMWindow(isolate->GetCurrentContext());
}

ExecutionContext* ToExecutionContext(v8::Local<v8::Context> context) {
  DCHECK(!context.IsEmpty());

  RUNTIME_CALL_TIMER_SCOPE(context->GetIsolate(),
                           RuntimeCallStats::CounterId::kToExecutionContext);

  v8::Local<v8::Object> global_proxy = context->Global();
  // There are several contexts other than Window, WorkerGlobalScope or
  // WorkletGlobalScope but entering into ToExecutionContext, namely GC context,
  // DevTools' context (debug context), and maybe more.  They all don't have
  // any internal field.
  if (global_proxy->InternalFieldCount() == 0)
    return nullptr;

  const WrapperTypeInfo* wrapper_type_info = ToWrapperTypeInfo(global_proxy);
  if (wrapper_type_info->Equals(&V8Window::wrapperTypeInfo))
    return V8Window::ToImpl(global_proxy)->GetExecutionContext();
  if (wrapper_type_info->IsSubclass(&V8WorkerGlobalScope::wrapperTypeInfo))
    return V8WorkerGlobalScope::ToImpl(global_proxy)->GetExecutionContext();
  if (wrapper_type_info->IsSubclass(&V8WorkletGlobalScope::wrapperTypeInfo))
    return V8WorkletGlobalScope::ToImpl(global_proxy)->GetExecutionContext();

  NOTREACHED();
  return nullptr;
}

ExecutionContext* CurrentExecutionContext(v8::Isolate* isolate) {
  return ToExecutionContext(isolate->GetCurrentContext());
}

LocalFrame* ToLocalFrameIfNotDetached(v8::Local<v8::Context> context) {
  LocalDOMWindow* window = ToLocalDOMWindow(context);
  if (window && window->IsCurrentlyDisplayedInFrame())
    return window->GetFrame();
  // We return 0 here because |context| is detached from the Frame. If we
  // did return |frame| we could get in trouble because the frame could be
  // navigated to another security origin.
  return nullptr;
}

void ToFlexibleArrayBufferView(v8::Isolate* isolate,
                               v8::Local<v8::Value> value,
                               FlexibleArrayBufferView& result,
                               void* storage) {
  DCHECK(value->IsArrayBufferView());
  v8::Local<v8::ArrayBufferView> buffer = value.As<v8::ArrayBufferView>();
  if (!storage) {
    result.SetFull(V8ArrayBufferView::ToImpl(buffer));
    return;
  }
  size_t length = buffer->ByteLength();
  buffer->CopyContents(storage, length);
  result.SetSmall(storage, length);
}

static ScriptState* ToScriptStateImpl(LocalFrame* frame,
                                      DOMWrapperWorld& world) {
  if (!frame)
    return nullptr;
  v8::Local<v8::Context> context = ToV8ContextEvenIfDetached(frame, world);
  if (context.IsEmpty())
    return nullptr;
  ScriptState* script_state = ScriptState::From(context);
  if (!script_state->ContextIsValid())
    return nullptr;
  DCHECK_EQ(frame, ToLocalFrameIfNotDetached(context));
  return script_state;
}

v8::Local<v8::Context> ToV8Context(ExecutionContext* context,
                                   DOMWrapperWorld& world) {
  DCHECK(context);
  if (auto* document = DynamicTo<Document>(context)) {
    if (LocalFrame* frame = document->GetFrame())
      return ToV8Context(frame, world);
  } else if (auto* scope = DynamicTo<WorkerOrWorkletGlobalScope>(context)) {
    if (WorkerOrWorkletScriptController* script = scope->ScriptController()) {
      if (script->GetScriptState()->ContextIsValid())
        return script->GetScriptState()->GetContext();
    }
  }
  return v8::Local<v8::Context>();
}

v8::Local<v8::Context> ToV8Context(LocalFrame* frame, DOMWrapperWorld& world) {
  ScriptState* script_state = ToScriptStateImpl(frame, world);
  if (!script_state)
    return v8::Local<v8::Context>();
  return script_state->GetContext();
}

v8::Local<v8::Context> ToV8ContextEvenIfDetached(LocalFrame* frame,
                                                 DOMWrapperWorld& world) {
  // TODO(yukishiino): this method probably should not force context creation,
  // but it does through WindowProxy() call.
  DCHECK(frame);
  return frame->WindowProxy(world)->ContextIfInitialized();
}

ScriptState* ToScriptState(LocalFrame* frame, DOMWrapperWorld& world) {
  v8::HandleScope handle_scope(ToIsolate(frame));
  return ToScriptStateImpl(frame, world);
}

ScriptState* ToScriptStateForMainWorld(LocalFrame* frame) {
  return ToScriptState(frame, DOMWrapperWorld::MainWorld());
}

bool IsValidEnum(const String& value,
                 const char** valid_values,
                 size_t length,
                 const String& enum_name,
                 ExceptionState& exception_state) {
  for (size_t i = 0; i < length; ++i) {
    // Avoid the strlen inside String::operator== (because of the StringView).
    if (WTF::Equal(value.Impl(), valid_values[i]))
      return true;
  }
  exception_state.ThrowTypeError("The provided value '" + value +
                                 "' is not a valid enum value of type " +
                                 enum_name + ".");
  return false;
}

bool IsValidEnum(const Vector<String>& values,
                 const char** valid_values,
                 size_t length,
                 const String& enum_name,
                 ExceptionState& exception_state) {
  for (auto value : values) {
    if (!IsValidEnum(value, valid_values, length, enum_name, exception_state))
      return false;
  }
  return true;
}

v8::Local<v8::Function> GetEsIteratorMethod(v8::Isolate* isolate,
                                            v8::Local<v8::Object> object,
                                            ExceptionState& exception_state) {
  const v8::Local<v8::Value> key = v8::Symbol::GetIterator(isolate);

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> iterator_method;
  if (!object->Get(isolate->GetCurrentContext(), key)
           .ToLocal(&iterator_method)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return v8::Local<v8::Function>();
  }

  if (iterator_method->IsNullOrUndefined())
    return v8::Local<v8::Function>();

  if (!iterator_method->IsFunction()) {
    exception_state.ThrowTypeError("Iterator must be callable function");
    return v8::Local<v8::Function>();
  }

  return iterator_method.As<v8::Function>();
}

v8::Local<v8::Object> GetEsIteratorWithMethod(
    v8::Isolate* isolate,
    v8::Local<v8::Function> getter_function,
    v8::Local<v8::Object> object,
    ExceptionState& exception_state) {
  v8::TryCatch block(isolate);
  v8::Local<v8::Value> iterator;
  if (!V8ScriptRunner::CallFunction(
           getter_function, ToExecutionContext(isolate->GetCurrentContext()),
           object, 0, nullptr, isolate)
           .ToLocal(&iterator)) {
    exception_state.RethrowV8Exception(block.Exception());
    return v8::Local<v8::Object>();
  }
  if (!iterator->IsObject()) {
    exception_state.ThrowTypeError("Iterator is not an object.");
    return v8::Local<v8::Object>();
  }
  return iterator.As<v8::Object>();
}

v8::Local<v8::Object> GetEsIterator(v8::Isolate* isolate,
                                    v8::Local<v8::Object> object,
                                    ExceptionState& exception_state) {
  v8::Local<v8::Function> iterator_getter =
      GetEsIteratorMethod(isolate, object, exception_state);
  if (exception_state.HadException())
    return v8::Local<v8::Object>();

  if (iterator_getter.IsEmpty()) {
    exception_state.ThrowTypeError("Iterator getter is not callable.");
    return v8::Local<v8::Object>();
  }

  return GetEsIteratorWithMethod(isolate, iterator_getter, object,
                                 exception_state);
}

bool HasCallableIteratorSymbol(v8::Isolate* isolate,
                               v8::Local<v8::Value> value,
                               ExceptionState& exception_state) {
  if (!value->IsObject())
    return false;
  v8::TryCatch block(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Value> iterator_getter;
  if (!value.As<v8::Object>()
           ->Get(context, v8::Symbol::GetIterator(isolate))
           .ToLocal(&iterator_getter)) {
    exception_state.RethrowV8Exception(block.Exception());
    return false;
  }
  return iterator_getter->IsFunction();
}

v8::Isolate* ToIsolate(ExecutionContext* context) {
  if (context && context->IsDocument())
    return V8PerIsolateData::MainThreadIsolate();
  return v8::Isolate::GetCurrent();
}

v8::Isolate* ToIsolate(LocalFrame* frame) {
  DCHECK(frame);
  return frame->GetWindowProxyManager()->GetIsolate();
}

v8::Local<v8::Value> FromJSONString(v8::Isolate* isolate,
                                    v8::Local<v8::Context> context,
                                    const String& stringified_json,
                                    ExceptionState& exception_state) {
  v8::Local<v8::Value> parsed;
  v8::TryCatch try_catch(isolate);
  if (!v8::JSON::Parse(context, V8String(isolate, stringified_json))
           .ToLocal(&parsed)) {
    if (try_catch.HasCaught())
      exception_state.RethrowV8Exception(try_catch.Exception());
  }

  return parsed;
}

Vector<String> GetOwnPropertyNames(v8::Isolate* isolate,
                                   const v8::Local<v8::Object>& object,
                                   ExceptionState& exception_state) {
  if (object.IsEmpty())
    return Vector<String>();

  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Array> property_names;
  if (!object->GetOwnPropertyNames(isolate->GetCurrentContext())
           .ToLocal(&property_names)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return Vector<String>();
  }

  return NativeValueTraits<IDLSequence<IDLString>>::NativeValue(
      isolate, property_names, exception_state);
}

}  // namespace blink
