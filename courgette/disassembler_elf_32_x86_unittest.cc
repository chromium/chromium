// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/disassembler_elf_32_x86.h"

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "courgette/assembly_program.h"
#include "courgette/base_test_unittest.h"
#include "courgette/image_utils.h"

namespace courgette {

namespace {

class TestDisassemblerElf32X86 : public DisassemblerElf32X86 {
 public:
  TestDisassemblerElf32X86(const uint8_t* start, size_t length)
      : DisassemblerElf32X86(start, length) {}
  ~TestDisassemblerElf32X86() override = default;

  void TestSectionHeaderFileOffsetOrder() {
    std::vector<FileOffset> file_offsets;
    for (Elf32_Half section_id : section_header_file_offset_order_) {
      const Elf32_Shdr* section_header = SectionHeader(section_id);
      file_offsets.push_back(section_header->sh_offset);
    }
    EXPECT_EQ(static_cast<size_t>(SectionHeaderCount()), file_offsets.size());
    EXPECT_TRUE(std::is_sorted(file_offsets.begin(), file_offsets.end()));
  }

  void TestSectionName() {
    std::set<std::string> name_set;
    for (const Elf32_Shdr& section_header : section_header_table_) {
      std::string name;
      EXPECT_TRUE(SectionName(section_header, &name));
      // Ensure |name| is unique and is printable (may be empty though).
      EXPECT_EQ(0U, name_set.count(name));
      EXPECT_TRUE(base::ranges::all_of(name, ::isprint));
      name_set.insert(name);
    }
    // Check for existence of a few common sections.
    EXPECT_EQ(1U, name_set.count(".text"));
    EXPECT_EQ(1U, name_set.count(".data"));
    EXPECT_EQ(1U, name_set.count(".rodata"));
    EXPECT_EQ(1U, name_set.count(".bss"));
    EXPECT_EQ(1U, name_set.count(".shstrtab"));
  }
};

class DisassemblerElf32X86Test : public BaseTest {
 public:
  void TestExe(const char* file_name,
               size_t expected_abs_count,
               size_t expected_rel_count) const;
};

void DisassemblerElf32X86Test::TestExe(const char* file_name,
                                       size_t expected_abs_count,
                                       size_t expected_rel_count) const {
  std::string file1 = FileContents(file_name);

  auto disassembler = std::make_unique<TestDisassemblerElf32X86>(
      reinterpret_cast<const uint8_t*>(file1.c_str()), file1.length());

  bool can_parse_header = disassembler->ParseHeader();
  EXPECT_TRUE(can_parse_header);
  EXPECT_TRUE(disassembler->ok());
  EXPECT_EQ(EXE_ELF_32_X86, disassembler->kind());
  EXPECT_EQ(0U, disassembler->image_base());

  // The length of the disassembled value will be slightly smaller than the
  // real file, since trailing debug info is not included
  EXPECT_EQ(file1.length(), disassembler->length());

  const uint8_t* offset_p = disassembler->FileOffsetToPointer(0);
  EXPECT_EQ(reinterpret_cast<const void*>(file1.c_str()),
            reinterpret_cast<const void*>(offset_p));
  EXPECT_EQ(0x7F, offset_p[0]);
  EXPECT_EQ('E', offset_p[1]);
  EXPECT_EQ('L', offset_p[2]);
  EXPECT_EQ('F', offset_p[3]);

  std::unique_ptr<AssemblyProgram> program = disassembler->CreateProgram(false);
  EXPECT_TRUE(nullptr != program.get());

  const std::vector<RVA>& abs32_list = disassembler->Abs32Locations();

  // Flatten the list typed rel32 to a list of rel32 RVAs.
  std::vector<RVA> rel32_list;
  rel32_list.reserve(disassembler->Rel32Locations().size());
  for (auto& typed_rel32 : disassembler->Rel32Locations())
    rel32_list.push_back(typed_rel32->rva());

  EXPECT_EQ(expected_abs_count, abs32_list.size());
  EXPECT_EQ(expected_rel_count, rel32_list.size());

  EXPECT_TRUE(std::is_sorted(abs32_list.begin(), abs32_list.end()));
  EXPECT_TRUE(std::is_sorted(rel32_list.begin(), rel32_list.end()));

  // Verify that rel32 RVAs do not overlap with abs32 RVAs.
  // TODO(huangs): Fix this to account for RVA's 4-byte width.
  bool found_match = false;
  std::vector<RVA>::const_iterator abs32_it = abs32_list.begin();
  std::vector<RVA>::const_iterator rel32_it = rel32_list.begin();
  while (abs32_it != abs32_list.end() && rel32_it != rel32_list.end()) {
    if (*abs32_it < *rel32_it) {
      ++abs32_it;
    } else if (*abs32_it > *rel32_it) {
      ++rel32_it;
    } else {
      found_match = true;
    }
  }
  EXPECT_FALSE(found_match);

  disassembler->TestSectionHeaderFileOffsetOrder();

  disassembler->TestSectionName();
}

}  // namespace

TEST_F(DisassemblerElf32X86Test, All) {
  TestExe("elf-32-1", 200, 3337);
  TestExe("elf-32-high-bss", 0, 4);
}

}  // namespace courgette
