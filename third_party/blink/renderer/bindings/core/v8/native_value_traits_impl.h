// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_NATIVE_VALUE_TRAITS_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_NATIVE_VALUE_TRAITS_IMPL_H_

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
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CallbackFunctionBase;
class CallbackInterfaceBase;
class EventListener;
class FlexibleArrayBufferView;
class IDLDictionaryBase;
class ScriptWrappable;
class XPathNSResolver;
struct WrapperTypeInfo;

namespace bindings {

class DictionaryBase;
class EnumerationBase;

CORE_EXPORT void NativeValueTraitsInterfaceNotOfType(
    const WrapperTypeInfo* wrapper_type_info,
    ExceptionState& exception_state);

CORE_EXPORT void NativeValueTraitsInterfaceNotOfType(
    const WrapperTypeInfo* wrapper_type_info,
    int argument_index,
    ExceptionState& exception_state);

}  // namespace bindings

// any
template <>
struct CORE_EXPORT NativeValueTraits<IDLAny>
    : public NativeValueTraitsBase<IDLAny> {
  static ScriptValue NativeValue(v8::Isolate* isolate,
                                 v8::Local<v8::Value> value,
                                 ExceptionState& exception_state) {
    return ScriptValue(isolate, value);
  }
};

// IDLNullable<IDLAny> must not be used.
template <>
struct NativeValueTraits<IDLNullable<IDLAny>>;

template <>
struct CORE_EXPORT NativeValueTraits<IDLOptional<IDLAny>>
    : public NativeValueTraitsBase<IDLAny> {
  static ScriptValue NativeValue(v8::Isolate* isolate,
                                 v8::Local<v8::Value> value,
                                 ExceptionState& exception_state) {
    return ScriptValue(isolate, value);
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
    : public NativeValueTraitsBase<IDLBoolean> {
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
template <V8StringResourceMode mode>
struct NativeValueTraits<IDLByteStringBase<mode>>
    : public NativeValueTraitsBase<IDLByteStringBase<mode>> {
  // http://heycam.github.io/webidl/#es-ByteString
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    V8StringResource<mode> string_resource(value);
    // 1. Let x be ToString(v)
    if (!string_resource.Prepare(isolate, exception_state))
      return String();
    String x = string_resource;
    // 2. If the value of any element of x is greater than 255, then throw a
    //    TypeError.
    if (!x.ContainsOnlyLatin1OrEmpty()) {
      exception_state.ThrowTypeError("Value is not a valid ByteString.");
      return String();
    }

    // 3. Return an IDL ByteString value whose length is the length of x, and
    //    where the value of each element is the value of the corresponding
    //    element of x.
    //    Blink: A ByteString is simply a String with a range constrained per
    //    the above, so this is the identity operation.
    return x;
  }
};

template <V8StringResourceMode mode>
struct NativeValueTraits<IDLStringBase<mode>>
    : public NativeValueTraitsBase<IDLStringBase<mode>> {
  // https://heycam.github.io/webidl/#es-DOMString
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    V8StringResource<mode> string(value);
    if (!string.Prepare(isolate, exception_state))
      return String();
    return string;
  }
};

template <V8StringResourceMode mode>
struct NativeValueTraits<IDLStringStringContextTrustedHTMLBase<mode>>
    : public NativeValueTraitsBase<
          IDLStringStringContextTrustedHTMLBase<mode>> {
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state,
                            ExecutionContext* execution_context) {
    if (V8TrustedHTML::HasInstance(value, isolate)) {
      TrustedHTML* trusted_value =
          V8TrustedHTML::ToImpl(v8::Local<v8::Object>::Cast(value));
      return trusted_value->toString();
    } else {
      V8StringResource<mode> string(value);
      if (!string.Prepare(isolate, exception_state))
        return String();
      return TrustedTypesCheckForHTML(string, execution_context,
                                      exception_state);
    }
  }
};

