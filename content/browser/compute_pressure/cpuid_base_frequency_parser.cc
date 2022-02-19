// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/cpuid_base_frequency_parser.h"

#include <stdint.h>

#include <limits>
#include <ostream>

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/re2/src/re2/stringpiece.h"

namespace content {

int64_t ParseBaseFrequencyFromCpuid(base::StringPiece brand_string) {
  // A perfectly accurate number pattern would be quite a bit more complex, as
  // we want to capture both integers (1133Mhz) and decimal fractions (1.20Ghz).
  // The current pattern is preferred as base::StringToDouble() can catch the
  // false positives.
  //
  // The unit pattern matches the strings "MHz" and "GHz" case-insensitively.
  re2::RE2 frequency_re("([0-9.]+)\\s*([GgMm][Hh][Zz])");

  // As matches are consumed, `input` will be mutated to reflect the
  // non-consumed string.
  re2::StringPiece input(brand_string.data(), brand_string.size());

  re2::StringPiece number_string;  // The frequency number.
  re2::StringPiece unit;           // MHz or GHz (case-insensitive)
  while (
      re2::RE2::FindAndConsume(&input, frequency_re, &number_string, &unit)) {
    DCHECK_GT(number_string.size(), 0u)
        << "The number pattern should only match non-empty strings";
    DCHECK_EQ(unit.size(), 3u)
        << "The unit pattern should match exactly 3 characters";

    double number;
    if (!base::StringToDouble(
            base::StringPiece(number_string.data(), number_string.size()),
            &number)) {
      continue;
    }
    DCHECK_GE(number, 0);

    double unit_multiplier =
        (unit[0] == 'G' || unit[0] == 'g') ? 1'000'000'000 : 1'000'000;

    double frequency = number * unit_multiplier;

    // Avoid conversion overflows. double can (imprecisely) store larger numbers
    // than int64_t.
    if (!base::IsValueInRangeForNumericType<int64_t>(frequency))
      continue;

    int64_t frequency_int = static_cast<int64_t>(frequency);

    // It's unlikely that Chrome can run on CPUs with clock speeds below 100MHz.
    // This cutoff can catch some bad parses.
    static constexpr int64_t kMinimumFrequency = 100'000'000;
    if (frequency_int < kMinimumFrequency)
      continue;

    return frequency_int;
  }

  return -1;
}

}  // namespace content
