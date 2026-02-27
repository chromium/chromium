/*
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_ATOMIC_STRING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_ATOMIC_STRING_H_

#include <cstring>
#include <iosfwd>
#include <type_traits>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/integer_to_string_conversion.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

// `AtomicString` is interned, so it's safe to hash; allow conversion to a byte
// span to facilitate this.
namespace base {
template <>
inline constexpr bool kCanSafelyConvertToByteSpan<::blink::AtomicString> = true;
}

namespace blink {

// An AtomicString instance represents a string, and multiple AtomicString
// instances can share their string storage if the strings are
// identical. Comparing two AtomicString instances is much faster than comparing
// two String instances because we just check string storage identity.
//
// When a method of this class is compatible with an equivalent method in
// `std::string`, we use the same method name as `std::string` (i.e.,
// `snake_case()`) rather than following the Google/Blink C++ style guide's
// naming rules. This improves consistency in string manipulation.
class WTF_EXPORT AtomicString {
  USING_FAST_MALLOC(AtomicString);

 public:
  using size_type = string_size_t;
  static constexpr size_type npos = kNotFound;

  // The function is defined in StringStatics.cpp.
  static void Init();

  AtomicString() = default;
  explicit AtomicString(const char* chars)
      // SAFETY: The below span creation is safe if `chars` points to a
      // NUL-terminated string.
      : AtomicString(base::as_bytes(
            UNSAFE_BUFFERS(base::span(chars, chars ? strlen(chars) : 0u)))) {}
  explicit AtomicString(base::span<const LChar> chars);
  explicit AtomicString(
      base::span<const UChar> chars,
      AtomicStringUCharEncoding encoding = AtomicStringUCharEncoding::kUnknown);
  explicit AtomicString(const UChar* chars);

  explicit AtomicString(const StringView& view);

  // Constructing an AtomicString from a String / StringImpl can be expensive if
  // the StringImpl is not already atomic.
  explicit AtomicString(StringImpl* impl) : string_(Add(impl)) {}
  explicit AtomicString(const String& s) : string_(Add(s.Impl())) {}
  explicit AtomicString(String&& s) : string_(Add(s.ReleaseImpl())) {}

  explicit operator bool() const { return !IsNull(); }
  operator const String&() const { return string_; }
  const String& GetString() const { return string_; }

  StringImpl* Impl() const { return string_.Impl(); }

  bool Is8Bit() const { return string_.Is8Bit(); }
  wtf_size_t length() const { return string_.length(); }
  base::span<const LChar> Span8() const { return string_.Span8(); }
  base::span<const UChar> Span16() const { return string_.Span16(); }

  UChar operator[](wtf_size_t i) const { return string_[i]; }

  // Find characters.
  wtf_size_t find(UChar c, wtf_size_t start = 0) const {
    return string_.find(c, start);
  }
  wtf_size_t find(LChar c, wtf_size_t start = 0) const {
    return string_.find(c, start);
  }
  wtf_size_t find(char c, wtf_size_t start = 0) const {
    return find(static_cast<LChar>(c), start);
  }
  wtf_size_t Find(CharacterMatchFunctionPtr match_function,
                  wtf_size_t start = 0) const {
    return string_.Find(match_function, start);
  }

  // Find substrings.

  // Find a substring. Returns the index of the match, or `kNotFound`.
  size_type find(const StringView& value, size_type start = 0) const {
    return string_.find(value, start);
  }

  // Unicode aware case insensitive string matching. Non-ASCII characters might
  // match to ASCII characters. This function is rarely used to implement web
  // platform features.  See crbug.com/40476285.
  wtf_size_t DeprecatedFindIgnoringCase(const StringView& value,
                                        wtf_size_t start = 0) const {
    return string_.DeprecatedFindIgnoringCase(value, start);
  }

  // ASCII case insensitive string matching.
  wtf_size_t FindIgnoringAsciiCase(const StringView& value,
                                   wtf_size_t start = 0) const {
    return string_.FindIgnoringAsciiCase(value, start);
  }

  // Returns `true` if this string contains the specified `c`.
  bool contains(UChar c) const { return find(c) != kNotFound; }
  bool contains(LChar c) const { return find(c) != kNotFound; }
  bool contains(char c) const { return find(c) != kNotFound; }
  // Returns `true` if this string contains the specified `value`.
  // If `value` is empty, this returns `true`.
  bool contains(const StringView& value) const;
  // Returns `true` if this string contains the specified `value`, using ASCII
  // case-insensitive matching.
  // If `value` is empty, this returns `true`.
  bool ContainsIgnoringAsciiCase(const StringView& value) const;

  // Find the last instance of a single character.
  // Returns `npos` if it's not found in this string.
  size_type rfind(UChar c, size_type start = npos) const {
    return string_.rfind(c, start);
  }
  // Find the last instance of a substring.
  // If `this` string is null, this function returns kNotFound even if
  // `value` is empty.
  wtf_size_t ReverseFind(const StringView& value,
                         wtf_size_t start = UINT_MAX) const {
    return string_.ReverseFind(value, start);
  }
  // Searches for the last occurrence of a substring within this string.
  //
  // This method performs a backward search starting from the 'start' index.
  // If 'start' is npos, the search begins from the end of the string.
  //
  // Returns the index of the start of the found substring, or npos if
  // no match is found.
  //
  // Special Cases:
  // - If 'value' is empty, the search always succeeds and returns
  //   the minimum of 'start' and length().
  // - Null strings and zero-length strings are treated as equivalent
  //   for both `this` string and the 'value' parameter.
  size_type rfind(const StringView& value, size_type start = npos) const {
    return string_.rfind(value, start);
  }

  bool starts_with(const StringView& prefix) const {
    return string_.starts_with(prefix);
  }
  bool StartsWithIgnoringAsciiCase(const StringView& prefix) const {
    return string_.StartsWithIgnoringAsciiCase(prefix);
  }
  bool starts_with(UChar character) const {
    return string_.starts_with(character);
  }

  bool ends_with(const StringView& suffix) const {
    return string_.ends_with(suffix);
  }
  bool ends_with(UChar character) const { return string_.ends_with(character); }
  // Returns true if this string ends with the specified `suffix`, using ASCII
  // case-insensitive matching. If `suffix` is empty, this returns `true`.
  bool EndsWithIgnoringAsciiCase(const StringView& suffix) const {
    return string_.EndsWithIgnoringAsciiCase(suffix);
  }

  // Returns a lowercase/uppercase version of the string.
  // These functions convert ASCII characters only.
  static AtomicString LowerASCII(AtomicString source);
  AtomicString LowerASCII() const;
  AtomicString UpperASCII() const;

  bool IsLowerASCII() const { return string_.IsLowerASCII(); }

  template <typename IntegerType>
  static AtomicString Number(IntegerType number) {
    IntegerToStringConverter<IntegerType> converter(number);
    return AtomicString(converter.Span());
  }

  static AtomicString Number(double, unsigned precision = 6);

  bool IsNull() const { return string_.IsNull(); }
  bool empty() const { return string_.empty(); }
  unsigned Hash() const { return string_.Impl()->ExistingHash(); }

#ifdef __OBJC__
  operator NSString*() const { return string_; }
#endif
  // AtomicString::fromUTF8 will return a null string if
  // the input data contains invalid UTF-8 sequences.
  static AtomicString FromUTF8(base::span<const uint8_t>);
  static AtomicString FromUTF8(const char*);
  static AtomicString FromUTF8(std::string_view);

  std::string Ascii() const { return string_.Ascii(); }
  std::string Latin1() const { return string_.Latin1(); }
  std::string Utf8(
      Utf8ConversionMode mode = Utf8ConversionMode::kLenient) const {
    return StringView(*this).Utf8(mode);
  }
  // Returns a std::u16string_view pointing this AtomicString.
  // This should be called only if !Is8Bit().
  //
  // This function should be removed after enabling C++23 because
  // std::u16string_view(Span16()) will work with C++23.
  std::u16string_view View16() const LIFETIME_BOUND {
    return base::as_string_view(Span16());
  }

  size_t CharactersSizeInBytes() const {
    return string_.CharactersSizeInBytes();
  }

  void WriteIntoTrace(perfetto::TracedValue context) const;

#ifndef NDEBUG
  void Show() const;
#endif

 private:
  friend struct HashTraits<AtomicString>;

  String string_;

  ALWAYS_INLINE static String Add(String&& r) {
    if (!r || r.Impl()->IsAtomic()) {
      return std::move(r);
    }
    return AddSlowCase(std::move(r));
  }

  ALWAYS_INLINE static String Add(StringImpl* r) {
    if (!r || r->IsAtomic())
      return r;
    return AddSlowCase(r);
  }
  static String AddSlowCase(String&&);
  static String AddSlowCase(StringImpl*);
};

inline bool operator==(const AtomicString& a, const AtomicString& b) {
  return a.Impl() == b.Impl();
}
inline bool operator==(const AtomicString& a, const String& b) {
  // We don't use EqualStringView so we get the IsAtomic() optimization inside
  // blink::Equal.
  return Equal(a.Impl(), b.Impl());
}
inline bool operator==(const String& a, const AtomicString& b) {
  return b == a;
}
inline bool operator==(const AtomicString& a, const char* b) {
  return EqualStringView(a, b);
}
inline bool operator==(const char* a, const AtomicString& b) {
  return b == a;
}

// Define external global variables for the commonly used atomic strings.
// These are only usable from the main thread.
WTF_EXPORT extern const AtomicString& g_null_atom;
WTF_EXPORT extern const AtomicString& g_empty_atom;
WTF_EXPORT extern const AtomicString& g_star_atom;
WTF_EXPORT extern const AtomicString& g_xml_atom;
WTF_EXPORT extern const AtomicString& g_xmlns_atom;
WTF_EXPORT extern const AtomicString& g_xlink_atom;
WTF_EXPORT extern const AtomicString& g_http_atom;
WTF_EXPORT extern const AtomicString& g_https_atom;

// Pretty printer for gtest and base/logging.*.  It prepends and appends
// double-quotes, and escapes characters other than ASCII printables.
WTF_EXPORT std::ostream& operator<<(std::ostream&, const AtomicString&);

inline StringView::StringView(const AtomicString& string LIFETIME_BOUND,
                              unsigned offset,
                              unsigned length)
    : StringView(string.Impl(), offset, length) {}
inline StringView::StringView(const AtomicString& string LIFETIME_BOUND,
                              unsigned offset)
    : StringView(string.Impl(), offset) {}
inline StringView::StringView(const AtomicString& string LIFETIME_BOUND)
    : StringView(string.Impl()) {}

template <typename T>
struct HashTraits;
// Defined in atomic_string_hash.h.
template <>
struct HashTraits<AtomicString>;

}  // namespace blink

// Mark `AtomicString` and `const char*` as having a common reference type (the
// type to which both can be converted or bound) of `String`. This makes them
// satisfy `std::equality_comparable`, which allows usage like:
// ```
//   std::vector<AtomicString<T>> v;
//   const char* e;
//   auto it = std::ranges::find(v, e);
// ```
// Without this, the `find()` call above would fail to compile with a cryptic
// error about being unable to invoke `std::ranges::equal_to()`.
template <template <typename> typename TQ, template <typename> typename UQ>
struct std::basic_common_reference<blink::AtomicString, const char*, TQ, UQ> {
  using type = blink::String;
};

template <template <typename> typename TQ, template <typename> typename UQ>
struct std::basic_common_reference<const char*, blink::AtomicString, TQ, UQ> {
  using type = blink::String;
};

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::AtomicString)

#include "third_party/blink/renderer/platform/wtf/text/string_operators_atomic.h"
#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_ATOMIC_STRING_H_
