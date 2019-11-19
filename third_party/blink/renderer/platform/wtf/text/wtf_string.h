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

#include <iosfwd>
#include "base/containers/span.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/integer_to_string_conversion.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

#ifdef __OBJC__
#include <objc/objc.h>
#endif

namespace WTF {

struct StringHash;

enum UTF8ConversionMode {
  kLenientUTF8Conversion,
  kStrictUTF8Conversion,
  kStrictUTF8ConversionReplacingUnpairedSurrogatesWithFFFD
};

#define DISPATCH_CASE_OP(caseSensitivity, op, args)     \
  ((caseSensitivity == kTextCaseSensitive)              \
       ? op args                                        \
       : (caseSensitivity == kTextCaseASCIIInsensitive) \
             ? op##IgnoringASCIICase args               \
             : op##IgnoringCase args)

// You can find documentation about this class in README.md in this directory.
class WTF_EXPORT String {
  USING_FAST_MALLOC(String);

 public:
  // Construct a null string, distinguishable from an empty string.
  String() = default;

  // Construct a string with UTF-16 data.
  String(const UChar* characters, unsigned length);

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
  String(const char16_t* chars)
      : String(reinterpret_cast<const UChar*>(chars)) {}

  // Construct a string with latin1 data.
  String(const LChar* characters, unsigned length);
  String(const char* characters, unsigned length);

#if defined(ARCH_CPU_64_BITS)
  // Only define a size_t constructor if size_t is 64 bit otherwise
  // we'd have a duplicate define.
  String(const char* characters, size_t length);
#endif  // defined(ARCH_CPU_64_BITS)

  // Construct a string with latin1 data, from a null-terminated source.
  String(const LChar* characters)
      : String(reinterpret_cast<const char*>(characters)) {}
  String(const char* characters)
      : String(characters, characters ? strlen(characters) : 0) {}

  // Construct a string referencing an existing StringImpl.
  String(StringImpl* impl) : impl_(impl) {}
  String(scoped_refptr<StringImpl> impl) : impl_(std::move(impl)) {}

  void swap(String& o) { impl_.swap(o.impl_); }

  template <typename CharType>
  static String Adopt(StringBuffer<CharType>& buffer) {
    if (!buffer.length())
      return StringImpl::empty_;
    return String(buffer.Release());
  }

  explicit operator bool() const { return !IsNull(); }
  bool IsNull() const { return !impl_; }
  bool IsEmpty() const { return !impl_ || !impl_->length(); }

  StringImpl* Impl() const { return impl_.get(); }
  scoped_refptr<StringImpl> ReleaseImpl() { return std::move(impl_); }

  unsigned length() const {
    if (!impl_)
      return 0;
    return impl_->length();
  }

  // Prefer Span8() and Span16() to Characters8() and Characters16().
  base::span<const LChar> Span8() const {
    if (!impl_)
      return {};
    DCHECK(impl_->Is8Bit());
    return impl_->Span8();
  }

  base::span<const UChar> Span16() const {
    if (!impl_)
      return {};
    DCHECK(!impl_->Is8Bit());
    return impl_->Span16();
  }

  const LChar* Characters8() const {
    if (!impl_)
      return nullptr;
    DCHECK(impl_->Is8Bit());
    return impl_->Characters8();
  }

  const UChar* Characters16() const {
    if (!impl_)
      return nullptr;
    DCHECK(!impl_->Is8Bit());
    return impl_->Characters16();
  }

  ALWAYS_INLINE const void* Bytes() const {
    if (!impl_)
      return nullptr;
    return impl_->Bytes();
  }

  // Return characters8() or characters16() depending on CharacterType.
  template <typename CharacterType>
  inline const CharacterType* GetCharacters() const;

  bool Is8Bit() const { return impl_->Is8Bit(); }

  std::string Ascii() const WARN_UNUSED_RESULT;
  std::string Latin1() const WARN_UNUSED_RESULT;
  std::string Utf8(UTF8ConversionMode = kLenientUTF8Conversion) const
      WARN_UNUSED_RESULT;

  UChar operator[](unsigned index) const {
    if (!impl_ || index >= impl_->length())
      return 0;
    return (*impl_)[index];
  }

  template <typename IntegerType>
  static String Number(IntegerType number) {
    IntegerToStringConverter<IntegerType> converter(number);
    return StringImpl::Create(converter.Characters8(), converter.length());
  }

  static String Number(float) WARN_UNUSED_RESULT;

  static String Number(double, unsigned precision = 6) WARN_UNUSED_RESULT;

  // Number to String conversion following the ECMAScript definition.
  static String NumberToStringECMAScript(double) WARN_UNUSED_RESULT;
  static String NumberToStringFixedWidth(double, unsigned decimal_places)
      WARN_UNUSED_RESULT;

  // Find characters.
  wtf_size_t find(UChar c, unsigned start = 0) const {
    return impl_ ? impl_->Find(c, start) : kNotFound;
  }
  wtf_size_t find(LChar c, unsigned start = 0) const {
    return impl_ ? impl_->Find(c, start) : kNotFound;
  }
  wtf_size_t find(char c, unsigned start = 0) const {
    return find(static_cast<LChar>(c), start);
  }
  wtf_size_t Find(CharacterMatchFunctionPtr match_function,
                  unsigned start = 0) const {
    return impl_ ? impl_->Find(match_function, start) : kNotFound;
  }

  // Find substrings.
  wtf_size_t Find(
      const StringView& value,
      unsigned start = 0,
      TextCaseSensitivity case_sensitivity = kTextCaseSensitive) const {
    return impl_
               ? DISPATCH_CASE_OP(case_sensitivity, impl_->Find, (value, start))
               : kNotFound;
  }

  // Unicode aware case insensitive string matching. Non-ASCII characters might
  // match to ASCII characters. This function is rarely used to implement web
  // platform features.
  wtf_size_t FindIgnoringCase(const StringView& value,
                              unsigned start = 0) const {
    return impl_ ? impl_->FindIgnoringCase(value, start) : kNotFound;
  }

  // ASCII case insensitive string matching.
  wtf_size_t FindIgnoringASCIICase(const StringView& value,
                                   unsigned start = 0) const {
    return impl_ ? impl_->FindIgnoringASCIICase(value, start) : kNotFound;
  }

  bool Contains(char c) const { return find(c) != kNotFound; }
  bool Contains(
      const StringView& value,
      TextCaseSensitivity case_sensitivity = kTextCaseSensitive) const {
    return Find(value, 0, case_sensitivity) != kNotFound;
  }

  // Find the last instance of a single character or string.
  wtf_size_t ReverseFind(UChar c, unsigned start = UINT_MAX) const {
    return impl_ ? impl_->ReverseFind(c, start) : kNotFound;
  }
  wtf_size_t ReverseFind(const StringView& value,
                         unsigned start = UINT_MAX) const {
    return impl_ ? impl_->ReverseFind(value, start) : kNotFound;
  }

  UChar32 CharacterStartingAt(unsigned) const;

  bool StartsWith(
      const StringView& prefix,
      TextCaseSensitivity case_sensitivity = kTextCaseSensitive) const {
    return impl_
               ? DISPATCH_CASE_OP(case_sensitivity, impl_->StartsWith, (prefix))
               : prefix.IsEmpty();
  }
  bool StartsWithIgnoringCase(const StringView& prefix) const {
    return impl_ ? impl_->StartsWithIgnoringCase(prefix) : prefix.IsEmpty();
  }
  bool StartsWithIgnoringASCIICase(const StringView& prefix) const {
    return impl_ ? impl_->StartsWithIgnoringASCIICase(prefix)
                 : prefix.IsEmpty();
  }
  bool StartsWith(UChar character) const {
    return impl_ ? impl_->StartsWith(character) : false;
  }

  bool EndsWith(
      const StringView& suffix,
      TextCaseSensitivity case_sensitivity = kTextCaseSensitive) const {
    return impl_ ? DISPATCH_CASE_OP(case_sensitivity, impl_->EndsWith, (suffix))
                 : suffix.IsEmpty();
  }
  bool EndsWithIgnoringCase(const StringView& prefix) const {
    return impl_ ? impl_->EndsWithIgnoringCase(prefix) : prefix.IsEmpty();
  }
  bool EndsWithIgnoringASCIICase(const StringView& prefix) const {
    return impl_ ? impl_->EndsWithIgnoringASCIICase(prefix) : prefix.IsEmpty();
  }
  bool EndsWith(UChar character) const {
    return impl_ ? impl_->EndsWith(character) : false;
  }

  // TODO(esprehn): replace strangely both modifies this String *and* return a
  // value. It should only do one of those.
  String& Replace(UChar pattern, UChar replacement) {
    if (impl_)
      impl_ = impl_->Replace(pattern, replacement);
    return *this;
  }
  String& Replace(UChar pattern, const StringView& replacement) {
    if (impl_)
      impl_ = impl_->Replace(pattern, replacement);
    return *this;
  }
  String& Replace(const StringView& pattern, const StringView& replacement) {
    if (impl_)
      impl_ = impl_->Replace(pattern, replacement);
    return *this;
  }
  String& replace(unsigned index,
                  unsigned length_to_replace,
                  const StringView& replacement) {
    if (impl_)
      impl_ = impl_->Replace(index, length_to_replace, replacement);
    return *this;
  }

  void Fill(UChar c) {
    if (impl_)
      impl_ = impl_->Fill(c);
  }

  void Ensure16Bit();

  void Truncate(unsigned length);
  void Remove(unsigned start, unsigned length = 1);

  String Substring(unsigned pos,
                   unsigned len = UINT_MAX) const WARN_UNUSED_RESULT;
  String Left(unsigned len) const WARN_UNUSED_RESULT {
    return Substring(0, len);
  }
  String Right(unsigned len) const WARN_UNUSED_RESULT {
    return Substring(length() - len, len);
  }

  // Returns a lowercase version of the string. This function might convert
  // non-ASCII characters to ASCII characters. For example, DeprecatedLower()
  // for U+212A is 'k'.
  // This function is rarely used to implement web platform features. See
  // crbug.com/627682.
  // This function is deprecated. We should use LowerASCII() or CaseMap.
  String DeprecatedLower() const WARN_UNUSED_RESULT;

  // Returns a lowercase version of the string.
  // This function converts ASCII characters only.
  String LowerASCII() const WARN_UNUSED_RESULT;
  // Returns a uppercase version of the string.
  // This function converts ASCII characters only.
  String UpperASCII() const WARN_UNUSED_RESULT;

  String StripWhiteSpace() const WARN_UNUSED_RESULT;
  String StripWhiteSpace(IsWhiteSpaceFunctionPtr) const WARN_UNUSED_RESULT;
  String SimplifyWhiteSpace(StripBehavior = kStripExtraWhiteSpace) const
      WARN_UNUSED_RESULT;
  String SimplifyWhiteSpace(IsWhiteSpaceFunctionPtr,
                            StripBehavior = kStripExtraWhiteSpace) const
      WARN_UNUSED_RESULT;

  String RemoveCharacters(CharacterMatchFunctionPtr) const WARN_UNUSED_RESULT;
  template <bool isSpecialCharacter(UChar)>
  bool IsAllSpecialCharacters() const;

  // Return the string with case folded for case insensitive comparison.
  String FoldCase() const WARN_UNUSED_RESULT;

  // Takes a printf format and args and prints into a String.
  // This function supports Latin-1 characters only.
  PRINTF_FORMAT(1, 2)
  static String Format(const char* format, ...) WARN_UNUSED_RESULT;

  // Returns a version suitable for gtest and base/logging.*.  It prepends and
  // appends double-quotes, and escapes characters other than ASCII printables.
  String EncodeForDebugging() const WARN_UNUSED_RESULT;

  // Returns an uninitialized string. The characters needs to be written
  // into the buffer returned in data before the returned string is used.
  // Failure to do this will have unpredictable results.
  static String CreateUninitialized(unsigned length,
                                    UChar*& data) WARN_UNUSED_RESULT {
    return StringImpl::CreateUninitialized(length, data);
  }
  static String CreateUninitialized(unsigned length,
                                    LChar*& data) WARN_UNUSED_RESULT {
    return StringImpl::CreateUninitialized(length, data);
  }

  void Split(const StringView& separator,
             bool allow_empty_entries,
             Vector<String>& result) const;
  void Split(const StringView& separator, Vector<String>& result) const {
    Split(separator, false, result);
  }
  void Split(UChar separator,
             bool allow_empty_entries,
             Vector<String>& result) const;
  void Split(UChar separator, Vector<String>& result) const {
    Split(separator, false, result);
  }

  // Copy characters out of the string. See StringImpl.h for detailed docs.
  unsigned CopyTo(UChar* buffer, unsigned start, unsigned max_length) const {
    return impl_ ? impl_->CopyTo(buffer, start, max_length) : 0;
  }
  template <typename BufferType>
  void AppendTo(BufferType&,
                unsigned start = 0,
                unsigned length = UINT_MAX) const;
  template <typename BufferType>
  void PrependTo(BufferType&,
                 unsigned start = 0,
                 unsigned length = UINT_MAX) const;

  // Convert the string into a number.

  // The following ToFooStrict functions accept:
  //  - leading '+'
  //  - leading Unicode whitespace
  //  - trailing Unicode whitespace
  //  - no "-0" (ToUIntStrict and ToUInt64Strict)
  //  - no out-of-range numbers which the resultant type can't represent
  //
  // If the input string is not acceptable, 0 is returned and |*ok| becomes
  // |false|.
  //
  // We can use these functions to implement a Web Platform feature only if the
  // input string is already valid according to the specification of the
  // feature.
  int ToIntStrict(bool* ok = nullptr) const;
  unsigned ToUIntStrict(bool* ok = nullptr) const;
  unsigned HexToUIntStrict(bool* ok) const;
  int64_t ToInt64Strict(bool* ok = nullptr) const;
  uint64_t ToUInt64Strict(bool* ok = nullptr) const;

  // The following ToFoo functions accept:
  //  - leading '+'
  //  - leading Unicode whitespace
  //  - trailing garbage
  //  - no "-0" (ToUInt and ToUInt64)
  //  - no out-of-range numbers which the resultant type can't represent
  //
  // If the input string is not acceptable, 0 is returned and |*ok| becomes
  // |false|.
  //
  // We can use these functions to implement a Web Platform feature only if the
  // input string is already valid according to the specification of the
  // feature.
  int ToInt(bool* ok = nullptr) const;
  unsigned ToUInt(bool* ok = nullptr) const;

  // These functions accepts:
  //  - leading '+'
  //  - numbers without leading zeros such as ".5"
  //  - numbers ending with "." such as "3."
  //  - scientific notation
  //  - leading whitespace (IsASCIISpace, not IsHTMLSpace)
  //  - no trailing whitespace
  //  - no trailing garbage
  //  - no numbers such as "NaN" "Infinity"
  //
  // A huge absolute number which a double/float can't represent is accepted,
  // and +Infinity or -Infinity is returned.
  //
  // A small absolute numbers which a double/float can't represent is accepted,
  // and 0 is returned
  //
  // If the input string is not acceptable, 0.0 is returned and |*ok| becomes
  // |false|.
  //
  // We can use these functions to implement a Web Platform feature only if the
  // input string is already valid according to the specification of the
  // feature.
  //
  // FIXME: Like the strict functions above, these give false for "ok" when
  // there is trailing garbage.  Like the non-strict functions above, these
  // return the value when there is trailing garbage.  It would be better if
  // these were more consistent with the above functions instead.
  double ToDouble(bool* ok = nullptr) const;
  float ToFloat(bool* ok = nullptr) const;

  String IsolatedCopy() const WARN_UNUSED_RESULT;
  bool IsSafeToSendToAnotherThread() const;

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

  static String Make8BitFrom16BitSource(const UChar*,
                                        wtf_size_t) WARN_UNUSED_RESULT;
  template <wtf_size_t inlineCapacity>
  static WARN_UNUSED_RESULT String
  Make8BitFrom16BitSource(const Vector<UChar, inlineCapacity>& buffer) {
    return Make8BitFrom16BitSource(buffer.data(), buffer.size());
  }

  static String Make16BitFrom8BitSource(const LChar*,
                                        wtf_size_t) WARN_UNUSED_RESULT;

  // String::fromUTF8 will return a null string if
  // the input data contains invalid UTF-8 sequences.
  // Does not strip BOMs.
  static String FromUTF8(const LChar*, size_t) WARN_UNUSED_RESULT;
  static String FromUTF8(const LChar*) WARN_UNUSED_RESULT;
  static String FromUTF8(const char* s, size_t length) WARN_UNUSED_RESULT {
    return FromUTF8(reinterpret_cast<const LChar*>(s), length);
  }
  static String FromUTF8(const char* s) WARN_UNUSED_RESULT {
    return FromUTF8(reinterpret_cast<const LChar*>(s));
  }
  static String FromUTF8(base::StringPiece) WARN_UNUSED_RESULT;

  // Tries to convert the passed in string to UTF-8, but will fall back to
  // Latin-1 if the string is not valid UTF-8.
  static String FromUTF8WithLatin1Fallback(const LChar*,
                                           size_t) WARN_UNUSED_RESULT;
  static String FromUTF8WithLatin1Fallback(const char* s,
                                           size_t length) WARN_UNUSED_RESULT {
    return FromUTF8WithLatin1Fallback(reinterpret_cast<const LChar*>(s),
                                      length);
  }

  bool ContainsOnlyASCIIOrEmpty() const {
    return !impl_ || impl_->ContainsOnlyASCIIOrEmpty();
  }
  bool ContainsOnlyLatin1OrEmpty() const;
  bool ContainsOnlyWhitespaceOrEmpty() const {
    return !impl_ || impl_->ContainsOnlyWhitespaceOrEmpty();
  }

  size_t CharactersSizeInBytes() const {
    return impl_ ? impl_->CharactersSizeInBytes() : 0;
  }

#ifndef NDEBUG
  // For use in the debugger.
  void Show() const;
#endif

 private:
  friend struct HashTraits<String>;

  scoped_refptr<StringImpl> impl_;
};

#undef DISPATCH_CASE_OP

inline bool operator==(const String& a, const String& b) {
  // We don't use equalStringView here since we want the isAtomic() fast path
  // inside WTF::equal.
  return Equal(a.Impl(), b.Impl());
}
inline bool operator==(const String& a, const char* b) {
  return EqualStringView(a, b);
}
inline bool operator==(const char* a, const String& b) {
  return b == a;
}

inline bool operator!=(const String& a, const String& b) {
  return !(a == b);
}
inline bool operator!=(const String& a, const char* b) {
  return !(a == b);
}
inline bool operator!=(const char* a, const String& b) {
  return !(a == b);
}

inline bool EqualIgnoringNullity(const String& a, const String& b) {
  return EqualIgnoringNullity(a.Impl(), b.Impl());
}

template <wtf_size_t inlineCapacity>
inline bool EqualIgnoringNullity(const Vector<UChar, inlineCapacity>& a,
                                 const String& b) {
  return EqualIgnoringNullity(a, b.Impl());
}

inline void swap(String& a, String& b) {
  a.swap(b);
}

// Definitions of string operations

template <wtf_size_t inlineCapacity>
String::String(const Vector<UChar, inlineCapacity>& vector)
    : impl_(vector.size() ? StringImpl::Create(vector.data(), vector.size())
                          : StringImpl::empty_) {}

template <>
inline const LChar* String::GetCharacters<LChar>() const {
  DCHECK(Is8Bit());
  return Characters8();
}

template <>
inline const UChar* String::GetCharacters<UChar>() const {
  DCHECK(!Is8Bit());
  return Characters16();
}

inline bool String::ContainsOnlyLatin1OrEmpty() const {
  if (IsEmpty())
    return true;

  if (Is8Bit())
    return true;

  const UChar* characters = Characters16();
  UChar ored = 0;
  for (wtf_size_t i = 0; i < impl_->length(); ++i)
    ored |= characters[i];
  return !(ored & 0xFF00);
}

#ifdef __OBJC__
// This is for situations in WebKit where the long standing behavior has been
// "nil if empty", so we try to maintain longstanding behavior for the sake of
// entrenched clients
inline NSString* NsStringNilIfEmpty(const String& str) {
  return str.IsEmpty() ? nil : (NSString*)str;
}
#endif

// Compare strings using code units, matching Javascript string ordering.  See
// https://infra.spec.whatwg.org/#code-unit-less-than.
WTF_EXPORT int CodeUnitCompare(const String&, const String&);

inline bool CodeUnitCompareLessThan(const String& a, const String& b) {
  return CodeUnitCompare(a.Impl(), b.Impl()) < 0;
}

WTF_EXPORT int CodeUnitCompareIgnoringASCIICase(const String&, const char*);

template <bool isSpecialCharacter(UChar)>
inline bool String::IsAllSpecialCharacters() const {
  return StringView(*this).IsAllSpecialCharacters<isSpecialCharacter>();
}

template <typename BufferType>
void String::AppendTo(BufferType& result,
                      unsigned position,
                      unsigned length) const {
  if (!impl_)
    return;
  impl_->AppendTo(result, position, length);
}

template <typename BufferType>
void String::PrependTo(BufferType& result,
                       unsigned position,
                       unsigned length) const {
  if (!impl_)
    return;
  impl_->PrependTo(result, position, length);
}

// StringHash is the default hash for String
template <typename T>
struct DefaultHash;
template <>
struct DefaultHash<String> {
  typedef StringHash Hash;
};

// Shared global empty string.
WTF_EXPORT extern const String& g_empty_string;
WTF_EXPORT extern const String& g_empty_string16_bit;
WTF_EXPORT extern const String& g_xmlns_with_colon;

// Pretty printer for gtest and base/logging.*.  It prepends and appends
// double-quotes, and escapes characters other than ASCII printables.
WTF_EXPORT std::ostream& operator<<(std::ostream&, const String&);

inline StringView::StringView(const String& string,
                              unsigned offset,
                              unsigned length)
    : StringView(string.Impl(), offset, length) {}
inline StringView::StringView(const String& string, unsigned offset)
    : StringView(string.Impl(), offset) {}
inline StringView::StringView(const String& string)
    : StringView(string.Impl()) {}

}  // namespace WTF

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(String)

using WTF::kStrictUTF8Conversion;
using WTF::kStrictUTF8ConversionReplacingUnpairedSurrogatesWithFFFD;
using WTF::String;
using WTF::g_empty_string;
using WTF::g_empty_string16_bit;
using WTF::Equal;
using WTF::Find;
using WTF::IsSpaceOrNewline;

#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_WTF_STRING_H_
