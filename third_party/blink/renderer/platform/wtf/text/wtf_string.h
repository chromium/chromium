/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012, 2013 Apple Inc.
 * All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_WTF_STRING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_WTF_STRING_H_

// This file would be called String.h, but that conflicts with <string.h>
// on systems without case-sensitive file systems.

#include <array>
#include <iosfwd>
#include <optional>
#include <string_view>
#include <type_traits>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace blink {

class CodePointIterator;

// You can find documentation about this class in README.md in this directory.
//
// When a method of this class is compatible with an equivalent method in
// `std::string`, we use the same method name as `std::string` (i.e.,
// `snake_case()`) rather than following the Google/Blink C++ style guide's
// naming rules. This improves consistency in string manipulation.
class WTF_EXPORT String {
  USING_FAST_MALLOC(String);

 public:
  using size_type = string_size_t;
  static constexpr size_type npos = kNotFound;

  // Factories ------------------------------------------------------

  // Returns an uninitialized string. The characters needs to be written
  // into the buffer returned in `data` before the returned string is used.
  // Failure to do this will have unpredictable results.
  [[nodiscard]] static String CreateUninitialized(size_type length,
                                                  base::span<UChar>& data) {
    return StringImpl::CreateUninitialized(length, data);
  }
  [[nodiscard]] static String CreateUninitialized(size_type length,
                                                  base::span<LChar>& data) {
    return StringImpl::CreateUninitialized(length, data);
  }

  // Creates an 8-bit string from a 16-bit source by copying characters.
  // All characters in the source must be Latin-1 (<= 0xFF).
  // If the source contains characters > 0xFF, it crashes in debug builds,
  // and yields undefined or platform-dependent results in release builds.
  [[nodiscard]] static String Make8BitFrom16BitSource(base::span<const UChar>);
  // Creates a 16-bit string from an 8-bit source by copying characters.
  [[nodiscard]] static String Make16BitFrom8BitSource(base::span<const LChar>);

  // String::FromUtf8 will return a null string if
  // the input data contains invalid UTF-8 sequences.
  // Does not strip BOMs.
  [[nodiscard]] static String FromUtf8(base::span<const uint8_t>);
  [[nodiscard]] static String FromUtf8(std::string_view s) {
    return FromUtf8(base::as_byte_span(s));
  }

  // Tries to convert the passed in string to UTF-8, but will fall back to
  // Latin-1 if the string is not valid UTF-8.
  [[nodiscard]] static String FromUtf8WithLatin1Fallback(
      base::span<const uint8_t>);
  [[nodiscard]] static String FromUtf8WithLatin1Fallback(std::string_view s) {
    return FromUtf8WithLatin1Fallback(base::as_byte_span(s));
  }

  template <typename CharType>
  static String Adopt(StringBuffer<CharType>& buffer) {
    if (!buffer.length()) {
      return StringImpl::empty_;
    }
    return String(buffer.Release());
  }

  static String Boolean(bool value) { return String(value ? "true" : "false"); }
  // Serialize an integer value.
  [[nodiscard]] static String Number(int value);
  // Serialize an integer value.
  [[nodiscard]] static String Number(unsigned value);
  // Serialize an integer value.
  [[nodiscard]] static String Number(long value);
  // Serialize an integer value.
  [[nodiscard]] static String Number(unsigned long value);
  // Serialize an integer value.
  [[nodiscard]] static String Number(long long value);
  // Serialize an integer value.
  [[nodiscard]] static String Number(unsigned long long value);
  [[nodiscard]] static String Number(float);

  [[nodiscard]] static String Number(double, unsigned precision = 6);

  // Number to String conversion following the ECMAScript definition.
  [[nodiscard]] static String NumberToStringEcmaScript(double);
  [[nodiscard]] static String NumberToStringFixedWidth(double,
                                                       unsigned decimal_places);

