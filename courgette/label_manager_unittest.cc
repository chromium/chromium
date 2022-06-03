// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/label_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/notreached.h"
#include "courgette/image_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace courgette {

namespace {

class TestLabelManager : public LabelManager {
 public:
  void SetLabels(const LabelVector& labels) { labels_ = labels; }
};

void CheckLabelManagerContent(LabelManager* label_manager,
                              const std::map<RVA, int32_t>& expected) {
  EXPECT_EQ(expected.size(), label_manager->Labels().size());
  for (const auto& rva_and_count : expected) {
    Label* label = label_manager->Find(rva_and_count.first);
    EXPECT_TRUE(label != nullptr);
    EXPECT_EQ(rva_and_count.first, label->rva_);
    EXPECT_EQ(rva_and_count.second, label->count_);
  }
}

// Instantiates a LabelVector with |n| instances. The |rva_| fields are assigned
// 0, ..., |n| - 1. The other fields are uninitialized.
LabelVector CreateLabelVectorBasic(size_t n) {
  LabelVector labels;
  labels.reserve(n);
  for (size_t i = 0; i < n; ++i)
    labels.push_back(Label(i));
  return labels;
}

// Instantiates a list of Labels, one per character |ch| in |encoded_index|.
// - |rva_| is assigned 0, 1, 2, ...
// - |count_| is assigned 1.
// - |index_| depends on |ch|: '.' => kNoIndex, 'A' => 0, ..., 'Z' => 25.
// Each letter (except '.') can appear at most once in |encoded_index|.
// For example, |encoded_index| = "A.E" initializes 3 Labels:
//   [{rva_: 0, count_: 1, index_: 0},
//    {rva_: 1, count_: 1, index_: kNoIndex},
//    {rva_: 2, count_: 1, index_: 4}].
LabelVector CreateLabelVectorWithIndexes(const std::string& encoded_index) {
  LabelVector labels;
  size_t n = encoded_index.size();
  labels.reserve(n);
  std::set<char> used_ch;
  for (size_t i = 0; i < n; ++i) {
    int index = Label::kNoIndex;
    char ch = encoded_index[i];
    if (ch != '.') {
      // Sanity check for test case.
      if (ch < 'A' || ch > 'Z' || used_ch.find(ch) != used_ch.end())
        NOTREACHED() << "Malformed test case: " << encoded_index;
      used_ch.insert(ch);
      index = ch - 'A';
    }
    labels.push_back(Label(i, index, 1));
  }
  return labels;
}

// Returns a string encoding for |index_| assignments for |label_| instances,
// with kNoIndex => '.', 0 => 'A', ..., '25' => 'Z'. Fails if any |index_|
// does not fit the above.
std::string EncodeLabelIndexes(const LabelVector& labels) {
  std::string encoded;
  encoded.reserve(labels.size());
  for (const Label& label : labels) {
    if (label.index_ == Label::kNoIndex)
      encoded += '.';
    else if (label.index_ >= 0 && label.index_ <= 'Z' - 'A')
      encoded += static_cast<char>(label.index_ + 'A');
    else
      NOTREACHED();
  }
  return encoded;
}

}  // namespace

TEST(LabelManagerTest, GetLabelIndexBound) {
  LabelVector labels0;
  EXPECT_EQ(0, LabelManager::GetLabelIndexBound(labels0));

  LabelVector labels1_uninit = CreateLabelVectorBasic(1);
  ASSERT_EQ(1U, labels1_uninit.size());
  EXPECT_EQ(0, LabelManager::GetLabelIndexBound(labels1_uninit));

  LabelVector labels1_init = CreateLabelVectorBasic(1);
  ASSERT_EQ(1U, labels1_init.size());
  labels1_init[0].index_ = 99;
  EXPECT_EQ(100, LabelManager::GetLabelIndexBound(labels1_init));

  LabelVector labels6_mixed = CreateLabelVectorBasic(6);
  ASSERT_EQ(6U, labels6_mixed.size());
  labels6_mixed[1].index_ = 5;
  labels6_mixed[2].index_ = 2;
  labels6_mixed[4].index_ = 15;
  labels6_mixed[5].index_ = 7;
  EXPECT_EQ(16, LabelManager::GetLabelIndexBound(labels6_mixed));
}

