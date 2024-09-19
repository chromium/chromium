/*
   Copyright (C) 2000-2001 Dawit Alemayehu <adawit@kde.org>
   Copyright (C) 2006 Alexey Proskuryakov <ap@webkit.org>
   Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
   Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License (LGPL)
   version 2 as published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
   USA.

   This code is based on the java implementation in HTTPClient
   package by Ronald Tschalaer Copyright (C) 1996-1999.
*/

#include "third_party/blink/renderer/platform/wtf/text/base64.h"

#include <limits.h>

#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/modp_b64/modp_b64.h"

namespace WTF {

namespace {

// https://infra.spec.whatwg.org/#ascii-whitespace
// Matches the definition of IsHTMLSpace in html_parser_idioms.h.
template <typename CharType>
bool IsAsciiWhitespace(CharType character) {
  return character <= ' ' &&
         (character == ' ' || character == '\n' || character == '\t' ||
          character == '\r' || character == '\f');
}

ModpDecodePolicy GetModpPolicy(Base64DecodePolicy policy) {
  switch (policy) {
    case Base64DecodePolicy::kForgiving:
      return ModpDecodePolicy::kForgiving;
    case Base64DecodePolicy::kNoPaddingValidation:
      return ModpDecodePolicy::kNoPaddingValidation;
  }
}

// Invokes modp_b64 without stripping whitespace.
bool Base64DecodeRaw(const StringView& in,
                     Vector<char>& out,
                     Base64DecodePolicy policy) {
  // Using StringUTF8Adaptor means we avoid allocations if the string is 8-bit
  // ascii, which is likely given that base64 is required to be ascii.
  StringUTF8Adaptor adaptor(in);
  out.resize(modp_b64_decode_len(adaptor.size()));
  size_t output_size = modp_b64_decode(out.data(), adaptor.data(), adaptor.size(),
                                       GetModpPolicy(policy));
  if (output_size == MODP_B64_ERROR)
    return false;

  out.resize(output_size);
  return true;
}

}  // namespace

String Base64Encode(base::span<const uint8_t> data) {
  size_t encode_len = modp_b64_encode_data_len(data.size());
  CHECK_LE(data.size(), MODP_B64_MAX_INPUT_LEN);
  StringBuffer<LChar> result(encode_len);
  if (encode_len == 0)
    return String();
  const size_t output_size = modp_b64_encode_data(
      reinterpret_cast<char*>(result.Characters()),
      reinterpret_cast<const char*>(data.data()), data.size());
  DCHECK_EQ(output_size, encode_len);
  return result.Release();
}

void Base64Encode(base::span<const uint8_t> data, Vector<char>& out) {
  size_t encode_len = modp_b64_encode_data_len(data.size());
  CHECK_LE(data.size(), MODP_B64_MAX_INPUT_LEN);
  if (encode_len == 0) {
    out.clear();
    return;
  }
  out.resize(encode_len);
  const size_t output_size = modp_b64_encode_data(
      out.data(), reinterpret_cast<const char*>(data.data()), data.size());
  DCHECK_EQ(output_size, encode_len);
}

bool Base64Decode(const StringView& in,
                  Vector<char>& out,
                  Base64DecodePolicy policy) {
  switch (policy) {
    case Base64DecodePolicy::kForgiving: {
      // https://infra.spec.whatwg.org/#forgiving-base64-decode
      // Step 1 is to remove all whitespace. However, checking for whitespace
      // slows down the "happy" path. Since any whitespace will fail normal
      // decoding from modp_b64_decode, just try again if we detect a failure.
      // This shouldn't be much slower for whitespace inputs.
      //
      // TODO(csharrison): Most callers use String inputs so ToString() should
      // be fast. Still, we should add a RemoveCharacters method to StringView
      // to avoid a double allocation for non-String-backed StringViews.
      return Base64DecodeRaw(in, out, policy) ||
             Base64DecodeRaw(in.ToString().RemoveCharacters(&IsAsciiWhitespace),
                             out, policy);
    }
    case Base64DecodePolicy::kNoPaddingValidation: {
      return Base64DecodeRaw(in, out, policy);
    }
  }
}

bool Base64UnpaddedURLDecode(const String& in, Vector<char>& out) {
  if (in.Contains('+') || in.Contains('/') || in.Contains('='))
    return false;

  return Base64Decode(NormalizeToBase64(in), out);
}

String Base64URLEncode(base::span<const uint8_t> data) {
  return Base64Encode(data).Replace('+', '-').Replace('/', '_');
}

String NormalizeToBase64(const String& encoding) {
  return String(encoding).Replace('-', '+').Replace('_', '/');
}

}  // namespace WTF
