// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/disassembler_ztf.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include "components/zucchini/buffer_view.h"
#include "components/zucchini/element_detection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

constexpr char kNormalText[] = R"(ZTxt
Hello World!
This is an example of an absolute reference <<1,1>>
And {-01,+05} is an example of a relative ref
txTZ
TRAILING DATA)";
// -1 to exclude null byte.
constexpr size_t kNormalTextExtraBytes = std::size("TRAILING DATA") - 1;

constexpr char kOutOfBoundsText[] = R"(ZTxt<1,1>
Hello World!
This is an example of an OOB absolute reference <890,605>
And {-050,+100} is an example of an OOB relative ref.
but [+00,+10] is valid at least. As is (1,5).
<1, 6> and { ,1} aren't nor is {4,5]
{7,6}<1,1><2,3>{+00,+00}{004,100}[+00,+60][+000,-100]<-000,-035>(-00,-00)txTZ
)";

// Converts a raw string into data.
std::vector<uint8_t> StrToData(std::string_view s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

// Compare if |a.location < b.location| as references have unique locations.
struct ReferenceCompare {
  bool operator()(const Reference& a, const Reference& b) const {
    return a.location < b.location;
  }
};

using ReferenceKey =
    std::pair<DisassemblerZtf::ReferencePool, DisassemblerZtf::ReferenceType>;
using ReferenceSets =
    std::map<ReferenceKey, std::set<Reference, ReferenceCompare>>;

// Write references in |refs_to_write| to |image|. Also validate the
// disassembler parses |image| such that it is of |expected_size|.
void WriteReferences(MutableBufferView image,
                     size_t expected_size,
                     const ReferenceSets& refs_to_write) {
  EXPECT_TRUE(DisassemblerZtf::QuickDetect(image));
  std::unique_ptr<DisassemblerZtf> dis =
      Disassembler::Make<DisassemblerZtf>(image);
  EXPECT_TRUE(dis);
  EXPECT_EQ(expected_size, dis->size());
  image.shrink(dis->size());
  auto reference_groups = dis->MakeReferenceGroups();
  for (const auto& group : reference_groups) {
    auto writer = group.GetWriter(image, dis.get());
    ReferenceKey key = {
        static_cast<DisassemblerZtf::ReferencePool>(group.pool_tag().value()),
        static_cast<DisassemblerZtf::ReferenceType>(group.type_tag().value())};
    if (!refs_to_write.count(key))
      continue;
    for (const auto& ref : refs_to_write.at(key))
      writer->PutNext(ref);
  }
}

// Read references in |refs_to_read| from |image|.  Once found
// the elements are removed from |refs_to_read|. Also validate the
// disassembler parses |image| such that it is of |expected_size|.
void ReadReferences(ConstBufferView image,
                    size_t expected_size,
                    ReferenceSets* refs_to_read) {
  EXPECT_TRUE(DisassemblerZtf::QuickDetect(image));
  std::unique_ptr<DisassemblerZtf> dis =
      Disassembler::Make<DisassemblerZtf>(image);
  EXPECT_TRUE(dis);
  EXPECT_EQ(expected_size, dis->size());
  auto reference_groups = dis->MakeReferenceGroups();
  for (const auto& group : reference_groups) {
    auto reader = group.GetReader(dis.get());
    ReferenceKey key = {
        static_cast<DisassemblerZtf::ReferencePool>(group.pool_tag().value()),
        static_cast<DisassemblerZtf::ReferenceType>(group.type_tag().value())};
    if (!refs_to_read->count(key)) {
      // No elements of this pool/type pair are expected so assert that none are
      // found.
      auto ref = reader->GetNext();
      EXPECT_FALSE(ref.has_value());
      continue;
    }
    // For each reference remove it from the set if it exists, error if
    // unexpected references are found.
    for (auto ref = reader->GetNext(); ref.has_value();
         ref = reader->GetNext()) {
      EXPECT_EQ(1UL, refs_to_read->at(key).erase(ref.value()));
    }
    EXPECT_EQ(0U, refs_to_read->at(key).size());
  }
}

void TestTranslation(const ZtfTranslator& translator,
                     offset_t expected_location,
                     ztf::LineCol lc) {
  // Check the lc is translated to the expected location.
  EXPECT_EQ(expected_location, translator.LineColToOffset(lc));
  auto new_lc = translator.OffsetToLineCol(expected_location);
  if (expected_location == kInvalidOffset) {
    EXPECT_FALSE(translator.IsValid(lc));
    EXPECT_FALSE(new_lc.has_value());
  } else {
    EXPECT_TRUE(translator.IsValid(lc));
    // Check that the reverse is true. |ztf::LineCol{0, 0}| is a sentinel and
    // should never be valid.
    EXPECT_EQ(lc.line, new_lc->line);
    EXPECT_EQ(lc.col, new_lc->col);
  }
}

template <typename T>
size_t CountDistinct(const std::vector<T>& v) {
  return std::set<T>(v.begin(), v.end()).size();
}

}  // namespace

