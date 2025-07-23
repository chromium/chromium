// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/equivalence_map.h"

#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "components/zucchini/encoded_view.h"
#include "components/zucchini/image_index.h"
#include "components/zucchini/suffix_array.h"
#include "components/zucchini/targets_affinity.h"
#include "components/zucchini/test_disassembler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

using OffsetDeque = std::deque<offset_t>;

// Make all references 2 bytes long.
constexpr offset_t kReferenceSize = 2;

// Creates and initialize an ImageIndex from |a| and with 2 types of references.
// The result is populated with |refs0| and |refs1|. |a| is expected to be a
// string literal valid for the lifetime of the object.
ImageIndex MakeImageIndexForTesting(const char* a,
                                    std::vector<Reference>&& refs0,
                                    std::vector<Reference>&& refs1) {
  TestDisassembler disasm(
      {kReferenceSize, TypeTag(0), PoolTag(0)}, std::move(refs0),
      {kReferenceSize, TypeTag(1), PoolTag(0)}, std::move(refs1),
      {kReferenceSize, TypeTag(2), PoolTag(1)}, {});

  ImageIndex image_index(
      ConstBufferView(reinterpret_cast<const uint8_t*>(a), std::strlen(a)));

  EXPECT_TRUE(image_index.Initialize(&disasm));
  return image_index;
}

std::vector<TargetsAffinity> MakeTargetsAffinitiesForTesting(
    const ImageIndex& old_image_index,
    const ImageIndex& new_image_index,
    const EquivalenceMap& equivalence_map) {
  std::vector<TargetsAffinity> target_affinities(old_image_index.PoolCount());
  for (const auto& old_pool_tag_and_targets : old_image_index.target_pools()) {
    PoolTag pool_tag = old_pool_tag_and_targets.first;
    target_affinities[pool_tag.value()].InferFromSimilarities(
        equivalence_map, old_pool_tag_and_targets.second.targets(),
        new_image_index.pool(pool_tag).targets());
  }
  return target_affinities;
}

}  // namespace

TEST(EquivalenceMapTest, GetTokenSimilarity) {
  ImageIndex old_index = MakeImageIndexForTesting(
      "ab1122334455", {{2, 0}, {4, 1}, {6, 2}, {8, 2}}, {{10, 3}});
  // Note: {4, 1} -> {6, 3} and {6, 2} -> {4, 1}, then result is sorted.
  ImageIndex new_index = MakeImageIndexForTesting(
      "a11b33224455", {{1, 0}, {4, 1}, {6, 3}, {8, 1}}, {{10, 2}});
  std::vector<TargetsAffinity> affinities = MakeTargetsAffinitiesForTesting(
      old_index, new_index,
      EquivalenceMap({{{0, 0, 1}, 1.0}, {{1, 3, 1}, 1.0}}));

  // Raw match.
  EXPECT_LT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 0, 0));
  // Raw mismatch.
  EXPECT_GT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 0, 1));
  EXPECT_GT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 1, 0));

  // Type mismatch.
  EXPECT_EQ(kMismatchFatal,
            GetTokenSimilarity(old_index, new_index, affinities, 0, 1));
  EXPECT_EQ(kMismatchFatal,
            GetTokenSimilarity(old_index, new_index, affinities, 2, 0));
  EXPECT_EQ(kMismatchFatal,
            GetTokenSimilarity(old_index, new_index, affinities, 2, 10));
  EXPECT_EQ(kMismatchFatal,
            GetTokenSimilarity(old_index, new_index, affinities, 10, 1));

  // Reference strong match.
  EXPECT_LT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 2, 1));
  EXPECT_LT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 4, 6));

  // Reference weak match.
  EXPECT_LT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 6, 4));
  EXPECT_LT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 6, 8));
  EXPECT_LT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 8, 4));

  // Weak match is not greater than strong match.
  EXPECT_LE(GetTokenSimilarity(old_index, new_index, affinities, 6, 4),
            GetTokenSimilarity(old_index, new_index, affinities, 2, 1));

  // Reference mismatch.
  EXPECT_GT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 2, 4));
  EXPECT_GT(0.0, GetTokenSimilarity(old_index, new_index, affinities, 2, 6));
}

