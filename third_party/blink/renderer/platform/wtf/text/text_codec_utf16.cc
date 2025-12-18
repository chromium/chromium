/*
 * Copyright (C) 2004, 2006, 2008, 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/wtf/text/text_codec_utf16.h"

#include <unicode/utf16.h>

#include <memory>

#include "base/numerics/byte_conversions.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"

namespace blink {

bool TextCodecUtf16::IsSupported(StringView canonical_name) {
  return EqualIgnoringASCIICase(canonical_name, "UTF-16LE") ||
         EqualIgnoringASCIICase(canonical_name, "UTF-16BE");
}

void TextCodecUtf16::RegisterEncodingNames(EncodingNameRegistrar registrar) {
  AtomicString utf_16le("UTF-16LE");
  AtomicString utf_16be("UTF-16BE");

  registrar("UTF-16LE", utf_16le);
  registrar("UTF-16BE", utf_16be);

  registrar("ISO-10646-UCS-2", utf_16le);
  registrar("UCS-2", utf_16le);
  registrar("UTF-16", utf_16le);
  registrar("Unicode", utf_16le);
  registrar("csUnicode", utf_16le);
  registrar("unicodeFEFF", utf_16le);

  registrar("unicodeFFFE", utf_16be);
}

static std::unique_ptr<TextCodec> NewStreamingTextDecoderUtf16le(
    const TextEncoding&) {
  return std::make_unique<TextCodecUtf16>(true);
}

static std::unique_ptr<TextCodec> NewStreamingTextDecoderUtf16be(
    const TextEncoding&) {
  return std::make_unique<TextCodecUtf16>(false);
}

void TextCodecUtf16::RegisterCodecs(TextCodecRegistrar registrar) {
  registrar("UTF-16LE", NewStreamingTextDecoderUtf16le);
  registrar("UTF-16BE", NewStreamingTextDecoderUtf16be);
}

String TextCodecUtf16::Decode(base::span<const uint8_t> bytes,
                              FlushBehavior flush,
                              bool,
                              bool& saw_error) {
  // For compatibility reasons, ignore flush from fetch EOF.
  const bool really_flush = flush != FlushBehavior::kDoNotFlush &&
                            flush != FlushBehavior::kFetchEOF;

  if (bytes.empty()) {
    if (really_flush && (have_lead_byte_ || have_lead_surrogate_)) {
      have_lead_byte_ = have_lead_surrogate_ = false;
      saw_error = true;
      return String(base::span_from_ref(uchar::kReplacementCharacter));
    }
    return String();
  }

  const wtf_size_t num_bytes = bytes.size() + have_lead_byte_;
  const bool will_have_extra_byte = num_bytes & 1;
  wtf_size_t num_chars_in = num_bytes / 2;
  const wtf_size_t max_chars_out =
      num_chars_in + (have_lead_surrogate_ ? 1 : 0) +
      (really_flush && will_have_extra_byte ? 1 : 0);

  auto in_span = bytes;
  StringBuffer<UChar> buffer(max_chars_out);
  auto out_span = buffer.Span();
  wtf_size_t out_span_cursor = 0;

  auto decode = [&](UChar c) {
    if (have_lead_surrogate_ && U_IS_TRAIL(c)) {
      out_span[out_span_cursor++] = lead_surrogate_;
      have_lead_surrogate_ = false;
      out_span[out_span_cursor++] = c;
    } else {
      if (have_lead_surrogate_) {
        have_lead_surrogate_ = false;
        saw_error = true;
        out_span[out_span_cursor++] = uchar::kReplacementCharacter;
      }

      if (U_IS_LEAD(c)) {
        have_lead_surrogate_ = true;
        lead_surrogate_ = c;
      } else if (U_IS_TRAIL(c)) {
        saw_error = true;
        out_span[out_span_cursor++] = uchar::kReplacementCharacter;
      } else {
        out_span[out_span_cursor++] = c;
      }
    }
  };

  if (have_lead_byte_) {
    UChar c = lead_byte_ | (in_span.take_first_elem() << 8);

    if (!little_endian_) {
      c = base::ByteSwap(c);
    }

    have_lead_byte_ = false;
    decode(c);
    num_chars_in--;
  }

  for (wtf_size_t i = 0; i < num_chars_in; ++i) {
    UChar c = base::U16FromLittleEndian(in_span.take_first<2ul>());
    if (!little_endian_) {
      c = base::ByteSwap(c);
    }
    decode(c);
  }

  DCHECK(!have_lead_byte_);
  if (will_have_extra_byte) {
    have_lead_byte_ = true;
    lead_byte_ = in_span[0];
  }

  if (really_flush && (have_lead_byte_ || have_lead_surrogate_)) {
    have_lead_byte_ = have_lead_surrogate_ = false;
    saw_error = true;
    out_span[out_span_cursor++] = uchar::kReplacementCharacter;
  }

  buffer.Shrink(static_cast<wtf_size_t>(out_span_cursor));

  return String::Adopt(buffer);
}

std::string TextCodecUtf16::Encode(base::span<const UChar> characters,
                                   UnencodableHandling) {
  // We need to be sure we can double the length without overflowing.
  // Since the passed-in length is the length of an actual existing
  // character buffer, each character is two bytes, and we know
  // the buffer doesn't occupy the entire address space, we can
  // assert here that doubling the length does not overflow wtf_size_t
  // and there's no need for a runtime check.
  DCHECK_LE(characters.size(), std::numeric_limits<wtf_size_t>::max() / 2);

  std::string result(characters.size() * 2, '\0');

  if (little_endian_) {
    for (size_t i = 0; i < characters.size(); ++i) {
      UChar c = characters[i];
      result[i * 2] = static_cast<char>(c);
      result[i * 2 + 1] = c >> 8;
    }
  } else {
    for (size_t i = 0; i < characters.size(); ++i) {
      UChar c = characters[i];
      result[i * 2] = c >> 8;
      result[i * 2 + 1] = static_cast<char>(c);
    }
  }

  return result;
}

std::string TextCodecUtf16::Encode(base::span<const LChar> characters,
                                   UnencodableHandling) {
  // In the LChar case, we do actually need to perform this check in release. :)
  CHECK_LE(characters.size(), std::numeric_limits<wtf_size_t>::max() / 2);

  std::string result(characters.size() * 2, '\0');

  if (little_endian_) {
    for (size_t i = 0; i < characters.size(); ++i) {
      result[i * 2] = characters[i];
      result[i * 2 + 1] = 0;
    }
  } else {
    for (size_t i = 0; i < characters.size(); ++i) {
      result[i * 2] = 0;
      result[i * 2 + 1] = characters[i];
    }
  }

  return result;
}

}  // namespace blink
