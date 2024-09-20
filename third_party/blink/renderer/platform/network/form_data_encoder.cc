/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
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
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/network/form_data_encoder.h"

#include <limits>
#include "base/rand_util.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

// Helper functions
static inline void Append(Vector<char>& buffer, char string) {
  buffer.push_back(string);
}

static inline void Append(Vector<char>& buffer, const char* string) {
  buffer.Append(string, static_cast<wtf_size_t>(strlen(string)));
}

static inline void Append(Vector<char>& buffer, const std::string& string) {
  buffer.AppendSpan(base::span(string));
}

static inline void AppendPercentEncoded(Vector<char>& buffer, unsigned char c) {
  const char kHexChars[] = "0123456789ABCDEF";
  const char tmp[] = {'%', kHexChars[c / 16], kHexChars[c % 16]};
  buffer.Append(tmp, sizeof(tmp));
}

static void AppendQuotedString(Vector<char>& buffer,
                               const std::string& string,
                               FormDataEncoder::Mode mode) {
  // Append a string as a quoted value, escaping quotes and line breaks.
  size_t length = string.length();
  for (size_t i = 0; i < length; ++i) {
    char c = string.data()[i];

    switch (c) {
      case 0x0a:
        if (mode == FormDataEncoder::kNormalizeCRLF) {
          Append(buffer, "%0D%0A");
        } else {
          Append(buffer, "%0A");
        }
        break;
      case 0x0d:
        if (mode == FormDataEncoder::kNormalizeCRLF) {
          Append(buffer, "%0D%0A");
          if (i + 1 < length && string.data()[i + 1] == 0x0a) {
            ++i;
          }
        } else {
          Append(buffer, "%0D");
        }
        break;
      case '"':
        Append(buffer, "%22");
        break;
      default:
        Append(buffer, c);
    }
  }
}

namespace {

inline void AppendNormalized(Vector<char>& buffer, const std::string& string) {
  size_t length = string.length();
  for (size_t i = 0; i < length; ++i) {
    char c = string.data()[i];
    if (c == '\n' ||
        (c == '\r' && (i + 1 >= length || string.data()[i + 1] != '\n'))) {
      Append(buffer, "\r\n");
    } else if (c != '\r') {
      Append(buffer, c);
    }
  }
}

}  // namespace

WTF::TextEncoding FormDataEncoder::EncodingFromAcceptCharset(
    const String& accept_charset,
    const WTF::TextEncoding& fallback_encoding) {
  DCHECK(fallback_encoding.IsValid());

  String normalized_accept_charset = accept_charset;
  normalized_accept_charset.Replace(',', ' ');

  Vector<String> charsets;
  normalized_accept_charset.Split(' ', charsets);

  for (const String& name : charsets) {
    WTF::TextEncoding encoding(name);
    if (encoding.IsValid())
      return encoding;
  }

  return fallback_encoding;
}

Vector<char> FormDataEncoder::GenerateUniqueBoundaryString() {
  Vector<char> boundary;

  // TODO(rsleevi): crbug.com/575779: Follow the spec or fix the spec.
  // The RFC 2046 spec says the alphanumeric characters plus the
  // following characters are legal for boundaries:  '()+_,-./:=?
  // However the following characters, though legal, cause some sites
  // to fail: (),./:=+
  //
  // Note that our algorithm makes it twice as much likely for 'A' or 'B'
  // to appear in the boundary string, because 0x41 and 0x42 are present in
  // the below array twice.
  static const char kAlphaNumericEncodingMap[64] = {
      0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B,
      0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56,
      0x57, 0x58, 0x59, 0x5A, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
      0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72,
      0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x30, 0x31, 0x32,
      0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x41, 0x42};

  // Start with an informative prefix.
  Append(boundary, "----WebKitFormBoundary");

  // Append 16 random 7bit ascii AlphaNumeric characters.
  char random_bytes[16];
  base::RandBytes(base::as_writable_byte_span(random_bytes));
  for (char& c : random_bytes)
    c = kAlphaNumericEncodingMap[c & 0x3F];
  boundary.AppendSpan(base::span(random_bytes));

  boundary.push_back(
      0);  // Add a 0 at the end so we can use this as a C-style string.
  return boundary;
}