template <V8StringResourceMode mode>
struct NativeValueTraits<IDLStringStringContextTrustedScriptBase<mode>>
    : public NativeValueTraitsBase<
          IDLStringStringContextTrustedScriptBase<mode>> {
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state,
                            ExecutionContext* execution_context) {
    if (V8TrustedScript::HasInstance(value, isolate)) {
      TrustedScript* trusted_value =
          V8TrustedScript::ToImpl(v8::Local<v8::Object>::Cast(value));
      return trusted_value->toString();
    } else {
      V8StringResource<mode> string(value);
      if (!string.Prepare(isolate, exception_state))
        return String();
      return TrustedTypesCheckForScript(string, execution_context,
                                        exception_state);
    }
  }
};

template <V8StringResourceMode mode>
struct NativeValueTraits<IDLUSVStringStringContextTrustedScriptURLBase<mode>>
    : public NativeValueTraitsBase<
          IDLUSVStringStringContextTrustedScriptURLBase<mode>> {
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state,
                            ExecutionContext* execution_context) {
    if (V8TrustedScriptURL::HasInstance(value, isolate)) {
      TrustedScriptURL* trusted_value =
          V8TrustedScriptURL::ToImpl(v8::Local<v8::Object>::Cast(value));
      return trusted_value->toString();
    } else {
      V8StringResource<mode> string(value);
      if (!string.Prepare(isolate, exception_state))
        return String();
      return TrustedTypesCheckForScriptURL(string, execution_context,
                                           exception_state);
    }
  }
};

template <V8StringResourceMode mode>
struct NativeValueTraits<IDLUSVStringBase<mode>>
    : public NativeValueTraitsBase<IDLUSVStringBase<mode>> {
  // http://heycam.github.io/webidl/#es-USVString
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state) {
    // 1. Let string be the result of converting V to a DOMString.
    V8StringResource<mode> string(value);
    if (!string.Prepare(isolate, exception_state))
      return String();
    // 2. Return an IDL USVString value that is the result of converting string
    //    to a sequence of Unicode scalar values.
    return ReplaceUnmatchedSurrogates(string);
  }
};