  // Serializes an unsigned 64-bit integer in hex. This adds no padding,
  // uses lowercase letters for a-f, and adds no "0x" prefix.
  //
  // For example, 266 becomes "10a", and 0 becomes "0".
  [[nodiscard]] static String HexNumber(uint64_t value);

  // Takes a printf format and args and prints into a String.
  // This function supports Latin-1 characters only.
  // PRECONDITIONS: `format` must be compatible with subsequent args.
  // Ideally, this would be UNSAFE_BUFFER_USAGE but there are too many
  // callers at present to investigate.
  [[nodiscard]] PRINTF_FORMAT(1, 2) static String
      Format(const char* format, ...);

  // [string.cons] --------------------------------------------------

  // Construct a null string, distinguishable from an empty string.
  String() = default;

  // Construct a string with UTF-16 data.
  explicit String(base::span<const UChar> utf16_data);

  // Construct a string by copying the contents of a vector.
  // This method will never create a null string. Vectors with size() == 0
  // will return the empty string.
  // NOTE: This is different from String(vector.data(), vector.size())
  // which will sometimes return a null string when vector.data() is null
  // which can only occur for vectors without inline capacity.
  // See: https://bugs.webkit.org/show_bug.cgi?id=109792
  template <wtf_size_t inlineCapacity>
  explicit String(const Vector<UChar, inlineCapacity>&);

  // Construct a string with UTF-16 data, from a null-terminated source.
  String(const UChar*);

  // Construct a string with latin1 data.
  explicit String(base::span<const LChar> latin1_data);
  explicit String(base::span<const char> latin1_data)
      : String(base::as_bytes(latin1_data)) {}
  explicit String(const std::string& s) : String(base::as_byte_span(s)) {}

  // Construct a string with latin1 data, from a null-terminated source. The
  // `LChar` constructor is explicit to avoid misinterpreting byte arrays.
  // If the conversion is implicit, functions with both `String` and
  // `base::span<const uint8_t>` overloads become ambiguous when called on
  // `uint8_t[N]`.
  explicit String(const LChar* characters)
      : String(reinterpret_cast<const char*>(characters)) {}
  String(const char* characters)  // NOLINT(google-explicit-constructor)
      : String(characters ? base::span(std::string_view(characters))
                          : base::span<const char>()) {}

  // Construct a string referencing an existing StringImpl.
  String(StringImpl* impl) : impl_(impl) {}
  String(scoped_refptr<StringImpl> impl) : impl_(std::move(impl)) {}

  // Copying a String is a relatively inexpensive, since the underlying data is
  // immutable and refcounted.
  String(const String&) = default;
  String& operator=(const String&) = default;

  String(String&&) noexcept = default;
  String& operator=(String&&) = default;

  // [string.iterators] ---------------------------------------------

  // `begin()` and `end()` return iterators for `UChar32`, neither `UChar` nor
  // `LChar`. If you'd like to iterate code units, use `[]` and `length()`.
  CodePointIterator begin() const;
  CodePointIterator end() const;

  // [string.capacity] ----------------------------------------------

  size_type length() const {
    if (!impl_) {
      return 0;
    }
    return impl_->length();
  }

  bool empty() const { return !impl_ || !impl_->length(); }

  explicit operator bool() const { return !IsNull(); }
  bool IsNull() const { return !impl_; }

  size_t CharactersSizeInBytes() const {
    return impl_ ? impl_->CharactersSizeInBytes() : 0;
  }

  // [string.access] ------------------------------------------------

  // Returns a code unit at the specified index.
  // This operator returns 0 if the specified index is out of range.
  UChar operator[](size_type index) const {
    if (!impl_ || index >= impl_->length()) {
      return 0;
    }
    // SAFETY: index checked against length above.
    return UNSAFE_BUFFERS((*impl_)[index]);
  }

  // Returns the Unicode code point starting at the specified offset of this
  // string. If the offset points an unpaired surrogate, this function returns
  // 0.
  UChar32 CodePointAtOrZero(size_type) const;

