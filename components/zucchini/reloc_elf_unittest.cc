// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/reloc_elf.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/numerics/safe_conversions.h"
#include "components/zucchini/address_translator.h"
#include "components/zucchini/algorithm.h"
#include "components/zucchini/disassembler_elf.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/test_utils.h"
#include "components/zucchini/type_elf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

template <class Elf_Shdr>
SectionDimensionsElf MakeSectionDimensions(const BufferRegion& region,
                                           offset_t entry_size) {
  using sh_offset_t = decltype(Elf_Shdr::sh_offset);
  using sh_size_t = decltype(Elf_Shdr::sh_size);
  using sh_entsize_t = decltype(Elf_Shdr::sh_entsize);
  return SectionDimensionsElf{Elf_Shdr{
      0,  // sh_name
      0,  // sh_type
      0,  // sh_flags
      0,  // sh_addr
      // sh_offset
      base::checked_cast<sh_offset_t>(region.offset),
      // sh_size
      base::checked_cast<sh_size_t>(region.size),
      0,  // sh_link
      0,  // sh_info
      0,  // sh_addralign
      // sh_entsize
      base::checked_cast<sh_entsize_t>(entry_size),
  }};
}

// Helper to manipulate an image with one or more relocation tables.
template <class ELF_INTEL_TRAITS>
class FakeImageWithReloc {
 public:
  using ElfIntelTraits = ELF_INTEL_TRAITS;
  struct RelocSpec {
    offset_t start;
    std::vector<uint8_t> data;
  };

  FakeImageWithReloc(size_t image_size,
                     rva_t base_rva,
                     const std::vector<RelocSpec>& reloc_specs)
      : image_data_(image_size, 0xFF),
        mutable_image_(&image_data_[0], image_data_.size()) {
    translator_.Initialize({{0, static_cast<offset_t>(image_size), base_rva,
                             static_cast<rva_t>(image_size)}});
    // Set up test image with reloc sections.
    for (const RelocSpec& reloc_spec : reloc_specs) {
      BufferRegion reloc_region = {reloc_spec.start, reloc_spec.data.size()};
      std::ranges::copy(reloc_spec.data,
                        image_data_.begin() + reloc_region.lo());
      section_dimensions_.emplace_back(
          MakeSectionDimensions<typename ElfIntelTraits::Elf_Shdr>(
              reloc_region, ElfIntelTraits::kVAWidth));
      reloc_regions_.push_back(reloc_region);
    }
  }

  std::vector<Reference> ExtractRelocReferences() {
    const size_t image_size = image_data_.size();
    ConstBufferView image = {image_data_.data(), image_size};

    // Make RelocReaderElf.
    auto reader = std::make_unique<RelocReaderElf>(
        image, ElfIntelTraits::kBitness, section_dimensions_,
        ElfIntelTraits::kRelType, 0, image_size, translator_);

    // Read all references and check.
    std::vector<Reference> refs;
    for (std::optional<Reference> ref = reader->GetNext(); ref.has_value();
         ref = reader->GetNext()) {
      refs.push_back(ref.value());
    }
    return refs;
  }

  std::unique_ptr<RelocWriterElf> MakeRelocWriter() {
    return std::move(std::make_unique<RelocWriterElf>(
        mutable_image_, ElfIntelTraits::kBitness, translator_));
  }

  std::vector<uint8_t> GetRawRelocData(int reloc_index) {
    BufferRegion reloc_region = reloc_regions_[reloc_index];
    return Sub(image_data_, reloc_region.lo(), reloc_region.hi());
  }

 private:
  std::vector<uint8_t> image_data_;
  MutableBufferView mutable_image_;
  std::vector<BufferRegion> reloc_regions_;
  std::vector<SectionDimensionsElf> section_dimensions_;
  AddressTranslator translator_;
};

}  // namespace