void FormDataEncoder::BeginMultiPartHeader(Vector<char>& buffer,
                                           const std::string& boundary,
                                           const std::string& name) {
  AddBoundaryToMultiPartHeader(buffer, boundary);

  // FIXME: This loses data irreversibly if the input name includes characters
  // you can't encode in the website's character set.
  Append(buffer, "Content-Disposition: form-data; name=\"");
  AppendQuotedString(buffer, name, kNormalizeCRLF);
  Append(buffer, '"');
}

void FormDataEncoder::AddBoundaryToMultiPartHeader(Vector<char>& buffer,
                                                   const std::string& boundary,
                                                   bool is_last_boundary) {
  Append(buffer, "--");
  Append(buffer, boundary);

  if (is_last_boundary)
    Append(buffer, "--");

  Append(buffer, "\r\n");
}

void FormDataEncoder::AddFilenameToMultiPartHeader(
    Vector<char>& buffer,
    const WTF::TextEncoding& encoding,
    const String& filename) {
  // Characters that cannot be encoded using the form's encoding will
  // be escaped using numeric character references, e.g. &#128514; for
  // 😂.
  //
  // This behavior is intended to match existing Firefox and Edge
  // behavior.
  //
  // This aspect of multipart file upload (how to replace filename
  // characters not representable in the form charset) is not
  // currently specified in HTML, though it may be a good candidate
  // for future standardization. An HTML issue tracker entry has
  // been added for this: https://github.com/whatwg/html/issues/3223
  //
  // This behavior also exactly matches the already-standardized
  // replacement behavior from HTML for entity names and values in
  // multipart form data. The HTML standard specifically overrides RFC
  // 7578 in this case and leaves the actual substitution mechanism
  // implementation-defined.
  //
  // See also:
  //
  // https://html.spec.whatwg.org/C/#multipart-form-data
  // https://www.chromestatus.com/feature/5634575908732928
  // https://crbug.com/661819
  // https://encoding.spec.whatwg.org/#concept-encoding-process
  // https://tools.ietf.org/html/rfc7578#section-4.2
  // https://tools.ietf.org/html/rfc5987#section-3.2
  Append(buffer, "; filename=\"");
  AppendQuotedString(buffer,
                     encoding.Encode(filename, WTF::kEntitiesForUnencodables),
                     kDoNotNormalizeCRLF);
  Append(buffer, '"');
}

void FormDataEncoder::AddContentTypeToMultiPartHeader(Vector<char>& buffer,
                                                      const String& mime_type) {
  Append(buffer, "\r\nContent-Type: ");
  Append(buffer, mime_type.Utf8());
}

void FormDataEncoder::FinishMultiPartHeader(Vector<char>& buffer) {
  Append(buffer, "\r\n\r\n");
}

void FormDataEncoder::AddKeyValuePairAsFormData(
    Vector<char>& buffer,
    const std::string& key,
    const std::string& value,
    EncodedFormData::EncodingType encoding_type,
    Mode mode) {
  if (encoding_type == EncodedFormData::kTextPlain) {
    DCHECK_EQ(mode, kNormalizeCRLF);
    AppendNormalized(buffer, key);
    Append(buffer, '=');
    AppendNormalized(buffer, value);
    Append(buffer, "\r\n");
  } else {
    if (!buffer.empty())
      Append(buffer, '&');
    EncodeStringAsFormData(buffer, key, mode);
    Append(buffer, '=');
    EncodeStringAsFormData(buffer, value, mode);
  }
}

void FormDataEncoder::EncodeStringAsFormData(Vector<char>& buffer,
                                             const std::string& string,
                                             Mode mode) {
  // Same safe characters as Netscape for compatibility.
  static const char kSafeCharacters[] = "-._*";

  // http://www.w3.org/TR/html4/interact/forms.html#h-17.13.4.1
  size_t length = string.length();
  for (size_t i = 0; i < length; ++i) {
    unsigned char c = string.data()[i];

    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || (c != '\0' && strchr(kSafeCharacters, c))) {
      Append(buffer, c);
    } else if (c == ' ') {
      Append(buffer, '+');
    } else {
      if (mode == kNormalizeCRLF) {
        if (c == '\n' ||
            (c == '\r' && (i + 1 >= length || string.data()[i + 1] != '\n'))) {
          Append(buffer, "%0D%0A");
        } else if (c != '\r') {
          AppendPercentEncoded(buffer, c);
        }
      } else {
        AppendPercentEncoded(buffer, c);
      }
    }
  }
}

}  // namespace blink
