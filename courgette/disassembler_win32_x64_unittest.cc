// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/disassembler_win32_x64.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/ranges/algorithm.h"
#include "courgette/base_test_unittest.h"

class DisassemblerWin32X64Test : public BaseTest {
 public:
  void TestExe() const;
  void TestExe32ShouldFail() const;
  void TestResourceDll() const;
};

void DisassemblerWin32X64Test::TestExe() const {
  std::string file1 = FileContents("chrome64_1.exe");

  std::unique_ptr<courgette::DisassemblerWin32X64> disassembler(
      new courgette::DisassemblerWin32X64(
          reinterpret_cast<const uint8_t*>(file1.c_str()), file1.length()));

  bool can_parse_header = disassembler->ParseHeader();
  EXPECT_TRUE(can_parse_header);

  // The executable is the whole file, not 'embedded' with the file
  EXPECT_EQ(file1.length(), disassembler->length());

  EXPECT_TRUE(disassembler->ok());
  EXPECT_TRUE(disassembler->has_text_section());
  EXPECT_EQ(488448U, disassembler->size_of_code());
  EXPECT_EQ(courgette::DisassemblerWin32X64::SectionName(
      disassembler->RVAToSection(0x00401234 - 0x00400000)),
      std::string(".text"));

  EXPECT_EQ(0U, disassembler->RVAToFileOffset(0));
  EXPECT_EQ(1024U, disassembler->RVAToFileOffset(4096));
  EXPECT_EQ(46928U, disassembler->RVAToFileOffset(50000));

  std::vector<courgette::RVA> relocs;
  bool can_parse_relocs = disassembler->ParseRelocs(&relocs);
  EXPECT_TRUE(can_parse_relocs);
  EXPECT_TRUE(base::ranges::is_sorted(relocs));

  const uint8_t* offset_p = disassembler->FileOffsetToPointer(0);
  EXPECT_EQ(reinterpret_cast<const void*>(file1.c_str()),
            reinterpret_cast<const void*>(offset_p));
  EXPECT_EQ('M', offset_p[0]);
  EXPECT_EQ('Z', offset_p[1]);

  const uint8_t* rva_p = disassembler->RVAToPointer(0);
  EXPECT_EQ(reinterpret_cast<const void*>(file1.c_str()),
            reinterpret_cast<const void*>(rva_p));
  EXPECT_EQ('M', rva_p[0]);
  EXPECT_EQ('Z', rva_p[1]);
}

void DisassemblerWin32X64Test::TestExe32ShouldFail() const {
  std::string file1 = FileContents("setup1.exe");

  std::unique_ptr<courgette::DisassemblerWin32X64> disassembler(
      new courgette::DisassemblerWin32X64(
          reinterpret_cast<const uint8_t*>(file1.c_str()), file1.length()));

  bool can_parse_header = disassembler->ParseHeader();
  EXPECT_FALSE(can_parse_header);

  // The executable is the whole file, not 'embedded' with the file
  EXPECT_EQ(file1.length(), disassembler->length());

  EXPECT_FALSE(disassembler->ok());
}

void DisassemblerWin32X64Test::TestResourceDll() const {
  std::string file1 = FileContents("en-US-64.dll");

  std::unique_ptr<courgette::DisassemblerWin32X64> disassembler(
      new courgette::DisassemblerWin32X64(
          reinterpret_cast<const uint8_t*>(file1.c_str()), file1.length()));

  bool can_parse_header = disassembler->ParseHeader();
  EXPECT_FALSE(can_parse_header);

  // The executable is the whole file, not 'embedded' with the file
  EXPECT_EQ(file1.length(), disassembler->length());

  EXPECT_FALSE(disassembler->ok());
}

TEST_F(DisassemblerWin32X64Test, All) {
  TestExe();
  TestExe32ShouldFail();
  TestResourceDll();
}
