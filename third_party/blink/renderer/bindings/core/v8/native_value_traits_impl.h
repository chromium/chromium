// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_NATIVE_VALUE_TRAITS_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_NATIVE_VALUE_TRAITS_IMPL_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/script_iterator.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_html.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_script.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_script_url.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/heap/heap_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CallbackFunctionBase;
class CallbackInterfaceBase;
class EventListener;
class FlexibleArrayBufferView;
class GPUColorTargetState;
class GPURenderPassColorAttachment;
class GPUVertexBufferLayout;
class ScriptWrappable;
struct WrapperTypeInfo;

namespace bindings {

class DictionaryBase;
class EnumerationBase;
class UnionBase;

CORE_EXPORT void NativeValueTraitsInterfaceNotOfType(
    const WrapperTypeInfo* wrapper_type_info,
    ExceptionState& exception_state);

CORE_EXPORT void NativeValueTraitsInterfaceNotOfType(
    const WrapperTypeInfo* wrapper_type_info,
    int argument_index,
    ExceptionState& exception_state);

// Class created for IDLAny types. Converts to either ScriptValue or
// v8::Local<v8::Value>.
class CORE_EXPORT NativeValueTraitsAnyAdapter {
  STACK_ALLOCATED();

 public:
  NativeValueTraitsAnyAdapter() = default;
  NativeValueTraitsAnyAdapter(const NativeValueTraitsAnyAdapter&) = delete;
  NativeValueTraitsAnyAdapter(NativeValueTraitsAnyAdapter&&) = default;
  explicit NativeValueTraitsAnyAdapter(v8::Isolate* isolate,
                                       v8::Local<v8::Value> value)
      : isolate_(isolate), v8_value_(value) {}

  NativeValueTraitsAnyAdapter& operator=(const NativeValueTraitsAnyAdapter&) =
      delete;
  NativeValueTraitsAnyAdapter& operator=(NativeValueTraitsAnyAdapter&&) =
      default;
  NativeValueTraitsAnyAdapter& operator=(const ScriptValue& value) {
    isolate_ = value.GetIsolate();
    v8_value_ = value.V8Value();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator v8::Local<v8::Value>() const { return v8_value_; }
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator ScriptValue() const { return ScriptValue(isolate_, v8_value_); }

 private:
  v8::Isolate* isolate_ = nullptr;
  v8::Local<v8::Value> v8_value_;
};

}  // namespace bindings

// any
template <>
struct CORE_EXPORT NativeValueTraits<IDLAny>
    : public NativeValueTraitsBase<IDLAny> {
  static bindings::NativeValueTraitsAnyAdapter NativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exception_state) {
    return bindings::NativeValueTraitsAnyAdapter(isolate, value);
  }
};

// IDLNullable<IDLAny> must not be used.
template <>
struct NativeValueTraits<IDLNullable<IDLAny>>;

template <>
struct CORE_EXPORT NativeValueTraits<IDLOptional<IDLAny>>
    : public NativeValueTraitsBase<IDLOptional<IDLAny>> {
  static bindings::NativeValueTraitsAnyAdapter NativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exception_state) {
    return bindings::NativeValueTraitsAnyAdapter(isolate, value);
  }
};

