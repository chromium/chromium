// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_diff.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {

class InspectorDiffTest : public testing::Test {
 public:
  InspectorDiffTest() = default;
  ~InspectorDiffTest() override = default;
};

struct Match {
  int pos1;
  int pos2;

  bool operator==(const Match& rh) const {
    return pos1 == rh.pos1 && pos2 == rh.pos2;
  }
};

class CompareArrayInput : public InspectorDiff::Input {
 public:
  CompareArrayInput(Vector<int>& list_a, Vector<int>& list_b)
      : list_a_(list_a), list_b_(list_b) {}

  int GetLength1() override { return list_a_.size(); }
  int GetLength2() override { return list_b_.size(); }
  bool Equals(int index1, int index2) override {
    return list_a_.at(index1) == list_b_.at(index2);
  }

  ~CompareArrayInput() override {}

 private:
  Vector<int>& list_a_;
  Vector<int>& list_b_;
};

class CompareArrayOutput : public InspectorDiff::Output {
 public:
  std::vector<Match> chunks;
  void AddMatch(int pos1, int pos2) override {
    chunks.emplace_back(Match({pos1, pos2}));
  }
};

TEST_F(InspectorDiffTest, CalculateMatches) {
  auto a = Vector<int>({1, 2, 3});
  auto b = Vector<int>({1, 2, 3});
  CompareArrayInput input(a, b);
  CompareArrayOutput output;

  InspectorDiff::CalculateMatches(&input, &output);

  EXPECT_EQ(output.chunks,
            std::vector({Match{0, 0}, Match{1, 1}, Match{2, 2}}));
}

TEST_F(InspectorDiffTest, CalculateMatchesAllDifferent) {
  auto a = Vector<int>({1, 2, 3});
  auto b = Vector<int>({4, 5, 6});
  CompareArrayInput input(a, b);
  CompareArrayOutput output;

  InspectorDiff::CalculateMatches(&input, &output);

  EXPECT_EQ(output.chunks.size(), 0ul);
}

TEST_F(InspectorDiffTest, CalculateMatchesDifferentInMiddle) {
  auto a = Vector<int>({1, 2, 3});
  auto b = Vector<int>({1, 999, 3});
  CompareArrayInput input(a, b);
  CompareArrayOutput output;

  InspectorDiff::CalculateMatches(&input, &output);

  EXPECT_EQ(output.chunks, std::vector({Match({0, 0}), Match({2, 2})}));
}

TEST_F(InspectorDiffTest, CalculateMatchesDifferentAtStart) {
  auto a = Vector<int>({999, 2, 3});
  auto b = Vector<int>({1, 2, 3});
  CompareArrayInput input(a, b);
  CompareArrayOutput output;

  InspectorDiff::CalculateMatches(&input, &output);

  EXPECT_EQ(output.chunks, std::vector({Match({1, 1}), Match({2, 2})}));
}

TEST_F(InspectorDiffTest, CalculateMatchesNoDifferentAtEnd) {
  auto a = Vector<int>({1, 2, 999});
  auto b = Vector<int>({1, 2, 3});
  CompareArrayInput input(a, b);
  CompareArrayOutput output;

  InspectorDiff::CalculateMatches(&input, &output);

  EXPECT_EQ(output.chunks, std::vector({Match({0, 0}), Match({1, 1})}));
}

TEST_F(InspectorDiffTest, CalculateMatchesRemoval) {
  auto a = Vector<int>({2, 3});
  auto b = Vector<int>({1, 2, 3});
  CompareArrayInput input(a, b);
  CompareArrayOutput output;

  InspectorDiff::CalculateMatches(&input, &output);

  EXPECT_EQ(output.chunks, std::vector({Match({0, 1}), Match({1, 2})}));
}

TEST_F(InspectorDiffTest, CalculateMatchesRemovalAndModifications) {
  auto a = Vector<int>({2, 4});
  auto b = Vector<int>({1, 2, 3});
  CompareArrayInput input(a, b);
  CompareArrayOutput output;

  InspectorDiff::CalculateMatches(&input, &output);

  EXPECT_EQ(output.chunks, std::vector({Match({0, 1})}));
}

