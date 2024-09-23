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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/wtf/text/text_codec_utf16.h"

#include <unicode/utf16.h>
#include <memory>

#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

void TextCodecUTF16::RegisterEncodingNames(EncodingNameRegistrar registrar) {
  registrar("UTF-16LE", "UTF-16LE");
  registrar("UTF-16BE", "UTF-16BE");

  registrar("ISO-10646-UCS-2", "UTF-16LE");
  registrar("UCS-2", "UTF-16LE");
  registrar("UTF-16", "UTF-16LE");
  registrar("Unicode", "UTF-16LE");
  registrar("csUnicode", "UTF-16LE");
  registrar("unicodeFEFF", "UTF-16LE");

  registrar("unicodeFFFE", "UTF-16BE");
}

static std::unique_ptr<TextCodec> NewStreamingTextDecoderUTF16LE(
    const TextEncoding&,
    const void*) {
  return std::make_unique<TextCodecUTF16>(true);
}

static std::unique_ptr<TextCodec> NewStreamingTextDecoderUTF16BE(
    const TextEncoding&,
    const void*) {
  return std::make_unique<TextCodecUTF16>(false);
}

void TextCodecUTF16::RegisterCodecs(TextCodecRegistrar registrar) {
  registrar("UTF-16LE", NewStreamingTextDecoderUTF16LE, nullptr);
  registrar("UTF-16BE", NewStreamingTextDecoderUTF16BE, nullptr);
}

String TextCodecUTF16::Decode(base::span<const uint8_t> bytes,
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
      return String(&kReplacementCharacter, 1u);
    }
    return String();
  }

  const uint8_t* p = bytes.data();
  const wtf_size_t num_bytes = bytes.size() + have_lead_byte_;
  const bool will_have_extra_byte = num_bytes & 1;
  const wtf_size_t num_chars_in = num_bytes / 2;
  const wtf_size_t max_chars_out =
      num_chars_in + (have_lead_surrogate_ ? 1 : 0) +
      (really_flush && will_have_extra_byte ? 1 : 0);

  StringBuffer<UChar> buffer(max_chars_out);
  UChar* q = buffer.Characters();

  for (wtf_size_t i = 0; i < num_chars_in; ++i) {
    UChar c;
    if (have_lead_byte_) {
      c = little_endian_ ? (lead_byte_ | (p[0] << 8))
                         : ((lead_byte_ << 8) | p[0]);
      have_lead_byte_ = false;
      ++p;
    } else {
      c = little_endian_ ? (p[0] | (p[1] << 8)) : ((p[0] << 8) | p[1]);
      p += 2;
    }

    // TODO(jsbell): If necessary for performance, m_haveLeadByte handling
    // can be pulled out and this loop split into distinct cases for
    // big/little endian. The logic from here to the end of the loop is
    // constant with respect to m_haveLeadByte and m_littleEndian.

    if (have_lead_surrogate_ && U_IS_TRAIL(c)) {
      *q++ = lead_surrogate_;
      have_lead_surrogate_ = false;
      *q++ = c;
    } else {
      if (have_lead_surrogate_) {
        have_lead_surrogate_ = false;
        saw_error = true;
        *q++ = kReplacementCharacter;
      }

      if (U_IS_LEAD(c)) {
        have_lead_surrogate_ = true;
        lead_surrogate_ = c;
      } else if (U_IS_TRAIL(c)) {
        saw_error = true;
        *q++ = kReplacementCharacter;
      } else {
        *q++ = c;
      }
    }
  }

  DCHECK(!have_lead_byte_);
  if (will_have_extra_byte) {
    have_lead_byte_ = true;
    lead_byte_ = p[0];
  }

  if (really_flush && (have_lead_byte_ || have_lead_surrogate_)) {
    have_lead_byte_ = have_lead_surrogate_ = false;
    saw_error = true;
    *q++ = kReplacementCharacter;
  }

  buffer.Shrink(static_cast<wtf_size_t>(q - buffer.Characters()));

  return String::Adopt(buffer);
}

std::string TextCodecUTF16::Encode(base::span<const UChar> characters,
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

std::string TextCodecUTF16::Encode(base::span<const LChar> characters,
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

}  // namespace WTF