  // Returns the Unicode code point ending with text[i - 1]. That is to say,
  //  - Returns a code point computed from text[i - 2] and text[i - 1] if i-1 is
  //    greater than start_offset and text[i - 2] is a leading surrogate and
  //    text[i - 1] is a trailing surrogate.
  //  - Otherwise, text[i - 1] is returned.
  //
  // `i` argument is updated to point the first code unit of the read character.
  // `start_offset` should be smaller than `i`.
  UChar32 CodePointAtAndPrevious(size_type start_offset, size_type& i) const;

  // Returns the Unicode code point starting at the specified offset of this
  // string. `i` argument is updated to point the next of the read character.
  UChar32 CodePointAtAndNext(size_type& i) const;

  // [string.modifiers] ---------------------------------------------

  // Removes `len` code units starting at `pos` from this string.
  // If `pos` is greater than the string length, it crashes.
  // If `len` exceeds the length from `pos` to the end of the string, the
  // part from `pos` to the end is removed.
  //
  // This function returns a reference to `this` string.
  String& erase(size_type pos, size_type len = npos);

  String& replace(size_type index,
                  size_type length_to_replace,
                  const StringView& replacement) {
    if (impl_) {
      impl_ = impl_->Replace(index, length_to_replace, replacement);
    }
    return *this;
  }
  String& Replace(UChar pattern, UChar replacement) {
    if (impl_) {
      impl_ = impl_->Replace(pattern, replacement);
    }
    return *this;
  }
  String& Replace(UChar pattern, const StringView& replacement) {
    if (impl_) {
      impl_ = impl_->Replace(pattern, replacement);
    }
    return *this;
  }
  String& Replace(const StringView& pattern, const StringView& replacement) {
    if (impl_) {
      impl_ = impl_->Replace(pattern, replacement);
    }
    return *this;
  }

  // Copy characters out of the string. See string_impl.h for detailed docs.
  size_t CopyTo(base::span<UChar> buffer, size_type start) const {
    return impl_ ? impl_->CopyTo(buffer, start) : 0;
  }
  template <typename BufferType>
  void AppendTo(BufferType&,
                size_type start = 0,
                size_type length = npos) const;

  void swap(String& o) { impl_.swap(o.impl_); }

  void Fill(UChar c) {
    if (impl_) {
      impl_ = impl_->Fill(c);
    }
  }

  // [string.operations] --------------------------------------------

  bool Is8Bit() const { return impl_->Is8Bit(); }
  void Ensure16Bit();

  StringImpl* Impl() const { return impl_.get(); }
  scoped_refptr<StringImpl> ReleaseImpl() { return std::move(impl_); }

  // Returns an LChar span of the underlying representation of the string.
  // This function must only be called on 8-bit strings.
  base::span<const LChar> Span8() const {
    if (!impl_)
      return {};
    DCHECK(impl_->Is8Bit());
    return impl_->Span8();
  }

  // Returns an UChar span of the underlying representation of the string.
  // This function must only be called on 16-bit strings.
  base::span<const UChar> Span16() const {
    if (!impl_)
      return {};
    DCHECK(!impl_->Is8Bit());
    return impl_->Span16();
  }

  // Returns a uint16_t span of the underlying representation of the string.
  // This function must only be called on 16-bit strings.
  base::span<const uint16_t> SpanUint16() const {
    if (!impl_) {
      return {};
    }
    DCHECK(!impl_->Is8Bit());
    return impl_->SpanUint16();
  }

  // This exposes the underlying representation of the string. Use with
  // care. When interpreting the string as a sequence of code units
  // Span8()/Span16() should be used.
  base::span<const uint8_t> RawByteSpan() const {
    if (!impl_) {
      return {};
    }
    return impl_->RawByteSpan();
  }

  // Returns a std::string containing the characters of this string.
  // Printable ASCII characters (0x20 to 0x7F) and the null character (0x00)
  // are preserved. Characters outside of this range (including control
  // characters and non-ASCII characters) are converted to '?'.
  [[nodiscard]] std::string Ascii() const;

