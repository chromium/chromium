// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/cpuid_base_frequency_parser.h"
#include <vector>

#include "base/cpu.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

TEST(ParseBaseFrequencyFromCpuidTest, ProductionData_NoCrash) {
  base::CPU cpu;
  SCOPED_TRACE(cpu.cpu_brand());

  int64_t base_frequency = ParseBaseFrequencyFromCpuid(cpu.cpu_brand());
  if (base_frequency < 0) {
    EXPECT_EQ(base_frequency, -1);
  } else {
    EXPECT_GE(base_frequency, 100'000'000);
  }
}

TEST(ParseBaseFrequencyFromCpuidTest, InvalidStrings) {
  std::vector<const char*> test_cases = {
      // Real CPUID vendor strings without frequency information.
      "AMD Ryzen 7 1800X Eight-Core Processor",
      "ARMv7 Processor rev 3 (v7l)",
      "AArch64 Processor rev 14 (aarch64)",

      // Parser edge cases.
      "MHz",  // Number-less units.
      "GHz",
      "FooMHz",  // Units preceded by non-numbers.
      "FooGHz",
      "\U0001f41bGHz",
      "1\U0001f41bGHz",
      "1.\U0001f41bGHz",
      "1.0\U0001f41bGHz",
      "1",       // Numbers without units.
      "1000MH",  // Numbers without units.
      "1000Mz",
      "1GH",
      "1Gz",
      "9223372037GHz",  // Overly large numbers.
      "9223372036855MHz",
      "",          // Empty string
      "900..Mhz",  // Regexp false-positives (invalid numbers).
      "900.9.9Mhz",
      "0MHz",
      "0GHz",
      "0.9MHz",
      "999.9.9Mhz",
      "1MHz",
      "99MHz",
      "0.09GHz",
      "0.099GHz",
      "0.0999GHz",
      "0.9.Ghz",
      "0.9.9Ghz",
      "0.9..Ghz",
      "..9Ghz",
      ".9.9Ghz",
  };

  for (const char* brand_string : test_cases) {
    SCOPED_TRACE(brand_string);
    EXPECT_EQ(-1, ParseBaseFrequencyFromCpuid(brand_string));
  }
}

TEST(ParseBaseFrequencyFromCpuidTest, ValidStrings) {
  struct TestCase {
    const char* brand_string;
    int64_t base_frequency;
  };

  std::vector<TestCase> test_cases = {
      // Real CPUID vendor strings.
      {"Intel(R) Pentium(R) III CPU - S 1133MHz", 1'133'000'000},
      {"Intel(R) Core(TM) i7-3770K CPU @ 3.50GHz", 3'500'000'000},
      {"Intel(R) Core(TM) m3-6Y30 CPU @ 0.90GHz", 900'000'000},
      {"Intel(R) Xeon(R) Gold 6154 CPU @ 3.00GHz", 3'000'000'000},
      {"VIA Esther processor 1200MHz", 1'200'000'000},

      // Parser edge cases.
      {"100MHz", 100'000'000},  // Smallest integer values for MHz and GHz.
      {"1GHz", 1'000'000'000},
      {"0.1GHz", 100'000'000},  // Smallest value for GHz.
      {"900mhz", 900'000'000},  // Case-insensitive matches for MHz.
      {"733Mhz", 733'000'000},
      {"4000MHZ", 4'000'000'000},
      {"1GHz", 1'000'000'000},  // Case-insensitive matches for GHz.
      {"2ghz", 2'000'000'000},
      {"3Ghz", 3'000'000'000},
      {"4GHZ", 4'000'000'000},
      {"@2GHz", 2'000'000'000},  // Text right before the number.
      {"\U0001f5782GHz", 2'000'000'000},
      {"\U0001f578 2GHz", 2'000'000'000},
      {"933 MHz", 933'000'000},  // Spaces between unit and frequency.
      {"1.2   GHz", 1'200'000'000},
      {"0.1GHz", 100'000'000},    // Smallest value for GHz.
      {"1.1GHz", 1'100'000'000},  // Decimal fractions.
      {"1.10GHz", 1'100'000'000},
      {"0.234GHz", 234'000'000},
      {".987Ghz", 987'000'000},
      {"1234.56789MHz", 1'234'567'890},
      {"9223372036Ghz", 9'223'372'036'000'000'000},  // Large numbers.
      {"9223372036800Mhz", 9'223'372'036'800'000'000},
      {"0GHz 0.00GHz 1.0.0Ghz 1.1Ghz 1.2Ghz", 1'100'000'000},  // False starts.
      {"100MHzabc", 100'000'000},  // Text after the frequency
      {"1GHz\U0001f578", 1'000'000'000},
  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.brand_string);
    EXPECT_EQ(test_case.base_frequency,
              ParseBaseFrequencyFromCpuid(test_case.brand_string));
  }
}

}  // namespace

}  // namespace content
