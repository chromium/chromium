/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_STRING_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_STRING_RESOURCE_H_

#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/string_resource.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "v8/include/v8.h"

namespace blink {

// V8StringResource is an adapter class that converts V8 values to Strings
// or AtomicStrings as appropriate, using multiple typecast operators.
enum V8StringResourceMode {
  kDefaultMode,
  kTreatNullAsEmptyString,
  kTreatNullAsNullString,
  kTreatNullAndUndefinedAsNullString
};

template <V8StringResourceMode Mode = kDefaultMode>
class V8StringResource {
  STACK_ALLOCATED();

 public:
  V8StringResource(const V8StringResource&) = delete;
  V8StringResource& operator=(const V8StringResource&) = delete;

  V8StringResource() : mode_(kExternalize) {}

  V8StringResource(v8::Local<v8::Value> object)
      : v8_object_(object), mode_(kExternalize) {}

  V8StringResource(const String& string)
      : mode_(kExternalize), string_(string) {}

  void operator=(v8::Local<v8::Value> object) { v8_object_ = object; }

  void operator=(const String& string) { SetString(string); }

  void operator=(std::nullptr_t) { SetString(String()); }

  bool Prepare() {  // DEPRECATED
    if (PrepareFast())
      return true;

    return v8_object_->ToString(v8::Isolate::GetCurrent()->GetCurrentContext())
        .ToLocal(&v8_object_);
  }

  bool Prepare(v8::Isolate* isolate, ExceptionState& exception_state) {
    return PrepareFast() || PrepareSlow(isolate, exception_state);
  }

  bool Prepare(ExceptionState& exception_state) {  // DEPRECATED
    return PrepareFast() ||
           PrepareSlow(v8::Isolate::GetCurrent(), exception_state);
  }

  // Implicit conversions needed to make Blink bindings easier to use.

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator String() const { return ToString<String>(); }

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator AtomicString() const { return ToString<AtomicString>(); }

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator StringView() const {
    if (LIKELY(!v8_object_.IsEmpty())) {
      return ToBlinkStringView(v8_object_.As<v8::String>(), backing_store_,
                               mode_);
    }

    return string_;
  }

 private:
  bool PrepareFast() {
    if (v8_object_.IsEmpty())
      return true;

    if (!IsValid()) {
      SetString(FallbackString());
      return true;
    }

    if (LIKELY(v8_object_->IsString()))
      return true;

    if (LIKELY(v8_object_->IsInt32())) {
      SetString(ToBlinkString(v8_object_.As<v8::Int32>()->Value()));
      return true;
    }

    mode_ = kDoNotExternalize;
    return false;
  }

  bool PrepareSlow(v8::Isolate* isolate, ExceptionState& exception_state) {
    v8::TryCatch try_catch(isolate);
    if (!v8_object_->ToString(isolate->GetCurrentContext())
             .ToLocal(&v8_object_)) {
      exception_state.RethrowV8Exception(try_catch.Exception());
      return false;
    }
    return true;
  }

  bool IsValid() const;
  String FallbackString() const;

  void SetString(const String& string) {
    string_ = string;
    v8_object_.Clear();  // To signal that String is ready.
  }

  template <class StringType>
  StringType ToString() const {
    if (LIKELY(!v8_object_.IsEmpty()))
      return ToBlinkString<StringType>(v8_object_.As<v8::String>(), mode_);

    return StringType(string_);
  }

  v8::Local<v8::Value> v8_object_;
  ExternalMode mode_;
  String string_;

  mutable WTF::StringView::StackBackingStore backing_store_;
};

template <>
inline bool V8StringResource<kDefaultMode>::IsValid() const {
  return true;
}

template <>
inline String V8StringResource<kDefaultMode>::FallbackString() const {
  NOTREACHED();
  return String();
}

template <>
inline bool V8StringResource<kTreatNullAsEmptyString>::IsValid() const {
  return !v8_object_->IsNull();
}

template <>
inline String V8StringResource<kTreatNullAsEmptyString>::FallbackString()
    const {
  return g_empty_string;
}

template <>
inline bool V8StringResource<kTreatNullAsNullString>::IsValid() const {
  return !v8_object_->IsNull();
}

template <>
inline String V8StringResource<kTreatNullAsNullString>::FallbackString() const {
  return String();
}

template <>
inline bool V8StringResource<kTreatNullAndUndefinedAsNullString>::IsValid()
    const {
  return !v8_object_->IsNull() && !v8_object_->IsUndefined();
}

template <>
inline String
V8StringResource<kTreatNullAndUndefinedAsNullString>::FallbackString() const {
  return String();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_STRING_RESOURCE_H_