// boolean
template <>
struct CORE_EXPORT NativeValueTraits<IDLBoolean>
    : public NativeValueTraitsBase<IDLBoolean> {
  static bool NativeValue(v8::Isolate* isolate,
                          v8::Local<v8::Value> value,
                          ExceptionState& exception_state) {
    return ToBoolean(isolate, value, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLOptional<IDLBoolean>>
    : public NativeValueTraitsBase<IDLOptional<IDLBoolean>> {
  static bool NativeValue(v8::Isolate* isolate,
                          v8::Local<v8::Value> value,
                          ExceptionState& exception_state) {
    return ToBoolean(isolate, value, exception_state);
  }
};

// Integer types
#define DEFINE_NATIVE_VALUE_TRAITS_INTEGER_TYPE(T, Func)             \
  template <bindings::IDLIntegerConvMode mode>                       \
  struct NativeValueTraits<IDLIntegerTypeBase<T, mode>>              \
      : public NativeValueTraitsBase<IDLIntegerTypeBase<T, mode>> {  \
    static T NativeValue(v8::Isolate* isolate,                       \
                         v8::Local<v8::Value> value,                 \
                         ExceptionState& exception_state) {          \
      return Func(isolate, value,                                    \
                  static_cast<IntegerConversionConfiguration>(mode), \
                  exception_state);                                  \
    }                                                                \
  }
DEFINE_NATIVE_VALUE_TRAITS_INTEGER_TYPE(int8_t, ToInt8);
DEFINE_NATIVE_VALUE_TRAITS_INTEGER_TYPE(uint8_t, ToUInt8);
DEFINE_NATIVE_VALUE_TRAITS_INTEGER_TYPE(int16_t, ToInt16);
DEFINE_NATIVE_VALUE_TRAITS_INTEGER_TYPE(uint16_t, ToUInt16);
DEFINE_NATIVE_VALUE_TRAITS_INTEGER_TYPE(int32_t, ToInt32);
DEFINE_NATIVE_VALUE_TRAITS_INTEGER_TYPE(uint32_t, ToUInt32);
DEFINE_NATIVE_VALUE_TRAITS_INTEGER_TYPE(int64_t, ToInt64);
DEFINE_NATIVE_VALUE_TRAITS_INTEGER_TYPE(uint64_t, ToUInt64);
#undef DEFINE_NATIVE_VALUE_TRAITS_INTEGER_TYPE

// Floats and doubles
template <>
struct CORE_EXPORT NativeValueTraits<IDLDouble>
    : public NativeValueTraitsBase<IDLDouble> {
  static double NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    return ToRestrictedDouble(isolate, value, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnrestrictedDouble>
    : public NativeValueTraitsBase<IDLUnrestrictedDouble> {
  static double NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    return ToDouble(isolate, value, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLFloat>
    : public NativeValueTraitsBase<IDLFloat> {
  static float NativeValue(v8::Isolate* isolate,
                           v8::Local<v8::Value> value,
                           ExceptionState& exception_state) {
    return ToRestrictedFloat(isolate, value, exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLUnrestrictedFloat>
    : public NativeValueTraitsBase<IDLUnrestrictedFloat> {
  static float NativeValue(v8::Isolate* isolate,
                           v8::Local<v8::Value> value,
                           ExceptionState& exception_state) {
    return ToFloat(isolate, value, exception_state);
  }
};

// Strings

namespace bindings {

// ToBlinkString implements AtomicString- and String-specific conversions from
// v8::String.  NativeValueTraitsStringAdapter helps select the best conversion.
//
// Example:
//   void F(const AtomicString& s);
//   void G(const String& s);
//
//   const NativeValueTraitsStringAdapter& x = ...;
//   F(x);  // ToBlinkString<AtomicString> is used.
//   G(x);  // ToBlinkString<String> is used.
class CORE_EXPORT NativeValueTraitsStringAdapter {
  STACK_ALLOCATED();

 public:
  NativeValueTraitsStringAdapter() = default;
  NativeValueTraitsStringAdapter(const NativeValueTraitsStringAdapter&) =
      delete;
  NativeValueTraitsStringAdapter(NativeValueTraitsStringAdapter&&) = default;
  explicit NativeValueTraitsStringAdapter(v8::Local<v8::String> value)
      : v8_string_(value) {}
  explicit NativeValueTraitsStringAdapter(const String& value)
      : wtf_string_(value) {}
  explicit NativeValueTraitsStringAdapter(int32_t value)
      : wtf_string_(ToBlinkString(value)) {}

  NativeValueTraitsStringAdapter& operator=(
      const NativeValueTraitsStringAdapter&) = delete;
  NativeValueTraitsStringAdapter& operator=(NativeValueTraitsStringAdapter&&) =
      default;
  NativeValueTraitsStringAdapter& operator=(const String& value) {
    v8_string_.Clear();
    wtf_string_ = value;
    return *this;
  }

  void Init(v8::Local<v8::String> value) {
    DCHECK(v8_string_.IsEmpty());
    DCHECK(wtf_string_.IsNull());
    v8_string_ = value;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator String() const { return ToString<String>(); }
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator AtomicString() const { return ToString<AtomicString>(); }
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator StringView() const& { return ToStringView(); }
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator StringView() const&& = delete;

 private:
  template <class StringType>
  StringType ToString() const {
    if (LIKELY(!v8_string_.IsEmpty()))
      return ToBlinkString<StringType>(v8_string_, kExternalize);
    return StringType(wtf_string_);
  }

  StringView ToStringView() const& {
    if (LIKELY(!v8_string_.IsEmpty())) {
      return ToBlinkStringView(v8_string_, string_view_backing_store_,
                               kExternalize);
    }
    return wtf_string_;
  }

  v8::Local<v8::String> v8_string_;
  String wtf_string_;
  mutable StringView::StackBackingStore string_view_backing_store_;
};

}  // namespace bindings

template <bindings::IDLStringConvMode mode>
struct NativeValueTraits<IDLByteStringBase<mode>>
    : public NativeValueTraitsBase<IDLByteStringBase<mode>> {
  // https://webidl.spec.whatwg.org/#es-ByteString
  static bindings::NativeValueTraitsStringAdapter NativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exception_state) {
    if (value->IsString() and value.As<v8::String>()->ContainsOnlyOneByte())
      return bindings::NativeValueTraitsStringAdapter(value.As<v8::String>());
    if (value->IsInt32()) {
      return bindings::NativeValueTraitsStringAdapter(
          value.As<v8::Int32>()->Value());
    }

    if (mode == bindings::IDLStringConvMode::kNullable) {
      if (value->IsNullOrUndefined())
        return bindings::NativeValueTraitsStringAdapter();
    }

    v8::TryCatch try_catch(isolate);
    v8::Local<v8::String> v8_string;
    if (!value->ToString(isolate->GetCurrentContext()).ToLocal(&v8_string)) {
      exception_state.RethrowV8Exception(try_catch.Exception());
      return bindings::NativeValueTraitsStringAdapter();
    }
    if (!v8_string->ContainsOnlyOneByte()) {
      exception_state.ThrowTypeError(
          "String contains non ISO-8859-1 code point.");
      return bindings::NativeValueTraitsStringAdapter();
    }
    return bindings::NativeValueTraitsStringAdapter(v8_string);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLNullable<IDLByteString>>
    : public NativeValueTraitsBase<IDLNullable<IDLByteString>> {
  static decltype(auto) NativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state) {
    return NativeValueTraits<IDLByteStringBase<
        bindings::IDLStringConvMode::kNullable>>::NativeValue(isolate, value,
                                                              exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLOptional<IDLByteString>>
    : public NativeValueTraitsBase<IDLOptional<IDLByteString>> {
  static decltype(auto) NativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state) {
    if (value->IsUndefined())
      return bindings::NativeValueTraitsStringAdapter();
    return NativeValueTraits<IDLByteString>::NativeValue(isolate, value,
                                                         exception_state);
  }
};

template <bindings::IDLStringConvMode mode>
struct NativeValueTraits<IDLStringBase<mode>>
    : public NativeValueTraitsBase<IDLStringBase<mode>> {
  // https://webidl.spec.whatwg.org/#es-DOMString
  static bindings::NativeValueTraitsStringAdapter NativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exception_state) {
    if (value->IsString())
      return bindings::NativeValueTraitsStringAdapter(value.As<v8::String>());
    if (value->IsInt32()) {
      return bindings::NativeValueTraitsStringAdapter(
          value.As<v8::Int32>()->Value());
    }

    if (mode == bindings::IDLStringConvMode::kNullable) {
      if (value->IsNullOrUndefined())
        return bindings::NativeValueTraitsStringAdapter();
    }
    if (mode == bindings::IDLStringConvMode::kTreatNullAsEmptyString) {
      if (value->IsNull())
        return bindings::NativeValueTraitsStringAdapter(g_empty_string);
    }

    v8::TryCatch try_catch(isolate);
    v8::Local<v8::String> v8_string;
    if (!value->ToString(isolate->GetCurrentContext()).ToLocal(&v8_string)) {
      exception_state.RethrowV8Exception(try_catch.Exception());
      return bindings::NativeValueTraitsStringAdapter();
    }
    return bindings::NativeValueTraitsStringAdapter(v8_string);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLNullable<IDLString>>
    : public NativeValueTraitsBase<IDLNullable<IDLString>> {
  static decltype(auto) NativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state) {
    return NativeValueTraits<IDLStringBase<
        bindings::IDLStringConvMode::kNullable>>::NativeValue(isolate, value,
                                                              exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLOptional<IDLString>>
    : public NativeValueTraitsBase<IDLOptional<IDLString>> {
  static decltype(auto) NativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state) {
    if (value->IsUndefined())
      return bindings::NativeValueTraitsStringAdapter();
    return NativeValueTraits<IDLString>::NativeValue(isolate, value,
                                                     exception_state);
  }
};

template <bindings::IDLStringConvMode mode>
struct NativeValueTraits<IDLUSVStringBase<mode>>
    : public NativeValueTraitsBase<IDLUSVStringBase<mode>> {
  // https://webidl.spec.whatwg.org/#es-USVString
  static bindings::NativeValueTraitsStringAdapter NativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exception_state) {
    String string = NativeValueTraits<IDLStringBase<mode>>::NativeValue(
        isolate, value, exception_state);
    if (exception_state.HadException())
      return bindings::NativeValueTraitsStringAdapter();

    return bindings::NativeValueTraitsStringAdapter(
        ReplaceUnmatchedSurrogates(string));
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLNullable<IDLUSVString>>
    : public NativeValueTraitsBase<IDLNullable<IDLUSVString>> {
  static decltype(auto) NativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state) {
    return NativeValueTraits<IDLUSVStringBase<
        bindings::IDLStringConvMode::kNullable>>::NativeValue(isolate, value,
                                                              exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLOptional<IDLUSVString>>
    : public NativeValueTraitsBase<IDLOptional<IDLUSVString>> {
  static decltype(auto) NativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state) {
    if (value->IsUndefined())
      return bindings::NativeValueTraitsStringAdapter();
    return NativeValueTraits<IDLUSVString>::NativeValue(isolate, value,
                                                        exception_state);
  }
};

template <bindings::IDLStringConvMode mode>
struct NativeValueTraits<IDLStringStringContextTrustedHTMLBase<mode>>
    : public NativeValueTraitsBase<
          IDLStringStringContextTrustedHTMLBase<mode>> {
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state,
                            ExecutionContext* execution_context) {
    if (TrustedHTML* trusted_html =
            V8TrustedHTML::ToImplWithTypeCheck(isolate, value)) {
      return trusted_html->toString();
    }

    auto&& string = NativeValueTraits<IDLStringBase<mode>>::NativeValue(
        isolate, value, exception_state);
    if (exception_state.HadException())
      return String();
    return TrustedTypesCheckForHTML(string, execution_context, exception_state);
  }
};

template <>
struct CORE_EXPORT
    NativeValueTraits<IDLNullable<IDLStringStringContextTrustedHTML>>
    : public NativeValueTraitsBase<
          IDLNullable<IDLStringStringContextTrustedHTML>> {
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state,
                            ExecutionContext* execution_context) {
    return NativeValueTraits<IDLStringStringContextTrustedHTMLBase<
        bindings::IDLStringConvMode::kNullable>>::
        NativeValue(isolate, value, exception_state, execution_context);
  }
};

template <bindings::IDLStringConvMode mode>
struct NativeValueTraits<IDLStringStringContextTrustedScriptBase<mode>>
    : public NativeValueTraitsBase<
          IDLStringStringContextTrustedScriptBase<mode>> {
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state,
                            ExecutionContext* execution_context) {
    if (TrustedScript* trusted_script =
            V8TrustedScript::ToImplWithTypeCheck(isolate, value)) {
      return trusted_script->toString();
    }

    auto&& string = NativeValueTraits<IDLStringBase<mode>>::NativeValue(
        isolate, value, exception_state);
    if (exception_state.HadException())
      return String();
    return TrustedTypesCheckForScript(string, execution_context,
                                      exception_state);
  }
};

template <>
struct CORE_EXPORT
    NativeValueTraits<IDLNullable<IDLStringStringContextTrustedScript>>
    : public NativeValueTraitsBase<
          IDLNullable<IDLStringStringContextTrustedScript>> {
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state,
                            ExecutionContext* execution_context) {
    return NativeValueTraits<IDLStringStringContextTrustedScriptBase<
        bindings::IDLStringConvMode::kNullable>>::
        NativeValue(isolate, value, exception_state, execution_context);
  }
};

template <bindings::IDLStringConvMode mode>
struct NativeValueTraits<IDLUSVStringStringContextTrustedScriptURLBase<mode>>
    : public NativeValueTraitsBase<
          IDLUSVStringStringContextTrustedScriptURLBase<mode>> {
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state,
                            ExecutionContext* execution_context) {
    if (TrustedScriptURL* trusted_script_url =
            V8TrustedScriptURL::ToImplWithTypeCheck(isolate, value)) {
      return trusted_script_url->toString();
    }

    auto&& string = NativeValueTraits<IDLUSVStringBase<mode>>::NativeValue(
        isolate, value, exception_state);
    if (exception_state.HadException())
      return String();
    return TrustedTypesCheckForScriptURL(string, execution_context,
                                         exception_state);
  }
};

template <>
struct CORE_EXPORT
    NativeValueTraits<IDLNullable<IDLUSVStringStringContextTrustedScriptURL>>
    : public NativeValueTraitsBase<
          IDLNullable<IDLUSVStringStringContextTrustedScriptURL>> {
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state,
                            ExecutionContext* execution_context) {
    return NativeValueTraits<IDLUSVStringStringContextTrustedScriptURLBase<
        bindings::IDLStringConvMode::kNullable>>::
        NativeValue(isolate, value, exception_state, execution_context);
  }
};

// Buffer source types
template <>
struct CORE_EXPORT NativeValueTraits<DOMArrayBuffer>
    : public NativeValueTraitsBase<DOMArrayBuffer*> {
  static DOMArrayBuffer* NativeValue(v8::Isolate* isolate,
                                     v8::Local<v8::Value> value,
                                     ExceptionState& exception_state);

  static DOMArrayBuffer* ArgumentValue(v8::Isolate* isolate,
                                       int argument_index,
                                       v8::Local<v8::Value> value,
                                       ExceptionState& exception_state);
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLNullable<DOMArrayBuffer>>
    : public NativeValueTraitsBase<DOMArrayBuffer*> {
  static DOMArrayBuffer* NativeValue(v8::Isolate* isolate,
                                     v8::Local<v8::Value> value,
                                     ExceptionState& exception_state);

  static DOMArrayBuffer* ArgumentValue(v8::Isolate* isolate,
                                       int argument_index,
                                       v8::Local<v8::Value> value,
                                       ExceptionState& exception_state);
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLAllowResizable<DOMArrayBuffer>>
    : public NativeValueTraitsBase<DOMArrayBuffer*> {
  static DOMArrayBuffer* NativeValue(v8::Isolate* isolate,
                                     v8::Local<v8::Value> value,
                                     ExceptionState& exception_state);

  static DOMArrayBuffer* ArgumentValue(v8::Isolate* isolate,
                                       int argument_index,
                                       v8::Local<v8::Value> value,
                                       ExceptionState& exception_state);
};

template <>
struct CORE_EXPORT NativeValueTraits<DOMSharedArrayBuffer>
    : public NativeValueTraitsBase<DOMSharedArrayBuffer*> {
  static DOMSharedArrayBuffer* NativeValue(v8::Isolate* isolate,
                                           v8::Local<v8::Value> value,
                                           ExceptionState& exception_state);

  static DOMSharedArrayBuffer* ArgumentValue(v8::Isolate* isolate,
                                             int argument_index,
                                             v8::Local<v8::Value> value,
                                             ExceptionState& exception_state);
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLNullable<DOMSharedArrayBuffer>>
    : public NativeValueTraitsBase<DOMSharedArrayBuffer*> {
  static DOMSharedArrayBuffer* NativeValue(v8::Isolate* isolate,
                                           v8::Local<v8::Value> value,
                                           ExceptionState& exception_state);

  static DOMSharedArrayBuffer* ArgumentValue(v8::Isolate* isolate,
                                             int argument_index,
                                             v8::Local<v8::Value> value,
                                             ExceptionState& exception_state);
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLAllowResizable<DOMSharedArrayBuffer>>
    : public NativeValueTraitsBase<DOMSharedArrayBuffer*> {
  static DOMSharedArrayBuffer* NativeValue(v8::Isolate* isolate,
                                           v8::Local<v8::Value> value,
                                           ExceptionState& exception_state);

  static DOMSharedArrayBuffer* ArgumentValue(v8::Isolate* isolate,
                                             int argument_index,
                                             v8::Local<v8::Value> value,
                                             ExceptionState& exception_state);
};

// DOMArrayBufferBase is the common base class of DOMArrayBuffer and
// DOMSharedArrayBuffer, so it behaves as "[AllowShared] ArrayBuffer" in
// Web IDL.
template <>
struct CORE_EXPORT NativeValueTraits<DOMArrayBufferBase>
    : public NativeValueTraitsBase<DOMArrayBufferBase*> {
  static DOMArrayBufferBase* NativeValue(v8::Isolate* isolate,
                                         v8::Local<v8::Value> value,
                                         ExceptionState& exception_state);

  static DOMArrayBufferBase* ArgumentValue(v8::Isolate* isolate,
                                           int argument_index,
                                           v8::Local<v8::Value> value,
                                           ExceptionState& exception_state);
};

template <>
struct CORE_EXPORT
    NativeValueTraits<IDLBufferSourceTypeNoSizeLimit<DOMArrayBufferBase>>
    : public NativeValueTraitsBase<DOMArrayBufferBase*> {
  // BufferSourceTypeNoSizeLimit must be used only as arguments.
  static DOMArrayBufferBase* NativeValue(v8::Isolate* isolate,
                                         v8::Local<v8::Value> value,
                                         ExceptionState& exception_state) =
      delete;

  static DOMArrayBufferBase* ArgumentValue(v8::Isolate* isolate,
                                           int argument_index,
                                           v8::Local<v8::Value> value,
                                           ExceptionState& exception_state);
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLNullable<DOMArrayBufferBase>>
    : public NativeValueTraitsBase<DOMArrayBufferBase*> {
  static DOMArrayBufferBase* NativeValue(v8::Isolate* isolate,
                                         v8::Local<v8::Value> value,
                                         ExceptionState& exception_state);

  static DOMArrayBufferBase* ArgumentValue(v8::Isolate* isolate,
                                           int argument_index,
                                           v8::Local<v8::Value> value,
                                           ExceptionState& exception_state);
};

template <>
struct CORE_EXPORT NativeValueTraits<
    IDLNullable<IDLBufferSourceTypeNoSizeLimit<DOMArrayBufferBase>>>
    : public NativeValueTraitsBase<DOMArrayBufferBase*> {
  // BufferSourceTypeNoSizeLimit must be used only as arguments.
  static DOMArrayBufferBase* NativeValue(v8::Isolate* isolate,
                                         v8::Local<v8::Value> value,
                                         ExceptionState& exception_state) =
      delete;

  static DOMArrayBufferBase* ArgumentValue(v8::Isolate* isolate,
                                           int argument_index,
                                           v8::Local<v8::Value> value,
                                           ExceptionState& exception_state);
};

template <typename T>
struct NativeValueTraits<
    T,
    typename std::enable_if_t<std::is_base_of<DOMArrayBufferView, T>::value>> {
  // NotShared<T> or MaybeShared<T> should be used instead.
  static T* NativeValue(v8::Isolate* isolate,
                        v8::Local<v8::Value> value,
                        ExceptionState& exception_state) = delete;
  static T* ArgumentValue(v8::Isolate* isolate,
                          int argument_index,
                          v8::Local<v8::Value> value,
                          ExceptionState& exception_state) = delete;
};

template <typename T>
struct NativeValueTraits<
    IDLNullable<T>,
    typename std::enable_if_t<std::is_base_of<DOMArrayBufferView, T>::value>> {
  // NotShared<T> or MaybeShared<T> should be used instead.
  static T* NativeValue(v8::Isolate* isolate,
                        v8::Local<v8::Value> value,
                        ExceptionState& exception_state) = delete;
  static T* ArgumentValue(v8::Isolate* isolate,
                          int argument_index,
                          v8::Local<v8::Value> value,
                          ExceptionState& exception_state) = delete;
};

template <typename T>
struct NativeValueTraits<
    NotShared<T>,
    typename std::enable_if_t<std::is_base_of<DOMArrayBufferView, T>::value>>
    : public NativeValueTraitsBase<NotShared<T>> {
  static NotShared<T> NativeValue(v8::Isolate* isolate,
                                  v8::Local<v8::Value> value,
                                  ExceptionState& exception_state);

  static NotShared<T> ArgumentValue(v8::Isolate* isolate,
                                    int argument_index,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state);
};

template <typename T>
struct NativeValueTraits<
    IDLNullable<NotShared<T>>,
    typename std::enable_if_t<std::is_base_of<DOMArrayBufferView, T>::value>>
    : public NativeValueTraitsBase<NotShared<T>> {
  static NotShared<T> NativeValue(v8::Isolate* isolate,
                                  v8::Local<v8::Value> value,
                                  ExceptionState& exception_state);

  static NotShared<T> ArgumentValue(v8::Isolate* isolate,
                                    int argument_index,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state);
};

template <typename T>
struct NativeValueTraits<
    MaybeShared<T>,
    typename std::enable_if_t<std::is_base_of<DOMArrayBufferView, T>::value>>
    : public NativeValueTraitsBase<MaybeShared<T>> {
  static MaybeShared<T> NativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state);

  static MaybeShared<T> ArgumentValue(v8::Isolate* isolate,
                                      int argument_index,
                                      v8::Local<v8::Value> value,
                                      ExceptionState& exception_state);
};

template <typename T>
struct NativeValueTraits<
    IDLBufferSourceTypeNoSizeLimit<MaybeShared<T>>,
    typename std::enable_if_t<std::is_base_of<DOMArrayBufferView, T>::value>>
    : public NativeValueTraitsBase<MaybeShared<T>> {
  // FlexibleArrayBufferView uses this in its implementation, so we cannot
  // delete it.
  static MaybeShared<T> NativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state);

  static MaybeShared<T> ArgumentValue(v8::Isolate* isolate,
                                      int argument_index,
                                      v8::Local<v8::Value> value,
                                      ExceptionState& exception_state);
};

template <typename T>
struct NativeValueTraits<
    IDLNullable<MaybeShared<T>>,
    typename std::enable_if_t<std::is_base_of<DOMArrayBufferView, T>::value>>
    : public NativeValueTraitsBase<MaybeShared<T>> {
  static MaybeShared<T> NativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state);

  static MaybeShared<T> ArgumentValue(v8::Isolate* isolate,
                                      int argument_index,
                                      v8::Local<v8::Value> value,
                                      ExceptionState& exception_state);
};

template <typename T>
struct NativeValueTraits<
    IDLNullable<IDLBufferSourceTypeNoSizeLimit<MaybeShared<T>>>,
    typename std::enable_if_t<std::is_base_of<DOMArrayBufferView, T>::value>>
    : public NativeValueTraitsBase<MaybeShared<T>> {
  // BufferSourceTypeNoSizeLimit must be used only as arguments.
  static MaybeShared<T> NativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state) = delete;

  static MaybeShared<T> ArgumentValue(v8::Isolate* isolate,
                                      int argument_index,
                                      v8::Local<v8::Value> value,
                                      ExceptionState& exception_state);
};

template <typename T>
struct NativeValueTraits<
    T,
    typename std::enable_if_t<
        std::is_base_of<FlexibleArrayBufferView, T>::value>>
    : public NativeValueTraitsBase<T> {
  // FlexibleArrayBufferView must be used only as arguments.
  static T NativeValue(v8::Isolate* isolate,
                       v8::Local<v8::Value> value,
                       ExceptionState& exception_state) = delete;

  static T ArgumentValue(v8::Isolate* isolate,
                         int argument_index,
                         v8::Local<v8::Value> value,
                         ExceptionState& exception_state);
};

template <typename T>
struct NativeValueTraits<
    IDLBufferSourceTypeNoSizeLimit<T>,
    typename std::enable_if_t<
        std::is_base_of<FlexibleArrayBufferView, T>::value>>
    : public NativeValueTraitsBase<T> {
  // BufferSourceTypeNoSizeLimit and FlexibleArrayBufferView must be used only
  // as arguments.
  static T NativeValue(v8::Isolate* isolate,
                       v8::Local<v8::Value> value,
                       ExceptionState& exception_state) = delete;

  static T ArgumentValue(v8::Isolate* isolate,
                         int argument_index,
                         v8::Local<v8::Value> value,
                         ExceptionState& exception_state);
};

template <typename T>
struct NativeValueTraits<
    IDLNullable<T>,
    typename std::enable_if_t<
        std::is_base_of<FlexibleArrayBufferView, T>::value>>
    : public NativeValueTraitsBase<T> {
  // FlexibleArrayBufferView must be used only as arguments.
  static T NativeValue(v8::Isolate* isolate,
                       v8::Local<v8::Value> value,
                       ExceptionState& exception_state) = delete;

  static T ArgumentValue(v8::Isolate* isolate,
                         int argument_index,
                         v8::Local<v8::Value> value,
                         ExceptionState& exception_state);
};

// object
template <>
struct CORE_EXPORT NativeValueTraits<IDLObject>
    : public NativeValueTraitsBase<IDLObject> {
  static ScriptValue NativeValue(v8::Isolate* isolate,
                                 v8::Local<v8::Value> value,
                                 ExceptionState& exception_state) {
    if (value->IsObject())
      return ScriptValue(isolate, value);
    exception_state.ThrowTypeError(
        ExceptionMessages::FailedToConvertJSValue("object"));
    return ScriptValue();
  }

  static ScriptValue ArgumentValue(v8::Isolate* isolate,
                                   int argument_index,
                                   v8::Local<v8::Value> value,
                                   ExceptionState& exception_state) {
    if (value->IsObject())
      return ScriptValue(isolate, value);
    exception_state.ThrowTypeError(
        ExceptionMessages::ArgumentNotOfType(argument_index, "object"));
    return ScriptValue();
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLNullable<IDLObject>>
    : public NativeValueTraitsBase<IDLNullable<IDLObject>> {
  static ScriptValue NativeValue(v8::Isolate* isolate,
                                 v8::Local<v8::Value> value,
                                 ExceptionState& exception_state) {
    if (value->IsObject())
      return ScriptValue(isolate, value);
    if (value->IsNullOrUndefined())
      return ScriptValue(isolate, v8::Null(isolate));
    exception_state.ThrowTypeError(
        ExceptionMessages::FailedToConvertJSValue("object"));
    return ScriptValue();
  }

  static ScriptValue ArgumentValue(v8::Isolate* isolate,
                                   int argument_index,
                                   v8::Local<v8::Value> value,
                                   ExceptionState& exception_state) {
    if (value->IsObject())
      return ScriptValue(isolate, value);
    if (value->IsNullOrUndefined())
      return ScriptValue(isolate, v8::Null(isolate));
    exception_state.ThrowTypeError(
        ExceptionMessages::ArgumentNotOfType(argument_index, "object"));
    return ScriptValue();
  }
};

// Promise types
template <>
struct CORE_EXPORT NativeValueTraits<IDLPromise>
    : public NativeValueTraitsBase<IDLPromise> {
  static ScriptPromise NativeValue(v8::Isolate* isolate,
                                   v8::Local<v8::Value> value,
                                   ExceptionState& exception_state) {
    return ScriptPromise::Cast(ScriptState::From(isolate->GetCurrentContext()),
                               value);
  }
};

// IDLNullable<IDLPromise> must not be used.
template <>
struct NativeValueTraits<IDLNullable<IDLPromise>>;

// Sequence types

// IDLSequence's implementation is a little tricky due to a historical reason.
// The following type mapping is used for IDLSequence and its variants.
//
// tl;dr: Only IDLNullable<IDLSequence<traceable_type>> is a reference type.
//   The others are value types.
//
// - IDLSequence<T> where T is not traceable
//   => Vector<T> as a value type
// - IDLSequence<T> where T is traceable
//   => HeapVector<T> as a value type despite that HeapVector is
//      GarbageCollected because HeapVector had been implemented as a non-GC
//      type (a value type) for years until 2021 January.  This point is very
//      inconsistent but kept unchanged so far.
// - IDLNullable<IDLSequence<T>> where T is not traceable
//   => absl::optional<Vector<T>> as a value type
// - IDLNullable<IDLSequence<T>> where T is traceable
//   => HeapVector<T>* as a reference type.  absl::optional<HeapVector<T>> is
//      not an option because it's not appropriately traceable despite that
//      the content HeapVector needs tracing.  As same as other
//      GarbageCollected types, pointer type is used to represent IDL nullable
//      type.
template <typename T>
struct NativeValueTraits<IDLSequence<T>>
    : public NativeValueTraitsBase<IDLSequence<T>> {
  // Nondependent types need to be explicitly qualified to be accessible.
  using typename NativeValueTraitsBase<IDLSequence<T>>::ImplType;

  // HeapVector is GarbageCollected, so HeapVector<T>* is used for IDLNullable
  // while absl::optional<Vector<T>> is used for IDLNullable<Vector<T>>.
  static constexpr bool has_null_value = WTF::IsTraceable<T>::value;

  // https://webidl.spec.whatwg.org/#es-sequence
  static ImplType NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state);
};

namespace bindings {

// Slow case: follow WebIDL's "Creating a sequence from an iterable" steps to
// iterate through each element.
template <typename T>
typename NativeValueTraits<IDLSequence<T>>::ImplType
CreateIDLSequenceFromIterator(v8::Isolate* isolate,
                              ScriptIterator script_iterator,
                              ExceptionState& exception_state) {
  // https://webidl.spec.whatwg.org/#create-sequence-from-iterable
  ExecutionContext* execution_context =
      ToExecutionContext(isolate->GetCurrentContext());
  typename NativeValueTraits<IDLSequence<T>>::ImplType result;
  // 3. Repeat:
  while (script_iterator.Next(execution_context, exception_state)) {
    // 3.1. Let next be ? IteratorStep(iter).
    DCHECK(!exception_state.HadException());
    // 3.3. Let nextItem be ? IteratorValue(next).
    //
    // The value should already be non-empty, as guaranteed by the call to
    // Next() and the |exception_state| check above.
    v8::Local<v8::Value> v8_element =
        script_iterator.GetValue().ToLocalChecked();
    // 3.4. Initialize Si to the result of converting nextItem to an IDL value
    //   of type T.
    auto&& element =
        NativeValueTraits<T>::NativeValue(isolate, v8_element, exception_state);
    if (exception_state.HadException())
      return {};
    result.push_back(std::move(element));
  }
  if (exception_state.HadException())
    return {};
  // 3.2. If next is false, then return an IDL sequence value of type
  //   sequence<T> of length i, where the value of the element at index j is Sj.
  return result;
}

// Faster case: non template-specialized implementation that iterates over an
// Array that adheres to %ArrayIteratorPrototype%'s protocol.
template <typename T>
typename NativeValueTraits<IDLSequence<T>>::ImplType
CreateIDLSequenceFromV8ArraySlow(v8::Isolate* isolate,
                                 v8::Local<v8::Array> v8_array,
                                 ExceptionState& exception_state) {
  // https://webidl.spec.whatwg.org/#create-sequence-from-iterable
  const uint32_t length = v8_array->Length();
  if (length > NativeValueTraits<IDLSequence<T>>::ImplType::MaxCapacity()) {
    exception_state.ThrowRangeError("Array length exceeds supported limit.");
    return {};
  }

  typename NativeValueTraits<IDLSequence<T>>::ImplType result;
  result.ReserveInitialCapacity(length);
  v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
  v8::TryCatch try_block(isolate);
  // Array length may change if array is mutated during iteration.
  for (uint32_t i = 0; i < v8_array->Length(); ++i) {
    v8::Local<v8::Value> v8_element;
    if (!v8_array->Get(current_context, i).ToLocal(&v8_element)) {
      exception_state.RethrowV8Exception(try_block.Exception());
      return {};
    }
    // 3.4. Initialize Si to the result of converting nextItem to an IDL value
    //   of type T.
    auto&& element =
        NativeValueTraits<T>::NativeValue(isolate, v8_element, exception_state);
    if (exception_state.HadException())
      return {};
    result.push_back(std::move(element));
  }
  // 3.2. If next is false, then return an IDL sequence value of type
  //   sequence<T> of length i, where the value of the element at index j is Sj.
  return result;
}

// Fastest case: template-specialized implementation that directly copies the
// contents of this JavaScript array into a C++ buffer.
template <typename T>
typename NativeValueTraits<IDLSequence<T>>::ImplType
CreateIDLSequenceFromV8Array(v8::Isolate* isolate,
                             v8::Local<v8::Array> v8_array,
                             ExceptionState& exception_state) {
  return CreateIDLSequenceFromV8ArraySlow<T>(isolate, v8_array,
                                             exception_state);
}

template <>
CORE_EXTERN_TEMPLATE_EXPORT
    typename NativeValueTraits<IDLSequence<IDLLong>>::ImplType
    CreateIDLSequenceFromV8Array<IDLLong>(v8::Isolate* isolate,
                                          v8::Local<v8::Array> v8_array,
                                          ExceptionState& exception_state);

}  // namespace bindings

template <typename T>
typename NativeValueTraits<IDLSequence<T>>::ImplType
NativeValueTraits<IDLSequence<T>>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  // TODO(https://crbug.com/715122): Checking for IsArray() may not be
  // enough. Other engines also prefer regular array iteration over a custom
  // @@iterator when the latter is defined, but it is not clear if this is a
  // valid optimization.
  if (value->IsArray()) {
    return bindings::CreateIDLSequenceFromV8Array<T>(
        isolate, value.As<v8::Array>(), exception_state);
  }

  // 1. If Type(V) is not Object, throw a TypeError.
  if (!value->IsObject()) {
    exception_state.ThrowTypeError(
        "The provided value cannot be converted to a sequence.");
    return ImplType();
  }

  // 2. Let method be ? GetMethod(V, @@iterator).
  // 3. If method is undefined, throw a TypeError.
  // 4. Return the result of creating a sequence from V and method.
  auto script_iterator = ScriptIterator::FromIterable(
      isolate, value.As<v8::Object>(), exception_state);
  if (exception_state.HadException())
    return ImplType();
  if (script_iterator.IsNull()) {
    // A null ScriptIterator with an empty |exception_state| means the
    // object is lacking a callable @@iterator property.
    exception_state.ThrowTypeError(
        "The object must have a callable @@iterator property.");
    return ImplType();
  }
  return bindings::CreateIDLSequenceFromIterator<T>(
      isolate, std::move(script_iterator), exception_state);
}

template <typename T>
struct NativeValueTraits<IDLNullable<IDLSequence<T>>,
                         typename std::enable_if_t<
                             NativeValueTraits<IDLSequence<T>>::has_null_value>>
    : public NativeValueTraitsBase<HeapVector<AddMemberIfNeeded<T>>*> {
  using ImplType = typename NativeValueTraits<IDLSequence<T>>::ImplType*;

  static ImplType NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    if (value->IsNullOrUndefined())
      return nullptr;

    auto on_stack = NativeValueTraits<IDLSequence<T>>::NativeValue(
        isolate, value, exception_state);
    if (exception_state.HadException())
      return nullptr;
    auto* on_heap = MakeGarbageCollected<HeapVector<AddMemberIfNeeded<T>>>();
    on_heap->swap(on_stack);
    return on_heap;
  }

  static ImplType ArgumentValue(v8::Isolate* isolate,
                                int argument_index,
                                v8::Local<v8::Value> value,
                                ExceptionState& exception_state) {
    if (value->IsNullOrUndefined())
      return nullptr;

    auto on_stack = NativeValueTraits<IDLSequence<T>>::ArgumentValue(
        isolate, argument_index, value, exception_state);
    if (exception_state.HadException())
      return nullptr;
    auto* on_heap = MakeGarbageCollected<HeapVector<AddMemberIfNeeded<T>>>();
    on_heap->swap(on_stack);
    return on_heap;
  }
};

template <typename T>
struct NativeValueTraits<IDLOptional<IDLSequence<T>>>
    : public NativeValueTraitsBase<IDLOptional<IDLSequence<T>>> {
  static typename NativeValueTraits<IDLSequence<T>>::ImplType NativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exception_state) {
    if (value->IsUndefined())
      return {};
    return NativeValueTraits<IDLSequence<T>>::NativeValue(isolate, value,
                                                          exception_state);
  }

  static typename NativeValueTraits<IDLSequence<T>>::ImplType ArgumentValue(
      v8::Isolate* isolate,
      int argument_index,
      v8::Local<v8::Value> value,
      ExceptionState& exception_state) {
    if (value->IsUndefined())
      return {};
    return NativeValueTraits<IDLSequence<T>>::ArgumentValue(
        isolate, argument_index, value, exception_state);
  }
};

// Frozen array types
template <typename T>
struct NativeValueTraits<IDLArray<T>>
    : public NativeValueTraits<IDLSequence<T>> {};

template <typename T>
struct NativeValueTraits<IDLNullable<IDLArray<T>>,
                         typename std::enable_if_t<
                             NativeValueTraits<IDLSequence<T>>::has_null_value>>
    : public NativeValueTraits<IDLNullable<IDLSequence<T>>> {};

// Record types
template <typename K, typename V>
struct NativeValueTraits<IDLRecord<K, V>>
    : public NativeValueTraitsBase<IDLRecord<K, V>> {
  // Nondependent types need to be explicitly qualified to be accessible.
  using typename NativeValueTraitsBase<IDLRecord<K, V>>::ImplType;

  // Converts a JavaScript value |O| to an IDL record<K, V> value. In C++, a
  // record is represented as a Vector<std::pair<k, v>> (or a HeapVector if
  // necessary). See https://webidl.spec.whatwg.org/#es-record.
  static ImplType NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> v8_value,
                              ExceptionState& exception_state) {
    v8::Local<v8::Context> context = isolate->GetCurrentContext();

    // "1. If Type(O) is not Object, throw a TypeError."
    if (!v8_value->IsObject()) {
      exception_state.ThrowTypeError(
          "Only objects can be converted to record<K,V> types");
      return ImplType();
    }
    v8::Local<v8::Object> v8_object = v8::Local<v8::Object>::Cast(v8_value);
    v8::TryCatch block(isolate);

    // "3. Let keys be ? O.[[OwnPropertyKeys]]()."
    v8::Local<v8::Array> keys;
    // While we could pass v8::ONLY_ENUMERABLE below, doing so breaks
    // web-platform-tests' headers-record.html. It might be worthwhile to try
    // changing the test.
    if (!v8_object
             ->GetOwnPropertyNames(context,
                                   static_cast<v8::PropertyFilter>(
                                       v8::PropertyFilter::ALL_PROPERTIES),
                                   v8::KeyConversionMode::kConvertToString)
             .ToLocal(&keys)) {
      exception_state.RethrowV8Exception(block.Exception());
      return ImplType();
    }
    if (keys->Length() > ImplType::MaxCapacity()) {
      exception_state.ThrowRangeError("Array length exceeds supported limit.");
      return ImplType();
    }

    // "2. Let result be a new empty instance of record<K, V>."
    ImplType result;
    result.ReserveInitialCapacity(keys->Length());

    // The conversion algorithm needs a data structure with fast insertion at
    // the end while at the same time requiring fast checks for previous insert
    // of a given key. |seenKeys| is a key/position in |result| map that aids in
    // the latter part.
    HashMap<String, uint32_t> seen_keys;

    for (uint32_t i = 0; i < keys->Length(); ++i) {
      // "4. Repeat, for each element key of keys in List order:"
      v8::Local<v8::Value> key;
      if (!keys->Get(context, i).ToLocal(&key)) {
        exception_state.RethrowV8Exception(block.Exception());
        return ImplType();
      }

      // "4.1. Let desc be ? O.[[GetOwnProperty]](key)."
      v8::Local<v8::Value> desc;
      if (!v8_object->GetOwnPropertyDescriptor(context, key.As<v8::Name>())
               .ToLocal(&desc)) {
        exception_state.RethrowV8Exception(block.Exception());
        return ImplType();
      }

      // "4.2. If desc is not undefined and desc.[[Enumerable]] is true:"
      // We can call ToLocalChecked() and ToChecked() here because
      // GetOwnPropertyDescriptor is responsible for catching any exceptions
      // and failures, and if we got to this point of the code we have a proper
      // object that was not created by a user.
      if (desc->IsUndefined())
        continue;
      DCHECK(desc->IsObject());
      v8::Local<v8::Value> enumerable =
          v8::Local<v8::Object>::Cast(desc)
              ->Get(context, V8AtomicString(isolate, "enumerable"))
              .ToLocalChecked();
      if (!enumerable->BooleanValue(isolate))
        continue;

      // "4.2.1. Let typedKey be key converted to an IDL value of type K."
      String typed_key =
          NativeValueTraits<K>::NativeValue(isolate, key, exception_state);
      if (exception_state.HadException())
        return ImplType();

      // "4.2.2. Let value be ? Get(O, key)."
      v8::Local<v8::Value> value;
      if (!v8_object->Get(context, key).ToLocal(&value)) {
        exception_state.RethrowV8Exception(block.Exception());
        return ImplType();
      }

      // "4.2.3. Let typedValue be value converted to an IDL value of type V."
      typename ImplType::ValueType::second_type typed_value =
          NativeValueTraits<V>::NativeValue(isolate, value, exception_state);
      if (exception_state.HadException())
        return ImplType();

      if (seen_keys.Contains(typed_key)) {
        // "4.2.4. If typedKey is already a key in result, set its value to
        //         typedValue.
        //         Note: This can happen when O is a proxy object."
        const uint32_t pos = seen_keys.at(typed_key);
        result[pos].second = std::move(typed_value);
      } else {
        // "4.2.5. Otherwise, append to result a mapping (typedKey,
        // typedValue)."
        // Note we can take this shortcut because we are always appending.
        const uint32_t pos = result.size();
        seen_keys.Set(typed_key, pos);
        result.UncheckedAppend(
            std::make_pair(std::move(typed_key), std::move(typed_value)));
      }
    }
    // "5. Return result."
    return result;
  }
};

// Callback function types
template <typename T>
struct NativeValueTraits<
    T,
    typename std::enable_if_t<std::is_base_of<CallbackFunctionBase, T>::value>>
    : public NativeValueTraitsBase<T*> {
  static T* NativeValue(v8::Isolate* isolate,
                        v8::Local<v8::Value> value,
                        ExceptionState& exception_state) {
    if (value->IsFunction())
      return T::Create(value.As<v8::Function>());
    exception_state.ThrowTypeError("The given value is not a function.");
    return nullptr;
  }

  static T* ArgumentValue(v8::Isolate* isolate,
                          int argument_index,
                          v8::Local<v8::Value> value,
                          ExceptionState& exception_state) {
    if (value->IsFunction())
      return T::Create(value.As<v8::Function>());
    exception_state.ThrowTypeError(
        ExceptionMessages::ArgumentNotOfType(argument_index, "Function"));
    return nullptr;
  }
};

template <typename T>
struct NativeValueTraits<
    IDLNullable<T>,
    typename std::enable_if_t<std::is_base_of<CallbackFunctionBase, T>::value>>
    : public NativeValueTraitsBase<IDLNullable<T>> {
  static T* NativeValue(v8::Isolate* isolate,
                        v8::Local<v8::Value> value,
                        ExceptionState& exception_state) {
    if (value->IsFunction())
      return T::Create(value.As<v8::Function>());
    if (value->IsNullOrUndefined())
      return nullptr;
    exception_state.ThrowTypeError("The given value is not a function.");
    return nullptr;
  }

  static T* ArgumentValue(v8::Isolate* isolate,
                          int argument_index,
                          v8::Local<v8::Value> value,
                          ExceptionState& exception_state) {
    if (value->IsFunction())
      return T::Create(value.As<v8::Function>());
    if (value->IsNullOrUndefined())
      return nullptr;
    exception_state.ThrowTypeError(
        ExceptionMessages::ArgumentNotOfType(argument_index, "Function"));
    return nullptr;
  }
};

// Callback interface types
template <typename T>
struct NativeValueTraits<
    T,
    typename std::enable_if_t<std::is_base_of<CallbackInterfaceBase, T>::value>>
    : public NativeValueTraitsBase<T*> {
  static T* NativeValue(v8::Isolate* isolate,
                        v8::Local<v8::Value> value,
                        ExceptionState& exception_state) {
    if (value->IsObject())
      return T::Create(value.As<v8::Object>());
    exception_state.ThrowTypeError("The given value is not an object.");
    return nullptr;
  }

  static T* ArgumentValue(v8::Isolate* isolate,
                          int argument_index,
                          v8::Local<v8::Value> value,
                          ExceptionState& exception_state) {
    if (value->IsObject())
      return T::Create(value.As<v8::Object>());
    exception_state.ThrowTypeError(
        ExceptionMessages::ArgumentNotOfType(argument_index, "Object"));
    return nullptr;
  }
};

template <typename T>
struct NativeValueTraits<
    IDLNullable<T>,
    typename std::enable_if_t<std::is_base_of<CallbackInterfaceBase, T>::value>>
    : public NativeValueTraitsBase<IDLNullable<T>> {
  static T* NativeValue(v8::Isolate* isolate,
                        v8::Local<v8::Value> value,
                        ExceptionState& exception_state) {
    if (value->IsObject())
      return T::Create(value.As<v8::Object>());
    if (value->IsNullOrUndefined())
      return nullptr;
    exception_state.ThrowTypeError("The given value is not an object.");
    return nullptr;
  }

  static T* ArgumentValue(v8::Isolate* isolate,
                          int argument_index,
                          v8::Local<v8::Value> value,
                          ExceptionState& exception_state) {
    if (value->IsObject())
      return T::Create(value.As<v8::Object>());
    if (value->IsNullOrUndefined())
      return nullptr;
    exception_state.ThrowTypeError(
        ExceptionMessages::ArgumentNotOfType(argument_index, "Object"));
    return nullptr;
  }
};

// Dictionary types
template <typename T>
struct NativeValueTraits<
    T,
    typename std::enable_if_t<
        std::is_base_of<bindings::DictionaryBase, T>::value>>
    : public NativeValueTraitsBase<T*> {
  static T* NativeValue(v8::Isolate* isolate,
                        v8::Local<v8::Value> value,
                        ExceptionState& exception_state) {
    return T::Create(isolate, value, exception_state);
  }
};

// We don't support nullable dictionary types in general since it's quite
// confusing and often misused.
template <typename T>
struct NativeValueTraits<
    IDLNullable<T>,
    typename std::enable_if_t<
        std::is_base_of<bindings::DictionaryBase, T>::value &&
        (std::is_same<T, GPUColorTargetState>::value ||
         std::is_same<T, GPURenderPassColorAttachment>::value ||
         std::is_same<T, GPUVertexBufferLayout>::value)>>
    : public NativeValueTraitsBase<T*> {
  static T* NativeValue(v8::Isolate* isolate,
                        v8::Local<v8::Value> value,
                        ExceptionState& exception_state) {
    if (value->IsNullOrUndefined())
      return nullptr;
    return T::Create(isolate, value, exception_state);
  }
};

// Enumeration types
template <typename T>
struct NativeValueTraits<
    T,
    typename std::enable_if_t<
        std::is_base_of<bindings::EnumerationBase, T>::value>>
    : public NativeValueTraitsBase<T> {
  static T NativeValue(v8::Isolate* isolate,
                       v8::Local<v8::Value> value,
                       ExceptionState& exception_state) {
    return T::Create(isolate, value, exception_state);
  }
};

// Interface types
template <typename T>
struct NativeValueTraits<
    T,
    typename std::enable_if_t<std::is_base_of<ScriptWrappable, T>::value>>
    : public NativeValueTraitsBase<T*> {
  static inline T* NativeValue(v8::Isolate* isolate,
                               v8::Local<v8::Value> value,
                               ExceptionState& exception_state) {
    const WrapperTypeInfo* wrapper_type_info = T::GetStaticWrapperTypeInfo();
    if (V8PerIsolateData::From(isolate)->HasInstance(wrapper_type_info, value))
      return ToScriptWrappable(value.As<v8::Object>())->template ToImpl<T>();

    bindings::NativeValueTraitsInterfaceNotOfType(wrapper_type_info,
                                                  exception_state);
    return nullptr;
  }

  static inline T* ArgumentValue(v8::Isolate* isolate,
                                 int argument_index,
                                 v8::Local<v8::Value> value,
                                 ExceptionState& exception_state) {
    const WrapperTypeInfo* wrapper_type_info = T::GetStaticWrapperTypeInfo();
    if (V8PerIsolateData::From(isolate)->HasInstance(wrapper_type_info, value))
      return ToScriptWrappable(value.As<v8::Object>())->template ToImpl<T>();

    bindings::NativeValueTraitsInterfaceNotOfType(
        wrapper_type_info, argument_index, exception_state);
    return nullptr;
  }
};

template <typename T>
struct NativeValueTraits<
    IDLNullable<T>,
    typename std::enable_if_t<std::is_base_of<ScriptWrappable, T>::value>>
    : public NativeValueTraitsBase<IDLNullable<T>> {
  static inline T* NativeValue(v8::Isolate* isolate,
                               v8::Local<v8::Value> value,
                               ExceptionState& exception_state) {
    const WrapperTypeInfo* wrapper_type_info = T::GetStaticWrapperTypeInfo();
    if (V8PerIsolateData::From(isolate)->HasInstance(wrapper_type_info, value))
      return ToScriptWrappable(value.As<v8::Object>())->template ToImpl<T>();

    if (value->IsNullOrUndefined())
      return nullptr;

    bindings::NativeValueTraitsInterfaceNotOfType(wrapper_type_info,
                                                  exception_state);
    return nullptr;
  }

  static inline T* ArgumentValue(v8::Isolate* isolate,
                                 int argument_index,
                                 v8::Local<v8::Value> value,
                                 ExceptionState& exception_state) {
    const WrapperTypeInfo* wrapper_type_info = T::GetStaticWrapperTypeInfo();
    if (V8PerIsolateData::From(isolate)->HasInstance(wrapper_type_info, value))
      return ToScriptWrappable(value.As<v8::Object>())->template ToImpl<T>();

    if (value->IsNullOrUndefined())
      return nullptr;

    bindings::NativeValueTraitsInterfaceNotOfType(
        wrapper_type_info, argument_index, exception_state);
    return nullptr;
  }
};

template <typename T>
struct NativeValueTraits<
    T,
    typename std::enable_if_t<std::is_base_of<bindings::UnionBase, T>::value>>
    : public NativeValueTraitsBase<T*> {
  static T* NativeValue(v8::Isolate* isolate,
                        v8::Local<v8::Value> value,
                        ExceptionState& exception_state) {
    return T::Create(isolate, value, exception_state);
  }

  static T* ArgumentValue(v8::Isolate* isolate,
                          int argument_index,
                          v8::Local<v8::Value> value,
                          ExceptionState& exception_state) {
    return T::Create(isolate, value, exception_state);
  }
};

template <typename T>
struct NativeValueTraits<
    IDLNullable<T>,
    typename std::enable_if_t<std::is_base_of<bindings::UnionBase, T>::value>>
    : public NativeValueTraitsBase<T*> {
  static T* NativeValue(v8::Isolate* isolate,
                        v8::Local<v8::Value> value,
                        ExceptionState& exception_state) {
    if (value->IsNullOrUndefined())
      return nullptr;
    return T::Create(isolate, value, exception_state);
  }

  static T* ArgumentValue(v8::Isolate* isolate,
                          int argument_index,
                          v8::Local<v8::Value> value,
                          ExceptionState& exception_state) {
    if (value->IsNullOrUndefined())
      return nullptr;
    return T::Create(isolate, value, exception_state);
  }
};

// Nullable types
template <typename InnerType>
struct NativeValueTraits<
    IDLNullable<InnerType>,
    typename std::enable_if_t<!NativeValueTraits<InnerType>::has_null_value>>
    : public NativeValueTraitsBase<IDLNullable<InnerType>> {
  // https://webidl.spec.whatwg.org/#es-nullable-type
  using ImplType =
      absl::optional<typename NativeValueTraits<InnerType>::ImplType>;

  static ImplType NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    if (value->IsNullOrUndefined())
      return absl::nullopt;
    return NativeValueTraits<InnerType>::NativeValue(isolate, value,
                                                     exception_state);
  }

  static ImplType ArgumentValue(v8::Isolate* isolate,
                                int argument_index,
                                v8::Local<v8::Value> value,
                                ExceptionState& exception_state) {
    if (value->IsNullOrUndefined())
      return absl::nullopt;
    return NativeValueTraits<InnerType>::ArgumentValue(isolate, argument_index,
                                                       value, exception_state);
  }
};

// IDLNullable<IDLNullable<T>> must not be used.
template <typename T>
struct NativeValueTraits<IDLNullable<IDLNullable<T>>>;

// Optional types
template <typename T>
struct NativeValueTraits<IDLOptional<T>,
                         typename std::enable_if_t<std::is_arithmetic<
                             typename NativeValueTraits<T>::ImplType>::value>>
    : public NativeValueTraitsBase<typename NativeValueTraits<T>::ImplType> {
  using ImplType = typename NativeValueTraits<T>::ImplType;

  static ImplType NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    // Just let ES undefined to be converted into 0.
    return NativeValueTraits<T>::NativeValue(isolate, value, exception_state);
  }

  static ImplType ArgumentValue(v8::Isolate* isolate,
                                int argument_index,
                                v8::Local<v8::Value> value,
                                ExceptionState& exception_state) {
    // Just let ES undefined to be converted into 0.
    return NativeValueTraits<T>::ArgumentValue(isolate, argument_index, value,
                                               exception_state);
  }
};

template <typename T>
struct NativeValueTraits<IDLOptional<T>,
                         typename std::enable_if_t<std::is_pointer<
                             typename NativeValueTraits<T>::ImplType>::value>>
    : public NativeValueTraitsBase<typename NativeValueTraits<T>::ImplType> {
  using ImplType = typename NativeValueTraits<T>::ImplType;

  static ImplType NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    if (value->IsUndefined())
      return nullptr;
    return NativeValueTraits<T>::NativeValue(isolate, value, exception_state);
  }

  static ImplType ArgumentValue(v8::Isolate* isolate,
                                int argument_index,
                                v8::Local<v8::Value> value,
                                ExceptionState& exception_state) {
    if (value->IsUndefined())
      return nullptr;
    return NativeValueTraits<T>::ArgumentValue(isolate, argument_index, value,
                                               exception_state);
  }
};

// Date
template <>
struct CORE_EXPORT NativeValueTraits<IDLDate>
    : public NativeValueTraitsBase<IDLDate> {
  // IDLDate must be always used as IDLNullable<IDLDate>.
  static absl::optional<base::Time> NativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exception_state) = delete;
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLNullable<IDLDate>>
    : public NativeValueTraitsBase<IDLNullable<IDLDate>> {
  static absl::optional<base::Time> NativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exception_state) {
    return ToCoreNullableDate(isolate, value, exception_state);
  }
};

// EventHandler
template <>
struct CORE_EXPORT NativeValueTraits<IDLEventHandler>
    : public NativeValueTraitsBase<IDLEventHandler> {
  static EventListener* NativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state);
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLOnBeforeUnloadEventHandler>
    : public NativeValueTraitsBase<IDLOnBeforeUnloadEventHandler> {
  static EventListener* NativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state);
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLOnErrorEventHandler>
    : public NativeValueTraitsBase<IDLOnErrorEventHandler> {
  static EventListener* NativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state);
};

// EventHandler and its family are nullable, so IDLNullable<IDLEventHandler>
// must not be used.
template <>
struct NativeValueTraits<IDLNullable<IDLEventHandler>>;
template <>
struct NativeValueTraits<IDLNullable<IDLOnBeforeUnloadEventHandler>>;
template <>
struct NativeValueTraits<IDLNullable<IDLOnErrorEventHandler>>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_NATIVE_VALUE_TRAITS_IMPL_H_
