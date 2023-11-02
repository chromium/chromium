// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/units.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

TEST(UnitsTest, Convertions) {
  EXPECT_EQ(100, ConvertUnit(100, 100, 100));
  EXPECT_EQ(-100, ConvertUnit(-100, 100, 100));
  EXPECT_EQ(0, ConvertUnit(0, 100, 100));
  EXPECT_EQ(99, ConvertUnit(99, 100, 100));
  EXPECT_EQ(101, ConvertUnit(101, 100, 100));
  EXPECT_EQ(99900, ConvertUnit(999, 10, 1000));
  EXPECT_EQ(100100, ConvertUnit(1001, 10, 1000));

  // Rounding.
  EXPECT_EQ(10, ConvertUnit(999, 1000, 10));
  EXPECT_EQ(10, ConvertUnit(950, 1000, 10));
  EXPECT_EQ(9, ConvertUnit(949, 1000, 10));
  EXPECT_EQ(10, ConvertUnit(1001, 1000, 10));
  EXPECT_EQ(10, ConvertUnit(1049, 1000, 10));
  EXPECT_EQ(11, ConvertUnit(1050, 1000, 10));
  EXPECT_EQ(-10, ConvertUnit(-999, 1000, 10));
  EXPECT_EQ(-10, ConvertUnit(-950, 1000, 10));
  EXPECT_EQ(-9, ConvertUnit(-949, 1000, 10));
  EXPECT_EQ(-10, ConvertUnit(-1001, 1000, 10));
  EXPECT_EQ(-10, ConvertUnit(-1049, 1000, 10));
  EXPECT_EQ(-11, ConvertUnit(-1050, 1000, 10));

  EXPECT_EQ(99920, ConvertUnit(999.2, 10, 1000));
  EXPECT_EQ(-94920, ConvertUnit(-949.2, 10, 1000));
  EXPECT_EQ(99970, ConvertUnit(999.7, 10, 1000));
  EXPECT_EQ(-94970, ConvertUnit(-949.7, 10, 1000));

  EXPECT_EQ(3996, ConvertUnit(999.1, 75, 300));
  EXPECT_EQ(3997, ConvertUnit(999.3, 75, 300));
  EXPECT_EQ(3998, ConvertUnit(999.5, 75, 300));
  EXPECT_EQ(3999, ConvertUnit(999.7, 75, 300));
  EXPECT_EQ(4000, ConvertUnit(999.9, 75, 300));

  EXPECT_EQ(0, ConvertUnit(2, 1000000000, 1));
  EXPECT_EQ(2000000000, ConvertUnit(2, 1, 1000000000));

  EXPECT_EQ(100, ConvertUnitFloat(100, 100, 100));
  EXPECT_EQ(-100, ConvertUnitFloat(-100, 100, 100));
  EXPECT_EQ(0, ConvertUnitFloat(0, 100, 100));
  EXPECT_FLOAT_EQ(0.000002, ConvertUnitFloat(2, 1000, 0.001));
  EXPECT_FLOAT_EQ(2000000, ConvertUnitFloat(2, 0.001, 1000));

  EXPECT_EQ(8.25, ConvertUnitFloat(11.0, kPixelsPerInch, kPointsPerInch));
  // Round down.
  EXPECT_EQ(8, ConvertUnit(11, kPixelsPerInch, kPointsPerInch));
  EXPECT_EQ(7.5, ConvertUnitFloat(10.0, kPixelsPerInch, kPointsPerInch));
  // Round up.
  EXPECT_EQ(8, ConvertUnit(10, kPixelsPerInch, kPointsPerInch));
  EXPECT_EQ(0, ConvertUnit(0, kPixelsPerInch, kPointsPerInch));
}

}  // namespace printing