TEST(EquivalenceMapTest, GetEquivalenceSimilarity) {
  ImageIndex image_index =
      MakeImageIndexForTesting("abcdef1122", {{6, 0}}, {{8, 1}});
  std::vector<TargetsAffinity> affinities =
      MakeTargetsAffinitiesForTesting(image_index, image_index, {});

  // Sanity check. These are no-op with length-0 equivalences.
  EXPECT_EQ(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {0, 0, 0}));
  EXPECT_EQ(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {0, 3, 0}));
  EXPECT_EQ(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {3, 0, 0}));

  // Now examine larger equivalences.
  EXPECT_LT(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {0, 0, 3}));
  EXPECT_GE(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {0, 3, 3}));
  EXPECT_GE(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {3, 0, 3}));

  EXPECT_LT(0.0, GetEquivalenceSimilarity(image_index, image_index, affinities,
                                          {6, 6, 4}));
}

TEST(EquivalenceMapTest, ExtendEquivalenceForward) {
  auto test_extend_forward =
      [](const ImageIndex old_index, const ImageIndex new_index,
         const EquivalenceCandidate& equivalence, double base_similarity) {
        return ExtendEquivalenceForward(
                   old_index, new_index,
                   MakeTargetsAffinitiesForTesting(old_index, new_index, {}),
                   equivalence, base_similarity)
            .eq;
      };

  EXPECT_EQ(Equivalence({0, 0, 0}),
            test_extend_forward(MakeImageIndexForTesting("", {}, {}),
                                MakeImageIndexForTesting("", {}, {}),
                                {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({0, 0, 0}),
            test_extend_forward(MakeImageIndexForTesting("banana", {}, {}),
                                MakeImageIndexForTesting("zzzz", {}, {}),
                                {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({0, 0, 6}),
            test_extend_forward(MakeImageIndexForTesting("banana", {}, {}),
                                MakeImageIndexForTesting("banana", {}, {}),
                                {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({2, 2, 4}),
            test_extend_forward(MakeImageIndexForTesting("banana", {}, {}),
                                MakeImageIndexForTesting("banana", {}, {}),
                                {{2, 2, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({0, 0, 6}),
            test_extend_forward(MakeImageIndexForTesting("bananaxx", {}, {}),
                                MakeImageIndexForTesting("bananayy", {}, {}),
                                {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(
      Equivalence({0, 0, 8}),
      test_extend_forward(MakeImageIndexForTesting("banana11", {{6, 0}}, {}),
                          MakeImageIndexForTesting("banana11", {{6, 0}}, {}),
                          {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(
      Equivalence({0, 0, 6}),
      test_extend_forward(MakeImageIndexForTesting("banana11", {{6, 0}}, {}),
                          MakeImageIndexForTesting("banana22", {}, {{6, 0}}),
                          {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(
      Equivalence({0, 0, 17}),
      test_extend_forward(MakeImageIndexForTesting("bananaxxpineapple", {}, {}),
                          MakeImageIndexForTesting("bananayypineapple", {}, {}),
                          {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(
      Equivalence({3, 0, 19}),
      test_extend_forward(
          MakeImageIndexForTesting("foobanana11xxpineapplexx", {{9, 0}}, {}),
          MakeImageIndexForTesting("banana11yypineappleyy", {{6, 0}}, {}),
          {{3, 0, 0}, 0.0}, 8.0));
}

TEST(EquivalenceMapTest, ExtendEquivalenceBackward) {
  auto test_extend_backward =
      [](const ImageIndex old_index, const ImageIndex new_index,
         const EquivalenceCandidate& equivalence, double base_similarity) {
        return ExtendEquivalenceBackward(
                   old_index, new_index,
                   MakeTargetsAffinitiesForTesting(old_index, new_index, {}),
                   equivalence, base_similarity)
            .eq;
      };

  EXPECT_EQ(Equivalence({0, 0, 0}),
            test_extend_backward(MakeImageIndexForTesting("", {}, {}),
                                 MakeImageIndexForTesting("", {}, {}),
                                 {{0, 0, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({6, 4, 0}),
            test_extend_backward(MakeImageIndexForTesting("banana", {}, {}),
                                 MakeImageIndexForTesting("zzzz", {}, {}),
                                 {{6, 4, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({0, 0, 6}),
            test_extend_backward(MakeImageIndexForTesting("banana", {}, {}),
                                 MakeImageIndexForTesting("banana", {}, {}),
                                 {{6, 6, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({2, 2, 6}),
            test_extend_backward(MakeImageIndexForTesting("xxbanana", {}, {}),
                                 MakeImageIndexForTesting("yybanana", {}, {}),
                                 {{8, 8, 0}, 0.0}, 8.0));

  EXPECT_EQ(
      Equivalence({0, 0, 8}),
      test_extend_backward(MakeImageIndexForTesting("11banana", {{0, 0}}, {}),
                           MakeImageIndexForTesting("11banana", {{0, 0}}, {}),
                           {{8, 8, 0}, 0.0}, 8.0));

  EXPECT_EQ(
      Equivalence({2, 2, 6}),
      test_extend_backward(MakeImageIndexForTesting("11banana", {{0, 0}}, {}),
                           MakeImageIndexForTesting("22banana", {}, {{0, 0}}),
                           {{8, 8, 0}, 0.0}, 8.0));

  EXPECT_EQ(Equivalence({0, 0, 17}),
            test_extend_backward(
                MakeImageIndexForTesting("bananaxxpineapple", {}, {}),
                MakeImageIndexForTesting("bananayypineapple", {}, {}),
                {{8, 8, 9}, 9.0}, 8.0));

  EXPECT_EQ(
      Equivalence({3, 0, 19}),
      test_extend_backward(
          MakeImageIndexForTesting("foobanana11xxpineapplexx", {{9, 0}}, {}),
          MakeImageIndexForTesting("banana11yypineappleyy", {{6, 0}}, {}),
          {{22, 19, 0}, 0.0}, 8.0));
}

TEST(EquivalenceMapTest, PruneEquivalencesAndSortBySource) {
  auto PruneEquivalencesAndSortBySourceTest =
      [](std::deque<Equivalence>&& equivalences) {
        OffsetMapper::PruneEquivalencesAndSortBySource(&equivalences);
        return std::move(equivalences);
      };

  EXPECT_EQ(std::deque<Equivalence>(),
            PruneEquivalencesAndSortBySourceTest({}));
  EXPECT_EQ(std::deque<Equivalence>({{0, 10, 1}}),
            PruneEquivalencesAndSortBySourceTest({{0, 10, 1}}));
  EXPECT_EQ(std::deque<Equivalence>(),
            PruneEquivalencesAndSortBySourceTest({{0, 10, 0}}));
  EXPECT_EQ(std::deque<Equivalence>({{0, 10, 1}, {1, 11, 1}}),
            PruneEquivalencesAndSortBySourceTest({{0, 10, 1}, {1, 11, 1}}));
  EXPECT_EQ(std::deque<Equivalence>({{0, 10, 2}, {2, 13, 1}}),
            PruneEquivalencesAndSortBySourceTest({{0, 10, 2}, {1, 12, 2}}));
  EXPECT_EQ(std::deque<Equivalence>({{0, 10, 2}}),
            PruneEquivalencesAndSortBySourceTest({{0, 10, 2}, {1, 12, 1}}));
  EXPECT_EQ(std::deque<Equivalence>({{0, 10, 2}, {2, 14, 1}}),
            PruneEquivalencesAndSortBySourceTest({{0, 10, 2}, {1, 13, 2}}));
  EXPECT_EQ(std::deque<Equivalence>({{0, 10, 1}, {1, 12, 3}}),
            PruneEquivalencesAndSortBySourceTest({{0, 10, 2}, {1, 12, 3}}));
  EXPECT_EQ(std::deque<Equivalence>({{0, 10, 3}, {3, 16, 2}}),
            PruneEquivalencesAndSortBySourceTest(
                {{0, 10, 3}, {1, 13, 3}, {3, 16, 2}}));  // Pruning is greedy
  // Test for crbug.com/1432457.
  EXPECT_EQ(std::deque<Equivalence>({{0, 10, +6}, {6, 23, +2}}),
            PruneEquivalencesAndSortBySourceTest(
                {{0, 10, +6}, {3, 20, +5}, {3, 30, +1}}));

  // Consider following pattern that may cause O(n^2) behavior if not handled
  // properly.
  //  ***************
  //            **********
  //             ********
  //              ******
  //               ****
  //                **
  //                 ***************
  // This test case makes sure the function does not stall on a large instance
  // of this pattern.
  EXPECT_EQ(std::deque<Equivalence>({{0, 10, +300000}, {300000, 30, +300000}}),
            PruneEquivalencesAndSortBySourceTest([] {
              std::deque<Equivalence> equivalenses;
              equivalenses.push_back({0, 10, +300000});
              for (offset_t i = 0; i < 100000; ++i)
                equivalenses.push_back({200000 + i, 20, +200000 - 2 * i});
              equivalenses.push_back({300000, 30, +300000});
              return equivalenses;
            }()));

  // Test sorting stability when multiple equivalences share |src_offset|.
  {
    std::vector<Equivalence> sorted_equivalences({
        {0, 10, +2},
        {0, 19, +2},
        {0, 24, +2},
        {0, 26, +2},
    });
    std::vector<size_t> order({0, 1, 2, 3});
    ASSERT_EQ(sorted_equivalences.size(), order.size());
    do {
      std::deque<Equivalence> equivalences;
      for (size_t i : order) {
        equivalences.push_back(sorted_equivalences[i]);
      }
      EXPECT_EQ(std::deque<Equivalence>({{0, 10, +2}}),
                PruneEquivalencesAndSortBySourceTest(std::move(equivalences)));
    } while (std::next_permutation(order.begin(), order.end()));
  }
}

TEST(EquivalenceMapTest, NaiveExtendedForwardProject) {
  constexpr size_t kOldImageSize = 1000U;
  constexpr size_t kNewImageSize = 1000U;
  OffsetMapper offset_mapper(std::deque<Equivalence>(), kOldImageSize,
                             kNewImageSize);

  // Convenience function to declutter.
  auto project = [&offset_mapper](const Equivalence& eq, offset_t offset) {
    return offset_mapper.NaiveExtendedForwardProject(eq, offset);
  };

  // Equivalence with delta = 0.
  Equivalence eq_stay = {10, 10, +5};  // [10,15) -> [10,15).
  for (offset_t offset = 0U; offset < 1000U; ++offset) {
    EXPECT_EQ(offset, project(eq_stay, offset));
  }
  // Saturate since result would overflow "new".
  EXPECT_EQ(999U, project(eq_stay, 1000U));
  EXPECT_EQ(999U, project(eq_stay, 2000U));
  EXPECT_EQ(999U, project(eq_stay, kOffsetBound - 1));

  // Equivalence with delta = -10.
  Equivalence eq_dec = {20, 10, +12};  // [20,32) --> [10,22).
  // Offsets in "old" block.
  EXPECT_EQ(10U, project(eq_dec, 20U));
  EXPECT_EQ(11U, project(eq_dec, 21U));
  EXPECT_EQ(21U, project(eq_dec, 31U));
  // Offsets before "old" block, no underflow
  EXPECT_EQ(9U, project(eq_dec, 19U));
  EXPECT_EQ(1U, project(eq_dec, 11U));
  EXPECT_EQ(0U, project(eq_dec, 10U));
  // Offsets before "old" block, underflow (possible since delta < 0).
  EXPECT_EQ(0U, project(eq_dec, 9U));
  EXPECT_EQ(0U, project(eq_dec, 5U));
  EXPECT_EQ(0U, project(eq_dec, 0U));
  // Offsets after "old" block, no overflow.
  EXPECT_EQ(20U, project(eq_dec, 30U));
  EXPECT_EQ(64U, project(eq_dec, 74U));
  EXPECT_EQ(90U, project(eq_dec, 100U));
  EXPECT_EQ(490U, project(eq_dec, 500U));
  EXPECT_EQ(999U, project(eq_dec, 1009U));
  // Offsets after "old" block, overflow.
  EXPECT_EQ(999U, project(eq_dec, 1010U));
  EXPECT_EQ(999U, project(eq_dec, 2000U));
  EXPECT_EQ(999U, project(eq_dec, kOffsetBound - 1));

  // Equivalence with delta = +10.
  Equivalence eq_inc = {7, 17, +80};  // [7,87) --> [17,97).
  // Offsets in "old" block.
  EXPECT_EQ(17U, project(eq_inc, 7U));
  EXPECT_EQ(60U, project(eq_inc, 50U));
  EXPECT_EQ(96U, project(eq_inc, 86U));
  // Offsets before "old" block, underflow impossible since delta >= 0.
  EXPECT_EQ(16U, project(eq_inc, 6U));
  EXPECT_EQ(10U, project(eq_inc, 0U));
  // Offsets after "old" block, no overflow.
  EXPECT_EQ(97U, project(eq_inc, 87U));
  EXPECT_EQ(510U, project(eq_inc, 500U));
  EXPECT_EQ(999U, project(eq_inc, 989U));
  // Offsets after "old" block, overflow.
  EXPECT_EQ(999U, project(eq_inc, 990U));
  EXPECT_EQ(999U, project(eq_inc, 2000U));
  EXPECT_EQ(999U, project(eq_inc, kOffsetBound - 1));
}

TEST(EquivalenceMapTest, ExtendedForwardProject) {
  // EquivalenceMaps provided must be sorted by "old" offset, and pruned.
  // [0,2) --> [10,12), [2,3) --> [13,14), [4,6) --> [16,18).
  OffsetMapper offset_mapper1({{0, 10, +2}, {2, 13, +1}, {4, 16, +2}}, 20U,
                              25U);
  EXPECT_EQ(10U, offset_mapper1.ExtendedForwardProject(0U));
  EXPECT_EQ(11U, offset_mapper1.ExtendedForwardProject(1U));
  EXPECT_EQ(13U, offset_mapper1.ExtendedForwardProject(2U));
  EXPECT_EQ(14U, offset_mapper1.ExtendedForwardProject(3U));  // Previous equiv.
  EXPECT_EQ(16U, offset_mapper1.ExtendedForwardProject(4U));
  EXPECT_EQ(17U, offset_mapper1.ExtendedForwardProject(5U));
  EXPECT_EQ(18U, offset_mapper1.ExtendedForwardProject(6U));  // Previous equiv.
  // Fake offsets.
  EXPECT_EQ(25U, offset_mapper1.ExtendedForwardProject(20U));
  EXPECT_EQ(26U, offset_mapper1.ExtendedForwardProject(21U));
  EXPECT_EQ(1005U, offset_mapper1.ExtendedForwardProject(1000U));
  EXPECT_EQ(kOffsetBound - 1,
            offset_mapper1.ExtendedForwardProject(kOffsetBound - 1));

  // [0,2) --> [10,12), [13,14) --> [2,3), [16,18) --> [4,6).
  OffsetMapper offset_mapper2({{0, 10, +2}, {13, 2, +1}, {16, 4, +2}}, 25U,
                              20U);
  EXPECT_EQ(10U, offset_mapper2.ExtendedForwardProject(0U));
  EXPECT_EQ(11U, offset_mapper2.ExtendedForwardProject(1U));
  EXPECT_EQ(2U, offset_mapper2.ExtendedForwardProject(13U));
  EXPECT_EQ(3U, offset_mapper2.ExtendedForwardProject(14U));  // Previous equiv.
  EXPECT_EQ(4U, offset_mapper2.ExtendedForwardProject(16U));
  EXPECT_EQ(5U, offset_mapper2.ExtendedForwardProject(17U));
  EXPECT_EQ(6U, offset_mapper2.ExtendedForwardProject(18U));  // Previous equiv.
  // Fake offsets.
  EXPECT_EQ(20U, offset_mapper2.ExtendedForwardProject(25U));
  EXPECT_EQ(21U, offset_mapper2.ExtendedForwardProject(26U));
  EXPECT_EQ(995U, offset_mapper2.ExtendedForwardProject(1000U));
  EXPECT_EQ(kOffsetBound - 1 - 5,
            offset_mapper2.ExtendedForwardProject(kOffsetBound - 1));
}

TEST(EquivalenceMapTest, ExtendedForwardProjectEncoding) {
  // Tests OffsetMapper::ExtendedForwardProject(), which maps every "old" offset
  // to a "new" offset, with possible overlap (even though blocks don't
  // overlap). Not testing real offsets only (no fake offsets).
  // |old_spec| is a string like "<<aaAAaabbBBbcCCc>>":
  // - Upper case letters are covered "old" offsets.
  // - Lower case letters are non-covered offsets that are properly mapped using
  //   nearest "old" block.
  // - '<' denotes underflow (clamped to 0).
  // - '>' denotes overflow (clampled to "new" size - 1).
  // |new_spec| is a string like "aaAA(ab)(ab)BBb..cCCc":
  // - Upper and lower case letters are mapped "new" targets, occurring in the
  //   order that they appear in |old_spec|.
  // - '.' are "new" offsets that appear as output.
  // - '(' and ')' surround a single "new" location that are repeated as output.
  int case_no = 0;
  auto run_test = [&case_no](std::deque<Equivalence>&& equivalences,
                             const std::string& old_spec,
                             const std::string& new_spec) {
    const size_t old_size = old_spec.length();
    // Build expected "new" offsets, queue up for each letter.
    std::map<char, std::deque<offset_t>> expected;
    offset_t cur_new_offset = 0;
    char state = ')';  // ')' = increase offset, '(' = stay.
    for (char ch : new_spec) {
      if (ch == '(' || ch == ')')
        state = ch;
      else
        expected[ch].push_back(cur_new_offset);
      cur_new_offset += (state == ')') ? 1 : 0;
    }
    const size_t new_size = cur_new_offset;
    // Forward-project for each "old" index, pull from queue from matching
    // letter, and compare.
    OffsetMapper offset_mapper(std::move(equivalences), old_size, new_size);
    for (offset_t old_offset = 0; old_offset < old_size; ++old_offset) {
      offset_t new_offset = offset_mapper.ExtendedForwardProject(old_offset);
      char ch = old_spec[old_offset];
      if (ch == '<') {  // Special case: Underflow.
        EXPECT_EQ(0U, new_offset) << "in case " << case_no;
      } else if (ch == '>') {  // Special case: Overflow.
        EXPECT_EQ(static_cast<offset_t>(new_size - 1), new_offset)
            << "in case " << case_no;
      } else {
        std::deque<offset_t>& q = expected[ch];
        ASSERT_FALSE(q.empty());
        EXPECT_EQ(q.front(), new_offset) << "in case " << case_no;
        q.pop_front();
        if (q.empty())
          expected.erase(ch);
      }
    }
    // Clear useless '.', and ensure everything is consumed.
    expected.erase('.');
    EXPECT_TRUE(expected.empty()) << "in case " << case_no;
    ++case_no;
  };

  // Trivial: [5,9) --> [5,9).
  run_test({{5, 5, +4}}, "aaaaaAAAAaaaaa", "aaaaaAAAAaaaaa");
  // Swap: [0,4) --> [6,10), [4,10) --> [0,6).
  run_test({{0, 6, +4}, {4, 0, +6}}, "AAAABBBBBB", "BBBBBBAAAA");
  // Overlap: [0,4) --> [2,6), [4,10) --> [3,9).
  run_test({{0, 2, +4}, {4, 3, +6}}, "AAAABBBBBB", "..A(AB)(AB)(AB)BBB.");
  // Converge: [1,3) --> [2,4), [7,8) --> [6,7).
  run_test({{1, 2, +2}, {7, 6, +1}}, "aAAaabbBbb", ".aAA(ab)(ab)Bbb.");
  // Converge with tie-breaker: [1,3) --> [2,4), [8,9) --> [7,8).
  run_test({{1, 2, +2}, {8, 7, +1}}, "aAAaaabbBb", ".aAAa(ab)(ab)Bb.");
  // Shift left: [6,8) --> [2,4): Underflow occurs.
  run_test({{6, 2, +2}}, "<<<<aaAAaa", "aaAAaa....");
  // Shift right: [2,5) --> [6,9): Overflow occurs.
  run_test({{2, 6, +3}}, "aaAAAa>>>>", "....aaAAAa");
  // Diverge: [3,5) --> [1,3], [7,9) --> [9,11).
  run_test({{3, 1, +2}, {7, 9, +2}}, "<<aAAabBBb>>", "aAAa....bBBb");
  // Pile-up: [0,2) --> [7,9), [9,11) --> [9,11), [18,20) --> [11,13).
  run_test({{0, 7, +2}, {9, 9, +2}, {18, 11, +2}}, "AAaaaabbbBBbbbbcccCC",
           "......b(Ab)(Abc)(Bac)(Bac)(Cab)(Cab)bb.....");
  // Inverse pile-up: [7,9) --> [0,2), [9,11) --> [9,11), [13,15) --> [18,20).
  run_test({{7, 0, +2}, {9, 9, +2}, {11, 18, +2}}, "<<<<<<<AABBCC>>>>>>>",
           "AA.......BB.......CC");
  // Sparse rotate: [3,4) -> [10,11), [10,11) --> [17,18), [17,18) --> [3,4).
  run_test({{3, 10, +1}, {10, 17, +1}, {17, 3, +1}}, "aaaAaaabbbBbbbcccCccc",
           "cccCcccaaaAaaabbbBbbb");
  // Messy swap: [2,4) --> [10,12), [12,16) --> [3,7).
  run_test({{2, 10, +2}, {12, 3, +4}}, "aaAAaa>><bbbBBBBbb",
           "bbbBBBBb(ab)aAAaa");
  // Messy expand: [6,8) --> [3,5), [10,11) -> [11,12), [14,17) --> [16,19).
  run_test({{6, 3, +2}, {10, 11, +1}, {14, 16, +3}}, "<<<aaaAAabBbbcCCCc>>>>>",
           "aaaAAa....bBbb.cCCCc");
  // Interleave: [1,2) --> [0,1), [5,6) --> [10,11), [6,8) --> [3,5),
  //             [11,13) --> [12,14), [14,16) --> [6,8), [17,18) --> [17,18).
  run_test({{1, 0, +1},
            {5, 10, +1},
            {6, 3, +2},
            {11, 12, +2},
            {14, 6, +2},
            {17, 17, +1}},
           "<AaabBCCccdDDdEEeFf>", "AaaCCc(Ec)EebBdDDd..Ff");
}

TEST(EquivalenceMapTest, ForwardProjectAll) {
  auto ForwardProjectAllTest = [](const OffsetMapper& offset_mapper,
                                  std::initializer_list<offset_t> offsets) {
    std::deque<offset_t> offsets_vec(offsets);
    offset_mapper.ForwardProjectAll(&offsets_vec);
    return offsets_vec;
  };

  // [0,2) --> [10,12), [2,3) --> [13,14), [4,6) --> [16,18).
  OffsetMapper offset_mapper1({{0, 10, +2}, {2, 13, +1}, {4, 16, +2}}, 100U,
                              100U);
  EXPECT_EQ(OffsetDeque({10}), ForwardProjectAllTest(offset_mapper1, {0}));
  EXPECT_EQ(OffsetDeque({13}), ForwardProjectAllTest(offset_mapper1, {2}));
  EXPECT_EQ(OffsetDeque({}), ForwardProjectAllTest(offset_mapper1, {3}));
  EXPECT_EQ(OffsetDeque({10, 13}),
            ForwardProjectAllTest(offset_mapper1, {0, 2}));
  EXPECT_EQ(OffsetDeque({11, 13, 17}),
            ForwardProjectAllTest(offset_mapper1, {1, 2, 5}));
  EXPECT_EQ(OffsetDeque({11, 17}),
            ForwardProjectAllTest(offset_mapper1, {1, 3, 5}));
  EXPECT_EQ(OffsetDeque({10, 11, 13, 16, 17}),
            ForwardProjectAllTest(offset_mapper1, {0, 1, 2, 3, 4, 5, 6}));

  // [0,2) --> [10,12), [13,14) --> [2,3), [16,18) --> [4,6).
  OffsetMapper offset_mapper2({{0, 10, +2}, {13, 2, +1}, {16, 4, +2}}, 100U,
                              100U);
  EXPECT_EQ(OffsetDeque({2}), ForwardProjectAllTest(offset_mapper2, {13}));
  EXPECT_EQ(OffsetDeque({10, 2}),
            ForwardProjectAllTest(offset_mapper2, {0, 13}));
  EXPECT_EQ(OffsetDeque({11, 2, 5}),
            ForwardProjectAllTest(offset_mapper2, {1, 13, 17}));
  EXPECT_EQ(OffsetDeque({11, 5}),
            ForwardProjectAllTest(offset_mapper2, {1, 14, 17}));
  EXPECT_EQ(OffsetDeque({10, 11, 2, 4, 5}),
            ForwardProjectAllTest(offset_mapper2, {0, 1, 13, 14, 16, 17, 18}));
}

TEST(EquivalenceMapTest, Build) {
  auto test_build_equivalence = [](const ImageIndex old_index,
                                   const ImageIndex new_index,
                                   double minimum_similarity) {
    auto affinities = MakeTargetsAffinitiesForTesting(old_index, new_index, {});

    EncodedView old_view(old_index);
    EncodedView new_view(new_index);

    for (const auto& old_pool_tag_and_targets : old_index.target_pools()) {
      PoolTag pool_tag = old_pool_tag_and_targets.first;
      std::vector<uint32_t> old_labels;
      std::vector<uint32_t> new_labels;
      size_t label_bound = affinities[pool_tag.value()].AssignLabels(
          1.0, &old_labels, &new_labels);
      old_view.SetLabels(pool_tag, std::move(old_labels), label_bound);
      new_view.SetLabels(pool_tag, std::move(new_labels), label_bound);
    }

    std::vector<offset_t> old_sa =
        MakeSuffixArray<InducedSuffixSort>(old_view, old_view.Cardinality());

    EquivalenceMap equivalence_map;
    equivalence_map.Build(old_sa, old_view, new_view, affinities,
                          minimum_similarity);

    offset_t current_dst_offset = 0;
    offset_t coverage = 0;
    for (const auto& candidate : equivalence_map) {
      EXPECT_GE(candidate.eq.dst_offset, current_dst_offset);
      EXPECT_GT(candidate.eq.length, offset_t(0));
      EXPECT_LE(candidate.eq.src_offset + candidate.eq.length,
                old_index.size());
      EXPECT_LE(candidate.eq.dst_offset + candidate.eq.length,
                new_index.size());
      EXPECT_GE(candidate.similarity, minimum_similarity);
      current_dst_offset = candidate.eq.dst_offset;
      coverage += candidate.eq.length;
    }
    return coverage;
  };

  EXPECT_EQ(0U,
            test_build_equivalence(MakeImageIndexForTesting("", {}, {}),
                                   MakeImageIndexForTesting("", {}, {}), 4.0));

  EXPECT_EQ(0U, test_build_equivalence(
                    MakeImageIndexForTesting("", {}, {}),
                    MakeImageIndexForTesting("banana", {}, {}), 4.0));

  EXPECT_EQ(0U,
            test_build_equivalence(MakeImageIndexForTesting("banana", {}, {}),
                                   MakeImageIndexForTesting("", {}, {}), 4.0));

  EXPECT_EQ(0U, test_build_equivalence(
                    MakeImageIndexForTesting("banana", {}, {}),
                    MakeImageIndexForTesting("zzzz", {}, {}), 4.0));

  EXPECT_EQ(6U, test_build_equivalence(
                    MakeImageIndexForTesting("banana", {}, {}),
                    MakeImageIndexForTesting("banana", {}, {}), 4.0));

  EXPECT_EQ(6U, test_build_equivalence(
                    MakeImageIndexForTesting("bananaxx", {}, {}),
                    MakeImageIndexForTesting("bananayy", {}, {}), 4.0));

  EXPECT_EQ(8U, test_build_equivalence(
                    MakeImageIndexForTesting("banana11", {{6, 0}}, {}),
                    MakeImageIndexForTesting("banana11", {{6, 0}}, {}), 4.0));

  EXPECT_EQ(6U, test_build_equivalence(
                    MakeImageIndexForTesting("banana11", {{6, 0}}, {}),
                    MakeImageIndexForTesting("banana22", {}, {{6, 0}}), 4.0));

  EXPECT_EQ(
      15U,
      test_build_equivalence(
          MakeImageIndexForTesting("banana11pineapple", {{6, 0}}, {}),
          MakeImageIndexForTesting("banana22pineapple", {}, {{6, 0}}), 4.0));

  EXPECT_EQ(
      15U,
      test_build_equivalence(
          MakeImageIndexForTesting("bananaxxxxxxxxpineapple", {}, {}),
          MakeImageIndexForTesting("bananayyyyyyyypineapple", {}, {}), 4.0));

  EXPECT_EQ(
      19U,
      test_build_equivalence(
          MakeImageIndexForTesting("foobanana11xxpineapplexx", {{9, 0}}, {}),
          MakeImageIndexForTesting("banana11yypineappleyy", {{6, 0}}, {}),
          4.0));
}

}  // namespace zucchini