// Strings for the new bindings generator

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
struct NativeValueTraits<IDLByteStringBaseV2<mode>>
    : public NativeValueTraitsBase<IDLByteStringBaseV2<mode>> {
  // http://heycam.github.io/webidl/#es-ByteString
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
struct CORE_EXPORT NativeValueTraits<IDLNullable<IDLByteStringV2>>
    : public NativeValueTraitsBase<IDLNullable<IDLByteStringV2>> {
  static decltype(auto) NativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state) {
    return NativeValueTraits<IDLByteStringBaseV2<
        bindings::IDLStringConvMode::kNullable>>::NativeValue(isolate, value,
                                                              exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLOptional<IDLByteStringV2>>
    : public NativeValueTraitsBase<IDLByteStringV2> {
  static decltype(auto) NativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state) {
    if (value->IsUndefined())
      return bindings::NativeValueTraitsStringAdapter();
    return NativeValueTraits<IDLByteStringV2>::NativeValue(isolate, value,
                                                           exception_state);
  }
};

template <bindings::IDLStringConvMode mode>
struct NativeValueTraits<IDLStringBaseV2<mode>>
    : public NativeValueTraitsBase<IDLStringBaseV2<mode>> {
  // https://heycam.github.io/webidl/#es-DOMString
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
struct CORE_EXPORT NativeValueTraits<IDLNullable<IDLStringV2>>
    : public NativeValueTraitsBase<IDLNullable<IDLStringV2>> {
  static decltype(auto) NativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state) {
    return NativeValueTraits<IDLStringBaseV2<
        bindings::IDLStringConvMode::kNullable>>::NativeValue(isolate, value,
                                                              exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLOptional<IDLStringV2>>
    : public NativeValueTraitsBase<IDLStringV2> {
  static decltype(auto) NativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state) {
    if (value->IsUndefined())
      return bindings::NativeValueTraitsStringAdapter();
    return NativeValueTraits<IDLStringV2>::NativeValue(isolate, value,
                                                       exception_state);
  }
};

template <bindings::IDLStringConvMode mode>
struct NativeValueTraits<IDLUSVStringBaseV2<mode>>
    : public NativeValueTraitsBase<IDLUSVStringBaseV2<mode>> {
  // http://heycam.github.io/webidl/#es-USVString
  static bindings::NativeValueTraitsStringAdapter NativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exception_state) {
    String string = NativeValueTraits<IDLStringBaseV2<mode>>::NativeValue(
        isolate, value, exception_state);
    if (exception_state.HadException())
      return bindings::NativeValueTraitsStringAdapter();

    return bindings::NativeValueTraitsStringAdapter(
        ReplaceUnmatchedSurrogates(string));
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLNullable<IDLUSVStringV2>>
    : public NativeValueTraitsBase<IDLNullable<IDLUSVStringV2>> {
  static decltype(auto) NativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state) {
    return NativeValueTraits<IDLUSVStringBaseV2<
        bindings::IDLStringConvMode::kNullable>>::NativeValue(isolate, value,
                                                              exception_state);
  }
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLOptional<IDLUSVStringV2>>
    : public NativeValueTraitsBase<IDLUSVStringV2> {
  static decltype(auto) NativeValue(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state) {
    if (value->IsUndefined())
      return bindings::NativeValueTraitsStringAdapter();
    return NativeValueTraits<IDLUSVStringV2>::NativeValue(isolate, value,
                                                          exception_state);
  }
};

template <bindings::IDLStringConvMode mode>
struct NativeValueTraits<IDLStringStringContextTrustedHTMLBaseV2<mode>>
    : public NativeValueTraitsBase<
          IDLStringStringContextTrustedHTMLBaseV2<mode>> {
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state,
                            ExecutionContext* execution_context) {
    if (TrustedHTML* trusted_html =
            V8TrustedHTML::ToImplWithTypeCheck(isolate, value)) {
      return trusted_html->toString();
    }

    auto&& string = NativeValueTraits<IDLStringBaseV2<mode>>::NativeValue(
        isolate, value, exception_state);
    if (exception_state.HadException())
      return String();
    return TrustedTypesCheckForHTML(string, execution_context, exception_state);
  }
};

template <>
struct CORE_EXPORT
    NativeValueTraits<IDLNullable<IDLStringStringContextTrustedHTMLV2>>
    : public NativeValueTraitsBase<
          IDLNullable<IDLStringStringContextTrustedHTMLV2>> {
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state,
                            ExecutionContext* execution_context) {
    return NativeValueTraits<IDLStringStringContextTrustedHTMLBaseV2<
        bindings::IDLStringConvMode::kNullable>>::
        NativeValue(isolate, value, exception_state, execution_context);
  }
};

template <bindings::IDLStringConvMode mode>
struct NativeValueTraits<IDLStringStringContextTrustedScriptBaseV2<mode>>
    : public NativeValueTraitsBase<
          IDLStringStringContextTrustedScriptBaseV2<mode>> {
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state,
                            ExecutionContext* execution_context) {
    if (TrustedScript* trusted_script =
            V8TrustedScript::ToImplWithTypeCheck(isolate, value)) {
      return trusted_script->toString();
    }

    auto&& string = NativeValueTraits<IDLStringBaseV2<mode>>::NativeValue(
        isolate, value, exception_state);
    if (exception_state.HadException())
      return String();
    return TrustedTypesCheckForScript(string, execution_context,
                                      exception_state);
  }
};

template <>
struct CORE_EXPORT
    NativeValueTraits<IDLNullable<IDLStringStringContextTrustedScriptV2>>
    : public NativeValueTraitsBase<
          IDLNullable<IDLStringStringContextTrustedScriptV2>> {
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state,
                            ExecutionContext* execution_context) {
    return NativeValueTraits<IDLStringStringContextTrustedScriptBaseV2<
        bindings::IDLStringConvMode::kNullable>>::
        NativeValue(isolate, value, exception_state, execution_context);
  }
};

template <bindings::IDLStringConvMode mode>
struct NativeValueTraits<IDLUSVStringStringContextTrustedScriptURLBaseV2<mode>>
    : public NativeValueTraitsBase<
          IDLUSVStringStringContextTrustedScriptURLBaseV2<mode>> {
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state,
                            ExecutionContext* execution_context) {
    if (TrustedScriptURL* trusted_script_url =
            V8TrustedScriptURL::ToImplWithTypeCheck(isolate, value)) {
      return trusted_script_url->toString();
    }

    auto&& string = NativeValueTraits<IDLUSVStringBaseV2<mode>>::NativeValue(
        isolate, value, exception_state);
    if (exception_state.HadException())
      return String();
    return TrustedTypesCheckForScriptURL(string, execution_context,
                                         exception_state);
  }
};

