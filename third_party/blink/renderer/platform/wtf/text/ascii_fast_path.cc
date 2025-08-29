// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/ascii_fast_path.h"

namespace blink {

namespace {

constexpr inline MachineWord FillCharsInWord(LChar ch) {
  if constexpr (sizeof(MachineWord) == 4) {
    return static_cast<uint32_t>(ch) * 0x01010101U;
  } else {
    return static_cast<uint64_t>(ch) * 0x0101010101010101ULL;
  }
}

}  // namespace

// Note: This function is heavily optimized to operate at the word-level where
// possible because it's a hotspot function. In particular, it's called as part
// of `blink::String::FromUTF8` as part of mojo conversion. See
// crbug.com/441107248.
template <>
WTF_EXPORT AsciiStringAttributes
CharacterAttributes(base::span<const LChar> chars) {
  DCHECK_GT(chars.size(), 0u);

  constexpr size_t kWordSize = sizeof(MachineWord);
  constexpr size_t kCharsPerWord = kWordSize / sizeof(LChar);

  MachineWord all_char_bits_word = 0;
  MachineWord contains_ascii_upper_case = 0;

  base::span<const LChar> remaining = chars;

  // First, process any unaligned characters.
  while (!remaining.empty() && !IsAlignedToMachineWord(remaining.data())) {
    all_char_bits_word |= remaining.front();
    contains_ascii_upper_case |= IsASCIIUpper(remaining.front());
    remaining = remaining.subspan(1u);
  }

  // Then, process aligned words.
  while (remaining.size() >= kCharsPerWord) {
    // This is the fast path where we can process all of the chars in a word
    // at once.
    const MachineWord word =
        *reinterpret_cast<const MachineWord*>(remaining.data());

    // The "IsAscii" check really just sees if any char in the string is >=
    // 128. We can efficiently check for that by or'ing all of the words of
    // the string together, and then check for a 1 in the most significant bit
    // of each char in the word at the end.
    all_char_bits_word |= word;

    // Uppercase Ascii Check Algorithm: For the lower 7 bits of each byte in
    // the word, determine if it's between A and Z. This algorithm uses only
    // bit operators and no conditionals. It uses the most significant bit
    // (msb) of each byte to store data.
    constexpr MachineWord kMSBMask = FillCharsInWord(0x80);
    constexpr MachineWord k7FMask = FillCharsInWord(0x7F);
    constexpr MachineWord kLowerAddend = FillCharsInWord(127 - ('A' - 1));
    constexpr MachineWord kUpperMinuend = FillCharsInWord(127 + ('Z' + 1));

    // 1. Strip the most significant bit from each character as we need it to
    // mark chars between A and Z (we correct for this later).
    const MachineWord word_stripped_msb = word & k7FMask;

    // 2. `wordStrippedMsb + kLowerAddend` sets msb for all chars >= 'A' and
    // is guaranteed not to overflow.
    const MachineWord greater_equal_upper_a = word_stripped_msb + kLowerAddend;

    // 3. `kUpperMinuend - wordStrippedMsb` sets msb for all chars <= 'Z' and
    // is guaranteed not to underflow.
    const MachineWord less_equal_upper_z = kUpperMinuend - word_stripped_msb;

    // 4. AND the two to set msb for all chars >= 'A' AND <= 'Z'.
    const MachineWord msb_for_upper_case =
        greater_equal_upper_a & less_equal_upper_z;

    // 5. Zero out the non-msb bits. At this point, any byte == 128
    // had its lower 7 bits between [A,Z].
    const MachineWord msb_only = msb_for_upper_case & kMSBMask;

    // 6. AND with ~word to remove the msb for chars that were >= 128 to begin
    // with. At this point, if the value is non-zero, then there is at least
    // one ascii uppercase character in the word.
    contains_ascii_upper_case |= msb_only & ~word;

    remaining = remaining.subspan(kCharsPerWord);
  }

  // Finally, process trailing unaligned bytes.
  while (!remaining.empty()) {
    all_char_bits_word |= remaining.front();
    contains_ascii_upper_case |= IsASCIIUpper(remaining.front());
    remaining = remaining.subspan(1u);
  }

  return AsciiStringAttributes(IsAllAscii<LChar>(all_char_bits_word),
                               !contains_ascii_upper_case);
}

}  // namespace blink
