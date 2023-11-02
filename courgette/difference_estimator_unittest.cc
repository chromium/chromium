// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/difference_estimator.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "courgette/region.h"

using courgette::DifferenceEstimator;
using courgette::Region;

TEST(DifferenceEstimatorTest, TestSame) {
  static const char kString1[] = "Hello world";
  // kString2 is stack allocated to prevent string sharing.
  const char kString2[] = "Hello world";
  DifferenceEstimator difference_estimator;
  DifferenceEstimator::Base* base =
      difference_estimator.MakeBase(Region(kString1, sizeof(kString1)));
  DifferenceEstimator::Subject* subject =
      difference_estimator.MakeSubject(Region(kString2, sizeof(kString2)));
  EXPECT_EQ(0U, difference_estimator.Measure(base, subject));
}

TEST(DifferenceEstimatorTest, TestDifferent) {
  static const char kString1[] = "Hello world";
  static const char kString2[] = "Hello universe";
  DifferenceEstimator difference_estimator;
  DifferenceEstimator::Base* base =
      difference_estimator.MakeBase(Region(kString1, sizeof(kString1)));
  DifferenceEstimator::Subject* subject =
      difference_estimator.MakeSubject(Region(kString2, sizeof(kString2)));
  EXPECT_EQ(10U, difference_estimator.Measure(base, subject));
}

TEST(DifferenceEstimatorTest, TestDifferentSuperstring) {
  static const char kString1[] = "abcdabcdabcd";
  static const char kString2[] = "abcdabcdabcdabcd";
  DifferenceEstimator difference_estimator;
  DifferenceEstimator::Base* base =
      difference_estimator.MakeBase(Region(kString1, sizeof(kString1)-1));
  DifferenceEstimator::Subject* subject =
      difference_estimator.MakeSubject(Region(kString2, sizeof(kString2)-1));
  EXPECT_EQ(1U, difference_estimator.Measure(base, subject));
}

TEST(DifferenceEstimatorTest, TestDifferentSubstring) {
  static const char kString1[] = "abcdabcdabcdabcd";
  static const char kString2[] = "abcdabcdabcd";
  DifferenceEstimator difference_estimator;
  DifferenceEstimator::Base* base =
      difference_estimator.MakeBase(Region(kString1, sizeof(kString1)-1));
  DifferenceEstimator::Subject* subject =
      difference_estimator.MakeSubject(Region(kString2, sizeof(kString2)-1));
  EXPECT_EQ(1U, difference_estimator.Measure(base, subject));
}
