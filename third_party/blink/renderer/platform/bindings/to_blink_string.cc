// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/to_blink_string.h"

#include <type_traits>

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/platform/bindings/string_resource.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

namespace {

template <class StringClass>
struct StringTraits {
  static const StringClass& FromStringResource(v8::Isolate* isolate,
                                               StringResourceBase*);
  template <typename V8StringTrait>
  static StringClass FromV8String(v8::Isolate*,
                                  v8::Local<v8::String>,
                                  uint32_t);
};

template <>
struct StringTraits<String> {
  static const String FromStringResource(v8::Isolate* isolate,
                                         StringResourceBase* resource) {
    return resource->GetWTFString();
  }
  template <typename V8StringTrait>
  static String FromV8String(v8::Isolate*, v8::Local<v8::String>, uint32_t);
};

template <>
struct StringTraits<AtomicString> {
  static const AtomicString FromStringResource(v8::Isolate* isolate,
                                               StringResourceBase* resource) {
    return resource->GetAtomicString(isolate);
  }
  template <typename V8StringTrait>
  static AtomicString FromV8String(v8::Isolate*,
                                   v8::Local<v8::String>,
                                   uint32_t);
};

struct V8StringTwoBytesTrait {
  typedef UChar CharType;
  ALWAYS_INLINE static void Write(v8::Isolate* isolate,
                                  v8::Local<v8::String> v8_string,
                                  base::span<CharType> buffer) {
    DCHECK_LE(buffer.size(), static_cast<uint32_t>(v8_string->Length()));
    v8_string->WriteV2(isolate, 0, base::checked_cast<uint32_t>(buffer.size()),
                       reinterpret_cast<uint16_t*>(buffer.data()));
  }
};

struct V8StringOneByteTrait {
  typedef LChar CharType;
  ALWAYS_INLINE static void Write(v8::Isolate* isolate,
                                  v8::Local<v8::String> v8_string,
                                  base::span<CharType> buffer) {
    DCHECK_LE(buffer.size(), static_cast<uint32_t>(v8_string->Length()));
    v8_string->WriteOneByteV2(
        isolate, 0, base::checked_cast<uint32_t>(buffer.size()), buffer.data());
  }
};

template <typename V8StringTrait>
String StringTraits<String>::FromV8String(v8::Isolate* isolate,
                                          v8::Local<v8::String> v8_string,
                                          uint32_t length) {
  DCHECK_EQ(static_cast<uint32_t>(v8_string->Length()), length);
  base::span<typename V8StringTrait::CharType> buffer;
  String result = String::CreateUninitialized(length, buffer);
  V8StringTrait::Write(isolate, v8_string, buffer);
  return result;
}

template <typename V8StringTrait>
AtomicString StringTraits<AtomicString>::FromV8String(
    v8::Isolate* isolate,
    v8::Local<v8::String> v8_string,
    uint32_t length) {
  DCHECK_EQ(static_cast<uint32_t>(v8_string->Length()), length);
  static const int kInlineBufferSize =
      32 / sizeof(typename V8StringTrait::CharType);
  if (length <= kInlineBufferSize) {
    typename V8StringTrait::CharType inline_buffer[kInlineBufferSize];
    base::span<typename V8StringTrait::CharType> buffer_span(inline_buffer);
    V8StringTrait::Write(isolate, v8_string, buffer_span.first(length));
    return AtomicString(buffer_span.first(length));
  }
  base::span<typename V8StringTrait::CharType> buffer;
  String string = String::CreateUninitialized(length, buffer);
  V8StringTrait::Write(isolate, v8_string, buffer);
  return AtomicString(string);
}

ALWAYS_INLINE bool CanExternalize(v8::Local<v8::String> v8_string,
                                  ExternalMode mode,
                                  bool is_one_byte) {
  const v8::String::Encoding requested_encoding =
      is_one_byte ? v8::String::ONE_BYTE_ENCODING
                  : v8::String::TWO_BYTE_ENCODING;
  return mode == kExternalize && v8_string->CanMakeExternal(requested_encoding);
}


// Converts a `v8_string` to a StringType optionally externalizing if
// `can_externalize` is true; sets `was_externalized` if on successful
// externalization.
//
// If the string was not successfully externalized, then the calling code
// may have the only reference to the StringType and must handle retaining
// it to keep it alive.
template <typename StringType>
ALWAYS_INLINE StringType
ConvertAndExternalizeString(v8::Isolate* isolate,
                            v8::Local<v8::String> v8_string,
                            bool can_externalize,
                            bool is_one_byte,
                            bool* was_externalized) {
  uint32_t length = v8_string->Length();
  StringType result =
      is_one_byte ? StringTraits<StringType>::template FromV8String<
                        V8StringOneByteTrait>(isolate, v8_string, length)
                  : StringTraits<StringType>::template FromV8String<
                        V8StringTwoBytesTrait>(isolate, v8_string, length);

  *was_externalized = false;
  if (can_externalize) [[likely]] {
    if (result.Is8Bit()) {
      StringResource8* string_resource = new StringResource8(isolate, result);
      if (!v8_string->MakeExternal(string_resource)) [[unlikely]] {
        string_resource->Unaccount(isolate);
        delete string_resource;
      } else {
        *was_externalized = true;
      }
    } else {
      StringResource16* string_resource = new StringResource16(isolate, result);
      if (!v8_string->MakeExternal(string_resource)) [[unlikely]] {
        string_resource->Unaccount(isolate);
        delete string_resource;
      } else {
        *was_externalized = true;
      }
    }
  }

  return result;
}

}  // namespace