TEST(RelocElfTest, ResolveOverlaps) {
  // Helper to keep test cases succinct. |sorted_regions| provides regions only,
  // and is used to construct the input list of SectionDimensionsElf.
  // |expected_mask| is a string whose characters corresponding to each element
  // in |sorted_regions|, indicating whether ("1") or not ("0") an element is
  // expected to remain in the final output.
  auto run_test = [](const std::string expected_mask,
                     std::vector<BufferRegion> sorted_regions) {
    // |entsize| doesn't matter; just use constant.
    constexpr size_t ENTSIZE = 4U;

    size_t n = sorted_regions.size();
    CHECK_EQ(n, expected_mask.length());
    std::vector<SectionDimensionsElf> sorted_dims;
    std::vector<SectionDimensionsElf> expected_dims;
    for (size_t i = 0; i < n; ++i) {
      auto region = sorted_regions[i];
      sorted_dims.emplace_back(region.offset, region.size, ENTSIZE);
      if (expected_mask[i] == '1') {
        expected_dims.emplace_back(sorted_dims.back());
      }
    }
    CHECK(std::is_sorted(sorted_dims.begin(), sorted_dims.end()));
    CHECK(std::is_sorted(expected_dims.begin(), expected_dims.end()));
    SectionDimensionsElf::ResolveOverlaps(&sorted_dims);
    EXPECT_EQ(expected_dims, sorted_dims);
  };

  // Trivial cases.
  run_test("", std::vector<BufferRegion>());
  run_test("1", {{0U, +100U}});

  // Zero-sized regions are pathological and should be removed.
  run_test("0", {{100U, +0U}});
  run_test("00", {{100U, +0U}, {100U, +0U}});
  run_test("110", {{100U, +10U}, {110U, +10U}, {110U, +0U}});

  // Two disjoint regions: All kept.
  run_test("11", {{0U, +100U}, {200U, +100U}});

  // Adjacent regions: All kept.
  run_test("111", {{100U, +100U}, {200U, +300U}, {500U, +20U}});

  // Slight overlap: Second region is removed.
  run_test("101", {{101U, +100U}, {200U, +300U}, {500U, +20U}});

  // Exact overlaps: Duplicated regions are removed.
  run_test("1101", {{50U, +20U}, {100U, +100U}, {100U, +100U}, {300U, +40U}});
  run_test("10100", {{1U, +4U}, {1U, +4U}, {7U, +4U}, {7U, +4U}, {7U, +4U}});

  // Overlap with prefix: Covered region is removed.
  run_test("1101", {{50U, +20U}, {100U, +100U}, {100U, +40U}, {300U, +40U}});

  // Overlap with suffix: Covered region is removed.
  run_test("1101", {{50U, +20U}, {100U, +100U}, {170U, +53U}, {300U, +40U}});

  // Complete coverage: Covered region is removed.
  run_test("1101", {{50U, +20U}, {100U, +100U}, {101U, +98U}, {300U, +40U}});

  // Second straddle between first and third: Second region is removed.
  run_test("101", {{100U, +50U}, {120U, +70U}, {150U, +100U}});

  // All three overlap: The first one wins.
  run_test("100", {{100U, +100U}, {120U, +100U}, {150U, +100U}});

  // Small regions shadowed by large one.
  run_test("1000011", {{100U, +100U},
                       {110U, +20U},
                       {130U, +20U},
                       {160U, +20U},
                       {190U, +20U},  // Still overlaps, so removed.
                       {205U, +5U},   // Overlaps with a removed one, so kept.
                       {210U, +20U}});

  // Linear overlap chain: The first (and alternating ones) survive, even though
  // they have smaller total coverage. This is owing to greedy algorithm.
  run_test("101010", {{100U, +10U},
                      {105U, +95U},
                      {200U, +10U},
                      {205U, +95U},
                      {300U, +10U},
                      {305U, +95U}});

  // Intersperse zero-sized regions with one non-zero-sized region.
  run_test("010000", {{90U, +0U},
                      {100U, +10U},
                      {100U, +0U},
                      {104U, +0U},
                      {110U, +0U},
                      {120U, +0U}});
}

