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

static const char kBase64EncMap[64] = {
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B,
    0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56,
    0x57, 0x58, 0x59, 0x5A, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72,
    0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x30, 0x31, 0x32,
    0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2B, 0x2F};

namespace {

class Base64EncoderImpl {
 public:
  explicit Base64EncoderImpl(wtf_size_t len);

  wtf_size_t out_length() const { return out_length_; }
  void Encode(base::span<const uint8_t> data, base::span<char> out) const;

 private:
  wtf_size_t in_length_ = 0;
  wtf_size_t out_length_ = 0;
};

Base64EncoderImpl::Base64EncoderImpl(wtf_size_t len) {
  if (!len)
    return;

  // If the input string is pathologically large, just return nothing.
  // Note: Keep this in sync with the "outLength" computation below.
  // Rather than being perfectly precise, this is a bit conservative.
  const unsigned kMaxInputBufferSize = UINT_MAX / 77 * 76 / 4 * 3 - 2;
  if (len > kMaxInputBufferSize)
    return;

  in_length_ = len;
  out_length_ = ((len + 2) / 3) * 4;
}

void Base64EncoderImpl::Encode(base::span<const uint8_t> data,
                               base::span<char> out) const {
  DCHECK_EQ(in_length_, data.size());
  DCHECK_EQ(out_length_, out.size());
  DCHECK_NE(0u, out.size());

  auto len = data.size();
  unsigned sidx = 0;
  unsigned didx = 0;

  // 3-byte to 4-byte conversion + 0-63 to ascii printable conversion
  if (len > 1) {
    while (sidx < len - 2) {
      out[didx++] = kBase64EncMap[(data[sidx] >> 2) & 077];
      out[didx++] = kBase64EncMap[((data[sidx + 1] >> 4) & 017) |
                                  ((data[sidx] << 4) & 077)];
      out[didx++] = kBase64EncMap[((data[sidx + 2] >> 6) & 003) |
                                  ((data[sidx + 1] << 2) & 077)];
      out[didx++] = kBase64EncMap[data[sidx + 2] & 077];
      sidx += 3;
    }
  }

  if (sidx < len) {
    out[didx++] = kBase64EncMap[(data[sidx] >> 2) & 077];
    if (sidx < len - 1) {
      out[didx++] = kBase64EncMap[((data[sidx + 1] >> 4) & 017) |
                                  ((data[sidx] << 4) & 077)];
      out[didx++] = kBase64EncMap[(data[sidx + 1] << 2) & 077];
    } else {
      out[didx++] = kBase64EncMap[(data[sidx] << 4) & 077];
    }
  }

  // Add padding
  while (didx < out.size()) {
    out[didx] = '=';
    ++didx;
  }
}

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
  Base64EncoderImpl encoder(data.size());
  auto size = encoder.out_length();
  if (size == 0)
    return String();

  StringBuffer<LChar> result(size);
  base::span<char> result_span(reinterpret_cast<char*>(result.Characters()),
                               result.length());
  encoder.Encode(data, result_span);
  return result.Release();
}

void Base64Encode(base::span<const uint8_t> data, Vector<char>& out) {
  Base64EncoderImpl encoder(data.size());
  auto size = encoder.out_length();
  if (size == 0) {
    out.clear();
    return;
  }

  out.resize(size);
  encoder.Encode(data, out);
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

String Base64URLEncode(const char* data, unsigned length) {
  return Base64Encode(base::as_bytes(base::make_span(data, length)))
      .Replace('+', '-')
      .Replace('/', '_');
}

String NormalizeToBase64(const String& encoding) {
  return String(encoding).Replace('-', '+').Replace('_', '/');
}

}  // namespace WTF
