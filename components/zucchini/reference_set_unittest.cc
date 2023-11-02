// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/reference_set.h"

#include <vector>

#include "components/zucchini/image_utils.h"
#include "components/zucchini/target_pool.h"
#include "components/zucchini/test_reference_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

constexpr offset_t kWidth = 2U;

}  // namespace

class ReferenceSetTest : public testing::Test {
 protected:
  // For simplicity, |target_pool_| has no type info (not needed here).
  TargetPool target_pool_ = TargetPool{{0, 2, 3, 5}};
  ReferenceSet reference_set_ =
      ReferenceSet{{kWidth, TypeTag(0), PoolTag(0)}, target_pool_};
};

TEST_F(ReferenceSetTest, InitReferencesFromReader) {
  EXPECT_EQ(std::vector<Reference>(), reference_set_.references());
  EXPECT_EQ(0U, reference_set_.size());
  std::vector<Reference> references = {{10, 0}, {12, 2}, {14, 5}};
  reference_set_.InitReferences(TestReferenceReader(references));
  EXPECT_EQ(references, reference_set_.references());
}

TEST_F(ReferenceSetTest, At) {
  reference_set_.InitReferences({{10, 0}, {12, 2}, {15, 5}});
  // Each references has kWidth = 2, so check all bytes covered.
  EXPECT_EQ(Reference({10, 0}), reference_set_.at(10));
  EXPECT_EQ(Reference({10, 0}), reference_set_.at(11));
  EXPECT_EQ(Reference({12, 2}), reference_set_.at(12));
  EXPECT_EQ(Reference({12, 2}), reference_set_.at(13));
  EXPECT_EQ(Reference({15, 5}), reference_set_.at(15));
  EXPECT_EQ(Reference({15, 5}), reference_set_.at(16));
}

}  // namespace zucchini