TEST(LabelManagerTest, Basic) {
  static const RVA kTestTargetsRaw[] = {
    0x04000010,
    0x04000030,
    0x04000020,
    0x04000010,  // Redundant.
    0xFEEDF00D,
    0x04000030,  // Redundant.
    0xFEEDF00D,  // Redundant.
    0x00000110,
    0x04000010,  // Redundant.
    0xABCD1234
  };
  std::vector<RVA> test_targets(std::begin(kTestTargetsRaw),
                                std::end(kTestTargetsRaw));
  TrivialRvaVisitor visitor(test_targets);

  // Preallocate targets, then populate.
  TestLabelManager label_manager;
  label_manager.Read(&visitor);

  static const std::pair<RVA, int32_t> kExpected1Raw[] = {
      {0x00000110, 1}, {0x04000010, 3}, {0x04000020, 1},
      {0x04000030, 2}, {0xABCD1234, 1}, {0xFEEDF00D, 2}};
  std::map<RVA, int32_t> expected1(std::begin(kExpected1Raw),
                                   std::end(kExpected1Raw));

  CheckLabelManagerContent(&label_manager, expected1);

  // Expect to *not* find labels for various RVAs that were never added.
  EXPECT_EQ(nullptr, label_manager.Find(RVA(0x00000000)));
  EXPECT_EQ(nullptr, label_manager.Find(RVA(0x0400000F)));
  EXPECT_EQ(nullptr, label_manager.Find(RVA(0x04000011)));
  EXPECT_EQ(nullptr, label_manager.Find(RVA(0x5F3759DF)));
  EXPECT_EQ(nullptr, label_manager.Find(RVA(0xFEEDFFF0)));
  EXPECT_EQ(nullptr, label_manager.Find(RVA(0xFFFFFFFF)));
}

TEST(LabelManagerTest, Single) {
  const RVA kRva = 12U;
  for (int dup = 1; dup < 8; ++dup) {
    // Test data: |dup| copies of kRva.
    std::vector<RVA> test_targets(dup, kRva);
    TrivialRvaVisitor visitor(test_targets);
    TestLabelManager label_manager;
    label_manager.Read(&visitor);
    EXPECT_EQ(1U, label_manager.Labels().size());  // Deduped to 1 Label.

    Label* label = label_manager.Find(kRva);
    EXPECT_NE(nullptr, label);
    EXPECT_EQ(kRva, label->rva_);
    EXPECT_EQ(dup, label->count_);

    for (RVA rva = 0U; rva < 16U; ++rva) {
      if (rva != kRva)
        EXPECT_EQ(nullptr, label_manager.Find(rva));
    }
  }
}

TEST(LabelManagerTest, Empty) {
  std::vector<RVA> empty_test_targets;
  TrivialRvaVisitor visitor(empty_test_targets);
  TestLabelManager label_manager;
  label_manager.Read(&visitor);
  EXPECT_EQ(0U, label_manager.Labels().size());
  for (RVA rva = 0U; rva < 16U; ++rva)
    EXPECT_EQ(nullptr, label_manager.Find(rva));
}

TEST(LabelManagerTest, EmptyAssign) {
  TestLabelManager label_manager_empty;
  label_manager_empty.DefaultAssignIndexes();
  label_manager_empty.UnassignIndexes();
  label_manager_empty.AssignRemainingIndexes();
}