TEST(ZtfTranslatorTest, Translate) {
  ztf::dim_t kMaxVal = INT16_MAX;
  ztf::dim_t kMinVal = INT16_MIN;

  const std::vector<uint8_t> text(StrToData(kOutOfBoundsText));
  ConstBufferView image(text.data(), text.size());
  ZtfTranslator translator;
  EXPECT_TRUE(translator.Init(image));

  // Absolute Translations:

  // Check a bunch of invalid locations.
  TestTranslation(translator, kInvalidOffset, ztf::LineCol{50, 60});
  TestTranslation(translator, kInvalidOffset, ztf::LineCol{0, 0});
  TestTranslation(translator, kInvalidOffset, ztf::LineCol{1, 0});
  TestTranslation(translator, kInvalidOffset, ztf::LineCol{0, 1});
  TestTranslation(translator, kInvalidOffset, ztf::LineCol{0, 1});
  TestTranslation(translator, kInvalidOffset, ztf::LineCol{1, -1});
  TestTranslation(translator, kInvalidOffset, ztf::LineCol{-1, 1});
  TestTranslation(translator, kInvalidOffset, ztf::LineCol{-1, -1});
  TestTranslation(translator, kInvalidOffset, ztf::LineCol{1, kMaxVal});
  TestTranslation(translator, kInvalidOffset, ztf::LineCol{kMaxVal, 1});
  TestTranslation(translator, kInvalidOffset, ztf::LineCol{1, kMinVal});
  TestTranslation(translator, kInvalidOffset, ztf::LineCol{kMinVal, 1});

  // Check the start of the file.
  TestTranslation(translator, 0, ztf::LineCol{1, 1});
  TestTranslation(translator, 1, ztf::LineCol{1, 2});

  // Check the boundary around a newline.
  TestTranslation(translator, 9, ztf::LineCol{1, 10});
  TestTranslation(translator, kInvalidOffset, ztf::LineCol{1, 11});
  TestTranslation(translator, 10, ztf::LineCol{2, 1});
  TestTranslation(translator, kInvalidOffset, ztf::LineCol{2, 0});

  // Check the end of the file.
  TestTranslation(translator, kInvalidOffset, ztf::LineCol{8, 1});
  TestTranslation(translator, kInvalidOffset, ztf::LineCol{7, 79});
  // Need to subtract to account for the newline.
  TestTranslation(translator, text.size() - 1, ztf::LineCol{7, 78});
  TestTranslation(translator, text.size() - 2, ztf::LineCol{7, 77});

  // Delta Validity
  // - Reminder! 0 -> 1:1

  // Common possible edge cases.
  EXPECT_TRUE(translator.IsValid(0, ztf::DeltaLineCol{0, 0}));
  EXPECT_TRUE(translator.IsValid(0, ztf::DeltaLineCol{0, 1}));
  EXPECT_TRUE(translator.IsValid(0, ztf::DeltaLineCol{1, 0}));
  EXPECT_FALSE(translator.IsValid(0, ztf::DeltaLineCol{-1, -1}));
  EXPECT_FALSE(translator.IsValid(0, ztf::DeltaLineCol{-1, 0}));
  EXPECT_FALSE(translator.IsValid(0, ztf::DeltaLineCol{0, -1}));
  EXPECT_FALSE(translator.IsValid(0, ztf::DeltaLineCol{0, -1}));
  EXPECT_FALSE(translator.IsValid(0, ztf::DeltaLineCol{0, kMaxVal}));
  EXPECT_FALSE(translator.IsValid(0, ztf::DeltaLineCol{kMaxVal, 0}));
  EXPECT_FALSE(translator.IsValid(0, ztf::DeltaLineCol{0, kMinVal}));
  EXPECT_FALSE(translator.IsValid(0, ztf::DeltaLineCol{kMinVal, 0}));
  EXPECT_FALSE(translator.IsValid(233, ztf::DeltaLineCol{0, kMaxVal}));
  EXPECT_FALSE(translator.IsValid(233, ztf::DeltaLineCol{kMaxVal, 0}));
  EXPECT_FALSE(translator.IsValid(233, ztf::DeltaLineCol{kMaxVal, kMaxVal}));

  // Newline area.
  EXPECT_TRUE(translator.IsValid(0, ztf::DeltaLineCol{0, 9}));
  EXPECT_FALSE(translator.IsValid(0, ztf::DeltaLineCol{0, 10}));
  EXPECT_FALSE(translator.IsValid(9, ztf::DeltaLineCol{0, 1}));
  EXPECT_FALSE(translator.IsValid(9, ztf::DeltaLineCol{-1, 0}));
  EXPECT_FALSE(translator.IsValid(9, ztf::DeltaLineCol{1, -10}));
  EXPECT_TRUE(translator.IsValid(9, ztf::DeltaLineCol{1, -9}));

  // End of file.
  EXPECT_FALSE(translator.IsValid(0, ztf::DeltaLineCol{7, 78}));
  EXPECT_FALSE(translator.IsValid(0, ztf::DeltaLineCol{7, 77}));
  EXPECT_FALSE(translator.IsValid(0, ztf::DeltaLineCol{6, 78}));
  EXPECT_TRUE(translator.IsValid(0, ztf::DeltaLineCol{6, 77}));
  EXPECT_FALSE(translator.IsValid(text.size() - 1, ztf::DeltaLineCol{0, 1}));
  EXPECT_FALSE(translator.IsValid(text.size() - 1, ztf::DeltaLineCol{1, 0}));
  EXPECT_TRUE(translator.IsValid(text.size() - 2, ztf::DeltaLineCol{0, 1}));
  EXPECT_FALSE(translator.IsValid(text.size() - 2, ztf::DeltaLineCol{1, 0}));
}

