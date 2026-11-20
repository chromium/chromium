// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include <array>

#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/simple_feature.h"

namespace extensions {

constexpr SimpleFeatureData kData = {.feature = {.name = "feature"}};
[[clang::no_destroy]] constinit const SimpleFeature kFeature{
    StaticFeatureData(kData)};

constexpr auto kNullEntry = std::to_array<const Feature*>({nullptr});
constexpr StaticFeatureTable kNullTable(kNullEntry);  // expected-error {{must be initialized by a constant expression}} expected-error {{call to consteval function}}

void RuntimeTable() {
  std::array<const Feature*, 1> features = {&kFeature};
  StaticFeatureTable table(features);  // expected-error {{call to consteval function}}
}

}  // namespace extensions
