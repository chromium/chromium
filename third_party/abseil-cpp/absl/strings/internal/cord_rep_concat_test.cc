// Copyright 2021 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdint>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/config.h"
#include "absl/strings/internal/cord_internal.h"
#include "absl/strings/internal/cord_rep_test_util.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace cord_internal {
namespace {

using ::absl::cordrep_testing::MakeFlat;
using ::absl::cordrep_testing::MakeSubstring;
using ::testing::Eq;

MATCHER_P2(EqExtractResult, tree, rep, "Equals ExtractResult") {
  if (arg.tree != tree || arg.extracted != rep) {
    *result_listener << "Expected {" << static_cast<const void*>(tree) << ", "
                     << static_cast<const void*>(rep) << "}, got {" << arg.tree
                     << ", " << arg.extracted << "}";
    return false;
  }
  return true;
}

CordRepConcat* MakeConcat(CordRep* left, CordRep* right, int depth) {
  CordRepConcat* concat = new CordRepConcat;
  concat->tag = CONCAT;
  concat->set_depth(depth);
  concat->length = left->length + right->length;
  concat->left = left;
  concat->right = right;
  return concat;
}

CordRepConcat::ExtractResult ExtractLast(CordRepConcat* concat,
                                         size_t extra_capacity = 1) {
  return CordRepConcat::ExtractAppendBuffer(concat, extra_capacity);
}

TEST(CordRepConcatTest, ExtractAppendBufferTwoFlats) {
  CordRepFlat* flat1 = MakeFlat("abc");
  CordRepFlat* flat2 = MakeFlat("defg");
  CordRepConcat* concat = MakeConcat(flat1, flat2, 0);
  EXPECT_THAT(ExtractLast(concat), EqExtractResult(flat1, flat2));
  CordRep::Unref(flat1);
  CordRep::Unref(flat2);
}

TEST(CordRepConcatTest, ExtractAppendBufferThreeFlatsOne) {
  CordRepFlat* flat1 = MakeFlat("abc");
  CordRepFlat* flat2 = MakeFlat("defg");
  CordRepFlat* flat3 = MakeFlat("hijkl");
  CordRepConcat* lconcat = MakeConcat(flat1, flat2, 0);
  CordRepConcat* concat = MakeConcat(lconcat, flat3, 1);
  EXPECT_THAT(ExtractLast(concat), EqExtractResult(lconcat, flat3));
  ASSERT_THAT(lconcat->length, Eq(7));
  CordRep::Unref(lconcat);
  CordRep::Unref(flat3);
}

TEST(CordRepConcatTest, ExtractAppendBufferThreeFlatsTwo) {
  CordRepFlat* flat1 = MakeFlat("hijkl");
  CordRepFlat* flat2 = MakeFlat("abc");
  CordRepFlat* flat3 = MakeFlat("defg");
  CordRepConcat* rconcat = MakeConcat(flat2, flat3, 0);
  CordRepConcat* concat = MakeConcat(flat1, rconcat, 1);
  EXPECT_THAT(ExtractLast(concat), EqExtractResult(concat, flat3));
  ASSERT_THAT(concat->length, Eq(8));
  CordRep::Unref(concat);
  CordRep::Unref(flat3);
}

TEST(CordRepConcatTest, ExtractAppendBufferShared) {
  CordRepFlat* flat1 = MakeFlat("hijkl");
  CordRepFlat* flat2 = MakeFlat("abc");
  CordRepFlat* flat3 = MakeFlat("defg");
  CordRepConcat* rconcat = MakeConcat(flat2, flat3, 0);
  CordRepConcat* concat = MakeConcat(flat1, rconcat, 1);

  CordRep::Ref(concat);
  EXPECT_THAT(ExtractLast(concat), EqExtractResult(concat, nullptr));
  CordRep::Unref(concat);

  CordRep::Ref(rconcat);
  EXPECT_THAT(ExtractLast(concat), EqExtractResult(concat, nullptr));
  CordRep::Unref(rconcat);

  CordRep::Ref(flat3);
  EXPECT_THAT(ExtractLast(concat), EqExtractResult(concat, nullptr));
  CordRep::Unref(flat3);

  CordRep::Unref(concat);
}

TEST(CordRepConcatTest, ExtractAppendBufferNotFlat) {
  CordRepFlat* flat1 = MakeFlat("hijkl");
  CordRepFlat* flat2 = MakeFlat("abc");
  CordRepFlat* flat3 = MakeFlat("defg");
  auto substr = MakeSubstring(1, 2, flat3);
  CordRepConcat* rconcat = MakeConcat(flat2, substr, 0);
  CordRepConcat* concat = MakeConcat(flat1, rconcat, 1);
  EXPECT_THAT(ExtractLast(concat), EqExtractResult(concat, nullptr));
  CordRep::Unref(concat);
}

TEST(CordRepConcatTest, ExtractAppendBufferNoCapacity) {
  CordRepFlat* flat1 = MakeFlat("hijkl");
  CordRepFlat* flat2 = MakeFlat("abc");
  CordRepFlat* flat3 = MakeFlat("defg");
  size_t avail = flat3->Capacity() - flat3->length;
  CordRepConcat* rconcat = MakeConcat(flat2, flat3, 0);
  CordRepConcat* concat = MakeConcat(flat1, rconcat, 1);

  // Should fail if 1 byte over, success if exactly matching
  EXPECT_THAT(ExtractLast(concat, avail + 1), EqExtractResult(concat, nullptr));
  EXPECT_THAT(ExtractLast(concat, avail), EqExtractResult(concat, flat3));

  CordRep::Unref(concat);
  CordRep::Unref(flat3);
}

}  // namespace
}  // namespace cord_internal
ABSL_NAMESPACE_END
}  // namespace absl
