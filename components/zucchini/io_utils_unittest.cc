// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/io_utils.h"

#include <stdint.h>

#include <sstream>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

TEST(IOUtilsTest, LimitedOutputStream) {
  std::ostringstream oss;
  LimitedOutputStream los(oss, 3);
  EXPECT_FALSE(los.full());
  EXPECT_EQ("", oss.str());
  // Line 1.
  los << "a" << 1 << "b" << 2 << "c" << 3 << std::endl;
  EXPECT_FALSE(los.full());
  EXPECT_EQ("a1b2c3\n", oss.str());
  // Line 2.
  oss.str("");
  los << "\r\r\n\n" << std::endl;  // Manual new lines don't count.
  EXPECT_FALSE(los.full());
  EXPECT_EQ("\r\r\n\n\n", oss.str());
  // Line 3.
  oss.str("");
  los << "blah" << 137;
  EXPECT_FALSE(los.full());
  los << std::endl;
  EXPECT_TRUE(los.full());
  EXPECT_EQ("blah137\n(Additional output suppressed)\n", oss.str());
  // Not testing adding more lines: the behavior is undefined since we rely on
  // caller suppressing output if |los.full()| is true.
}

TEST(IOUtilsTest, AsHex) {
  std::ostringstream oss;
  // Helper for single-line tests. Eats dummy std::ostream& from operator<<().
  auto extract = [&oss](std::ostream&) -> std::string {
    std::string ret = oss.str();
    oss.str("");
    return ret;
  };

  EXPECT_EQ("00000000", extract(oss << AsHex<8>(0)));
  EXPECT_EQ("12345678", extract(oss << AsHex<8>(0x12345678U)));
  EXPECT_EQ("9ABCDEF0", extract(oss << AsHex<8>(0x9ABCDEF0U)));
  EXPECT_EQ("(00000064)", extract(oss << "(" << AsHex<8>(100) << ")"));
  EXPECT_EQ("00FFFF", extract(oss << AsHex<6>(0xFFFFU)));
  EXPECT_EQ("FFFF", extract(oss << AsHex<4>(0xFFFFU)));
  EXPECT_EQ("...FF", extract(oss << AsHex<2>(0xFFFFU)));
  EXPECT_EQ("...00", extract(oss << AsHex<2>(0x100U)));
  EXPECT_EQ("FF\n", extract(oss << AsHex<2>(0xFFU) << std::endl));
  EXPECT_EQ("132457689BACDEF0",
            extract(oss << AsHex<16, uint64_t>(0x132457689BACDEF0LLU)));
  EXPECT_EQ("000000000001", extract(oss << AsHex<12, uint8_t>(1)));
  EXPECT_EQ("00000089", extract(oss << AsHex<8, int32_t>(137)));
  EXPECT_EQ("...FFFFFFFF", extract(oss << AsHex<8, int32_t>(-1)));
  EXPECT_EQ("7FFF", extract(oss << AsHex<4, int16_t>(0x7FFFU)));
  EXPECT_EQ("...8000", extract(oss << AsHex<4, int16_t>(0x8000U)));
  EXPECT_EQ("8000", extract(oss << AsHex<4, uint16_t>(0x8000U)));
}

TEST(IOUtilsTest, PrefixSep) {
  std::ostringstream oss;
  PrefixSep sep(",");
  oss << sep << 3;
  EXPECT_EQ("3", oss.str());
  oss << sep << 1;
  EXPECT_EQ("3,1", oss.str());
  oss << sep << 4 << sep << 1 << sep << "59";
  EXPECT_EQ("3,1,4,1,59", oss.str());
}

TEST(IOUtilsTest, PrefixSepAlt) {
  std::ostringstream oss;
  PrefixSep sep("  ");
  oss << sep << 3;
  EXPECT_EQ("3", oss.str());
  oss << sep << 1;
  EXPECT_EQ("3  1", oss.str());
  oss << sep << 4 << sep << 1 << sep << "59";
  EXPECT_EQ("3  1  4  1  59", oss.str());
}

TEST(IOUtilsTest, EatChar) {
  std::istringstream main_iss;
  // Helper for single-line tests.
  auto iss = [&main_iss](const std::string s) -> std::istringstream& {
    main_iss.clear();
    main_iss.str(s);
    return main_iss;
  };

  EXPECT_TRUE(iss("a,1") >> EatChar('a') >> EatChar(',') >> EatChar('1'));
  EXPECT_FALSE(iss("a,a") >> EatChar('a') >> EatChar(',') >> EatChar('1'));
  EXPECT_FALSE(iss("a") >> EatChar('a') >> EatChar(',') >> EatChar('1'));
  EXPECT_FALSE(iss("x") >> EatChar('X'));
  EXPECT_TRUE(iss("_\n") >> EatChar('_') >> EatChar('\n'));
}

TEST(IOUtilsTest, StrictUInt) {
  std::istringstream main_iss;
  // Helper for single-line tests.
  auto iss = [&main_iss](const std::string& s) -> std::istringstream& {
    main_iss.clear();
    main_iss.str(s);
    return main_iss;
  };

  uint32_t u32 = 0;
  EXPECT_TRUE(iss("1234") >> StrictUInt<uint32_t>(u32));
  EXPECT_EQ(uint32_t(1234), u32);
  EXPECT_TRUE(iss("001234") >> StrictUInt<uint32_t>(u32));
  EXPECT_EQ(uint32_t(1234), u32);
  EXPECT_FALSE(iss("blahblah") >> StrictUInt<uint32_t>(u32));
  EXPECT_EQ(uint32_t(1234), u32);  // No overwrite on failure.
  EXPECT_TRUE(iss("137suffix") >> StrictUInt<uint32_t>(u32));
  EXPECT_EQ(uint32_t(137), u32);
  EXPECT_FALSE(iss(" 1234") >> StrictUInt<uint32_t>(u32));
  EXPECT_FALSE(iss("-1234") >> StrictUInt<uint32_t>(u32));

  uint16_t u16 = 0;
  EXPECT_TRUE(iss("65535") >> StrictUInt<uint16_t>(u16));
  EXPECT_EQ(uint16_t(65535), u16);
  EXPECT_FALSE(iss("65536") >> StrictUInt<uint16_t>(u16));  // Overflow.

  uint64_t u64 = 0;
  EXPECT_TRUE(iss("1000000000001") >> StrictUInt<uint64_t>(u64));
  EXPECT_EQ(uint64_t(1000000000001LL), u64);

  // uint8_t is stubbed out, so no tests for it.
}

TEST(IOUtilsTest, ParseSimpleEquations) {
  std::istringstream iss("123+456=579,4-3=1");
  uint32_t a = 0;
  uint32_t b = 0;
  uint32_t c = 0;
  EXPECT_TRUE(iss >> StrictUInt<uint32_t>(a) >> EatChar('+') >>
              StrictUInt<uint32_t>(b) >> EatChar('=') >>
              StrictUInt<uint32_t>(c));
  EXPECT_EQ(uint32_t(123), a);
  EXPECT_EQ(uint32_t(456), b);
  EXPECT_EQ(uint32_t(579), c);
  EXPECT_TRUE(iss >> EatChar(','));
  EXPECT_TRUE(iss >> StrictUInt<uint32_t>(a) >> EatChar('-') >>
              StrictUInt<uint32_t>(b) >> EatChar('=') >>
              StrictUInt<uint32_t>(c));
  EXPECT_EQ(uint32_t(4), a);
  EXPECT_EQ(uint32_t(3), b);
  EXPECT_EQ(uint32_t(1), c);
}

}  // namespace zucchini
