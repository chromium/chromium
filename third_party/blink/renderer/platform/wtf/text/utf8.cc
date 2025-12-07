/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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

#include "third_party/blink/renderer/platform/wtf/text/utf8.h"

#include <unicode/utf16.h>

#include <array>

#include "base/check.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"

namespace blink::unicode {

namespace {

inline size_t InlineUtf8SequenceLengthNonAscii(uint8_t b0) {
  if ((b0 & 0xC0) != 0xC0)
    return 0;
  if ((b0 & 0xE0) == 0xC0)
    return 2;
  if ((b0 & 0xF0) == 0xE0)
    return 3;
  if ((b0 & 0xF8) == 0xF0)
    return 4;
  return 0;
}

inline size_t InlineUtf8SequenceLength(uint8_t b0) {
  return IsASCII(b0) ? 1 : InlineUtf8SequenceLengthNonAscii(b0);
}

// Once the bits are split out into bytes of UTF-8, this is a mask OR-ed
// into the first byte, depending on how many bytes follow.  There are
// as many entries in this table as there are UTF-8 sequence types.
// (I.e., one byte sequence, two byte... etc.). Remember that sequences
// for *legal* UTF-8 will be 4 or fewer bytes total.
static constexpr std::array<uint8_t, 7> kFirstByteMark = {
    0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC};

ConversionStatus ConvertLatin1ToUtf8Internal(base::span<const LChar>& source,
                                             base::span<uint8_t>& target) {
  ConversionStatus status = kConversionOK;
  size_t source_cursor = 0;
  size_t target_cursor = 0;
  size_t target_end = target.size();

  while (source_cursor < source.size()) {
    UChar32 ch;
    uint8_t bytes_to_write = 0;
    const UChar32 kByteMask = 0xBF;
    const UChar32 kByteMark = 0x80;
    const size_t old_source_cursor = source_cursor;
    ch = static_cast<UChar32>(source[source_cursor++]);

    // Figure out how many bytes the result will require
    if (ch < static_cast<UChar32>(0x80)) {
      bytes_to_write = 1;
    } else {
      bytes_to_write = 2;
    }

    target_cursor += bytes_to_write;
    if (target_cursor > target_end) {
      source_cursor = old_source_cursor;  // Back up source index!
      target_cursor -= bytes_to_write;
      status = kTargetExhausted;
      break;
    }
    switch (bytes_to_write) {
      case 2:
        target[--target_cursor] =
            static_cast<uint8_t>((ch | kByteMark) & kByteMask);
        ch >>= 6;
        [[fallthrough]];
      case 1:
        target[--target_cursor] =
            static_cast<uint8_t>(ch | kFirstByteMark[bytes_to_write]);
    }
    target_cursor += bytes_to_write;
  }
  source = source.subspan(source_cursor);
  target = target.subspan(target_cursor);
  return status;
}

ConversionStatus ConvertUtf16ToUtf8Internal(base::span<const UChar>& source,
                                            base::span<uint8_t>& target,
                                            bool strict) {
  ConversionStatus status = kConversionOK;
  size_t source_cursor = 0;
  size_t target_cursor = 0;
  size_t source_end = source.size();
  size_t target_end = target.size();

  while (source_cursor < source_end) {
    UChar32 ch;
    uint8_t bytes_to_write = 0;
    const UChar32 kByteMask = 0xBF;
    const UChar32 kByteMark = 0x80;
    const size_t old_source_cursor = source_cursor;
    ch = static_cast<UChar32>(source[source_cursor++]);
    // If we have a surrogate pair, convert to UChar32 first.
    if (ch >= 0xD800 && ch <= 0xDBFF) {
      // If the 16 bits following the high surrogate are in the source buffer...
      if (source_cursor < source_end) {
        UChar32 ch2 = static_cast<UChar32>(source[source_cursor]);
        // If it's a low surrogate, convert to UChar32.
        if (ch2 >= 0xDC00 && ch2 <= 0xDFFF) {
          ch = ((ch - 0xD800) << 10) + (ch2 - 0xDC00) + 0x0010000;
          ++source_cursor;
        } else if (strict) {  // it's an unpaired high surrogate
          --source_cursor;    // return to the illegal value itself
          status = kSourceIllegal;
          break;
        }
      } else {  // We don't have the 16 bits following the high surrogate.
        --source_cursor;  // return to the high surrogate
        status = kSourceExhausted;
        break;
      }
    } else if (strict) {
      // UTF-16 surrogate values are illegal in UTF-32
      if (ch >= 0xDC00 && ch <= 0xDFFF) {
        --source_cursor;  // return to the illegal value itself
        status = kSourceIllegal;
        break;
      }
    }
    // Figure out how many bytes the result will require
    if (ch < static_cast<UChar32>(0x80)) {
      bytes_to_write = 1;
    } else if (ch < static_cast<UChar32>(0x800)) {
      bytes_to_write = 2;
    } else if (ch < static_cast<UChar32>(0x10000)) {
      bytes_to_write = 3;
    } else if (ch < static_cast<UChar32>(0x110000)) {
      bytes_to_write = 4;
    } else {
      // Surrogate pairs cannot represent codepoints higher than 0x10FFFF, so
      // this should not be reachable.
      NOTREACHED();
    }

    target_cursor += bytes_to_write;
    if (target_cursor > target_end) {
      source_cursor = old_source_cursor;  // Back up source index!
      target_cursor -= bytes_to_write;
      status = kTargetExhausted;
      break;
    }
    switch (bytes_to_write) {
      case 4:
        target[--target_cursor] =
            static_cast<uint8_t>((ch | kByteMark) & kByteMask);
        ch >>= 6;
        [[fallthrough]];
      case 3:
        target[--target_cursor] =
            static_cast<uint8_t>((ch | kByteMark) & kByteMask);
        ch >>= 6;
        [[fallthrough]];
      case 2:
        target[--target_cursor] =
            static_cast<uint8_t>((ch | kByteMark) & kByteMask);
        ch >>= 6;
        [[fallthrough]];
      case 1:
        target[--target_cursor] =
            static_cast<uint8_t>(ch | kFirstByteMark[bytes_to_write]);
    }
    target_cursor += bytes_to_write;
  }
  source = source.subspan(source_cursor);
  target = target.subspan(target_cursor);
  return status;
}

// This must be called with the length pre-determined by the first byte.
// If presented with a length > 4, this returns false.  The Unicode
// definition of UTF-8 goes up to 4-byte sequences.
bool IsLegalUtf8(const base::span<const uint8_t> source) {
  uint8_t a;
  size_t src_cursor = source.size();
  switch (source.size()) {
    default:
      return false;
    case 4:
      if ((a = (source[--src_cursor])) < 0x80 || a > 0xBF) {
        return false;
      }
      [[fallthrough]];
    case 3:
      if ((a = (source[--src_cursor])) < 0x80 || a > 0xBF) {
        return false;
      }
      [[fallthrough]];
    case 2:
      if ((a = (source[--src_cursor])) > 0xBF) {
        return false;
      }

      // no fall-through in this inner switch
      switch (source[0]) {
        case 0xE0:
          if (a < 0xA0)
            return false;
          break;
        case 0xED:
          if (a < 0x80 || a > 0x9F) {
            return false;
          }
          break;
        case 0xF0:
          if (a < 0x90)
            return false;
          break;
        case 0xF4:
          if (a < 0x80 || a > 0x8F) {
            return false;
          }
          break;
        default:
          if (a < 0x80)
            return false;
      }
      [[fallthrough]];

    case 1:
      if ((a = source[0]) >= 0x80 && a < 0xC2) {
        return false;
      }
  }
  if (source[0] > 0xF4) {
    return false;
  }
  return true;
}

inline UChar32 ReadUtf8Sequence(base::span<const uint8_t> source,
                                size_t length) {
  DCHECK_LT(0u, length);
  DCHECK_GT(5u, length);

  if (length == 1) {
    return source[0];
  }

  const uint8_t b0 = source[0];
  const uint8_t b1 = source[1];

  if (length == 2) {
    // 2-byte sequence: 110xxxxx 10xxxxxx
    return ((b0 & 0x1F) << 6) | (b1 & 0x3F);
  }

  const uint8_t b2 = source[2];
  if (length == 3) {
    // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
    return ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
  }

  // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
  const uint8_t b3 = source[3];
  return ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) |
         (b3 & 0x3F);
}

ConversionStatus ConvertUtf8ToUtf16Internal(base::span<const uint8_t>& source,
                                            base::span<UChar>& target,
                                            bool strict) {
  ConversionStatus status = kConversionOK;

  using MachineWord = uintptr_t;
  constexpr size_t kWordWidth = sizeof(MachineWord);
  constexpr MachineWord kAsciiMask =
      (kWordWidth == 8) ? 0x8080808080808080ULL : 0x80808080UL;
  constexpr uintptr_t kMachineWordAlignmentMask = kWordWidth - 1;

  while (!source.empty()) {
    // Attempt the fast path if we have enough data for a full, aligned word.
    if (source.size() >= kWordWidth && target.size() >= kWordWidth &&
        !(reinterpret_cast<uintptr_t>(source.data()) &
          kMachineWordAlignmentMask)) {
      const MachineWord word =
          *reinterpret_cast<const MachineWord*>(source.data());

      if ((word & kAsciiMask) == 0) {
        // All bytes in the aligned word are ASCII. Convert them in a simple
        // loop.
        for (size_t i = 0; i < kWordWidth; ++i) {
          target[i] = source[i];
        }
        source = source.subspan(kWordWidth);
        target = target.subspan(kWordWidth);
        continue;
      }
    }

    // Process one character using the scalar path.
    const size_t utf8_sequence_length = InlineUtf8SequenceLength(source[0]);
    if (source.size() < utf8_sequence_length) {
      status = kSourceExhausted;
      break;
    }

    // Do this check whether lenient or strict
    if (!IsLegalUtf8(source.first(utf8_sequence_length))) {
      status = kSourceIllegal;
      break;
    }

    const UChar32 character = ReadUtf8Sequence(source, utf8_sequence_length);

    if (U_IS_BMP(character)) {
      if (target.empty()) {
        status = kTargetExhausted;
        break;
      }
      // UTF-16 surrogate values are illegal in UTF-32
      if (U_IS_SURROGATE(character)) {
        if (strict) {
          status = kSourceIllegal;
          break;
        }
        target[0] = blink::uchar::kReplacementCharacter;
      } else {
        target[0] = static_cast<UChar>(character);
      }
      source = source.subspan(utf8_sequence_length);
      target = target.subspan(1u);
    } else if (U_IS_SUPPLEMENTARY(character)) {
      // target is a character in range 0xFFFF - 0x10FFFF
      if (target.size() < 2u) {
        status = kTargetExhausted;
        break;
      }
      target[0] = U16_LEAD(character);
      target[1] = U16_TRAIL(character);
      source = source.subspan(utf8_sequence_length);
      target = target.subspan(2u);
    } else {
      // This should never happen; InlineUTF8SequenceLength() can never return
      // a value higher than 4, and a 4-byte UTF-8 sequence can never encode
      // anything higher than 0x10FFFF.
      NOTREACHED();
    }
  }

  return status;
}

}  // namespace

