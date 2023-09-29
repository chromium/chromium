// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/feature_unittest.test-mojom-features.h"
#include "mojo/public/cpp/bindings/tests/feature_unittest.test-mojom-forward.h"
#include "mojo/public/cpp/bindings/tests/feature_unittest.test-mojom-shared.h"
#include "mojo/public/cpp/bindings/tests/feature_unittest.test-mojom.h"

#include <utility>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo::test::feature_unittest {

//// Tests /////

TEST(FeatureTest, FeatureBasics) {
  EXPECT_TRUE(base::FeatureList::IsEnabled(
      mojo::test::feature_unittest::mojom::TestFeatureOn));
  EXPECT_FALSE(base::FeatureList::IsEnabled(
      mojo::test::feature_unittest::mojom::TestFeatureOff));
}

TEST(FeatureTest, ScopedFeatures) {
  base::test::ScopedFeatureList feature_list1;
  // --enable-features=TestFeatureOff --disable-features=TestFeatureOn.
  feature_list1.InitFromCommandLine("TestFeatureOff", "TestFeatureOn");

  // Check state is affected by the command line.
  EXPECT_FALSE(base::FeatureList::IsEnabled(
      mojo::test::feature_unittest::mojom::TestFeatureOn));
  EXPECT_TRUE(base::FeatureList::IsEnabled(
      mojo::test::feature_unittest::mojom::TestFeatureOff));
}

}  // namespace mojo::test::feature_unittest
