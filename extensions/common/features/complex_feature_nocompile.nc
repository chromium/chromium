// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "extensions/common/features/complex_feature.h"

namespace extensions {

constexpr SimpleFeatureData kSingleChild[] = {
    {.feature = {.name = "single"}},
};
constexpr ComplexFeatureData kSingleChildData{
    .feature = {.name = "single"},
    .features = StaticSpan(kSingleChild),
    .feature_type = ComplexFeatureType::kSimple,
};
constexpr auto kInvalidSingleChild = StaticFeatureData(kSingleChildData);  // expected-error {{must be initialized by a constant expression}} expected-error {{call to consteval function}}

constexpr SimpleFeatureData kMismatchedInternalChildren[] = {
    {.feature = {.name = "internal"},
     .config = {.is_internal = true}},
    {.feature = {.name = "public"}},
};
constexpr ComplexFeatureData kMismatchedInternalData{
    .feature = {.name = "internal"},
    .features = StaticSpan(kMismatchedInternalChildren),
    .feature_type = ComplexFeatureType::kSimple,
};
constexpr auto kInvalidMismatchedInternal = StaticFeatureData(kMismatchedInternalData);  // expected-error {{must be initialized by a constant expression}} expected-error {{call to consteval function}}

constexpr SimpleFeatureData kMismatchedNoParentChildren[] = {
    {.feature = {.name = "child", .no_parent = true}},
    {.feature = {.name = "parent"}},
};
constexpr ComplexFeatureData kMismatchedNoParentData{
    .feature = {.name = "parent"},
    .features = StaticSpan(kMismatchedNoParentChildren),
    .feature_type = ComplexFeatureType::kSimple,
};
constexpr auto kInvalidMismatchedNoParent = StaticFeatureData(kMismatchedNoParentData);  // expected-error {{must be initialized by a constant expression}} expected-error {{call to consteval function}}

}  // namespace extensions
