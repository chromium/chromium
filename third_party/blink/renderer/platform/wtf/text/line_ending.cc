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

#if BUILDFLAG(IS_WIN)
void InternalNormalizeLineEndingsToCRLF(const std::string& from,
                                        Vector<char>& buffer) {
  size_t new_len = RequiredSizeForCRLF(base::span(from));
  if (new_len < from.length())
    return;

  if (new_len == from.length()) {
    buffer.AppendSpan(base::span(from));
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
  // Compute the new length.
  wtf_size_t new_len = 0, index = 0;
  bool need_fix = false;
  char from_ending_char = '\r';
  char to_ending_char = '\n';
  while (index < from.length()) {
    char c = from[index++];
    if (c == '\r' && from[index] == '\n') {
      // Turn CRLF into CR or LF.
      index++;
      need_fix = true;
    } else if (c == from_ending_char) {
      // Turn CR/LF into LF/CR.
      need_fix = true;
    }
    new_len += 1;
  }

  // If no need to fix the string, just copy the string over.
  if (!need_fix) {
    result.AppendSpan(base::span(from));
    return;
  }

  index = 0;
  wtf_size_t old_result_size = result.size();
  wtf_size_t index_out = old_result_size;
  result.Grow(old_result_size + new_len);

  // Make a copy of the string.
  while (index < from.length()) {
    char c = from[index++];
    if (c == '\r' && from[index] == '\n') {
      // Turn CRLF or CR into CR or LF.
      index++;
      c = to_ending_char;
    } else if (c == from_ending_char) {
      // Turn CR/LF into LF/CR.
      c = to_ending_char;
    }
    result[index_out++] = c;
  }
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
