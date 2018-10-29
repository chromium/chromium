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

VTTScanner::VTTScanner(const String& line) : is8_bit_(line.Is8Bit()) {
  if (is8_bit_) {
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

bool VTTScanner::Scan(const LChar* characters, wtf_size_t characters_count) {
  wtf_size_t match_length =
      is8_bit_
          ? static_cast<wtf_size_t>(end_.characters8 - data_.characters8)
          : static_cast<wtf_size_t>(end_.characters16 - data_.characters16);
  if (match_length < characters_count)
    return false;
  bool matched;
  if (is8_bit_)
    matched = WTF::Equal(data_.characters8, characters, characters_count);
  else
    matched = WTF::Equal(data_.characters16, characters, characters_count);
  if (matched)
    Advance(characters_count);
  return matched;
}

bool VTTScanner::ScanRun(const Run& run, const String& to_match) {
  DCHECK_EQ(run.Start(), GetPosition());
  DCHECK_LE(run.Start(), end());
  DCHECK_GE(run.end(), run.Start());
  DCHECK_LE(run.end(), end());
  wtf_size_t match_length = run.length();
  if (to_match.length() > match_length)
    return false;
  bool matched;
  if (is8_bit_)
    matched = WTF::Equal(to_match.Impl(), data_.characters8, match_length);
  else
    matched = WTF::Equal(to_match.Impl(), data_.characters16, match_length);
  if (matched)
    SeekTo(run.end());
  return matched;
}

void VTTScanner::SkipRun(const Run& run) {
  DCHECK_LE(run.Start(), end());
  DCHECK_GE(run.end(), run.Start());
  DCHECK_LE(run.end(), end());
  SeekTo(run.end());
}

String VTTScanner::ExtractString(const Run& run) {
  DCHECK_EQ(run.Start(), GetPosition());
  DCHECK_LE(run.Start(), end());
  DCHECK_GE(run.end(), run.Start());
  DCHECK_LE(run.end(), end());
  String s;
  if (is8_bit_)
    s = String(data_.characters8, run.length());
  else
    s = String(data_.characters16, run.length());
  SeekTo(run.end());
  return s;
}

String VTTScanner::RestOfInputAsString() {
  Run rest(GetPosition(), end(), is8_bit_);
  return ExtractString(rest);
}

unsigned VTTScanner::ScanDigits(unsigned& number) {
  Run run_of_digits = CollectWhile<IsASCIIDigit>();
  if (run_of_digits.IsEmpty()) {
    number = 0;
    return 0;
  }
  bool valid_number;
  wtf_size_t num_digits = run_of_digits.length();
  if (is8_bit_) {
    number = CharactersToUInt(data_.characters8, num_digits,
                              WTF::NumberParsingOptions::kNone, &valid_number);
  } else {
    number = CharactersToUInt(data_.characters16, num_digits,
                              WTF::NumberParsingOptions::kNone, &valid_number);
  }

  // Since we know that scanDigits only scanned valid (ASCII) digits (and
  // hence that's what got passed to charactersToUInt()), the remaining
  // failure mode for charactersToUInt() is overflow, so if |validNumber| is
  // not true, then set |number| to the maximum unsigned value.
  if (!valid_number)
    number = std::numeric_limits<unsigned>::max();
  // Consume the digits.
  SeekTo(run_of_digits.end());
  return num_digits;
}

bool VTTScanner::ScanDouble(double& number) {
  Run integer_run = CollectWhile<IsASCIIDigit>();
  SeekTo(integer_run.end());
  Run decimal_run(GetPosition(), GetPosition(), is8_bit_);
  if (Scan('.')) {
    decimal_run = CollectWhile<IsASCIIDigit>();
    SeekTo(decimal_run.end());
  }

  // At least one digit required.
  if (integer_run.IsEmpty() && decimal_run.IsEmpty()) {
    // Restore to starting position.
    SeekTo(integer_run.Start());
    return false;
  }

  size_t length_of_double =
      Run(integer_run.Start(), GetPosition(), is8_bit_).length();
  bool valid_number;
  if (is8_bit_) {
    number = CharactersToDouble(integer_run.Start(), length_of_double,
                                &valid_number);
  } else {
    number =
        CharactersToDouble(reinterpret_cast<const UChar*>(integer_run.Start()),
                           length_of_double, &valid_number);
  }

  if (number == std::numeric_limits<double>::infinity())
    return false;

  if (!valid_number)
    number = std::numeric_limits<double>::max();
  return true;
}

bool VTTScanner::ScanPercentage(double& percentage) {
  Position saved_position = GetPosition();
  if (!ScanDouble(percentage))
    return false;
  if (Scan('%'))
    return true;
  // Restore scanner position.
  SeekTo(saved_position);
  return false;
}
}  // namespace blink
