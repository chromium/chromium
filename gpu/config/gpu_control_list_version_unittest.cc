// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "gpu/config/gpu_control_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {

constexpr auto kNumerical = GpuControlList::kVersionStyleNumerical;
constexpr auto kLexical = GpuControlList::kVersionStyleLexical;

constexpr auto kCommon = GpuControlList::kVersionSchemaCommon;
constexpr auto kIntelDriver = GpuControlList::kVersionSchemaIntelDriver;
constexpr auto kNvidiaDriver = GpuControlList::kVersionSchemaNvidiaDriver;

constexpr auto kBetween = GpuControlList::kBetween;
constexpr auto kEQ = GpuControlList::kEQ;
constexpr auto kLT = GpuControlList::kLT;
constexpr auto kLE = GpuControlList::kLE;
constexpr auto kGT = GpuControlList::kGT;
constexpr auto kGE = GpuControlList::kGE;
constexpr auto kAny = GpuControlList::kAny;

}  // namespace anonymous

class VersionTest : public testing::Test {
 public:
  VersionTest() = default;
  ~VersionTest() override = default;

  typedef GpuControlList::Version Version;
};

TEST_F(VersionTest, VersionComparison) {
  {
    Version info = {kAny, kNumerical, kCommon, nullptr, nullptr};
    EXPECT_TRUE(info.Contains("0"));
    EXPECT_TRUE(info.Contains("8.9"));
    EXPECT_TRUE(info.Contains("100"));
    EXPECT_TRUE(info.Contains("1.9.alpha"));
  }
  {
    Version info = {kGT, kNumerical, kCommon, "8.9", nullptr};
    EXPECT_FALSE(info.Contains("7"));
    EXPECT_FALSE(info.Contains("8.9"));
    EXPECT_FALSE(info.Contains("8.9.hs762"));
    EXPECT_FALSE(info.Contains("8.9.1"));
    EXPECT_TRUE(info.Contains("9"));
    EXPECT_TRUE(info.Contains("9.hs762"));
  }
  {
    Version info = {kGE, kNumerical, kCommon, "8.9", nullptr};
    EXPECT_FALSE(info.Contains("7"));
    EXPECT_FALSE(info.Contains("7.07hdy"));
    EXPECT_TRUE(info.Contains("8.9"));
    EXPECT_TRUE(info.Contains("8.9.1"));
    EXPECT_TRUE(info.Contains("8.9.1beta0"));
    EXPECT_TRUE(info.Contains("9"));
    EXPECT_TRUE(info.Contains("9.0rel"));
  }
  {
    Version info = {kEQ, kNumerical, kCommon, "8.9", nullptr};
    EXPECT_FALSE(info.Contains("7"));
    EXPECT_TRUE(info.Contains("8"));
    EXPECT_TRUE(info.Contains("8.1uhdy"));
    EXPECT_TRUE(info.Contains("8.9"));
    EXPECT_TRUE(info.Contains("8.9.8alp9"));
    EXPECT_TRUE(info.Contains("8.9.1"));
    EXPECT_FALSE(info.Contains("9"));
  }
  {
    Version info = {kLT, kNumerical, kCommon, "8.9", nullptr};
    EXPECT_TRUE(info.Contains("7"));
    EXPECT_TRUE(info.Contains("7.txt"));
    EXPECT_TRUE(info.Contains("8.8"));
    EXPECT_TRUE(info.Contains("8.8.test"));
    EXPECT_FALSE(info.Contains("8"));
    EXPECT_FALSE(info.Contains("8.9"));
    EXPECT_FALSE(info.Contains("8.9.1"));
    EXPECT_FALSE(info.Contains("8.9.duck"));
    EXPECT_FALSE(info.Contains("9"));
  }
  {
    Version info = {kLE, kNumerical, kCommon, "8.9", nullptr};
    EXPECT_TRUE(info.Contains("7"));
    EXPECT_TRUE(info.Contains("8.8"));
    EXPECT_TRUE(info.Contains("8"));
    EXPECT_TRUE(info.Contains("8.9"));
    EXPECT_TRUE(info.Contains("8.9.chicken"));
    EXPECT_TRUE(info.Contains("8.9.1"));
    EXPECT_FALSE(info.Contains("9"));
    EXPECT_FALSE(info.Contains("9.pork"));
  }
  {
    Version info = {kBetween, kNumerical, kCommon, "8.9", "9.1"};
    EXPECT_FALSE(info.Contains("7"));
    EXPECT_FALSE(info.Contains("8.8"));
    EXPECT_TRUE(info.Contains("8"));
    EXPECT_TRUE(info.Contains("8.9"));
    EXPECT_TRUE(info.Contains("8.9.1"));
    EXPECT_TRUE(info.Contains("9"));
    EXPECT_TRUE(info.Contains("9.1"));
    EXPECT_TRUE(info.Contains("9.1.9"));
    EXPECT_FALSE(info.Contains("9.2"));
    EXPECT_FALSE(info.Contains("10"));
  }
}

