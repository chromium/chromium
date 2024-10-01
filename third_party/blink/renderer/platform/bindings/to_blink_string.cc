// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/to_blink_string.h"

#include <type_traits>

#include "third_party/blink/renderer/platform/bindings/string_resource.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

namespace {

template <class StringClass>
struct StringTraits {
  static const StringClass& FromStringResource(v8::Isolate* isolate,
                                               StringResourceBase*);
  template <typename V8StringTrait>
  static StringClass FromV8String(v8::Isolate*, v8::Local<v8::String>, int);
};

template <>
struct StringTraits<String> {
  static const String FromStringResource(v8::Isolate* isolate,
                                         StringResourceBase* resource) {
    return resource->GetWTFString();
  }
  template <typename V8StringTrait>
  static String FromV8String(v8::Isolate*, v8::Local<v8::String>, int);
};

template <>
struct StringTraits<AtomicString> {
  static const AtomicString FromStringResource(v8::Isolate* isolate,
                                               StringResourceBase* resource) {
    return resource->GetAtomicString(isolate);
  }
  template <typename V8StringTrait>
  static AtomicString FromV8String(v8::Isolate*, v8::Local<v8::String>, int);
};

struct V8StringTwoBytesTrait {
  typedef UChar CharType;
  ALWAYS_INLINE static void Write(v8::Isolate* isolate,
                                  v8::Local<v8::String> v8_string,
                                  base::span<CharType> buffer) {
    v8_string->Write(isolate, reinterpret_cast<uint16_t*>(buffer.data()), 0,
                     static_cast<int>(buffer.size()));
  }
};

struct V8StringOneByteTrait {
  typedef LChar CharType;
  ALWAYS_INLINE static void Write(v8::Isolate* isolate,
                                  v8::Local<v8::String> v8_string,
                                  base::span<CharType> buffer) {
    v8_string->WriteOneByte(isolate, buffer.data(), 0,
                            static_cast<int>(buffer.size()));
  }
};

template <typename V8StringTrait>
String StringTraits<String>::FromV8String(v8::Isolate* isolate,
                                          v8::Local<v8::String> v8_string,
                                          int length) {
  DCHECK(v8_string->Length() == length);
  base::span<typename V8StringTrait::CharType> buffer;
  String result = String::CreateUninitialized(length, buffer);
  V8StringTrait::Write(isolate, v8_string, buffer);
  return result;
}

template <typename V8StringTrait>
AtomicString StringTraits<AtomicString>::FromV8String(
    v8::Isolate* isolate,
    v8::Local<v8::String> v8_string,
    int length) {
  DCHECK(v8_string->Length() == length);
  static const int kInlineBufferSize =
      32 / sizeof(typename V8StringTrait::CharType);
  if (length <= kInlineBufferSize) {
    typename V8StringTrait::CharType inline_buffer[kInlineBufferSize];
    base::span<typename V8StringTrait::CharType> buffer_span(inline_buffer);
    V8StringTrait::Write(isolate, v8_string, buffer_span);
    return AtomicString(buffer_span.first(static_cast<size_t>(length)));
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

// Retrieves the StringResourceBase from `v8_string`.
//
// Returns a nullptr if there was no previous externalization.
ALWAYS_INLINE StringResourceBase* GetExternalizedString(
    v8::Isolate* isolate,
    v8::Local<v8::String> v8_string) {
  v8::String::Encoding encoding;
  v8::String::ExternalStringResourceBase* resource =
      v8_string->GetExternalStringResourceBase(isolate, &encoding);
  if (!!resource) [[likely]] {
    // Inheritance:
    // - V8 side: v8::String::ExternalStringResourceBase
    //   -> v8::External{One,}ByteStringResource
    // - Both: StringResource{8,16}Base inherits from the matching v8 class.
    static_assert(std::is_base_of<v8::String::ExternalOneByteStringResource,
                                  StringResource8Base>::value,
                  "");
    static_assert(std::is_base_of<v8::String::ExternalStringResource,
                                  StringResource16Base>::value,
                  "");
    static_assert(
        std::is_base_of<StringResourceBase, StringResource8Base>::value, "");
    static_assert(
        std::is_base_of<StringResourceBase, StringResource16Base>::value, "");
    // Then StringResource{8,16}Base allows to go from one ancestry path to
    // the other one. Even though it's empty, removing it causes UB, see
    // crbug.com/909796.
    StringResourceBase* base;
    if (encoding == v8::String::ONE_BYTE_ENCODING)
      base = static_cast<StringResource8Base*>(resource);
    else
      base = static_cast<StringResource16Base*>(resource);
    return base;
  }

  return nullptr;
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
  int length = v8_string->Length();
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
        delete string_resource;
      } else {
        *was_externalized = true;
      }
    } else {
      StringResource16* string_resource = new StringResource16(isolate, result);
      if (!v8_string->MakeExternal(string_resource)) [[unlikely]] {
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
      GetExternalizedString(isolate, v8_string);
  if (string_resource) {
    return StringTraits<StringType>::FromStringResource(isolate,
                                                        string_resource);
  }

  int length = v8_string->Length();
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

StringView ToBlinkStringView(v8::Isolate* isolate,
                             v8::Local<v8::String> v8_string,
                             StringView::StackBackingStore& backing_store,
                             ExternalMode mode) {
  // Be very careful in this code to ensure it is RVO friendly. Accidentally
  // breaking RVO will degrade some of the blink_perf benchmarks by a few
  // percent. This includes moving the StringTraits<>::FromStringResource() call
  // into GetExternalizedString() as it becomes impossible for the calling code
  // to satisfy all RVO constraints.
  StringResourceBase* string_resource =
      GetExternalizedString(isolate, v8_string);
  if (string_resource) {
    return StringTraits<AtomicString>::FromStringResource(isolate,
                                                          string_resource)
        .Impl();
  }

  int length = v8_string->Length();
  if (!length) [[unlikely]] {
    return StringView(g_empty_atom);
  }

  // Note that this code path looks very similar to ToBlinkString(). The
  // critical difference in ToBlinkStringView(), if `can_externalize` is false,
  // there is no attempt to create either an AtomicString or an String. This
  // can very likely avoid a heap allocation and definitely avoids refcount
  // churn which can be significantly faster in some hot paths.
  const bool is_one_byte = v8_string->IsOneByte();
  bool can_externalize = CanExternalize(v8_string, mode, is_one_byte);
  if (can_externalize) [[likely]] {
    bool was_externalized;
    // An AtomicString is always used here for externalization. Using a String
    // would avoid the AtomicStringTable insert however it also means APIs
    // consuming the returned StringView must do O(l) operations on equality
    // checking.
    //
    // Given that externalization implies reuse of the string, taking the single
    // O(l) hit to insert into the AtomicStringTable ends up being faster in
    // most cases.
    //
    // If the caller explicitly wants a String, then using ToBlinkString<String>
    // is the better option.
    //
    // If the caller wants a disposable serialization where it knows the
    // v8::String is unlikely to be re-projected into Blink (seems rare?) then
    // calling this with kDoNotExternalize and relying on the
    // StringView::StackBackingStore yields the most efficient code.
    AtomicString blink_string = ConvertAndExternalizeString<AtomicString>(
        isolate, v8_string, can_externalize, is_one_byte, &was_externalized);
    if (was_externalized) {
      return StringView(blink_string.Impl());
    }
  }

  // The string has not been externalized. Serialize into `backing_store` and
  // return.
  //
  // Note on platforms with v8 pointer compression, this is the hot path
  // for short strings like "id" as those are never externalized whereas on
  // platforms without pointer compression GetExternalizedString() is the hot
  // path.
  //
  // This is particularly important when optimizing for blink_perf.bindings as
  // x64 vs ARM performance will have very different behavior; x64 has
  // pointer compression but ARM does not. Since a common string used in the
  // {get,set}-attribute benchmarks is "id", this means optimizations
  // that affect the microbenchmark in one architecture likely have no effect
  // (or even a negative effect due to different expectations in branch
  // prediction) in the other.
  //
  // When pointer compression is on, short strings always cause a
  // serialization to Blink and thus if there are 1000 runs of an API
  // asking to convert the same `v8_string` to a Blink string, each run will
  // behavior similarly.
  //
  // When pointer compression is off, the first run will externalize the string
  // going through this path, but subsequent runs will enter the
  // GetExternalizedString() path and be much faster as it is just extracting
  // a pointer.
  //
  // Confusingly, the ARM and x64 absolute numbers for the benchmarks look
  // similar (80-90 runs/s on a pixel2 and a Lenovo P920). This can give the
  // mistaken belief that they are related numbers even though they are
  // testing almost entirely completely different codepaths. When optimizing
  // this code, it is instructive to increase the test attribute name string
  // length. Using something like something like "abcd1234" will make all
  // platforms externalize and x64 will likely run much much faster (local
  // test sees 260 runs/s on a x64 P920).
  //
  // TODO(ajwong): Revisit if the length restriction on externalization makes
  // sense. It's odd that pointer compression changes externalization
  // behavior.
  if (is_one_byte) {
    LChar* lchar = backing_store.Realloc<LChar>(length);
    v8_string->WriteOneByte(isolate, lchar, 0, length);
    return StringView(lchar, length);
  }

  UChar* uchar = backing_store.Realloc<UChar>(length);
  static_assert(sizeof(UChar) == sizeof(uint16_t),
                "UChar isn't the same as uint16_t");
  // reinterpret_cast is needed on windows as UChar is a wchar_t and not
  // an int64_t.
  v8_string->Write(isolate, reinterpret_cast<uint16_t*>(uchar), 0, length);
  return StringView(uchar, length);
}

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
