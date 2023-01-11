// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "media/learning/impl/test_random_number_generator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class RandomNumberGeneratorTest : public testing::Test {
 public:
  RandomNumberGeneratorTest() : rng_(0) {}

  void GenerateAndVerify(base::RepeatingCallback<int64_t()> generate,
                         int64_t lower_inclusive,
                         int64_t upper_inclusive) {
    const size_t n = 10000;
    std::map<int64_t, size_t> counts;
    for (size_t i = 0; i < n; i++)
      counts[generate.Run()]++;
    // Verify that it's uniform over |lower_inclusive| and
    // |upper_inclusive|, at least approximately.
    size_t min_counts = counts[lower_inclusive];
    size_t max_counts = min_counts;
    size_t total_counts = min_counts;
    for (int64_t i = lower_inclusive + 1; i <= upper_inclusive; i++) {
      size_t c = counts[i];
      if (c < min_counts)
        min_counts = c;

      if (c > max_counts)
        max_counts = c;

      total_counts += c;
    }

    // See if the min and the max are too far from the expected counts.
    // Note that this will catch only egregious problems.  Also note that
    // random variation might actually exceed these limits fairly often.
    // It's only because the test rng has no variation that we know it
    // won't happen.  However, there might be reasonable implementation
    // changes that trip these tests (deterministically!); manual
    // verification of the result is needed in those cases.
    //
    // These just catch things like "rng range is off by one", etc.
    size_t expected_counts = n / (upper_inclusive - lower_inclusive + 1);
    EXPECT_LT(max_counts, expected_counts * 1.05);
    EXPECT_GT(max_counts, expected_counts * 0.95);
    EXPECT_LT(min_counts, expected_counts * 1.05);
    EXPECT_GT(min_counts, expected_counts * 0.95);

    // Verify that the counts between the limits accounts for all of them.
    // Otherwise, some rng values were out of range.
    EXPECT_EQ(total_counts, n);
  }

  // We use TestRandomNumberGenerator, since we really want to test the base
  // class method implementations with a predictable random source.
  TestRandomNumberGenerator rng_;
};

TEST_F(RandomNumberGeneratorTest, ExclusiveUpTo) {
  // Try Generate with something that's not a divisor of max_int, to try to
  // catch any bias.  I.e., an implementation like "rng % range" should fail
  // this test.
  //
  // Unfortunately, it won't.
  //
  // With uint64_t random values, it's unlikely that we would ever notice such a
  // problem.  For example, a range of size three would just  remove ~three from
  // the upper range of the rng, and it's unlikely that we'd ever pick the three
  // highest values anyway.  If, instead, we make |range| really big, then we're
  // not going to sample enough points to notice the deviation from uniform.
  //
  // However, we still look for issues like "off by one".
  const uint64_t range = 5;
  GenerateAndVerify(base::BindRepeating(
                        [](RandomNumberGenerator* rng, uint64_t range) {
                          return static_cast<int64_t>(rng->Generate(range));
                        },
                        &rng_, range),
                    0, range - 1);
}

TEST_F(RandomNumberGeneratorTest, DoublesStayInRange) {
  const double limit = 1000.5;
  int num_non_integer = 0;
  for (int i = 0; i < 1000; i++) {
    double v = rng_.GenerateDouble(limit);
    EXPECT_GE(v, 0.);
    EXPECT_LT(v, limit);
    // Also count how many non-integers we get.
    num_non_integer += (v != static_cast<int>(v));
  }

  // Expect a lot of non-integers.
  EXPECT_GE(num_non_integer, 900);
}

}  // namespace media
