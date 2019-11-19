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

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/integer_to_string_conversion.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace WTF {

struct AtomicStringHash;

// An AtomicString instance represents a string, and multiple AtomicString
// instances can share their string storage if the strings are
// identical. Comparing two AtomicString instances is much faster than comparing
// two String instances because we just check string storage identity.
//
// AtomicString instances are not thread-safe. An AtomicString instance created
// in a thread must be used only in the creator thread.  If multiple threads
// access a single AtomicString instance, we have race condition of a reference
// count in StringImpl, and would hit a runtime CHECK in
// AtomicStringTable::remove().
//
// Exception: g_null_atom and g_empty_atom, are shared in multiple threads, and
// are never stored in AtomicStringTable.
class WTF_EXPORT AtomicString {
  USING_FAST_MALLOC(AtomicString);

 public:
  // The function is defined in StringStatics.cpp.
  static void Init();

  AtomicString() = default;
  AtomicString(const LChar* chars)
      : AtomicString(chars,
                     chars ? strlen(reinterpret_cast<const char*>(chars)) : 0) {
  }

#if defined(ARCH_CPU_64_BITS)
  // Only define a size_t constructor if size_t is 64 bit otherwise
  // we'd have a duplicate define.
  AtomicString(const LChar* chars, size_t length);
#endif  // defined(ARCH_CPU_64_BITS)

  AtomicString(const char* chars)
      : AtomicString(reinterpret_cast<const LChar*>(chars)) {}
  AtomicString(const LChar* chars, unsigned length);
  AtomicString(const UChar* chars, unsigned length);
  AtomicString(const UChar* chars);
  AtomicString(const char16_t* chars)
      : AtomicString(reinterpret_cast<const UChar*>(chars)) {}

  template <wtf_size_t inlineCapacity>
  explicit AtomicString(const Vector<UChar, inlineCapacity>& vector)
      : AtomicString(vector.data(), vector.size()) {}

  // Constructing an AtomicString from a String / StringImpl can be expensive if
  // the StringImpl is not already atomic.
  explicit AtomicString(StringImpl* impl) : string_(Add(impl)) {}
  explicit AtomicString(const String& s) : string_(Add(s.Impl())) {}

  explicit operator bool() const { return !IsNull(); }
  operator const String&() const { return string_; }
  const String& GetString() const { return string_; }

  StringImpl* Impl() const { return string_.Impl(); }

  bool Is8Bit() const { return string_.Is8Bit(); }
  const LChar* Characters8() const { return string_.Characters8(); }
  const UChar* Characters16() const { return string_.Characters16(); }
  wtf_size_t length() const { return string_.length(); }

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
  wtf_size_t Find(
      const StringView& value,
      wtf_size_t start = 0,
      TextCaseSensitivity case_sensitivity = kTextCaseSensitive) const {
    return string_.Find(value, start, case_sensitivity);
  }

  // Unicode aware case insensitive string matching. Non-ASCII characters might
  // match to ASCII characters. This function is rarely used to implement web
  // platform features.
  wtf_size_t FindIgnoringCase(const StringView& value,
                              wtf_size_t start = 0) const {
    return string_.FindIgnoringCase(value, start);
  }

  // ASCII case insensitive string matching.
  wtf_size_t FindIgnoringASCIICase(const StringView& value,
                                   wtf_size_t start = 0) const {
    return string_.FindIgnoringASCIICase(value, start);
  }

  bool Contains(char c) const { return find(c) != kNotFound; }
  bool Contains(
      const StringView& value,
      TextCaseSensitivity case_sensitivity = kTextCaseSensitive) const {
    return Find(value, 0, case_sensitivity) != kNotFound;
  }

  // Find the last instance of a single character or string.
  wtf_size_t ReverseFind(UChar c, wtf_size_t start = UINT_MAX) const {
    return string_.ReverseFind(c, start);
  }
  wtf_size_t ReverseFind(const StringView& value,
                         wtf_size_t start = UINT_MAX) const {
    return string_.ReverseFind(value, start);
  }

  bool StartsWith(
      const StringView& prefix,
      TextCaseSensitivity case_sensitivity = kTextCaseSensitive) const {
    return string_.StartsWith(prefix, case_sensitivity);
  }
  bool StartsWithIgnoringCase(const StringView& prefix) const {
    return string_.StartsWithIgnoringCase(prefix);
  }
  bool StartsWithIgnoringASCIICase(const StringView& prefix) const {
    return string_.StartsWithIgnoringASCIICase(prefix);
  }
  bool StartsWith(UChar character) const {
    return string_.StartsWith(character);
  }

  bool EndsWith(
      const StringView& suffix,
      TextCaseSensitivity case_sensitivity = kTextCaseSensitive) const {
    return string_.EndsWith(suffix, case_sensitivity);
  }
  bool EndsWith(UChar character) const { return string_.EndsWith(character); }

  // Returns a lowercase/uppercase version of the string.
  // These functions convert ASCII characters only.
  AtomicString LowerASCII() const;
  AtomicString UpperASCII() const;

  // See comments in WTFString.h.
  int ToInt(bool* ok = nullptr) const { return string_.ToInt(ok); }
  double ToDouble(bool* ok = nullptr) const { return string_.ToDouble(ok); }
  float ToFloat(bool* ok = nullptr) const { return string_.ToFloat(ok); }

  template <typename IntegerType>
  static AtomicString Number(IntegerType number) {
    IntegerToStringConverter<IntegerType> converter(number);
    return AtomicString(converter.Characters8(), converter.length());
  }

  static AtomicString Number(double, unsigned precision = 6);

  bool IsNull() const { return string_.IsNull(); }
  bool IsEmpty() const { return string_.IsEmpty(); }

#ifdef __OBJC__
  AtomicString(NSString* s) : string_(Add((CFStringRef)s)) {}
  operator NSString*() const { return string_; }
#endif
  // AtomicString::fromUTF8 will return a null string if
  // the input data contains invalid UTF-8 sequences.
  // NOTE: Passing a zero size means use the whole string.
  static AtomicString FromUTF8(const char*, size_t length);
  static AtomicString FromUTF8(const char*);

  std::string Ascii() const { return string_.Ascii(); }
  std::string Latin1() const { return string_.Latin1(); }
  std::string Utf8(UTF8ConversionMode mode = kLenientUTF8Conversion) const {
    return string_.Utf8(mode);
  }

  size_t CharactersSizeInBytes() const {
    return string_.CharactersSizeInBytes();
  }

  bool IsSafeToSendToAnotherThread() const {
    return string_.IsSafeToSendToAnotherThread();
  }

#ifndef NDEBUG
  void Show() const;
#endif

 private:
  friend struct HashTraits<AtomicString>;

  String string_;

  ALWAYS_INLINE static scoped_refptr<StringImpl> Add(StringImpl* r) {
    if (!r || r->IsAtomic())
      return r;
    return AddSlowCase(r);
  }
  static scoped_refptr<StringImpl> AddSlowCase(StringImpl*);
#if defined(OS_MACOSX)
  static scoped_refptr<StringImpl> Add(CFStringRef);
#endif
};

inline bool operator==(const AtomicString& a, const AtomicString& b) {
  return a.Impl() == b.Impl();
}
inline bool operator==(const AtomicString& a, const String& b) {
  // We don't use equalStringView so we get the isAtomic() optimization inside
  // WTF::equal.
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

inline bool operator!=(const AtomicString& a, const AtomicString& b) {
  return a.Impl() != b.Impl();
}
inline bool operator!=(const AtomicString& a, const String& b) {
  return !(a == b);
}
inline bool operator!=(const String& a, const AtomicString& b) {
  return !(a == b);
}
inline bool operator!=(const AtomicString& a, const char* b) {
  return !(a == b);
}
inline bool operator!=(const char* a, const AtomicString& b) {
  return !(a == b);
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

// AtomicStringHash is the default hash for AtomicString
template <typename T>
struct DefaultHash;
template <>
struct DefaultHash<AtomicString> {
  typedef AtomicStringHash Hash;
};

// Pretty printer for gtest and base/logging.*.  It prepends and appends
// double-quotes, and escapes characters other than ASCII printables.
WTF_EXPORT std::ostream& operator<<(std::ostream&, const AtomicString&);

inline StringView::StringView(const AtomicString& string,
                              unsigned offset,
                              unsigned length)
    : StringView(string.Impl(), offset, length) {}
inline StringView::StringView(const AtomicString& string, unsigned offset)
    : StringView(string.Impl(), offset) {}
inline StringView::StringView(const AtomicString& string)
    : StringView(string.Impl()) {}

}  // namespace WTF

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(AtomicString)

using WTF::AtomicString;
using WTF::g_null_atom;
using WTF::g_empty_atom;
using WTF::g_star_atom;
using WTF::g_xml_atom;
using WTF::g_xmlns_atom;
using WTF::g_xlink_atom;

#include "third_party/blink/renderer/platform/wtf/text/string_concatenate.h"
#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_ATOMIC_STRING_H_