  // Returns a std::string containing the characters of this string encoded as
  // Latin-1. Characters in the Latin-1 range (0x00 to 0xFF) are preserved.
  // Characters outside of this range (U+0100 and above) are converted to '?'.
  [[nodiscard]] std::string Latin1() const;
  [[nodiscard]] std::string Utf8(
      Utf8ConversionMode mode = Utf8ConversionMode::kLenient) const {
    return StringView(*this).Utf8(mode);
  }
  // Returns a std::u16string_view pointing this string.
  // This should be called only if !Is8Bit().
  //
  // This function should be removed after enabling C++23 because
  // std::u16string_view(Span16()) will work with C++23.
  std::u16string_view View16() const LIFETIME_BOUND {
    return base::as_string_view(Span16());
  }

  // Find substrings.
  size_type find(const StringView& value, size_type start = 0) const;

  // Unicode aware case insensitive string matching. Non-ASCII characters might
  // match to ASCII characters. This function is rarely used to implement web
  // platform features.  See crbug.com/40476285.
  size_type DeprecatedFindIgnoringCase(const StringView& value,
                                       size_type start = 0) const {
    return impl_ ? impl_->DeprecatedFindIgnoringCase(value, start) : npos;
  }

  // ASCII case insensitive string matching.
  size_type FindIgnoringAsciiCase(const StringView& value,
                                  size_type start = 0) const {
    return impl_ ? impl_->FindIgnoringAsciiCase(value, start) : npos;
  }

  // Find characters.
  size_type find(UChar c, size_type start = 0) const {
    return impl_ ? impl_->Find(c, start) : npos;
  }
  size_type find(LChar c, size_type start = 0) const {
    return impl_ ? impl_->Find(c, start) : npos;
  }
  size_type find(char c, size_type start = 0) const {
    return find(static_cast<LChar>(c), start);
  }
  size_type Find(CharacterMatchFunctionPtr match_function,
                 size_type start = 0) const {
    return impl_ ? impl_->Find(match_function, start) : npos;
  }
  size_type Find(base::RepeatingCallback<bool(UChar)> match_callback,
                 size_type index = 0) const;

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
  size_type rfind(const StringView& value, size_type start = npos) const;

  // Find the last instance of a single character.
  // Returns `npos` if it's not found in this string.
  size_type rfind(UChar c, size_type start = npos) const {
    return impl_ ? impl_->ReverseFind(c, start) : npos;
  }

  // We have no find_first_of(), find_last_of(), find_first_not_of(), and
  // find_last_not_of().  Feel free to add them if necessary.

  // Returns a substring.
  //
  // If `pos` is greater than the string length, unlike `std::string::substr`,
  // it crashes (`std::string::substr` throws an `std::out_of_range` exception).
  // If `len` exceeds the length from `pos` to the end of the string, the
  // substring from `pos` to the end is returned.
  //
  // This copies the content of the substring. If you don't need to copy the
  // content, use `subview(pos, len)` instead.
  [[nodiscard]] String substr(size_type pos, size_type len = npos) const;
  // Returns a StringView of the substring.
  //
  // If `pos` is greater than the string length, unlike `std::string::substr`,
  // it crashes (`std::string::substr` throws an `std::out_of_range` exception).
  // If `len` exceeds the length from `pos` to the end of the string, the
  // substring from `pos` to the end is returned.
  //
  // `str.subview(pos, len)` is similar to `StringView(str, pos, len)`, however
  // the latter doesn't accept `len` exceeding the length of `str`.
  [[nodiscard]] StringView subview(size_type pos, size_type len = npos) const;
  // Returns a substring.
  //
  // If `pos` is greater than or equal to the string length, returns an empty
  // string. If `len` exceeds the length from `pos` to the end of the string,
  // the substring from `pos` to the end is returned.
  //
  // This method is deprecated. Use `str.substr(pos, len)` if `pos` is
  // guaranteed to be <= `str.length()`. If `pos` might be greater than
  // `str.length()`, use `str.substr(std::min(pos, str.length()), len)`.
  [[nodiscard]] String DeprecatedSubstring(size_type pos,
                                           size_type len = npos) const;

