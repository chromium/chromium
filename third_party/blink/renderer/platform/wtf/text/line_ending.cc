/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/wtf/text/line_ending.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {
namespace {

template <typename CharType>
wtf_size_t RequiredSizeForCRLF(base::span<const CharType> data) {
  wtf_size_t new_len = 0;
  wtf_size_t index = 0;
  while (index < data.size()) {
    CharType c = data[index++];
    if (c == '\r') {
      if (index >= data.size() || data[index] != '\n') {
        // Turn CR into CRLF.
        new_len += 2;
      } else {
        // We already have \r\n. We don't count this \r, and the
        // following \n will count 2.
      }
    } else if (c == '\n') {
      // Turn LF into CRLF.
      new_len += 2;
    } else {
      // Leave other characters alone.
      new_len += 1;
    }
  }
  return new_len;
}

template <typename CharType>
void NormalizeToCRLF(base::span<const CharType> src, base::span<CharType> dst) {
  wtf_size_t src_length = src.size();
  wtf_size_t index = 0, index_out = 0;

  while (index < src_length) {
    CharType c = src[index++];
    if (c == '\r') {
      if (index >= src_length || src[index] != '\n') {
        // Turn CR into CRLF.
        dst[index_out++] = '\r';
        dst[index_out++] = '\n';
      }
    } else if (c == '\n') {
      // Turn LF into CRLF.
      dst[index_out++] = '\r';
      dst[index_out++] = '\n';
    } else {
      // Leave other characters alone.
      dst[index_out++] = c;
    }
  }
}

template <typename CharType>
std::optional<wtf_size_t> RequiredSizeForLF(base::span<const CharType> src) {
  bool need_change = false;
  wtf_size_t new_len = 0;
  wtf_size_t index = 0;
  while (index < src.size()) {
    const CharType c = src[index++];
    // Replace CR or CRLF.
    if (c == '\r') {
      // If CR is followed by LF, skip the LF as well, and thus only count one
      // character in the output.
      if (index < src.size() && src[index] == '\n') {
        ++index;
      }
      need_change = true;
    }
    ++new_len;
  }
  return need_change ? std::make_optional(new_len) : std::nullopt;
}

template <typename CharType>
void NormalizeToLF(base::span<const CharType> src, base::span<CharType> dst) {
  wtf_size_t index = 0;
  wtf_size_t index_out = 0;
  while (index < src.size()) {
    CharType c = src[index++];
    // Replace CR or CRLF.
    if (c == '\r') {
      // If CR is followed by LF, skip the LF as well.
      if (index < src.size() && src[index] == '\n') {
        ++index;
      }
      c = '\n';
    }
    dst[index_out++] = c;
  }
}

#if BUILDFLAG(IS_WIN)
void InternalNormalizeLineEndingsToCRLF(const std::string& from,
                                        Vector<char>& buffer) {
  size_t new_len = RequiredSizeForCRLF(base::span(from));
  if (new_len < from.length())
    return;

  if (new_len == from.length()) {
    buffer.append_range(from);
    return;
  }

  wtf_size_t old_buffer_size = buffer.size();
  buffer.Grow(old_buffer_size + new_len);
  NormalizeToCRLF(base::span(from),
                  base::span(buffer).subspan(old_buffer_size));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

void NormalizeLineEndingsToLF(const std::string& from, Vector<char>& result) {
  // Compute the new length. Use byte-spans to avoid unnecessary instances.
  std::optional<wtf_size_t> new_len =
      RequiredSizeForLF(base::as_byte_span(from));

  // If no need to fix the string, just copy the string over.
  if (!new_len) {
    result.append_range(from);
    return;
  }

  const wtf_size_t old_result_size = result.size();
  result.Grow(old_result_size + *new_len);
  auto dst = base::as_writable_byte_span(result).subspan(old_result_size);

  // Copy and normalize.
  NormalizeToLF(base::as_byte_span(from), dst);
}

String NormalizeLineEndingsToLF(const String& src) {
  if (src.empty()) {
    return src;
  }
  return VisitCharacters(src, [&src](auto chars) {
    std::optional<wtf_size_t> new_length = RequiredSizeForLF(chars);
    if (!new_length) {
      return src;
    }
    using CharType = decltype(chars)::value_type;
    StringBuffer<CharType> buffer(*new_length);
    NormalizeToLF(chars, buffer.Span());
    return String::Adopt(buffer);
  });
}

String NormalizeLineEndingsToCRLF(const String& src) {
  wtf_size_t length = src.length();
  if (length == 0)
    return src;
  if (src.Is8Bit()) {
    wtf_size_t new_length = RequiredSizeForCRLF(src.Span8());
    if (new_length == length)
      return src;
    StringBuffer<LChar> buffer(new_length);
    NormalizeToCRLF(src.Span8(), buffer.Span());
    return String::Adopt(buffer);
  }
  wtf_size_t new_length = RequiredSizeForCRLF(src.Span16());
  if (new_length == length)
    return src;
  StringBuffer<UChar> buffer(new_length);
  NormalizeToCRLF(src.Span16(), buffer.Span());
  return String::Adopt(buffer);
}

void NormalizeLineEndingsToNative(const std::string& from,
                                  Vector<char>& result) {
#if BUILDFLAG(IS_WIN)
  InternalNormalizeLineEndingsToCRLF(from, result);
#else
  NormalizeLineEndingsToLF(from, result);
#endif
}

}  // namespace blink