TEST_F(VersionTest, DateComparison) {
  // When we use '-' as splitter, we assume a format of mm-dd-yyyy
  // or mm-yyyy, i.e., a date.
  {
    Version info = {kEQ, kNumerical, kCommon, "1976.3.21", nullptr};
    EXPECT_TRUE(info.Contains("3-21-1976", '-'));
    EXPECT_TRUE(info.Contains("3-1976", '-'));
    EXPECT_TRUE(info.Contains("03-1976", '-'));
    EXPECT_FALSE(info.Contains("21-3-1976", '-'));
  }
  {
    Version info = {kGT, kNumerical, kCommon, "1976.3.21", nullptr};
    EXPECT_TRUE(info.Contains("3-22-1976", '-'));
    EXPECT_TRUE(info.Contains("4-1976", '-'));
    EXPECT_TRUE(info.Contains("04-1976", '-'));
    EXPECT_FALSE(info.Contains("3-1976", '-'));
    EXPECT_FALSE(info.Contains("2-1976", '-'));
  }
  {
    Version info = {kBetween, kNumerical, kCommon, "1976.3.21", "2012.12.25"};
    EXPECT_FALSE(info.Contains("3-20-1976", '-'));
    EXPECT_TRUE(info.Contains("3-21-1976", '-'));
    EXPECT_TRUE(info.Contains("3-22-1976", '-'));
    EXPECT_TRUE(info.Contains("3-1976", '-'));
    EXPECT_TRUE(info.Contains("4-1976", '-'));
    EXPECT_TRUE(info.Contains("1-1-2000", '-'));
    EXPECT_TRUE(info.Contains("1-2000", '-'));
    EXPECT_TRUE(info.Contains("2000", '-'));
    EXPECT_TRUE(info.Contains("11-2012", '-'));
    EXPECT_TRUE(info.Contains("12-2012", '-'));
    EXPECT_TRUE(info.Contains("12-24-2012", '-'));
    EXPECT_TRUE(info.Contains("12-25-2012", '-'));
    EXPECT_FALSE(info.Contains("12-26-2012", '-'));
    EXPECT_FALSE(info.Contains("1-2013", '-'));
    EXPECT_FALSE(info.Contains("2013", '-'));
  }
}

TEST_F(VersionTest, LexicalComparison) {
  // When we use lexical style, we assume a format major.minor.*.
  // We apply numerical comparison to major, lexical comparison to others.
  {
    Version info = {kLT, kLexical, kCommon, "8.201", nullptr};
    EXPECT_TRUE(info.Contains("8.001.100"));
    EXPECT_TRUE(info.Contains("8.109"));
    EXPECT_TRUE(info.Contains("8.10900"));
    EXPECT_TRUE(info.Contains("8.109.100"));
    EXPECT_TRUE(info.Contains("8.2"));
    EXPECT_TRUE(info.Contains("8.20"));
    EXPECT_TRUE(info.Contains("8.200"));
    EXPECT_TRUE(info.Contains("8.20.100"));
    EXPECT_FALSE(info.Contains("8.201"));
    EXPECT_FALSE(info.Contains("8.2010"));
    EXPECT_FALSE(info.Contains("8.21"));
    EXPECT_FALSE(info.Contains("8.21.100"));
    EXPECT_FALSE(info.Contains("9.002"));
    EXPECT_FALSE(info.Contains("9.201"));
    EXPECT_FALSE(info.Contains("12"));
    EXPECT_FALSE(info.Contains("12.201"));
  }
  {
    Version info = {kLT, kLexical, kCommon, "9.002", nullptr};
    EXPECT_TRUE(info.Contains("8.001.100"));
    EXPECT_TRUE(info.Contains("8.109"));
    EXPECT_TRUE(info.Contains("8.10900"));
    EXPECT_TRUE(info.Contains("8.109.100"));
    EXPECT_TRUE(info.Contains("8.2"));
    EXPECT_TRUE(info.Contains("8.20"));
    EXPECT_TRUE(info.Contains("8.200"));
    EXPECT_TRUE(info.Contains("8.20.100"));
    EXPECT_TRUE(info.Contains("8.201"));
    EXPECT_TRUE(info.Contains("8.2010"));
    EXPECT_TRUE(info.Contains("8.21"));
    EXPECT_TRUE(info.Contains("8.21.100"));
    EXPECT_FALSE(info.Contains("9.002"));
    EXPECT_FALSE(info.Contains("9.201"));
    EXPECT_FALSE(info.Contains("12"));
    EXPECT_FALSE(info.Contains("12.201"));
  }
}