  bool starts_with(const StringView& prefix) const {
    return impl_ ? impl_->StartsWith(prefix) : prefix.empty();
  }
  // Unicode aware case insensitive string matching. Non-ASCII characters might
  // match to ASCII characters. This function is rarely used to implement web
  // platform features.  See crbug.com/40476285.
  bool DeprecatedStartsWithIgnoringCase(const StringView& prefix) const {
    return impl_ ? impl_->DeprecatedStartsWithIgnoringCase(prefix)
                 : prefix.empty();
  }
  bool StartsWithIgnoringCaseAndAccents(const StringView& prefix) const {
    return impl_ ? impl_->StartsWithIgnoringCaseAndAccents(prefix)
                 : prefix.empty();
  }
  bool StartsWithIgnoringAsciiCase(const StringView& prefix) const {
    return impl_ ? impl_->StartsWithIgnoringAsciiCase(prefix) : prefix.empty();
  }
  bool starts_with(UChar character) const {
    return impl_ ? impl_->StartsWith(character) : false;
  }

  bool ends_with(const StringView& suffix) const {
    return impl_ ? impl_->EndsWith(suffix) : suffix.empty();
  }
  // Unicode aware case insensitive string matching. Non-ASCII characters might
  // match to ASCII characters. This function is rarely used to implement web
  // platform features.  See crbug.com/40476285.
  bool DeprecatedEndsWithIgnoringCase(const StringView& prefix) const {
    return impl_ ? impl_->DeprecatedEndsWithIgnoringCase(prefix)
                 : prefix.empty();
  }
  // Returns true if this string ends with the specified `suffix`, using ASCII
  // case-insensitive matching. If `suffix` is empty, this returns `true`.
  bool EndsWithIgnoringAsciiCase(const StringView& suffix) const {
    return impl_ ? impl_->EndsWithIgnoringAsciiCase(suffix) : suffix.empty();
  }
  bool ends_with(UChar character) const {
    return impl_ ? impl_->EndsWith(character) : false;
  }

  bool contains(const StringView& value) const { return find(value) != npos; }
  bool contains(UChar c) const { return find(c) != npos; }
  bool contains(LChar c) const { return find(c) != npos; }
  bool contains(char c) const { return find(c) != npos; }

  // Functions to analyze the content -------------------------------

  bool ContainsNoAsciiUpper() const {
    return !impl_ || impl_->ContainsNoAsciiUpper();
  }

  bool ContainsOnlyAsciiOrEmpty() const {
    return !impl_ || impl_->ContainsOnlyAsciiOrEmpty();
  }
  bool ContainsOnlyLatin1OrEmpty() const;
  bool ContainsOnlyWhitespaceOrEmpty() const {
    return !impl_ || impl_->ContainsOnlyWhitespaceOrEmpty();
  }

  template <bool isSpecialCharacter(UChar)>
  bool IsAllSpecialCharacters() const;

  // Functions creating new string(s) from `this` string ------------

  // Returns a lowercase version of the string. This function might convert
  // non-ASCII characters to ASCII characters. For example, DeprecatedLower()
  // for U+212A is 'k'.
  // This function is rarely used to implement web platform features. See
  // crbug.com/627682.
  // This function is deprecated. We should use ToAsciiLower() or CaseMap.
  [[nodiscard]] String DeprecatedLower() const;

  // Returns a lowercase version of the string.
  // This function converts ASCII characters only.
  [[nodiscard]] String ToAsciiLower() const;
  // Returns a uppercase version of the string.
  // This function converts ASCII characters only.
  [[nodiscard]] String ToAsciiUpper() const;

