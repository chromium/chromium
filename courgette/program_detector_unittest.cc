// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/program_detector.h"

#include <string>

#include "courgette/base_test_unittest.h"
#include "courgette/courgette.h"
#include "courgette/disassembler.h"
#include "courgette/disassembler_elf_32_x86.h"
#include "courgette/disassembler_win32_x64.h"
#include "courgette/disassembler_win32_x86.h"

namespace courgette {

namespace {

class ProgramDetectorTest : public BaseTest {
 public:
  void TestQuickDetect(const std::string& test_data,
                       ExecutableType expected_type) const;
  void TestDetectDisassembler(const std::string& test_data,
                              ExecutableType expected_type) const;
};

void ProgramDetectorTest::TestQuickDetect(const std::string& test_data,
                                          ExecutableType expected_type) const {
  // QuickDetect() should return true only for the |expected_type|.
  EXPECT_EQ(expected_type == EXE_WIN_32_X86,
            DisassemblerWin32X86::QuickDetect(
                reinterpret_cast<const uint8_t*>(test_data.data()),
                test_data.size()));
  EXPECT_EQ(expected_type == EXE_WIN_32_X64,
            DisassemblerWin32X64::QuickDetect(
                reinterpret_cast<const uint8_t*>(test_data.data()),
                test_data.size()));
  EXPECT_EQ(expected_type == EXE_ELF_32_X86,
            DisassemblerElf32X86::QuickDetect(
                reinterpret_cast<const uint8_t*>(test_data.data()),
                test_data.size()));
}

void ProgramDetectorTest::TestDetectDisassembler(
    const std::string& test_data,
    ExecutableType expected_type) const {
  ExecutableType detected_type = EXE_UNKNOWN;
  size_t detected_length = 0;
  DetectExecutableType(reinterpret_cast<const uint8_t*>(test_data.data()),
                       test_data.size(), &detected_type, &detected_length);
  EXPECT_EQ(expected_type, detected_type);
  EXPECT_EQ(test_data.size(), detected_length);
}

TEST_F(ProgramDetectorTest, All) {
  std::string win32_x86 = FileContents("setup1.exe");
  std::string win32_x64 = FileContents("chrome64_1.exe");
  std::string elf_32 = FileContents("elf-32-1");

  TestQuickDetect(win32_x86, EXE_WIN_32_X86);
  TestQuickDetect(win32_x64, EXE_WIN_32_X64);
  TestQuickDetect(elf_32, EXE_ELF_32_X86);

  TestDetectDisassembler(win32_x86, EXE_WIN_32_X86);
  TestDetectDisassembler(win32_x64, EXE_WIN_32_X64);
  TestDetectDisassembler(elf_32, EXE_ELF_32_X86);
}

}  // namespace

}  // namespace courgette
