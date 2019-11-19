/*
 * Copyright (C) 2004, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov <ap@nypop.com>
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

#include <memory>
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace WTF {

TextEncoding::TextEncoding(const char* name)
    : name_(AtomicCanonicalTextEncodingName(name)) {
}

TextEncoding::TextEncoding(const String& name)
    : name_(AtomicCanonicalTextEncodingName(name)) {
}

String TextEncoding::Decode(const char* data,
                            wtf_size_t length,
                            bool stop_on_error,
                            bool& saw_error) const {
  if (!name_)
    return String();

  return NewTextCodec(*this)->Decode(data, length, FlushBehavior::kDataEOF,
                                     stop_on_error, saw_error);
}

std::string TextEncoding::Encode(const String& string,
                                 UnencodableHandling handling) const {
  if (!name_)
    return std::string();

  if (string.IsEmpty())
    return std::string();

  std::unique_ptr<TextCodec> text_codec = NewTextCodec(*this);
  std::string encoded_string;
  if (string.Is8Bit())
    encoded_string =
        text_codec->Encode(string.Characters8(), string.length(), handling);
  else
    encoded_string =
        text_codec->Encode(string.Characters16(), string.length(), handling);
  return encoded_string;
}

bool TextEncoding::UsesVisualOrdering() const {
  if (NoExtendedTextEncodingNameUsed())
    return false;

  static const char* const kA = AtomicCanonicalTextEncodingName("ISO-8859-8");
  return name_ == kA;
}

bool TextEncoding::IsNonByteBasedEncoding() const {
  if (NoExtendedTextEncodingNameUsed()) {
    return *this == UTF16LittleEndianEncoding() ||
           *this == UTF16BigEndianEncoding();
  }

  return *this == UTF16LittleEndianEncoding() ||
         *this == UTF16BigEndianEncoding();
}

const TextEncoding& TextEncoding::ClosestByteBasedEquivalent() const {
  if (IsNonByteBasedEncoding())
    return UTF8Encoding();
  return *this;
}

// HTML5 specifies that UTF-8 be used in form submission when a form is is a
// part of a document in UTF-16 probably because UTF-16 is not a byte-based
// encoding and can contain 0x00.
const TextEncoding& TextEncoding::EncodingForFormSubmission() const {
  if (IsNonByteBasedEncoding())
    return UTF8Encoding();
  return *this;
}

const TextEncoding& ASCIIEncoding() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const TextEncoding, global_ascii_encoding,
                                  ("ASCII"));
  return global_ascii_encoding;
}

const TextEncoding& Latin1Encoding() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const TextEncoding, global_latin1_encoding,
                                  ("latin1"));
  return global_latin1_encoding;
}

const TextEncoding& UTF16BigEndianEncoding() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      const TextEncoding, global_utf16_big_endian_encoding, ("UTF-16BE"));
  return global_utf16_big_endian_encoding;
}

const TextEncoding& UTF16LittleEndianEncoding() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      const TextEncoding, global_utf16_little_endian_encoding, ("UTF-16LE"));
  return global_utf16_little_endian_encoding;
}

const TextEncoding& UTF8Encoding() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const TextEncoding, global_utf8_encoding,
                                  ("UTF-8"));
  DCHECK(global_utf8_encoding.IsValid());
  return global_utf8_encoding;
}

const TextEncoding& WindowsLatin1Encoding() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      const TextEncoding, global_windows_latin1_encoding, ("WinLatin1"));
  return global_windows_latin1_encoding;
}

const TextEncoding& UnknownEncoding() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const TextEncoding, global_unknown_encoding,
                                  ("Unknown"));
  return global_unknown_encoding;
}

}  // namespace WTF