  // Returns the length of the string after stripping white spaces.
  // This is equivalent (minus the allocation overhead) of doing:
  // `string.StripWhiteSpace().length()`
  [[nodiscard]] size_type LengthWithStrippedWhiteSpace() const;
  [[nodiscard]] String StripWhiteSpace() const;
  [[nodiscard]] String StripWhiteSpace(IsWhiteSpaceFunctionPtr) const;
  [[nodiscard]] String SimplifyWhiteSpace(
      StripBehavior = kStripExtraWhiteSpace) const;
  [[nodiscard]] String SimplifyWhiteSpace(
      IsWhiteSpaceFunctionPtr,
      StripBehavior = kStripExtraWhiteSpace) const;

  [[nodiscard]] String RemoveCharacters(CharacterMatchFunctionPtr) const;

  // Return the string with case folded for case insensitive comparison.
  [[nodiscard]] String FoldCase() const;

  // Returns a list of substrings of `this`, separated by `separator`.
  // This function copies the content of the string. Please consider if
  // StringView::Split() is applicable.
  //
  // `String("a, , b").Split(", ")` produces ["a", "", "b"], and
  // `String("").Split(",")` produces [""].
  Vector<String> Split(const StringView& separator) const;
  // Returns a list of substrings of `this`, separated by `separator`.
  // This function copies the content of the string. Please consider if
  // StringView::Split() is applicable.
  //
  // `String("a,,b").Split(',')` produces ["a", "", "b"], and
  // `String("").Split(',')` produces [""].
  Vector<String> Split(UChar separator) const;

  // Returns a list of substrings of `this`, separated by the positions where
  // `finder` returns a length.
  //
  // `finder` should be a callable object that takes `StringView` and
  // `size_type` and returns the length of the separator if the specified offset
  // points to a separator, or std::nullopt otherwise.
  template <typename Finder>
    requires requires(Finder finder, const StringView& s, size_type pos) {
      requires std::is_same_v<decltype(finder(s, pos)),
                              std::optional<size_type>>;
    }
  Vector<String> Split(Finder finder) const {
    return internal::SplitByFinder<String, Finder,
                                   /* allow_empty_entries */ true>(*this,
                                                                   finder);
  }

  // Returns a list of substrings of `this`, separated by `separator`.
  // This doesn't produce empty substrings.
  // This function copies the content of the string. Please consider if
  // StringView::SplitSkippingEmpty() is applicable.
  //
  // `String(" a  b").SplitSkippingEmpty(' ')` produces ["a", "b"], and
  // `String("").SplitSkippingEmpty(',')` produces an empty list.
  Vector<String> SplitSkippingEmpty(UChar separator) const;

  // Returns a list of substrings of `this`, separated by the positions where
  // `finder` returns a length. This doesn't produce empty substrings.
  //
  // `finder` should be a callable object that takes `StringView` and
  // `size_type` and returns the length of the separator if the specified offset
  // points to a separator, or std::nullopt otherwise.
  template <typename Finder>
    requires requires(Finder finder, const StringView& s, size_type pos) {
      requires std::is_same_v<decltype(finder(s, pos)),
                              std::optional<size_type>>;
    }
  Vector<String> SplitSkippingEmpty(Finder finder) const {
    return internal::SplitByFinder<String, Finder,
                                   /* allow_empty_entries */ false>(*this,
                                                                    finder);
  }

#ifdef __OBJC__
  String(NSString*);

  // This conversion maps null string to "", which loses the meaning of null
  // string, but we need this mapping because AppKit crashes when passed nil
  // NSStrings.
  operator NSString*() const {
    if (!impl_)
      return @"";
    return *impl_;
  }
#endif

#ifndef NDEBUG
  // For use in the debugger.
  void Show() const;
#endif

  // Returns a version suitable for gtest and base/logging.*.  It prepends and
  // appends double-quotes, and escapes characters other than ASCII printables.
  [[nodiscard]] String EncodeForDebugging() const;