ConversionResult<uint8_t> ConvertLatin1ToUtf8(base::span<const LChar> source,
                                              base::span<uint8_t> target) {
  auto original_source = source;
  auto original_target = target;
  auto status = ConvertLatin1ToUtf8Internal(source, target);
  return {
      original_target.first(original_target.size() - target.size()),
      original_source.size() - source.size(),
      status,
  };
}

ConversionResult<uint8_t> ConvertUtf16ToUtf8(base::span<const UChar> source,
                                             base::span<uint8_t> target,
                                             bool strict) {
  auto original_source = source;
  auto original_target = target;
  auto status = ConvertUtf16ToUtf8Internal(source, target, strict);
  return {
      original_target.first(original_target.size() - target.size()),
      original_source.size() - source.size(),
      status,
  };
}

ConversionResult<UChar> ConvertUtf8ToUtf16(base::span<const uint8_t> source,
                                           base::span<UChar> target,
                                           bool strict) {
  auto original_source = source;
  auto original_target = target;
  auto status = ConvertUtf8ToUtf16Internal(source, target, strict);
  return {
      original_target.first(original_target.size() - target.size()),
      original_source.size() - source.size(),
      status,
  };
}

unsigned CalculateStringLengthFromUtf8(base::span<const uint8_t> data,
                                       bool& seen_non_ascii,
                                       bool& seen_non_latin1) {
  seen_non_ascii = false;
  seen_non_latin1 = false;
  if (data.empty()) {
    return 0;
  }

  unsigned utf16_length = 0;

  size_t data_cursor = 0;
  size_t data_end = data.size();

  while (data_cursor < data_end) {
    if (IsASCII(data[data_cursor])) {
      data_cursor++;
      utf16_length++;
      continue;
    }

    seen_non_ascii = true;
    size_t utf8_sequence_length =
        InlineUtf8SequenceLengthNonAscii(data[data_cursor]);

    if (data_end - data_cursor < utf8_sequence_length) {
      return 0;
    }

    if (!IsLegalUtf8(data.subspan(data_cursor, utf8_sequence_length))) {
      return 0;
    }

    UChar32 character =
        ReadUtf8Sequence(data.subspan(data_cursor), utf8_sequence_length);
    DCHECK(!IsASCII(character));
    data_cursor += utf8_sequence_length;

    if (character > 0xff) {
      seen_non_latin1 = true;
    }

    if (U_IS_BMP(character)) {
      // UTF-16 surrogate values are illegal in UTF-32
      if (U_IS_SURROGATE(character))
        return 0;
      utf16_length++;
    } else if (U_IS_SUPPLEMENTARY(character)) {
      utf16_length += 2;
    } else {
      return 0;
    }
  }

  data = data.first(data_cursor);
  return utf16_length;
}

}  // namespace blink::unicode