// Ensures that ReferenceGroups from DisassemblerZtf::MakeReferenceGroups()
// cover each non-sentinel element in ReferenceType in order, exactly once. Also
// ensures that the ReferenceType elements are grouped by ReferencePool, and
// listed in increasing order.
TEST(DisassemblerZtfTest, ReferenceGroups) {
  std::vector<uint32_t> pool_list;
  std::vector<uint32_t> type_list;
  DisassemblerZtf dis;
  for (ReferenceGroup group : dis.MakeReferenceGroups()) {
    pool_list.push_back(static_cast<uint32_t>(group.pool_tag().value()));
    type_list.push_back(static_cast<uint32_t>(group.type_tag().value()));
  }

  // Check ReferenceByte coverage.
  constexpr size_t kNumTypes = DisassemblerZtf::kNumTypes;
  EXPECT_EQ(kNumTypes, type_list.size());
  EXPECT_EQ(kNumTypes, CountDistinct(type_list));
  EXPECT_TRUE(std::is_sorted(type_list.begin(), type_list.end()));

  // Check that ReferenceType elements are grouped by ReferencePool. Note that
  // repeats can occur, and pools can be skipped.
  EXPECT_TRUE(std::is_sorted(pool_list.begin(), pool_list.end()));
}

TEST(DisassemblerZtfTest, BadMagic) {
  // Test a case where there is no header so a disassembler cannot be created.
  {
    const std::vector<uint8_t> text(StrToData("foobarbaz bazbarfoo"));
    ConstBufferView image(text.data(), text.size());
    EXPECT_FALSE(DisassemblerZtf::QuickDetect(image));
    EXPECT_FALSE(Disassembler::Make<DisassemblerZtf>(image));
  }
  // Test a case where there is no footer so a disassembler cannot be created.
  {
    const std::vector<uint8_t> text(StrToData("ZTxtfoobarbaz bazbarfootxTZ"));
    ConstBufferView image(text.data(), text.size());
    EXPECT_TRUE(DisassemblerZtf::QuickDetect(image));
    EXPECT_FALSE(Disassembler::Make<DisassemblerZtf>(image));
  }
  // Test when the header is too short
  {
    const std::vector<uint8_t> text(StrToData("ZTxtxTZ\n"));
    ConstBufferView image(text.data(), text.size());
    EXPECT_FALSE(DisassemblerZtf::QuickDetect(image));
    EXPECT_FALSE(Disassembler::Make<DisassemblerZtf>(image));
  }
}