  void WriteIntoTrace(perfetto::TracedValue context) const;

 private:
  friend struct HashTraits<String>;

  scoped_refptr<StringImpl> impl_;
};

#undef DISPATCH_CASE_OP

inline bool operator==(const String& a, const String& b) {
  // We don't use EqualStringView here since we want the IsAtomic() fast path
  // inside blink::Equal.
  return Equal(a.Impl(), b.Impl());
}
inline bool operator==(const String& a, const char* b) {
  return EqualStringView(a, b);
}
inline bool operator==(const char* a, const String& b) {
  return b == a;
}

inline bool EqualIgnoringNullity(const String& a, const String& b) {
  return EqualIgnoringNullity(a.Impl(), b.Impl());
}

inline void swap(String& a, String& b) {
  a.swap(b);
}

// Definitions of string operations

template <wtf_size_t inlineCapacity>
String::String(const Vector<UChar, inlineCapacity>& vector)
    : impl_(vector.size() ? StringImpl::Create(vector) : StringImpl::empty_) {}

inline bool String::ContainsOnlyLatin1OrEmpty() const {
  if (empty())
    return true;

  if (Is8Bit())
    return true;

  return std::ranges::all_of(Span16(), [](UChar ch) { return ch < 0x0100; });
}

#ifdef __OBJC__
// This is for situations in WebKit where the long standing behavior has been
// "nil if empty", so we try to maintain longstanding behavior for the sake of
// entrenched clients
inline NSString* NsStringNilIfEmpty(const String& str) {
  return str.empty() ? nil : (NSString*)str;
}
#endif

// Compare strings using code units, matching Javascript string ordering.  See
// https://infra.spec.whatwg.org/#code-unit-less-than.
WTF_EXPORT int CodeUnitCompare(const String&, const String&);

inline bool CodeUnitCompareLessThan(const String& a, const String& b) {
  return CodeUnitCompare(a.Impl(), b.Impl()) < 0;
}

template <bool isSpecialCharacter(UChar)>
inline bool String::IsAllSpecialCharacters() const {
  return StringView(*this).IsAllSpecialCharacters<isSpecialCharacter>();
}

template <typename BufferType>
void String::AppendTo(BufferType& result,
                      size_type position,
                      size_type length) const {
  if (!impl_)
    return;
  impl_->AppendTo(result, position, length);
}

// Shared global empty string.
WTF_EXPORT extern const String& g_empty_string;
WTF_EXPORT extern const String& g_empty_string16_bit;
WTF_EXPORT extern const String& g_xmlns_with_colon;

// Table representing common HTML strings of type '\n<space>*'.
class WTF_EXPORT NewlineThenWhitespaceStringsTable {
 public:
  // The constant is kept small to minimize the overhead of the table (496
  // bytes).
  static constexpr size_t kTableSize = 32;
  using TableType = std::array<String, kTableSize>;

  static void Init();

  static inline String GetStringForLength(size_t string_length) {
    return g_table_[string_length];
  }

  static bool IsNewlineThenWhitespaces(const StringView& view);

 private:
  static const TableType& g_table_;
};

// Pretty printer for gtest and base/logging.*.  It prepends and appends
// double-quotes, and escapes characters other than ASCII printables.
WTF_EXPORT std::ostream& operator<<(std::ostream&, const String&);

inline StringView::StringView(const String& string LIFETIME_BOUND,
                              size_type offset,
                              size_type length)
    : StringView(string.Impl(), offset, length) {}
inline StringView::StringView(const String& string LIFETIME_BOUND,
                              size_type offset)
    : StringView(string.Impl(), offset) {}
inline StringView::StringView(const String& string LIFETIME_BOUND)
    : StringView(string.Impl()) {}

template <typename T>
struct HashTraits;
// Defined in string_hash.h.
template <>
struct HashTraits<String>;

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(String)

#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_operators.h"
#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_WTF_STRING_H_
