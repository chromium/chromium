// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_SCOPED_MUTUALLY_EXCLUSIVE_FEATURE_LIST_H_
#define NET_TEST_SCOPED_MUTUALLY_EXCLUSIVE_FEATURE_LIST_H_

#include <concepts>
#include <ranges>
#include <vector>

#include "base/test/scoped_feature_list.h"

namespace net::test {

// A helper for parameterized tests where one of a set of features should be
// enabled depending on the value of an enum parameter. Features in the set that
// are not enabled will be explicitly disabled so that the test functions the
// same regardless of the features' default values.
//
// Example usage:
//
//
// using ::testing::Values;
//
// // First define an enum:
// enum class FooConfiguration {
//   kConfig1,
//   kConfig2,
// };
//
// // Define the mapping from enum to feature. The struct must have exactly two
// // members, named `test_case` and `feature`, in that order.
// const struct {
//   const FooConfiguration test_case;
//   base::test::FeatureRef feature;
// } kFooConfigurationToFeatureMapping[] = {
//   { FooConfiguration::kConfig1, features::FooFeature1 },
//   { FooConfiguration::kConfig2, features::FooFeature2 },
// };
//
// // Define the text fixture.
// class FooConfigurationTest :
//   public testing::TestWithParam<FooConfiguration> {
//  public:
//   FooConfigurationTest() :
//     foo_feature_list_(GetParam(), kFooConfigurationToFeatureMapping) {}
//
//  private:
//   ScopedMutuallyExclusiveFeatureList foo_feature_list_;
// };
//
// // Instantiate the test suite.
// INSTANTIATE_TEST_SUITE_P(
//   All,
//   FooConfigurationTest,
//   Values(FooConfiguration::kConfig1, FooConfiguration::kConfig2),
//   [](const testing::TestParamInfo<FooConfiguration>& info) {
//     return info.param == kConfig1 ? "Config1" : "Config2";
//   });
//
// // Write some tests.
//
// TEST_P(FooConfigurationTest, Something) {
//   Foo foo;
//   foo.DoSomething();
//   EXPECT_TRUE(foo.something_was_done());
// }
//
//
// This will result in two tests being run,
// All/FooConfigurationTest.Something/Config1 and
// All/FooConfigurationTest.Something/Config2. The first will run with
// FooFeature1 enabled and FooFeature2 disabled, and the second will run with
// the opposite configuration.

class ScopedMutuallyExclusiveFeatureList {
 public:
  template <typename Enum, typename Range>
    requires std::ranges::forward_range<Range> && requires(const Range& r) {
      {
        std::ranges::begin(r)->test_case
      } -> std::equality_comparable_with<Enum>;
      {
        std::ranges::begin(r)->feature
      } -> std::same_as<const base::test::FeatureRef&>;
    }
  ScopedMutuallyExclusiveFeatureList(Enum param, const Range& mapping) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    for (const auto& [test_case, feature] : mapping) {
      if (param == test_case) {
        enabled_features.push_back(feature);
      } else {
        disabled_features.push_back(feature);
      }
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace net::test

#endif  // NET_TEST_SCOPED_MUTUALLY_EXCLUSIVE_FEATURE_LIST_H_