TEST(DisassemblerZtfTest, ZtfSizeBound) {
  {
    std::vector<uint8_t> text(StrToData("ZTxt"));
    std::fill_n(std::back_inserter(text), ztf::kMaxDimValue - 2, '\n');
    text.insert(text.end(), {'t', 'x', 'T', 'Z', '\n'});
    ConstBufferView image(text.data(), text.size());
    EXPECT_TRUE(DisassemblerZtf::QuickDetect(image));
    EXPECT_TRUE(Disassembler::Make<DisassemblerZtf>(image));
  }
  {
    std::vector<uint8_t> text(StrToData("ZTxt"));
    std::fill_n(std::back_inserter(text), ztf::kMaxDimValue - 1, '\n');
    text.insert(text.end(), {'t', 'x', 'T', 'Z', '\n'});
    ConstBufferView image(text.data(), text.size());
    EXPECT_TRUE(DisassemblerZtf::QuickDetect(image));
    EXPECT_FALSE(Disassembler::Make<DisassemblerZtf>(image));
  }
}

// Try reading from a well formed source.
TEST(DisassemblerZtfTest, NormalRead) {
  const std::vector<uint8_t> text(StrToData(kNormalText));
  ConstBufferView image(text.data(), text.size());
  ReferenceSets expected_map = {
      {{DisassemblerZtf::kAngles, DisassemblerZtf::kAnglesAbs1},
       {Reference({63, 0})}},
      {{DisassemblerZtf::kBraces, DisassemblerZtf::kBracesRel2},
       {Reference({74, 27})}},
  };
  ReadReferences(image, text.size() - kNormalTextExtraBytes, &expected_map);
}

// Try writing to a well formed source and ensure that what is read back
// reflects what was written.
TEST(DisassemblerZtfTest, NormalWrite) {
  std::vector<uint8_t> mutable_text(StrToData(kNormalText));
  MutableBufferView image(mutable_text.data(), mutable_text.size());
  ReferenceSets change_map = {
      {{DisassemblerZtf::kParentheses, DisassemblerZtf::kParenthesesAbs1},
       {Reference({63, 71})}},
      {{DisassemblerZtf::kBrackets, DisassemblerZtf::kBracketsRel3},
       {Reference({74, 4})}},
  };
  WriteReferences(image, mutable_text.size() - kNormalTextExtraBytes,
                  change_map);

  // As a sanity check see if a disassembler can identify the same references.
  ConstBufferView const_image(image);
  ReadReferences(const_image, mutable_text.size() - kNormalTextExtraBytes,
                 &change_map);
}

