/*
 * Copyright (C) 2004, 2006, 2008, 2011 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/wtf/text/text_codec_utf8.h"

#include <memory>
#include "base/memory/ptr_util.h"
#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/text_codec_ascii_fast_path.h"

namespace WTF {

// We'll use nonCharacter* constants to signal invalid utf-8.
// The number in the name signals how many input bytes were invalid.
const int kNonCharacter1 = -1;
const int kNonCharacter2 = -2;
const int kNonCharacter3 = -3;

bool IsNonCharacter(int character) {
  return character >= kNonCharacter3 && character <= kNonCharacter1;
}

std::unique_ptr<TextCodec> TextCodecUTF8::Create(const TextEncoding&,
                                                 const void*) {
  return base::WrapUnique(new TextCodecUTF8());
}

void TextCodecUTF8::RegisterEncodingNames(EncodingNameRegistrar registrar) {
  registrar("UTF-8", "UTF-8");

  // Additional aliases that originally were present in the encoding
  // table in WebKit on Macintosh, and subsequently added by
  // TextCodecICU. Perhaps we can prove some are not used on the web
  // and remove them.
  registrar("unicode11utf8", "UTF-8");
  registrar("unicode20utf8", "UTF-8");
  registrar("utf8", "UTF-8");
  registrar("x-unicode20utf8", "UTF-8");

  // Additional aliases present in the WHATWG Encoding Standard
  // (http://encoding.spec.whatwg.org/)
  // and Firefox (24), but not in ICU 4.6.
  registrar("unicode-1-1-utf-8", "UTF-8");
}

void TextCodecUTF8::RegisterCodecs(TextCodecRegistrar registrar) {
  registrar("UTF-8", Create, nullptr);
}

static inline int NonASCIISequenceLength(uint8_t first_byte) {
  static const uint8_t kLengths[256] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  return kLengths[first_byte];
}

static inline int DecodeNonASCIISequence(const uint8_t* sequence,
                                         unsigned length) {
  DCHECK(!IsASCII(sequence[0]));
  if (length == 2) {
    DCHECK_LE(sequence[0], 0xDF);
    if (sequence[0] < 0xC2)
      return kNonCharacter1;
    if (sequence[1] < 0x80 || sequence[1] > 0xBF)
      return kNonCharacter1;
    return ((sequence[0] << 6) + sequence[1]) - 0x00003080;
  }
  if (length == 3) {
    DCHECK_GE(sequence[0], 0xE0);
    DCHECK_LE(sequence[0], 0xEF);
    switch (sequence[0]) {
      case 0xE0:
        if (sequence[1] < 0xA0 || sequence[1] > 0xBF)
          return kNonCharacter1;
        break;
      case 0xED:
        if (sequence[1] < 0x80 || sequence[1] > 0x9F)
          return kNonCharacter1;
        break;
      default:
        if (sequence[1] < 0x80 || sequence[1] > 0xBF)
          return kNonCharacter1;
    }
    if (sequence[2] < 0x80 || sequence[2] > 0xBF)
      return kNonCharacter2;
    return ((sequence[0] << 12) + (sequence[1] << 6) + sequence[2]) -
           0x000E2080;
  }
  DCHECK_EQ(length, 4u);
  DCHECK_GE(sequence[0], 0xF0);
  DCHECK_LE(sequence[0], 0xF4);
  switch (sequence[0]) {
    case 0xF0:
      if (sequence[1] < 0x90 || sequence[1] > 0xBF)
        return kNonCharacter1;
      break;
    case 0xF4:
      if (sequence[1] < 0x80 || sequence[1] > 0x8F)
        return kNonCharacter1;
      break;
    default:
      if (sequence[1] < 0x80 || sequence[1] > 0xBF)
        return kNonCharacter1;
  }
  if (sequence[2] < 0x80 || sequence[2] > 0xBF)
    return kNonCharacter2;
  if (sequence[3] < 0x80 || sequence[3] > 0xBF)
    return kNonCharacter3;
  return ((sequence[0] << 18) + (sequence[1] << 12) + (sequence[2] << 6) +
          sequence[3]) -
         0x03C82080;
}

static inline UChar* AppendCharacter(UChar* destination, int character) {
  DCHECK(!IsNonCharacter(character));
  DCHECK(!U_IS_SURROGATE(character));
  if (U_IS_BMP(character)) {
    *destination++ = static_cast<UChar>(character);
  } else {
    *destination++ = U16_LEAD(character);
    *destination++ = U16_TRAIL(character);
  }
  return destination;
}

void TextCodecUTF8::ConsumePartialSequenceBytes(int num_bytes) {
  DCHECK_GE(partial_sequence_size_, num_bytes);
  partial_sequence_size_ -= num_bytes;
  memmove(partial_sequence_, partial_sequence_ + num_bytes,
          partial_sequence_size_);
}

void TextCodecUTF8::HandleError(int character,
                                UChar*& destination,
                                bool stop_on_error,
                                bool& saw_error) {
  saw_error = true;
  if (stop_on_error)
    return;
  // Each error generates a replacement character and consumes 1-3 bytes.
  *destination++ = kReplacementCharacter;
  DCHECK(IsNonCharacter(character));
  int num_bytes_consumed = -character;
  DCHECK_GE(num_bytes_consumed, 1);
  DCHECK_LE(num_bytes_consumed, 3);
  ConsumePartialSequenceBytes(num_bytes_consumed);
}

template <>
bool TextCodecUTF8::HandlePartialSequence<LChar>(LChar*& destination,
                                                 const uint8_t*& source,
                                                 const uint8_t* end,
                                                 bool flush,
                                                 bool,
                                                 bool&) {
  DCHECK(partial_sequence_size_);
  do {
    if (IsASCII(partial_sequence_[0])) {
      *destination++ = partial_sequence_[0];
      ConsumePartialSequenceBytes(1);
      continue;
    }
    int count = NonASCIISequenceLength(partial_sequence_[0]);
    if (!count)
      return true;

    if (count > partial_sequence_size_) {
      if (count - partial_sequence_size_ > end - source) {
        if (!flush) {
          // The new data is not enough to complete the sequence, so
          // add it to the existing partial sequence.
          memcpy(partial_sequence_ + partial_sequence_size_, source,
                 end - source);
          partial_sequence_size_ += end - source;
          return false;
        }
        // An incomplete partial sequence at the end is an error, but it will
        // create a 16 bit string due to the replacementCharacter. Let the 16
        // bit path handle the error.
        return true;
      }
      memcpy(partial_sequence_ + partial_sequence_size_, source,
             count - partial_sequence_size_);
      source += count - partial_sequence_size_;
      partial_sequence_size_ = count;
    }
    int character = DecodeNonASCIISequence(partial_sequence_, count);
    if (character & ~0xff)
      return true;

    partial_sequence_size_ -= count;
    *destination++ = static_cast<LChar>(character);
  } while (partial_sequence_size_);

  return false;
}

template <>
bool TextCodecUTF8::HandlePartialSequence<UChar>(UChar*& destination,
                                                 const uint8_t*& source,
                                                 const uint8_t* end,
                                                 bool flush,
                                                 bool stop_on_error,
                                                 bool& saw_error) {
  DCHECK(partial_sequence_size_);
  do {
    if (IsASCII(partial_sequence_[0])) {
      *destination++ = partial_sequence_[0];
      ConsumePartialSequenceBytes(1);
      continue;
    }
    int count = NonASCIISequenceLength(partial_sequence_[0]);
    if (!count) {
      HandleError(kNonCharacter1, destination, stop_on_error, saw_error);
      if (stop_on_error)
        return false;
      continue;
    }
    if (count > partial_sequence_size_) {
      if (count - partial_sequence_size_ > end - source) {
        if (!flush) {
          // The new data is not enough to complete the sequence, so
          // add it to the existing partial sequence.
          memcpy(partial_sequence_ + partial_sequence_size_, source,
                 end - source);
          partial_sequence_size_ += end - source;
          return false;
        }
        // An incomplete partial sequence at the end is an error.
        HandleError(kNonCharacter1, destination, stop_on_error, saw_error);
        if (stop_on_error)
          return false;
        continue;
      }
      memcpy(partial_sequence_ + partial_sequence_size_, source,
             count - partial_sequence_size_);
      source += count - partial_sequence_size_;
      partial_sequence_size_ = count;
    }
    int character = DecodeNonASCIISequence(partial_sequence_, count);
    if (IsNonCharacter(character)) {
      HandleError(character, destination, stop_on_error, saw_error);
      if (stop_on_error)
        return false;
      continue;
    }

    partial_sequence_size_ -= count;
    destination = AppendCharacter(destination, character);
  } while (partial_sequence_size_);

  return false;
}

String TextCodecUTF8::Decode(const char* bytes,
                             wtf_size_t length,
                             FlushBehavior flush,
                             bool stop_on_error,
                             bool& saw_error) {
  const bool do_flush = flush != FlushBehavior::kDoNotFlush;
  // Each input byte might turn into a character.
  // That includes all bytes in the partial-sequence buffer because
  // each byte in an invalid sequence will turn into a replacement character.
  StringBuffer<LChar> buffer(
      base::CheckAdd(partial_sequence_size_, length).ValueOrDie());

  const uint8_t* source = reinterpret_cast<const uint8_t*>(bytes);
  const uint8_t* end = source + length;
  const uint8_t* aligned_end = AlignToMachineWord(end);
  LChar* destination = buffer.Characters();

  do {
    if (partial_sequence_size_) {
      // Explicitly copy destination and source pointers to avoid taking
      // pointers to the local variables, which may harm code generation by
      // disabling some optimizations in some compilers.
      LChar* destination_for_handle_partial_sequence = destination;
      const uint8_t* source_for_handle_partial_sequence = source;
      if (HandlePartialSequence(destination_for_handle_partial_sequence,
                                source_for_handle_partial_sequence, end,
                                do_flush, stop_on_error, saw_error)) {
        source = source_for_handle_partial_sequence;
        goto upConvertTo16Bit;
      }
      destination = destination_for_handle_partial_sequence;
      source = source_for_handle_partial_sequence;
      if (partial_sequence_size_)
        break;
    }

    while (source < end) {
      if (IsASCII(*source)) {
        // Fast path for ASCII. Most UTF-8 text will be ASCII.
        if (IsAlignedToMachineWord(source)) {
          while (source < aligned_end) {
            MachineWord chunk =
                *reinterpret_cast_ptr<const MachineWord*>(source);
            if (!IsAllASCII<LChar>(chunk))
              break;
            CopyASCIIMachineWord(destination, source);
            source += sizeof(MachineWord);
            destination += sizeof(MachineWord);
          }
          if (source == end)
            break;
          if (!IsASCII(*source))
            continue;
        }
        *destination++ = *source++;
        continue;
      }
      int count = NonASCIISequenceLength(*source);
      int character;
      if (count == 0) {
        character = kNonCharacter1;
      } else {
        if (count > end - source) {
          SECURITY_DCHECK(end - source <
                          static_cast<ptrdiff_t>(sizeof(partial_sequence_)));
          DCHECK(!partial_sequence_size_);
          partial_sequence_size_ = static_cast<wtf_size_t>(end - source);
          memcpy(partial_sequence_, source, partial_sequence_size_);
          source = end;
          break;
        }
        character = DecodeNonASCIISequence(source, count);
      }
      if (IsNonCharacter(character)) {
        saw_error = true;
        if (stop_on_error)
          break;

        goto upConvertTo16Bit;
      }
      if (character > 0xff)
        goto upConvertTo16Bit;

      source += count;
      *destination++ = static_cast<LChar>(character);
    }
  } while (do_flush && partial_sequence_size_);

  buffer.Shrink(static_cast<wtf_size_t>(destination - buffer.Characters()));

  return String::Adopt(buffer);

upConvertTo16Bit:
  StringBuffer<UChar> buffer16(
      base::CheckAdd(partial_sequence_size_, length).ValueOrDie());

  UChar* destination16 = buffer16.Characters();

  // Copy the already converted characters
  for (LChar* converted8 = buffer.Characters(); converted8 < destination;)
    *destination16++ = *converted8++;

  do {
    if (partial_sequence_size_) {
      // Explicitly copy destination and source pointers to avoid taking
      // pointers to the local variables, which may harm code generation by
      // disabling some optimizations in some compilers.
      UChar* destination_for_handle_partial_sequence = destination16;
      const uint8_t* source_for_handle_partial_sequence = source;
      HandlePartialSequence(destination_for_handle_partial_sequence,
                            source_for_handle_partial_sequence, end, do_flush,
                            stop_on_error, saw_error);
      destination16 = destination_for_handle_partial_sequence;
      source = source_for_handle_partial_sequence;
      if (partial_sequence_size_)
        break;
    }

    while (source < end) {
      if (IsASCII(*source)) {
        // Fast path for ASCII. Most UTF-8 text will be ASCII.
        if (IsAlignedToMachineWord(source)) {
          while (source < aligned_end) {
            MachineWord chunk =
                *reinterpret_cast_ptr<const MachineWord*>(source);
            if (!IsAllASCII<LChar>(chunk))
              break;
            CopyASCIIMachineWord(destination16, source);
            source += sizeof(MachineWord);
            destination16 += sizeof(MachineWord);
          }
          if (source == end)
            break;
          if (!IsASCII(*source))
            continue;
        }
        *destination16++ = *source++;
        continue;
      }
      int count = NonASCIISequenceLength(*source);
      int character;
      if (count == 0) {
        character = kNonCharacter1;
      } else {
        if (count > end - source) {
          SECURITY_DCHECK(end - source <
                          static_cast<ptrdiff_t>(sizeof(partial_sequence_)));
          DCHECK(!partial_sequence_size_);
          partial_sequence_size_ = static_cast<wtf_size_t>(end - source);
          memcpy(partial_sequence_, source, partial_sequence_size_);
          source = end;
          break;
        }
        character = DecodeNonASCIISequence(source, count);
      }
      if (IsNonCharacter(character)) {
        saw_error = true;
        if (stop_on_error)
          break;
        // Each error generates one replacement character and consumes the
        // 'largest subpart' of the incomplete character.
        // Note that the nonCharacterX constants go from -1..-3 and contain
        // the negative of number of bytes comprising the broken encoding
        // detected. So subtracting c (when isNonCharacter(c)) adds the number
        // of broken bytes.
        *destination16++ = kReplacementCharacter;
        source -= character;
        continue;
      }
      source += count;
      destination16 = AppendCharacter(destination16, character);
    }
  } while (do_flush && partial_sequence_size_);

  buffer16.Shrink(
      static_cast<wtf_size_t>(destination16 - buffer16.Characters()));

  return String::Adopt(buffer16);
}

template <typename CharType>
std::string TextCodecUTF8::EncodeCommon(const CharType* characters,
                                        wtf_size_t length) {
  // The maximum number of UTF-8 bytes needed per UTF-16 code unit is 3.
  // BMP characters take only one UTF-16 code unit and can take up to 3 bytes
  // (3x).
  // Non-BMP characters take two UTF-16 code units and can take up to 4 bytes
  // (2x).
  CHECK_LE(length, std::numeric_limits<wtf_size_t>::max() / 3);
  Vector<uint8_t> bytes(length * 3);

  wtf_size_t i = 0;
  wtf_size_t bytes_written = 0;
  while (i < length) {
    UChar32 character;
    U16_NEXT(characters, i, length, character);
    // U16_NEXT will simply emit a surrogate code point if an unmatched
    // surrogate is encountered; we must convert it to a
    // U+FFFD (REPLACEMENT CHARACTER) here.
    if (0xD800 <= character && character <= 0xDFFF)
      character = kReplacementCharacter;
    U8_APPEND_UNSAFE(bytes.data(), bytes_written, character);
  }

  return std::string(reinterpret_cast<char*>(bytes.data()), bytes_written);
}

template <typename CharType>
TextCodec::EncodeIntoResult TextCodecUTF8::EncodeIntoCommon(
    const CharType* characters,
    wtf_size_t length,
    unsigned char* destination,
    size_t capacity) {
  TextCodec::EncodeIntoResult encode_into_result{0, 0};

  wtf_size_t i = 0;
  wtf_size_t previous_code_unit_index = 0;
  bool is_error = false;
  while (i < length && encode_into_result.bytes_written < capacity &&
         !is_error) {
    UChar32 character;
    previous_code_unit_index = i;
    U16_NEXT(characters, i, length, character);
    // U16_NEXT will simply emit a surrogate code point if an unmatched
    // surrogate is encountered. See comment in EncodeCommon() for more info.
    if (0xD800 <= character && character <= 0xDFFF)
      character = kReplacementCharacter;
    U8_APPEND(destination, encode_into_result.bytes_written, capacity,
              character, is_error);
  }

  // |is_error| is only true when U8_APPEND cannot append the UTF8 bytes that
  // represent a given UTF16 code point, due to limited capacity. In that case,
  // the last code point read was not used, so we must not include its code
  // units in our final |code_units_read| count.
  if (is_error)
    encode_into_result.code_units_read = previous_code_unit_index;
  else
    encode_into_result.code_units_read = i;

  return encode_into_result;
}

std::string TextCodecUTF8::Encode(const UChar* characters,
                                  wtf_size_t length,
                                  UnencodableHandling) {
  return EncodeCommon(characters, length);
}

std::string TextCodecUTF8::Encode(const LChar* characters,
                                  wtf_size_t length,
                                  UnencodableHandling) {
  return EncodeCommon(characters, length);
}

TextCodec::EncodeIntoResult TextCodecUTF8::EncodeInto(
    const UChar* characters,
    wtf_size_t length,
    unsigned char* destination,
    size_t capacity) {
  return EncodeIntoCommon(characters, length, destination, capacity);
}

TextCodec::EncodeIntoResult TextCodecUTF8::EncodeInto(
    const LChar* characters,
    wtf_size_t length,
    unsigned char* destination,
    size_t capacity) {
  return EncodeIntoCommon(characters, length, destination, capacity);
}

}  // namespace WTF