template <>
struct CORE_EXPORT
    NativeValueTraits<IDLNullable<IDLUSVStringStringContextTrustedScriptURLV2>>
    : public NativeValueTraitsBase<
          IDLNullable<IDLUSVStringStringContextTrustedScriptURLV2>> {
  static String NativeValue(v8::Isolate* isolate,
                            v8::Local<v8::Value> value,
                            ExceptionState& exception_state,
                            ExecutionContext* execution_context) {
    return NativeValueTraits<IDLUSVStringStringContextTrustedScriptURLBaseV2<
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
template <typename T>
struct NativeValueTraits<IDLSequence<T>>
    : public NativeValueTraitsBase<IDLSequence<T>> {
  // Nondependent types need to be explicitly qualified to be accessible.
  using typename NativeValueTraitsBase<IDLSequence<T>>::ImplType;

  // https://heycam.github.io/webidl/#es-sequence
  static ImplType NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    // 1. If Type(V) is not Object, throw a TypeError.
    if (!value->IsObject()) {
      exception_state.ThrowTypeError(
          "The provided value cannot be converted to a sequence.");
      return ImplType();
    }

    ImplType result;
    // TODO(https://crbug.com/715122): Checking for IsArray() may not be
    // enough. Other engines also prefer regular array iteration over a custom
    // @@iterator when the latter is defined, but it is not clear if this is a
    // valid optimization.
    if (value->IsArray()) {
      ConvertSequenceFast(isolate, value.As<v8::Array>(), exception_state,
                          result);
    } else {
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
      ConvertSequenceSlow(isolate, std::move(script_iterator), exception_state,
                          result);
    }

    if (exception_state.HadException())
      return ImplType();
    return result;
  }

  // https://heycam.github.io/webidl/#es-sequence
  // This is a special case, used when converting an IDL union that contains a
  // sequence or frozen array type.
  static ImplType NativeValue(v8::Isolate* isolate,
                              ScriptIterator script_iterator,
                              ExceptionState& exception_state) {
    DCHECK(!script_iterator.IsNull());
    ImplType result;
    ConvertSequenceSlow(isolate, std::move(script_iterator), exception_state,
                        result);
    return result;
  }

 private:
  // Fast case: we're interating over an Array that adheres to
  // %ArrayIteratorPrototype%'s protocol.
  static void ConvertSequenceFast(v8::Isolate* isolate,
                                  v8::Local<v8::Array> v8_array,
                                  ExceptionState& exception_state,
                                  ImplType& result) {
    const uint32_t length = v8_array->Length();
    if (length > ImplType::MaxCapacity()) {
      exception_state.ThrowRangeError("Array length exceeds supported limit.");
      return;
    }
    result.ReserveInitialCapacity(length);
    v8::TryCatch block(isolate);
    // Array length may change if array is mutated during iteration.
    for (uint32_t i = 0; i < v8_array->Length(); ++i) {
      v8::Local<v8::Value> element;
      if (!v8_array->Get(isolate->GetCurrentContext(), i).ToLocal(&element)) {
        exception_state.RethrowV8Exception(block.Exception());
        return;
      }
      result.push_back(
          NativeValueTraits<T>::NativeValue(isolate, element, exception_state));
      if (exception_state.HadException())
        return;
    }
  }

  // Slow case: follow WebIDL's "Creating a sequence from an iterable" steps to
  // iterate through each element.
  static void ConvertSequenceSlow(v8::Isolate* isolate,
                                  ScriptIterator script_iterator,
                                  ExceptionState& exception_state,
                                  ImplType& result) {
    // https://heycam.github.io/webidl/#create-sequence-from-iterable
    // 2. Initialize i to be 0.
    // 3. Repeat:
    ExecutionContext* execution_context =
        ToExecutionContext(isolate->GetCurrentContext());
    while (script_iterator.Next(execution_context, exception_state)) {
      // 3.1. Let next be ? IteratorStep(iter).
      // 3.2. If next is false, then return an IDL sequence value of type
      //      sequence<T> of length i, where the value of the element at index
      //      j is Sj.
      // 3.3. Let nextItem be ? IteratorValue(next).
      if (exception_state.HadException())
        return;

      // The value should already be non-empty, as guaranteed by the call to
      // Next() and the |exception_state| check above.
      v8::Local<v8::Value> element =
          script_iterator.GetValue().ToLocalChecked();
      DCHECK(!element.IsEmpty());

      // 3.4. Initialize Si to the result of converting nextItem to an IDL
      //      value of type T.
      // 3.5. Set i to i + 1.
      result.push_back(
          NativeValueTraits<T>::NativeValue(isolate, element, exception_state));
      if (exception_state.HadException())
        return;
    }
  }
};

template <typename T>
struct NativeValueTraits<IDLOptional<IDLSequence<T>>>
    : public NativeValueTraitsBase<IDLSequence<T>> {
  static typename NativeValueTraits<IDLSequence<T>>::ImplType NativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exception_state) {
    if (value->IsUndefined())
      return {};
    return NativeValueTraits<IDLSequence<T>>::NativeValue(isolate, value,
                                                          exception_state);
  }
};