// Try reading from a source rife with errors.
TEST(DisassemblerZtfTest, ReadOutOfBoundsRefs) {
  const std::vector<uint8_t> text(StrToData(kOutOfBoundsText));
  ConstBufferView image(text.data(), text.size());
  ReferenceSets expected_map = {
      {{DisassemblerZtf::kAngles, DisassemblerZtf::kAnglesAbs1},
       {Reference({4, 0}), Reference({223, 0}), Reference({228, 12})}},
      {{DisassemblerZtf::kBrackets, DisassemblerZtf::kBracketsRel2},
       {Reference({139, 149})}},
      {{DisassemblerZtf::kBraces, DisassemblerZtf::kBracesAbs1},
       {Reference({218, 223})}},
      {{DisassemblerZtf::kBraces, DisassemblerZtf::kBracesRel2},
       {Reference({233, 233})}},
      {{DisassemblerZtf::kParentheses, DisassemblerZtf::kParenthesesAbs1},
       {Reference({174, 4})}},
  };
  ReadReferences(image, text.size(), &expected_map);
}

// Try writing to a source rife with errors (malformed references or ones that
// reference non-existent locations. Some of the values written are also bad. To
// validate check if the expected set of references are read back.
TEST(DisassemblerZtfTest, WriteOutOfBoundsRefs) {
  // Replace |old_val| (provided for checking) with |new_val| in |set|.
  auto update_set = [](Reference old_ref, Reference new_ref,
                       std::set<Reference, ReferenceCompare>* set) {
    auto it = set->find(old_ref);
    EXPECT_NE(it, set->cend());
    EXPECT_EQ(*it, old_ref);
    set->erase(it);
    set->insert(new_ref);
  };

  // Replace |old_val| (provided for checking) with |new_val| in the set which
  // is the value corresponding to |key| in |map|.
  auto update_map =
      [update_set](
          ReferenceKey key, Reference old_ref, Reference new_ref,
          std::map<ReferenceKey, std::set<Reference, ReferenceCompare>>* map) {
        auto it = map->find(key);
        EXPECT_NE(it, map->cend());
        update_set(old_ref, new_ref, &(it->second));
      };

  std::vector<uint8_t> mutable_text(StrToData(kOutOfBoundsText));
  MutableBufferView image(mutable_text.data(), mutable_text.size());
  ReferenceSets change_map = {
      {{DisassemblerZtf::kAngles, DisassemblerZtf::kAnglesAbs1},
       {Reference({223, 15}), Reference({228, 13})}},
      {{DisassemblerZtf::kAngles, DisassemblerZtf::kAnglesAbs3},
       {Reference({4, 50})}},  // This should fail to write.
      {{DisassemblerZtf::kBrackets, DisassemblerZtf::kBracketsRel2},
       {Reference({139, static_cast<offset_t>(
                            mutable_text.size())})}},  // This should fail.
      {{DisassemblerZtf::kParentheses, DisassemblerZtf::kParenthesesAbs1},
       {Reference({174, 21})}},  // This should fail.
      {{DisassemblerZtf::kBraces, DisassemblerZtf::kBracesAbs1},
       {Reference({218, 219})}},
      {{DisassemblerZtf::kBraces, DisassemblerZtf::kBracesRel2},
       {Reference({233, 174})}},
  };
  WriteReferences(image, mutable_text.size(), change_map);

  // As a sanity check see if a disassembler can identify the same references
  // (excluding the invalid ones).
  change_map.erase(change_map.find(
      {DisassemblerZtf::kAngles, DisassemblerZtf::kAnglesAbs3}));
  change_map.at({DisassemblerZtf::kAngles, DisassemblerZtf::kAnglesAbs1})
      .emplace(Reference{4, 0});
  update_map({DisassemblerZtf::kBrackets, DisassemblerZtf::kBracketsRel2},
             Reference({139, static_cast<offset_t>(mutable_text.size())}),
             Reference({139, 149}), &change_map);
  update_map({DisassemblerZtf::kParentheses, DisassemblerZtf::kParenthesesAbs1},
             Reference({174, 21}), Reference({174, 4}), &change_map);
  ConstBufferView const_image(image);
  ReadReferences(const_image, mutable_text.size(), &change_map);
}

}  // namespace zucchini
