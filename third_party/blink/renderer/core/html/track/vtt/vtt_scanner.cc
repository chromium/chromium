/*
 * Copyright (c) 2013, Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opera Software ASA nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/html/track/vtt/vtt_scanner.h"

#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"

namespace blink {

VTTScanner::VTTScanner(const String& line) : is_8bit_(line.Is8Bit()) {
  if (is_8bit_) {
    data_.characters8 = line.Characters8();
    end_.characters8 = data_.characters8 + line.length();
  } else {
    data_.characters16 = line.Characters16();
    end_.characters16 = data_.characters16 + line.length();
  }
}

bool VTTScanner::Scan(char c) {
  if (!Match(c))
    return false;
  Advance();
  return true;
}

bool VTTScanner::Scan(StringView str) {
  const auto characters = str.Span8();
  if (Remaining() < characters.size()) {
    return false;
  }
  bool matched;
  if (is_8bit_) {
    matched = WTF::Equal(data_.characters8, characters.data(),
                         base::checked_cast<wtf_size_t>(characters.size()));
  } else {
    matched = WTF::Equal(data_.characters16, characters.data(),
                         base::checked_cast<wtf_size_t>(characters.size()));
  }
  if (matched)
    Advance(base::checked_cast<wtf_size_t>(characters.size()));
  return matched;
}

String VTTScanner::ExtractString(size_t length) {
  DCHECK_LE(length, Remaining());
  String s;
  if (is_8bit_) {
    s = String(data_.characters8, base::checked_cast<wtf_size_t>(length));
    data_.characters8 += length;
  } else {
    s = String(data_.characters16, base::checked_cast<wtf_size_t>(length));
    data_.characters16 += length;
  }
  return s;
}

String VTTScanner::RestOfInputAsString() {
  return ExtractString(Remaining());
}

size_t VTTScanner::ScanDigits(unsigned& number) {
  const size_t num_digits = CountWhile<IsASCIIDigit>();
  if (num_digits == 0) {
    number = 0;
    return 0;
  }
  bool valid_number;
  if (is_8bit_) {
    number = CharactersToUInt(data_.characters8, num_digits,
                              WTF::NumberParsingOptions(), &valid_number);
    // Consume the digits.
    data_.characters8 += num_digits;
  } else {
    number = CharactersToUInt(data_.characters16, num_digits,
                              WTF::NumberParsingOptions(), &valid_number);
    // Consume the digits.
    data_.characters16 += num_digits;
  }

  // Since we know that scanDigits only scanned valid (ASCII) digits (and
  // hence that's what got passed to charactersToUInt()), the remaining
  // failure mode for charactersToUInt() is overflow, so if |validNumber| is
  // not true, then set |number| to the maximum unsigned value.
  if (!valid_number)
    number = std::numeric_limits<unsigned>::max();
  return num_digits;
}

bool VTTScanner::ScanDouble(double& number) {
  const Position saved_position = GetPosition();
  const size_t num_integer_digits = CountWhile<IsASCIIDigit>();
  AdvanceIfNonZero(num_integer_digits);
  size_t length_of_double = num_integer_digits;
  size_t num_decimal_digits = 0;
  if (Scan('.')) {
    length_of_double++;
    num_decimal_digits = CountWhile<IsASCIIDigit>();
    AdvanceIfNonZero(num_decimal_digits);
    length_of_double += num_decimal_digits;
  }

  // At least one digit required.
  if (num_integer_digits == 0 && num_decimal_digits == 0) {
    // Restore to starting position.
    DCHECK_LE(saved_position, end());
    data_.characters8 = saved_position;
    return false;
  }

  bool valid_number;
  if (is_8bit_) {
    number =
        CharactersToDouble(saved_position, length_of_double, &valid_number);
  } else {
    number = CharactersToDouble(reinterpret_cast<const UChar*>(saved_position),
                                length_of_double, &valid_number);
  }

  if (number == std::numeric_limits<double>::infinity())
    return false;

  if (!valid_number)
    number = std::numeric_limits<double>::max();
  return true;
}

bool VTTScanner::ScanPercentage(double& percentage) {
  const Position saved_position = GetPosition();
  if (!ScanDouble(percentage))
    return false;
  if (Scan('%'))
    return true;
  // Restore scanner position.
  DCHECK_LE(saved_position, end());
  data_.characters8 = saved_position;
  return false;
}
}  // namespace blink