TEST(LabelManagerTest, TrivialAssign) {
  for (size_t size = 0; size < 20; ++size) {
    TestLabelManager label_manager;
    label_manager.SetLabels(CreateLabelVectorBasic(size));

    // Sanity check.
    for (size_t i = 0; i < size; ++i)
      EXPECT_EQ(Label::kNoIndex, label_manager.Labels()[i].index_);

    // Default assign.
    label_manager.DefaultAssignIndexes();
    for (size_t i = 0; i < size; ++i)
      EXPECT_EQ(static_cast<int>(i), label_manager.Labels()[i].index_);

    // Heuristic assign, but since everything's assigned, so no change.
    label_manager.AssignRemainingIndexes();
    for (size_t i = 0; i < size; ++i)
      EXPECT_EQ(static_cast<int>(i), label_manager.Labels()[i].index_);

    // Unassign.
    label_manager.UnassignIndexes();
    for (size_t i = 0; i < size; ++i)
      EXPECT_EQ(Label::kNoIndex, label_manager.Labels()[i].index_);
  }
}

// Tests SimpleIndexAssigner fill strategies independently.
TEST(LabelManagerTest, SimpleIndexAssigner) {
  using SimpleIndexAssigner = LabelManager::SimpleIndexAssigner;
  // See CreateLabelVectorWithIndexes() explanation on how we encode LabelVector
  // |index_| values as a string.
  const struct TestCase {
    const char* input;
    const char* expect_forward;
    const char* expect_backward;
    const char* expect_in;
  } kTestCases[] = {
    {"", "", "", ""},
    {".", "A", "A", "A"},
    {"....", "ABCD", "ABCD", "ABCD"},
    {"A...", "ABCD", "ABCD", "ABCD"},
    {".A..", ".ABC", ".ACD", "BACD"},
    {"...A", "...A", "...A", "BCDA"},
    {"C...", "CD.A", "C..D", "CABD"},
    {".C..", "ACD.", "BC.D", "ACBD"},
    {"...C", "AB.C", ".ABC", "ABDC"},
    {"D...", "D.AB", "D...", "DABC"},
    {".D..", "AD..", "CD..", "ADBC"},
    {"...D", "ABCD", "ABCD", "ABCD"},
    {"Z...", "Z.AB", "Z...", "ZABC"},
    {".Z..", "AZ..", "YZ..", "AZBC"},
    {"...Z", "ABCZ", "WXYZ", "ABCZ"},
    {"..AZ..", "..AZ..", "..AZ..", "BCAZDE"},
    {"..ZA..", "..ZABC", "XYZA..", "BCZADE"},
    {"A....Z", "ABCDEZ", "AVWXYZ", "ABCDEZ"},
    {"Z....A", "Z....A", "Z....A", "ZBCDEA"},
    {"..CD..", "ABCDEF", "ABCDEF", "ABCDEF"},
    {"..DC..", "ABDC..", "..DCEF", "ABDCEF"},
    {"..MN..", "ABMN..", "KLMN..", "ABMNCD"},
    {"..NM..", "ABNM..", "..NM..", "ABNMCD"},
    {".B.D.F.", "ABCDEFG", "ABCDEFG", "ABCDEFG"},
    {".D.G.J.", "ADEGHJ.", "CDFGIJ.", "ADBGCJE"},
    {".D.Z.J.", "ADEZ.JK", "CDYZIJ.", "ADBZCJE"},
    {".B..E..", "ABCDEFG", "ABCDEFG", "ABCDEFG"},
    {".B..D..", "ABC.DEF", "AB.CDFG", "ABCEDFG"},
  };
  const int kNumFuns = 3;
  // TestCase member variable pointers to enable iteration.
  const char* TestCase::*expect_ptr[kNumFuns] = {
    &TestCase::expect_forward,
    &TestCase::expect_backward,
    &TestCase::expect_in
  };
  // SimpleIndexAssigner member function pointers to enable iteration.
  void (SimpleIndexAssigner::*fun_ptrs[kNumFuns])() = {
    &SimpleIndexAssigner::DoForwardFill,
    &SimpleIndexAssigner::DoBackwardFill,
    &SimpleIndexAssigner::DoInFill
  };
  for (const auto& test_case : kTestCases) {
    // Loop over {forward fill, backward fill, infill}.
    for (int i = 0; i < kNumFuns; ++i) {
      std::string expect = test_case.*(expect_ptr[i]);
      LabelVector labels = CreateLabelVectorWithIndexes(test_case.input);
      SimpleIndexAssigner assigner(&labels);
      (assigner.*(fun_ptrs[i]))();
      std::string result = EncodeLabelIndexes(labels);
      EXPECT_EQ(expect, result);
    }
  }
}