TEST_F(InspectorDiffTest, CalculateMatchesFindsLCS) {
  auto a = Vector<int>({1, 2, 5, 3, 4, 5});
  auto b = Vector<int>({1, 2, 3, 4, 5});
  CompareArrayInput input(a, b);
  CompareArrayOutput output;

  InspectorDiff::CalculateMatches(&input, &output);

  EXPECT_EQ(output.chunks,
            std::vector({Match({0, 0}), Match({1, 1}), Match({3, 2}),
                         Match({4, 3}), Match({5, 4})}));
}

TEST_F(InspectorDiffTest, FindLCSMappingSameElements) {
  auto a = Vector<String>({"a", "b"});
  auto b = Vector<String>({"a", "b"});
  InspectorIndexMap a_to_b;
  InspectorIndexMap b_to_a;

  InspectorDiff::FindLCSMapping(a, b, &a_to_b, &b_to_a);

  EXPECT_EQ(a_to_b.size(), 2ul);
  EXPECT_EQ(b_to_a.size(), 2ul);
  EXPECT_EQ(a_to_b.at(0), 0ul);
  EXPECT_EQ(a_to_b.at(1), 1ul);
  EXPECT_EQ(b_to_a.at(0), 0ul);
  EXPECT_EQ(b_to_a.at(1), 1ul);
}

TEST_F(InspectorDiffTest, FindLCSMappingOneElement) {
  auto a = Vector<String>({"a", "b"});
  auto b = Vector<String>({"b", "a"});
  InspectorIndexMap a_to_b;
  InspectorIndexMap b_to_a;

  InspectorDiff::FindLCSMapping(a, b, &a_to_b, &b_to_a);

  EXPECT_EQ(a_to_b.size(), 1ul);
  EXPECT_EQ(b_to_a.size(), 1ul);
  EXPECT_EQ(a_to_b.at(1), 0ul);
  EXPECT_EQ(b_to_a.at(0), 1ul);
}

TEST_F(InspectorDiffTest, FindLCSMappingDifferentCase) {
  auto a = Vector<String>({"blue", "blue", "green", "red", "blue"});
  auto b = Vector<String>({"red", "blue", "green"});
  InspectorIndexMap a_to_b;
  InspectorIndexMap b_to_a;

  InspectorDiff::FindLCSMapping(a, b, &a_to_b, &b_to_a);
  EXPECT_EQ(b_to_a.size(), 2ul);
  EXPECT_EQ(b_to_a.size(), 2ul);
  EXPECT_EQ(a_to_b.at(1), 1ul);
  EXPECT_EQ(a_to_b.at(2), 2ul);
  EXPECT_EQ(b_to_a.at(1), 1ul);
  EXPECT_EQ(b_to_a.at(2), 2ul);
}

TEST_F(InspectorDiffTest, FindLCSMappingNoElements) {
  auto a = Vector<String>({"a", "b"});
  auto b = Vector<String>({"nota", "notb"});
  InspectorIndexMap a_to_b;
  InspectorIndexMap b_to_a;

  InspectorDiff::FindLCSMapping(a, b, &a_to_b, &b_to_a);

  EXPECT_EQ(a_to_b.size(), 0ul);
  EXPECT_EQ(b_to_a.size(), 0ul);
}

TEST_F(InspectorDiffTest, FindLCSMappingFindsLCSMapping) {
  auto a = Vector<String>({"a", "b", "e", "c", "d", "e"});
  auto b = Vector<String>({"b", "a", "b", "c", "d", "e", "f"});
  InspectorIndexMap a_to_b;
  InspectorIndexMap b_to_a;

  InspectorDiff::FindLCSMapping(a, b, &a_to_b, &b_to_a);

  EXPECT_EQ(a_to_b.size(), 5ul);
  EXPECT_EQ(b_to_a.size(), 5ul);
  EXPECT_EQ(a_to_b.at(0), 1ul);
  EXPECT_EQ(a_to_b.at(1), 2ul);
  EXPECT_FALSE(a_to_b.Contains(2));
  EXPECT_EQ(a_to_b.at(3), 3ul);
  EXPECT_EQ(a_to_b.at(4), 4ul);
  EXPECT_EQ(a_to_b.at(5), 5ul);
  EXPECT_FALSE(b_to_a.Contains(0));
  EXPECT_EQ(b_to_a.at(1), 0ul);
  EXPECT_EQ(b_to_a.at(2), 1ul);
  EXPECT_EQ(b_to_a.at(3), 3ul);
  EXPECT_EQ(b_to_a.at(4), 4ul);
  EXPECT_EQ(b_to_a.at(5), 5ul);
}

}  // namespace blink
