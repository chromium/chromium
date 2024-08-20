// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_SCOPED_MUTUALLY_EXCLUSIVE_FEATURE_LIST_H_
#define NET_TEST_SCOPED_MUTUALLY_EXCLUSIVE_FEATURE_LIST_H_

#include <concepts>

#include "base/containers/span.h"
#include "base/test/scoped_feature_list.h"

namespace net::test {

class ScopedMutuallyExclusiveFeatureList {
 public:
  template <typename Enum, typename Struct>
    requires requires(Struct s) {
      { s.test_case } -> std::convertible_to<Enum>;
      { s.feature } -> std::same_as<base::test::FeatureRef&>;
    }
  ScopedMutuallyExclusiveFeatureList(Enum param,
                                     base::span<const Struct> mapping) {
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
