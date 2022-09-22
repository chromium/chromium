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

namespace WTF {

static const char kBase64EncMap[64] = {
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B,
    0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56,
    0x57, 0x58, 0x59, 0x5A, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72,
    0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x30, 0x31, 0x32,
    0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2B, 0x2F};

static const char kBase64DecMap[128] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x3F,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12,
    0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24,
    0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
    0x31, 0x32, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00};

namespace {

class Base64EncoderImpl {
 public:
  Base64EncoderImpl(wtf_size_t len, Base64EncodePolicy policy);

  wtf_size_t out_length() const { return out_length_; }
  void Encode(base::span<const uint8_t> data, base::span<char> out) const;

 private:
  wtf_size_t in_length_ = 0;
  wtf_size_t out_length_ = 0;
  bool insert_lfs_ = false;
};

Base64EncoderImpl::Base64EncoderImpl(wtf_size_t len,
                                     Base64EncodePolicy policy) {
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

  // Deal with the 76 character per line limit specified in RFC 2045.
  insert_lfs_ = (policy == kBase64InsertLFs && out_length_ > 76);
  if (insert_lfs_)
    out_length_ += ((out_length_ - 1) / 76);
}

void Base64EncoderImpl::Encode(base::span<const uint8_t> data,
                               base::span<char> out) const {
  DCHECK_EQ(in_length_, data.size());
  DCHECK_EQ(out_length_, out.size());
  DCHECK_NE(0u, out.size());

  auto len = data.size();
  unsigned sidx = 0;
  unsigned didx = 0;

  int count = 0;

  // 3-byte to 4-byte conversion + 0-63 to ascii printable conversion
  if (len > 1) {
    while (sidx < len - 2) {
      if (insert_lfs_) {
        if (count && !(count % 76))
          out[didx++] = '\n';
        count += 4;
      }
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
    if (insert_lfs_ && (count > 0) && !(count % 76))
      out[didx++] = '\n';

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

}  // namespace

String Base64Encode(base::span<const uint8_t> data, Base64EncodePolicy policy) {
  Base64EncoderImpl encoder(data.size(), policy);
  auto size = encoder.out_length();
  if (size == 0)
    return String();

  StringBuffer<LChar> result(size);
  base::span<char> result_span(reinterpret_cast<char*>(result.Characters()),
                               result.length());
  encoder.Encode(data, result_span);
  return result.Release();
}

void Base64Encode(base::span<const uint8_t> data,
                  Vector<char>& out,
                  Base64EncodePolicy policy) {
  Base64EncoderImpl encoder(data.size(), policy);
  auto size = encoder.out_length();
  if (size == 0) {
    out.clear();
    return;
  }

  out.resize(size);
  encoder.Encode(data, out);
}

bool Base64Decode(const Vector<char>& in,
                  Vector<char>& out,
                  CharacterMatchFunctionPtr should_ignore_character,
                  Base64DecodePolicy policy) {
  out.clear();

  // If the input string is pathologically large, just return nothing.
  if (in.size() > UINT_MAX)
    return false;

  return Base64Decode(in.data(), in.size(), out, should_ignore_character,
                      policy);
}

template <typename T>
static inline bool Base64DecodeInternal(
    const T* data,
    unsigned length,
    Vector<char>& out,
    CharacterMatchFunctionPtr should_ignore_character,
    Base64DecodePolicy policy) {
  out.clear();
  if (!length)
    return true;

  out.Grow(length);

  unsigned equals_sign_count = 0;
  unsigned out_length = 0;
  bool had_error = false;
  for (unsigned idx = 0; idx < length; ++idx) {
    UChar ch = data[idx];
    if (ch == '=') {
      ++equals_sign_count;
      // There should never be more than 2 padding characters.
      if (policy == kBase64ValidatePadding && equals_sign_count > 2) {
        had_error = true;
        break;
      }
    } else if (('0' <= ch && ch <= '9') || ('A' <= ch && ch <= 'Z') ||
               ('a' <= ch && ch <= 'z') || ch == '+' || ch == '/') {
      if (equals_sign_count) {
        had_error = true;
        break;
      }
      out[out_length++] = kBase64DecMap[ch];
    } else if (!should_ignore_character || !should_ignore_character(ch)) {
      had_error = true;
      break;
    }
  }

  if (out_length < out.size())
    out.Shrink(out_length);

  if (had_error)
    return false;

  if (!out_length)
    return !equals_sign_count;

  // There should be no padding if length is a multiple of 4.
  // We use (outLength + equalsSignCount) instead of length because we don't
  // want to account for ignored characters.
  if (policy == kBase64ValidatePadding && equals_sign_count &&
      (out_length + equals_sign_count) % 4)
    return false;

  // Valid data is (n * 4 + [0,2,3]) characters long.
  if ((out_length % 4) == 1)
    return false;

  // 4-byte to 3-byte conversion
  out_length -= (out_length + 3) / 4;
  if (!out_length)
    return false;

  unsigned sidx = 0;
  unsigned didx = 0;
  if (out_length > 1) {
    while (didx < out_length - 2) {
      out[didx] = (((out[sidx] << 2) & 255) | ((out[sidx + 1] >> 4) & 003));
      out[didx + 1] =
          (((out[sidx + 1] << 4) & 255) | ((out[sidx + 2] >> 2) & 017));
      out[didx + 2] = (((out[sidx + 2] << 6) & 255) | (out[sidx + 3] & 077));
      sidx += 4;
      didx += 3;
    }
  }

  if (didx < out_length)
    out[didx] = (((out[sidx] << 2) & 255) | ((out[sidx + 1] >> 4) & 003));

  if (++didx < out_length)
    out[didx] = (((out[sidx + 1] << 4) & 255) | ((out[sidx + 2] >> 2) & 017));

  if (out_length < out.size())
    out.Shrink(out_length);

  return true;
}

bool Base64Decode(const char* data,
                  unsigned length,
                  Vector<char>& out,
                  CharacterMatchFunctionPtr should_ignore_character,
                  Base64DecodePolicy policy) {
  return Base64DecodeInternal<LChar>(reinterpret_cast<const LChar*>(data),
                                     length, out, should_ignore_character,
                                     policy);
}

bool Base64Decode(const UChar* data,
                  unsigned length,
                  Vector<char>& out,
                  CharacterMatchFunctionPtr should_ignore_character,
                  Base64DecodePolicy policy) {
  return Base64DecodeInternal<UChar>(data, length, out, should_ignore_character,
                                     policy);
}

bool Base64Decode(const String& in,
                  Vector<char>& out,
                  CharacterMatchFunctionPtr should_ignore_character,
                  Base64DecodePolicy policy) {
  if (in.IsEmpty())
    return Base64DecodeInternal<LChar>(nullptr, 0, out, should_ignore_character,
                                       policy);
  if (in.Is8Bit())
    return Base64DecodeInternal<LChar>(in.Characters8(), in.length(), out,
                                       should_ignore_character, policy);
  return Base64DecodeInternal<UChar>(in.Characters16(), in.length(), out,
                                     should_ignore_character, policy);
}

bool Base64UnpaddedURLDecode(const String& in,
                             Vector<char>& out,
                             CharacterMatchFunctionPtr should_ignore_character,
                             Base64DecodePolicy policy) {
  if (in.Contains('+') || in.Contains('/') || in.Contains('='))
    return false;

  return Base64Decode(NormalizeToBase64(in), out, should_ignore_character,
                      policy);
}

String Base64URLEncode(const char* data,
                       unsigned length,
                       Base64EncodePolicy policy) {
  return Base64Encode(base::as_bytes(base::make_span(data, length)), policy)
      .Replace('+', '-')
      .Replace('/', '_');
}

String NormalizeToBase64(const String& encoding) {
  return String(encoding).Replace('-', '+').Replace('_', '/');
}

}  // namespace WTF