TEST(RelocElfTest, ReadWrite32) {
  // Set up mock image: Size = 0x3000, .reloc at 0x600. RVA is 0x40000 + offset.
  constexpr size_t kImageSize = 0x3000;
  constexpr rva_t kBaseRva = 0x40000;

  constexpr offset_t kRelocStart0 = 0x600;
  // "C0 10 04 00 08 00 00 00" represents
  // (r_sym, r_type, r_offset) = (0x000000, 0x08, 0x000410C0).
  // r_type = 0x08 = R_386_RELATIVE, and so |r_offset| is an RVA 0x000410C0.
  // Zucchini does not care about |r_sym|.
  std::vector<uint8_t> reloc_data0 = ParseHexString(
      "C0 10 04 00 08 00 00 00 "   // R_386_RELATIVE.
      "F8 10 04 00 08 AB CD EF "   // R_386_RELATIVE.
      "00 10 04 00 00 AB CD EF "   // R_386_NONE.
      "00 10 04 00 07 AB CD EF");  // R_386_JMP_SLOT.

  constexpr offset_t kRelocStart1 = 0x620;
  std::vector<uint8_t> reloc_data1 = ParseHexString(
      "BC 20 04 00 08 00 00 00 "   // R_386_RELATIVE.
      "A0 20 04 00 08 AB CD EF");  // R_386_RELATIVE.

  FakeImageWithReloc<Elf32IntelTraits> fake_image(
      kImageSize, kBaseRva,
      {{kRelocStart0, reloc_data0}, {kRelocStart1, reloc_data1}});

  // Only R_386_RELATIVE references are extracted. Targets are translated from
  // address (e.g., 0x000420BC) to offset (e.g., 0x20BC).
  std::vector<Reference> exp_refs{
      {0x600, 0x10C0}, {0x608, 0x10F8}, {0x620, 0x20BC}, {0x628, 0x20A0}};
  EXPECT_EQ(exp_refs, fake_image.ExtractRelocReferences());

  // Write reference, extract bytes and check.
  std::unique_ptr<RelocWriterElf> writer = fake_image.MakeRelocWriter();

  writer->PutNext({0x608, 0x1F83});
  std::vector<uint8_t> exp_reloc_data0 = ParseHexString(
      "C0 10 04 00 08 00 00 00 "   // R_386_RELATIVE.
      "83 1F 04 00 08 AB CD EF "   // R_386_RELATIVE (address modified).
      "00 10 04 00 00 AB CD EF "   // R_386_NONE.
      "00 10 04 00 07 AB CD EF");  // R_386_JMP_SLOT.
  EXPECT_EQ(exp_reloc_data0, fake_image.GetRawRelocData(0));

  writer->PutNext({0x628, 0x2950});
  std::vector<uint8_t> exp_reloc_data1 = ParseHexString(
      "BC 20 04 00 08 00 00 00 "   // R_386_RELATIVE.
      "50 29 04 00 08 AB CD EF");  // R_386_RELATIVE (address modified).
  EXPECT_EQ(exp_reloc_data1, fake_image.GetRawRelocData(1));
}

