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

#include "third_party/blink/renderer/core/html/track/vtt/vtt_scanner.h"

#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"

namespace blink {

VTTScanner::VTTScanner(const String& line) {
  if (line.Is8Bit()) {
    state_.emplace<base::span<const LChar>>(line.Span8());
  } else {
    state_.emplace<base::span<const UChar>>(line.Span16());
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
  return Invoke([&characters](auto& buf) {
    auto [to_match, rest] = buf.split_at(characters.size());
    if (to_match != characters) {
      return false;
    }
    buf = rest;
    return true;
  });
}

String VTTScanner::ExtractString(size_t length) {
  return Invoke([length](auto& buf) {
    auto [string_data, rest] = buf.split_at(length);
    buf = rest;
    return String(string_data);
  });
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
  number = Invoke([num_digits, &valid_number](auto& buf) {
    auto [number_data, rest] = buf.split_at(num_digits);
    // Consume the digits.
    buf = rest;
    return CharactersToUInt(number_data, WTF::NumberParsingOptions(),
                            &valid_number);
  });

  // Since we know that scanDigits only scanned valid (ASCII) digits (and
  // hence that's what got passed to charactersToUInt()), the remaining
  // failure mode for charactersToUInt() is overflow, so if |validNumber| is
  // not true, then set |number| to the maximum unsigned value.
  if (!valid_number)
    number = std::numeric_limits<unsigned>::max();
  return num_digits;
}

bool VTTScanner::ScanDouble(double& number) {
  const State start_state = state_;
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
    state_ = start_state;
    return false;
  }

  bool valid_number;
  number = Invoke(
      [length_of_double, &valid_number](auto& buf) {
        return CharactersToDouble(buf.first(length_of_double), &valid_number);
      },
      start_state);

  if (number == std::numeric_limits<double>::infinity())
    return false;

  if (!valid_number)
    number = std::numeric_limits<double>::max();
  return true;
}

bool VTTScanner::ScanPercentage(double& percentage) {
  const State saved_state = state_;
  if (!ScanDouble(percentage))
    return false;
  if (Scan('%'))
    return true;
  // Restore scanner position.
  state_ = saved_state;
  return false;
}

}  // namespace blink