// Record types
template <typename K, typename V>
struct NativeValueTraits<IDLRecord<K, V>>
    : public NativeValueTraitsBase<IDLRecord<K, V>> {
  // Nondependent types need to be explicitly qualified to be accessible.
  using typename NativeValueTraitsBase<IDLRecord<K, V>>::ImplType;

  // Converts a JavaScript value |O| to an IDL record<K, V> value. In C++, a
  // record is represented as a Vector<std::pair<k, v>> (or a HeapVector if
  // necessary). See https://heycam.github.io/webidl/#es-record.
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

// We don't support nullable dictionary types for the time being since it's
// quite confusing.
template <typename T>
struct NativeValueTraits<
    IDLNullable<T>,
    typename std::enable_if_t<
        std::is_base_of<bindings::DictionaryBase, T>::value>>;

// Migration Adapters: Nullable dictionary types generated by the old bindings
// generator.
template <typename T>
struct NativeValueTraits<
    IDLNullable<T>,
    typename std::enable_if_t<std::is_base_of<IDLDictionaryBase, T>::value>>;

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

// Migration Adapters: union types generated by the old bindings generator.
template <typename T>
struct NativeValueTraits<IDLUnionNotINT<T>> : public NativeValueTraitsBase<T> {
  static T NativeValue(v8::Isolate* isolate,
                       v8::Local<v8::Value> value,
                       ExceptionState& exception_state) {
    T impl;
    V8TypeOf<T>::Type::ToImpl(isolate, value, impl,
                              UnionTypeConversionMode::kNotNullable,
                              exception_state);
    return impl;
  }

  static T ArgumentValue(v8::Isolate* isolate,
                         int argument_index,
                         v8::Local<v8::Value> value,
                         ExceptionState& exception_state) {
    return NativeValue(isolate, value, exception_state);
  }
};

template <typename T>
struct NativeValueTraits<IDLUnionINT<T>> : public NativeValueTraitsBase<T> {
  static T NativeValue(v8::Isolate* isolate,
                       v8::Local<v8::Value> value,
                       ExceptionState& exception_state) {
    T impl;
    V8TypeOf<T>::Type::ToImpl(isolate, value, impl,
                              UnionTypeConversionMode::kNullable,
                              exception_state);
    return impl;
  }

  static T ArgumentValue(v8::Isolate* isolate,
                         int argument_index,
                         v8::Local<v8::Value> value,
                         ExceptionState& exception_state) {
    return NativeValue(isolate, value, exception_state);
  }
};

template <typename T>
struct NativeValueTraits<IDLNullable<IDLUnionNotINT<T>>>
    : public NativeValueTraitsBase<T> {
  static T NativeValue(v8::Isolate* isolate,
                       v8::Local<v8::Value> value,
                       ExceptionState& exception_state) {
    if (value->IsNullOrUndefined())
      return T();
    return NativeValueTraits<IDLUnionNotINT<T>>::NativeValue(isolate, value,
                                                             exception_state);
  }

  static T ArgumentValue(v8::Isolate* isolate,
                         int argument_index,
                         v8::Local<v8::Value> value,
                         ExceptionState& exception_state) {
    return NativeValue(isolate, value, exception_state);
  }
};

// IDLNullable<IDLUnionINT<T>> must not be used.
template <typename T>
struct NativeValueTraits<IDLNullable<IDLUnionINT<T>>>;

template <typename T>
struct NativeValueTraits<IDLOptional<IDLUnionNotINT<T>>>
    : public NativeValueTraitsBase<T> {
  static T NativeValue(v8::Isolate* isolate,
                       v8::Local<v8::Value> value,
                       ExceptionState& exception_state) {
    if (value->IsUndefined())
      return T();
    return NativeValueTraits<IDLUnionNotINT<T>>::NativeValue(isolate, value,
                                                             exception_state);
  }

  static T ArgumentValue(v8::Isolate* isolate,
                         int argument_index,
                         v8::Local<v8::Value> value,
                         ExceptionState& exception_state) {
    return NativeValue(isolate, value, exception_state);
  }
};

template <typename T>
struct NativeValueTraits<IDLOptional<IDLUnionINT<T>>>
    : public NativeValueTraitsBase<T> {
  static T NativeValue(v8::Isolate* isolate,
                       v8::Local<v8::Value> value,
                       ExceptionState& exception_state) {
    return NativeValueTraits<IDLUnionINT<T>>::NativeValue(isolate, value,
                                                          exception_state);
  }

  static T ArgumentValue(v8::Isolate* isolate,
                         int argument_index,
                         v8::Local<v8::Value> value,
                         ExceptionState& exception_state) {
    return NativeValue(isolate, value, exception_state);
  }
};

// Nullable types
template <typename InnerType>
struct NativeValueTraits<
    IDLNullable<InnerType>,
    typename std::enable_if_t<!NativeValueTraits<InnerType>::has_null_value>>
    : public NativeValueTraitsBase<IDLNullable<InnerType>> {
  // https://heycam.github.io/webidl/#es-nullable-type
  using ImplType =
      base::Optional<typename NativeValueTraits<InnerType>::ImplType>;

  static ImplType NativeValue(v8::Isolate* isolate,
                              v8::Local<v8::Value> value,
                              ExceptionState& exception_state) {
    if (value->IsNullOrUndefined())
      return base::nullopt;
    return NativeValueTraits<InnerType>::NativeValue(isolate, value,
                                                     exception_state);
  }

  static ImplType ArgumentValue(v8::Isolate* isolate,
                                int argument_index,
                                v8::Local<v8::Value> value,
                                ExceptionState& exception_state) {
    if (value->IsNullOrUndefined())
      return base::nullopt;
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
  static base::Optional<base::Time> NativeValue(
      v8::Isolate* isolate,
      v8::Local<v8::Value> value,
      ExceptionState& exception_state) = delete;
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLNullable<IDLDate>>
    : public NativeValueTraitsBase<IDLNullable<IDLDate>> {
  static base::Optional<base::Time> NativeValue(
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

// Workaround https://crbug.com/345529
template <>
struct CORE_EXPORT NativeValueTraits<XPathNSResolver>
    : public NativeValueTraitsBase<XPathNSResolver*> {
  static XPathNSResolver* NativeValue(v8::Isolate* isolate,
                                      v8::Local<v8::Value> value,
                                      ExceptionState& exception_state);

  static XPathNSResolver* ArgumentValue(v8::Isolate* isolate,
                                        int argument_index,
                                        v8::Local<v8::Value> value,
                                        ExceptionState& exception_state);
};

template <>
struct CORE_EXPORT NativeValueTraits<IDLNullable<XPathNSResolver>>
    : public NativeValueTraitsBase<IDLNullable<XPathNSResolver>> {
  static XPathNSResolver* NativeValue(v8::Isolate* isolate,
                                      v8::Local<v8::Value> value,
                                      ExceptionState& exception_state);

  static XPathNSResolver* ArgumentValue(v8::Isolate* isolate,
                                        int argument_index,
                                        v8::Local<v8::Value> value,
                                        ExceptionState& exception_state);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_NATIVE_VALUE_TRAITS_IMPL_H_
