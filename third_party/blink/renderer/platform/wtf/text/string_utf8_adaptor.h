/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_UTF8_ADAPTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_UTF8_ADAPTOR_H_

#include "base/strings/string_piece.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/cstring.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

// This class lets you get UTF-8 data out of a String without mallocing a
// separate buffer to hold the data if the String happens to be 8 bit and
// contain only ASCII characters.
class StringUTF8Adaptor final {
  DISALLOW_NEW();

 public:
  StringUTF8Adaptor(const String& string,
                    UTF8ConversionMode mode = kLenientUTF8Conversion)
      : data_(nullptr), length_(0) {
    if (string.IsEmpty())
      return;
    // Unfortunately, 8 bit WTFStrings are encoded in Latin-1 and GURL uses
    // UTF-8 when processing 8 bit strings. If |relative| is entirely ASCII, we
    // luck out and can avoid mallocing a new buffer to hold the UTF-8 data
    // because UTF-8 and Latin-1 use the same code units for ASCII code points.
    if (string.Is8Bit() && string.ContainsOnlyASCIIOrEmpty()) {
      data_ = reinterpret_cast<const char*>(string.Characters8());
      length_ = string.length();
    } else {
      utf8_buffer_ = string.Utf8(mode);
      data_ = utf8_buffer_.data();
      length_ = utf8_buffer_.length();
    }
  }

  const char* Data() const { return data_; }
  wtf_size_t length() const { return length_; }

  base::StringPiece AsStringPiece() const {
    return base::StringPiece(data_, length_);
  }

 private:
  CString utf8_buffer_;
  const char* data_;
  wtf_size_t length_;
};

}  // namespace WTF

using WTF::StringUTF8Adaptor;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_UTF8_ADAPTOR_H_
