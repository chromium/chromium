/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_STRING_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_STRING_H_

#include <string>
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/strings/latin1_string_conversions.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string16.h"
#include "third_party/blink/public/platform/web_common.h"

#if INSIDE_BLINK
#include "third_party/blink/renderer/platform/wtf/forward.h"  // nogncheck
#endif

namespace WTF {
class StringImpl;
}

namespace blink {

// Use either one of static methods to convert ASCII, Latin1, UTF-8 or
// UTF-16 string into WebString:
//
// * WebString::FromASCII(const std::string& ascii)
// * WebString::FromLatin1(const std::string& latin1)
// * WebString::FromUTF8(const std::string& utf8)
// * WebString::FromUTF16(const base::string16& utf16)
// * WebString::FromUTF16(const base::NullableString16& utf16)
// * WebString::FromUTF16(const base::Optional<base::string16>& utf16)
//
// Similarly, use either of following methods to convert WebString to
// ASCII, Latin1, UTF-8 or UTF-16:
//
// * webstring.Ascii()
// * webstring.Latin1()
// * webstring.Utf8()
// * webstring.Utf16()
// * WebString::ToNullableString16(webstring)
// * WebString::ToOptionalString16(webstring)
//
// Note that if you need to convert the UTF8 string converted from WebString
// back to WebString with FromUTF8() you may want to specify Strict
// UTF8ConversionMode when you call Utf8(), as FromUTF8 rejects strings
// with invalid UTF8 characters.
//
// Some types like GURL and base::FilePath can directly take either utf-8 or
// utf-16 strings. Use following methods to convert WebString to/from GURL or
// FilePath rather than going through intermediate string types:
//
// * GURL WebStringToGURL(const WebString&)
// * base::FilePath WebStringToFilePath(const WebString&)
// * WebString FilePathToWebString(const base::FilePath&);
//
// It is inexpensive to copy a WebString object.
// WARNING: It is not safe to pass a WebString across threads!!!
//
class WebString {
 public:
  enum class UTF8ConversionMode {
    // Ignores errors for invalid characters.
    kLenient,
    // Errors out on invalid characters, returns null string.
    kStrict,
    // Replace invalid characters with 0xFFFD.
    // (This is the same conversion mode as base::UTF16ToUTF8)
    kStrictReplacingErrorsWithFFFD,
  };

  BLINK_PLATFORM_EXPORT ~WebString();
  BLINK_PLATFORM_EXPORT WebString();
  BLINK_PLATFORM_EXPORT WebString(const WebUChar* data, size_t len);

  BLINK_PLATFORM_EXPORT WebString(const WebString&);
  BLINK_PLATFORM_EXPORT WebString(WebString&&);

  BLINK_PLATFORM_EXPORT WebString& operator=(const WebString&);
  BLINK_PLATFORM_EXPORT WebString& operator=(WebString&&);

  BLINK_PLATFORM_EXPORT void Reset();

  BLINK_PLATFORM_EXPORT bool Equals(const WebString&) const;
  BLINK_PLATFORM_EXPORT bool Equals(const char* characters, size_t len) const;
  bool Equals(const char* characters) const {
    return Equals(characters, characters ? strlen(characters) : 0);
  }

  BLINK_PLATFORM_EXPORT size_t length() const;

  bool IsEmpty() const { return !length(); }
  bool IsNull() const { return !impl_; }

  BLINK_PLATFORM_EXPORT std::string Utf8(
      UTF8ConversionMode = UTF8ConversionMode::kLenient) const;

  BLINK_PLATFORM_EXPORT static WebString FromUTF8(const char* data,
                                                  size_t length);
  static WebString FromUTF8(const std::string& s) {
    return FromUTF8(s.data(), s.length());
  }

  base::string16 Utf16() const {
    return base::Latin1OrUTF16ToUTF16(length(), Data8(), Data16());
  }

  BLINK_PLATFORM_EXPORT static WebString FromUTF16(const base::string16&);
  BLINK_PLATFORM_EXPORT static WebString FromUTF16(
      const base::NullableString16&);
  BLINK_PLATFORM_EXPORT static WebString FromUTF16(
      const base::Optional<base::string16>&);

  static base::NullableString16 ToNullableString16(const WebString& s) {
    return base::NullableString16(ToOptionalString16(s));
  }

  static base::Optional<base::string16> ToOptionalString16(const WebString& s) {
    return s.IsNull() ? base::nullopt : base::make_optional(s.Utf16());
  }

  BLINK_PLATFORM_EXPORT std::string Latin1() const;

  BLINK_PLATFORM_EXPORT static WebString FromLatin1(const WebLChar* data,
                                                    size_t length);

  static WebString FromLatin1(const std::string& s) {
    return FromLatin1(reinterpret_cast<const WebLChar*>(s.data()), s.length());
  }

  // This asserts if the string contains non-ascii characters.
  // Use this rather than calling base::UTF16ToASCII() which always incurs
  // (likely unnecessary) string16 conversion.
  BLINK_PLATFORM_EXPORT std::string Ascii() const;

  // Use this rather than calling base::IsStringASCII().
  BLINK_PLATFORM_EXPORT bool ContainsOnlyASCII() const;

  // Does same as FromLatin1 but asserts if the given string has non-ascii char.
  BLINK_PLATFORM_EXPORT static WebString FromASCII(const std::string&);

  template <int N>
  WebString(const char (&data)[N]) : WebString(FromUTF8(data, N - 1)) {}

  template <int N>
  WebString& operator=(const char (&data)[N]) {
    *this = FromUTF8(data, N - 1);
    return *this;
  }

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebString(const WTF::String&);
  BLINK_PLATFORM_EXPORT WebString& operator=(const WTF::String&);
  BLINK_PLATFORM_EXPORT operator WTF::String() const;

  BLINK_PLATFORM_EXPORT operator WTF::StringView() const;

  BLINK_PLATFORM_EXPORT WebString(const WTF::AtomicString&);
  BLINK_PLATFORM_EXPORT WebString& operator=(const WTF::AtomicString&);
  BLINK_PLATFORM_EXPORT operator WTF::AtomicString() const;
#endif

 private:
  BLINK_PLATFORM_EXPORT bool Is8Bit() const;
  BLINK_PLATFORM_EXPORT const WebLChar* Data8() const;
  BLINK_PLATFORM_EXPORT const WebUChar* Data16() const;

  scoped_refptr<WTF::StringImpl> impl_;
};

inline bool operator==(const WebString& a, const char* b) {
  return a.Equals(b);
}

inline bool operator!=(const WebString& a, const char* b) {
  return !(a == b);
}

inline bool operator==(const char* a, const WebString& b) {
  return b == a;
}

inline bool operator!=(const char* a, const WebString& b) {
  return !(b == a);
}

inline bool operator==(const WebString& a, const WebString& b) {
  return a.Equals(b);
}

inline bool operator!=(const WebString& a, const WebString& b) {
  return !(a == b);
}

}  // namespace blink

#endif