TEST_F(VersionTest, IntelDriverSchema) {
  {
    Version info = {kLT, kNumerical, kIntelDriver, "25.20.100.6952", nullptr};
    EXPECT_TRUE(info.Contains("0.0.100.6000"));
    EXPECT_FALSE(info.Contains("0.0.100.7000"));
    EXPECT_FALSE(info.Contains("0.0.200.6000"));
    EXPECT_TRUE(info.Contains("26.20.100.6000"));
    EXPECT_FALSE(info.Contains("24.20.100.7000"));
    EXPECT_TRUE(info.Contains("23.20.16.5037"));
  }
  {
    Version info = {kGT, kNumerical, kIntelDriver, "10.18.15.4256", nullptr};
    EXPECT_TRUE(info.Contains("0.0.15.6000"));
    EXPECT_FALSE(info.Contains("0.0.15.4000"));
    EXPECT_TRUE(info.Contains("10.18.15.4279"));
    EXPECT_FALSE(info.Contains("15.40.15.4058"));
    EXPECT_TRUE(info.Contains("26.20.100.6000"));
    EXPECT_TRUE(info.Contains("26.20.100.4000"));
  }
}

TEST_F(VersionTest, NvidiaDriverSchema) {
  {
    // Nvidia drivers, XX.XX.XXXA.AABB, only AAA.BB is considered.  The version
    // is specified as "AAA.BB" or "AAA" in the workaround file.
    {
      // "AAA.BB" should exactly specify one version.
      Version info = {kLT, kNumerical, kNvidiaDriver, "234.56", nullptr};
      EXPECT_TRUE(info.Contains("26.10.0012.3455"));
      EXPECT_TRUE(info.Contains("00.00.0012.3455"));
      EXPECT_TRUE(info.Contains("00.00.012.3455"));
      EXPECT_TRUE(info.Contains("00.00.12.3455"));
      EXPECT_FALSE(info.Contains("26.10.0012.3456"));
      EXPECT_FALSE(info.Contains("26.10.012.3456"));
      EXPECT_FALSE(info.Contains("26.10.12.3456"));
      EXPECT_FALSE(info.Contains("26.10.0012.3457"));
      EXPECT_FALSE(info.Contains("00.00.0012.3457"));
      EXPECT_TRUE(info.Contains("26.10.0012.2457"));
      EXPECT_TRUE(info.Contains("26.10.0011.3457"));

      // Leading zeros in the third stanza are okay.
      EXPECT_TRUE(info.Contains("26.10.0002.3455"));
      EXPECT_FALSE(info.Contains("26.10.0002.3456"));
      EXPECT_FALSE(info.Contains("26.10.0002.3457"));
      EXPECT_TRUE(info.Contains("26.10.0010.3457"));
      EXPECT_TRUE(info.Contains("26.10.0000.3457"));

      // Missing zeros in the fourth stanza are replaced.
      EXPECT_TRUE(info.Contains("26.10.0012.455"));
      EXPECT_TRUE(info.Contains("26.10.0012.57"));
      EXPECT_FALSE(info.Contains("26.10.0013.456"));
      EXPECT_FALSE(info.Contains("26.10.0013.57"));

      // Too short is rejected.
      EXPECT_FALSE(info.Contains("26.10..57"));
      EXPECT_FALSE(info.Contains("26.10.100"));
      EXPECT_FALSE(info.Contains("26.10.100."));
    }

    {
      // "AAA" should allow "AAA.*"
      Version info = {kEQ, kNumerical, kNvidiaDriver, "234", nullptr};
      EXPECT_FALSE(info.Contains("26.10.0012.3556"));
      EXPECT_TRUE(info.Contains("26.10.0012.3456"));
    }
  }
}

}  // namespace gpu