// Tests integrated AssignRemainingIndexes().
TEST(LabelManagerTest, AssignRemainingIndexes) {
  const struct {
    const char* input;
    const char* expect;
  } kTestCases[] = {
    // Trivial.
    {"", ""},
    {"M", "M"},
    {"ABCDEFG", "ABCDEFG"},
    {"THEQUICKBROWNFXJMPSVLAZYDG", "THEQUICKBROWNFXJMPSVLAZYDG"},
    // Forward fill only.
    {".", "A"},
    {".......", "ABCDEFG"},
    {"....E..", "ABCDEFG"},  // "E" is at right place.
    {".D.B.H.F.", "ADEBCHIFG"},
    {"ZN....", "ZNOPQR"},  // "Z" exists, so 'OPQR" are defined.
    {"H.D...A..", "HIDEFGABC"},
    {"...K.DE..H..Z", "ABCKLDEFGHIJZ"},  // "Z" exists, so "L" defined.
    // Infill only.
    {"E.", "EA"},
    {"...A", "BCDA"},
    {"Z...A", "ZBCDA"},
    {"AN...", "ANBCD"},
    {"...AZ", "BCDAZ"},
    {"....AC", "BDEFAC"},
    {"ED...C...B....A", "EDFGHCIJKBLMNOA"},
    // Forward fill and infill.
    {"E..", "EBA"},          // Forward: "A"; in: "B".
    {"Z....", "ZDABC"},      // Forward: "ABC"; in: "D".
    {".E.....", "AEFGBCD"},  // Forward: "A", "FG"; in: "BCD".
    {"....C..", "ABFGCDE"},  // Forward: "AB", "DE"; in: "FG".
    {"...Z...", "ABCZDEF"},  // Forward: "ABC"; in: "DEF".
    {"...A...", "EFGABCD"},  // Forward: "BCD"; in: "EFG".
    // Backward fill only.
    {".CA", "BCA"},
    {"...ZA", "WXYZA"},
    {"BA...Z", "BAWXYZ"},
    {"ANM..Z....L...T", "ANMXYZHIJKLQRST"},
    {"....G..Z...LAH", "CDEFGXYZIJKLAH"},
    // Forward fill and backward fill.
    {"..ZA..", "XYZABC"},    // Forward: "BC"; backward: "XY".
    {".....ZD", "ABCXYZD"},  // Forward: "ABC"; backward: "XY".
    {"DA.....", "DABCEFG"},  // Forward: "BC"; backward: "EFG".
    // Backward fill and infill.
    {"G....DA", "GEFBCDA"},  // Backward: "BC"; in: "EF".
    {"..ZBA..", "XYZBACD"},  // Backward: "XY"; in: "CD".
    // All.
    {".....ZED.", "ABCXYZEDF"},  // Forward: "ABC"; backward: "XY"; in: "F".
    {".....GD.", "ABCHFGDE"},  // Forward: "ABC", "E"; backward: "F"; in: "H".
    {"..FE..GD..", "ABFECHGDIJ"},  // Forward: "AB"; backward: "IJ"; in: "CH".
  };
  for (const auto& test_case : kTestCases) {
    TestLabelManager label_manager;
    label_manager.SetLabels(CreateLabelVectorWithIndexes(test_case.input));

    label_manager.AssignRemainingIndexes();
    std::string result = EncodeLabelIndexes(label_manager.Labels());
    EXPECT_EQ(test_case.expect, result);
  }
}

}  // namespace courgette