TEST(RelocElfTest, Limit32) {
  constexpr size_t kImageSize = 0x3000;
  constexpr offset_t kBaseRva = 0x40000;
  constexpr offset_t kRelocStart = 0x600;
  // All R_386_RELATIVE.
  std::vector<uint8_t> reloc_data = ParseHexString(
      // Strictly within file.
      "00 00 04 00 08 00 00 00 "
      "00 10 04 00 08 00 00 00 "
      "F0 2F 04 00 08 00 00 00 "
      "F8 2F 04 00 08 00 00 00 "
      "FC 2F 04 00 08 00 00 00 "
      // Straddles end of file.
      "FD 2F 04 00 08 00 00 00 "
      "FE 2F 04 00 08 00 00 00 "
      "FF 2F 04 00 08 00 00 00 "
      // Beyond end of file.
      "00 30 04 00 08 00 00 00 "
      "01 30 04 00 08 00 00 00 "
      "FC FF FF 7F 08 00 00 00 "
      "FE FF FF 7F 08 00 00 00 "
      "00 00 00 80 08 00 00 00 "
      "FC FF FF FF 08 00 00 00 "
      "FF FF FF FF 08 00 00 00 "
      // Another good reference.
      "34 12 04 00 08 00 00 00");

  FakeImageWithReloc<Elf32IntelTraits> fake_image(kImageSize, kBaseRva,
                                                  {{kRelocStart, reloc_data}});

  std::vector<Reference> exp_refs{{0x600, 0x0000}, {0x608, 0x1000},
                                  {0x610, 0x2FF0}, {0x618, 0x2FF8},
                                  {0x620, 0x2FFC}, {0x678, 0x1234}};
  EXPECT_EQ(exp_refs, fake_image.ExtractRelocReferences());
}

TEST(RelocElfTest, Limit64) {
  constexpr size_t kImageSize = 0x3000;
  constexpr offset_t kBaseRva = 0x40000;

  constexpr offset_t kRelocStart = 0x600;
  // All R_X86_64_RELATIVE.
  std::vector<uint8_t> reloc_data = ParseHexString(
      // Strictly within file.
      "00 00 04 00 00 00 00 00 08 00 00 00 00 00 00 00 "
      "00 10 04 00 00 00 00 00 08 00 00 00 00 00 00 00 "
      "F0 2F 04 00 00 00 00 00 08 00 00 00 00 00 00 00 "
      "F4 2F 04 00 00 00 00 00 08 00 00 00 00 00 00 00 "
      "F8 2F 04 00 00 00 00 00 08 00 00 00 00 00 00 00 "
      // Straddles end of file.
      "F9 2F 04 00 00 00 00 00 08 00 00 00 00 00 00 00 "
      "FC 2F 04 00 00 00 00 00 08 00 00 00 00 00 00 00 "
      "FF 2F 04 00 00 00 00 00 08 00 00 00 00 00 00 00 "
      // Beyond end of file.
      "00 30 04 00 00 00 00 00 08 00 00 00 00 00 00 00 "
      "01 30 04 00 00 00 00 00 08 00 00 00 00 00 00 00 "
      "FC FF FF 7F 00 00 00 00 08 00 00 00 00 00 00 00 "
      "FE FF FF 7F 00 00 00 00 08 00 00 00 00 00 00 00 "
      "00 00 00 80 00 00 00 00 08 00 00 00 00 00 00 00 "
      "FC FF FF FF 00 00 00 00 08 00 00 00 00 00 00 00 "
      "FF FF FF FF 00 00 00 00 08 00 00 00 00 00 00 00 "
      "00 00 04 00 01 00 00 00 08 00 00 00 00 00 00 00 "
      "FF FF FF FF FF FF FF FF 08 00 00 00 00 00 00 00 "
      "F8 FF FF FF FF FF FF FF 08 00 00 00 00 00 00 00 "
      // Another good reference.
      "34 12 04 00 00 00 00 00 08 00 00 00 00 00 00 00");

  FakeImageWithReloc<Elf64IntelTraits> fake_image(kImageSize, kBaseRva,
                                                  {{kRelocStart, reloc_data}});

  std::vector<Reference> exp_refs{{0x600, 0x0000}, {0x610, 0x1000},
                                  {0x620, 0x2FF0}, {0x630, 0x2FF4},
                                  {0x640, 0x2FF8}, {0x720, 0x1234}};
  EXPECT_EQ(exp_refs, fake_image.ExtractRelocReferences());
}

}  // namespace zucchini