template <typename StringType>
StringType ToBlinkString(v8::Isolate* isolate,
                         v8::Local<v8::String> v8_string,
                         ExternalMode mode) {
  // Be very careful in this code to ensure it is RVO friendly. Accidentally
  // breaking RVO will degrade some of the blink_perf benchmarks by a few
  // percent. This includes moving the StringTraits<>::FromStringResource() call
  // into GetExternalizedString() as it becomes impossible for the calling code
  // to satisfy all RVO constraints.

  // Check for an already externalized string first as this is a very
  // common case for all platforms with the one exception being super short
  // strings on for platforms with v8 pointer compression.
  StringResourceBase* string_resource =
      StringResourceBase::GetExternalizedString(isolate, v8_string);
  if (string_resource) {
    return StringTraits<StringType>::FromStringResource(isolate,
                                                        string_resource);
  }

  uint32_t length = v8_string->Length();
  if (!length) [[unlikely]] {
    return StringType(g_empty_atom);
  }

  // It is safe to ignore externalization failures as it just means later
  // calls will recreate the string.
  bool was_externalized;
  const bool is_one_byte = v8_string->IsOneByte();
  return ConvertAndExternalizeString<StringType>(
      isolate, v8_string, CanExternalize(v8_string, mode, is_one_byte),
      is_one_byte, &was_externalized);
}

// Explicitly instantiate the above template with the expected
// parameterizations, to ensure the compiler generates the code; otherwise link
// errors can result in GCC 4.4.
template String ToBlinkString<String>(v8::Isolate* isolate,
                                      v8::Local<v8::String>,
                                      ExternalMode);
template AtomicString ToBlinkString<AtomicString>(v8::Isolate* isolate,
                                                  v8::Local<v8::String>,
                                                  ExternalMode);


// Fast but non thread-safe version.
static String ToBlinkStringFast(int value) {
  // Caching of small strings below is not thread safe: newly constructed
  // AtomicString are not safely published.
  DCHECK(IsMainThread());

  // Most numbers used are <= 100. Even if they aren't used there's very little
  // cost in using the space.
  const int kLowNumbers = 100;
  DEFINE_STATIC_LOCAL(Vector<AtomicString>, low_numbers, (kLowNumbers + 1));
  String web_core_string;
  if (0 <= value && value <= kLowNumbers) {
    web_core_string = low_numbers[value];
    if (!web_core_string) {
      low_numbers[value] = AtomicString::Number(value);
      web_core_string = low_numbers[value];
    }
  } else {
    web_core_string = String::Number(value);
  }
  return web_core_string;
}

String ToBlinkString(int value) {
  // If we are on the main thread (this should always true for non-workers),
  // call the faster one.
  if (IsMainThread())
    return ToBlinkStringFast(value);
  return String::Number(value);
}

}  // namespace blink
