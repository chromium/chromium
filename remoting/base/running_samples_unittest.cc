// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/base/running_samples.h"

#include <stddef.h>
#include <stdint.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

typedef void (*TestFunction)(size_t i, RunningSamples& samples);

static const int64_t kTestValues[] = {10, 20, 30, 10, 25, 16, 15};

// Test framework that verifies average() and max() at beginning, iterates
// through all elements and meanwhile calls your own test function
static void TestFramework(int windowSize, TestFunction testFn) {
  RunningSamples samples(windowSize);
  EXPECT_EQ(0, samples.Average());
  EXPECT_EQ(0, samples.Max());

  for (size_t i = 0; i < std::size(kTestValues); ++i) {
    samples.Record(kTestValues[i]);
    testFn(i, samples);
  }
}

// Average across a single element, i.e. just return the most recent.
TEST(RunningSamplesTest, AverageOneElementWindow) {
  TestFramework(1, [](size_t i, RunningSamples& samples) {
    EXPECT_EQ(static_cast<double>(kTestValues[i]), samples.Average());
  });
}

// Average the two most recent elements.
TEST(RunningSamplesTest, AverageTwoElementWindow) {
  TestFramework(2, [](size_t i, RunningSamples& samples) {
    double expected = kTestValues[i];
    if (i > 0) {
      expected = (expected + kTestValues[i - 1]) / 2;
    }

    EXPECT_EQ(expected, samples.Average());
  });
}

// Average across all the elements if the window size exceeds the element count.
TEST(RunningSamplesTest, AverageLongWindow) {
  TestFramework(std::size(kTestValues) + 1,
                [](size_t i, RunningSamples& samples) {
                  double expected = 0.0;
                  for (size_t j = 0; j <= i; ++j) {
                    expected += kTestValues[j];
                  }
                  expected /= i + 1;

                  EXPECT_EQ(expected, samples.Average());
                });
}

// Max of a single element, i.e. just return the most recent.
TEST(RunningSamplesTest, MaxOneElementWindow) {
  TestFramework(1, [](size_t i, RunningSamples& samples) {
    EXPECT_EQ(static_cast<double>(kTestValues[i]), samples.Max());
  });
}

// Max of the two most recent elements.
TEST(RunningSamplesTest, MaxTwoElementWindow) {
  TestFramework(2, [](size_t i, RunningSamples& samples) {
    double expected = kTestValues[i];
    if (i > 0) {
      expected = expected > kTestValues[i - 1] ? expected : kTestValues[i - 1];
    }

    EXPECT_EQ(expected, samples.Max());
  });
}

// Max of all the elements if the window size exceeds the element count.
TEST(RunningSamplesTest, MaxLongWindow) {
  TestFramework(
      std::size(kTestValues) + 1, [](size_t i, RunningSamples& samples) {
        int64_t expected = -1;
        for (size_t j = 0; j <= i; ++j) {
          expected = expected > kTestValues[j] ? expected : kTestValues[j];
        }

        EXPECT_EQ(expected, samples.Max());
      });
}

}  // namespace remoting
