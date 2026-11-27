// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/element_detection.h"

#include <map>
#include <set>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/buildflags.h"
#include "components/zucchini/disassembler_no_op.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_DEX)
#include "components/zucchini/disassembler_dex.h"
#endif  // BUILDFLAG(ENABLE_DEX)

#if BUILDFLAG(ENABLE_ELF)
#include "components/zucchini/disassembler_elf.h"
#endif  // BUILDFLAG(ENABLE_ELF)

#if BUILDFLAG(ENABLE_WIN)
#include "components/zucchini/disassembler_win32.h"
#endif  // BUILDFLAG(ENABLE_WIN)

#if BUILDFLAG(ENABLE_ZTF)
#include "components/zucchini/disassembler_ztf.h"
#endif  // BUILDFLAG(ENABLE_ZTF)

namespace zucchini {
namespace {
// This test uses a mock archive format where regions are determined by their
// consecutive byte values rather than parsing real executables.
//
// 0 - Padding or raw data (not mapped to an executable).
// 1 - A Win32x86 executable.
// 2 - A Win32x64 executable.
//
// So an example archive file of;
// 0 1 1 1 0 1 1 0 0 2 2 2 2
// contains (in order left to right):
// - One padding byte
// - Three byte Win32x86 executable
// - One padding byte
// - Two byte Win32x86 executable
// - Two padding bytes
// - Four byte Win32x64 executable

class ElementDetectionTest : public ::testing::Test {
 protected:
  using ElementVector = std::vector<Element>;
  using ExeTypeMap = std::map<uint8_t, ExecutableType>;

  ElementDetectionTest()
      : exe_map_({{1, kExeTypeWin32X86}, {2, kExeTypeWin32X64}}) {}

  ElementVector TestElementFinder(std::vector<uint8_t> buffer,
                                  offset_t init_pos) {
    ConstBufferView image(buffer.data(), buffer.size());

    ElementFinder finder(
        image,
        base::BindRepeating(
            [](ExeTypeMap exe_map, ConstBufferView image,
               ConstBufferView region) -> std::optional<Element> {
              EXPECT_GE(region.begin(), image.begin());
              EXPECT_LE(region.end(), image.end());
              EXPECT_GE(region.size(), 0U);

              if (region[0] != 0) {
                offset_t length = 1;
                while (length < region.size() && region[length] == region[0])
                  ++length;
                return Element{{0, length}, exe_map[region[0]]};
              }
              return std::nullopt;
            },
            exe_map_, image),
        init_pos);
    std::vector<Element> elements;
    for (auto element = finder.GetNext(); element; element = finder.GetNext()) {
      elements.push_back(*element);
    }
    return elements;
  }

  ElementVector TestElementFinder(std::vector<uint8_t> buffer) {
    return TestElementFinder(buffer, /* init_pos= */ 0U);
  }

  // Translation map from mock archive bytes to actual types used in Zucchini.
  ExeTypeMap exe_map_;
};

TEST_F(ElementDetectionTest, ElementFinderEmpty) {
  std::vector<uint8_t> buffer(10, 0);
  ElementFinder finder(
      ConstBufferView(buffer.data(), buffer.size()),
      base::BindRepeating([](ConstBufferView image) -> std::optional<Element> {
        return std::nullopt;
      }),
      /* init_pos= */ 0U);
  EXPECT_EQ(std::nullopt, finder.GetNext());
}

TEST_F(ElementDetectionTest, ElementFinder) {
  EXPECT_EQ(ElementVector(), TestElementFinder({}));
  EXPECT_EQ(ElementVector(), TestElementFinder({0, 0}));
  EXPECT_EQ(ElementVector({{{0, 2}, kExeTypeWin32X86}}),
            TestElementFinder({1, 1}));
  EXPECT_EQ(
      ElementVector({{{0, 2}, kExeTypeWin32X86}, {{2, 2}, kExeTypeWin32X64}}),
      TestElementFinder({1, 1, 2, 2}));
  EXPECT_EQ(ElementVector({{{1, 2}, kExeTypeWin32X86}}),
            TestElementFinder({0, 1, 1, 0}));
  EXPECT_EQ(
      ElementVector({{{1, 2}, kExeTypeWin32X86}, {{3, 3}, kExeTypeWin32X64}}),
      TestElementFinder({0, 1, 1, 2, 2, 2}));
  EXPECT_EQ(
      ElementVector({{{1, 2}, kExeTypeWin32X86}, {{4, 3}, kExeTypeWin32X64}}),
      TestElementFinder({0, 1, 1, 0, 2, 2, 2}));
  EXPECT_EQ(ElementVector({{{4, 3}, kExeTypeWin32X64}}),
            TestElementFinder({0, 1, 1, 0, 2, 2, 2}, /* init_pos= */ 3U));
}

TEST(ElementDetectionVersionTest, GetSupportedDisassemblers) {
  const base::span<const DisassemblerVersionInfo> disassemblers =
      GetSupportedDisassemblers();
  ASSERT_FALSE(disassemblers.empty());

  std::set<ExecutableType> seen;
  for (const auto& info : disassemblers) {
    EXPECT_TRUE(seen.insert(info.exe_type).second);
    EXPECT_FALSE(info.name.empty());
    EXPECT_NE(info.version, kInvalidVersion);
  }
  EXPECT_EQ(DisassemblerVersionOfType(kExeTypeUnknown), kInvalidVersion);

  EXPECT_EQ(1U, seen.count(kExeTypeNoOp));
  EXPECT_EQ(DisassemblerNoOp::kVersion,
            DisassemblerVersionOfType(kExeTypeNoOp));
#if BUILDFLAG(ENABLE_WIN)
  EXPECT_EQ(1U, seen.count(kExeTypeWin32X86));
  EXPECT_EQ(1U, seen.count(kExeTypeWin32X64));
  EXPECT_EQ(DisassemblerWin32X86::kVersion,
            DisassemblerVersionOfType(kExeTypeWin32X86));
  EXPECT_EQ(DisassemblerWin32X64::kVersion,
            DisassemblerVersionOfType(kExeTypeWin32X64));
#else
  EXPECT_EQ(0U, seen.count(kExeTypeWin32X86));
  EXPECT_EQ(0U, seen.count(kExeTypeWin32X64));
  EXPECT_EQ(kInvalidVersion, DisassemblerVersionOfType(kExeTypeWin32X86));
  EXPECT_EQ(kInvalidVersion, DisassemblerVersionOfType(kExeTypeWin32X64));
#endif
#if BUILDFLAG(ENABLE_ELF)
  EXPECT_EQ(1U, seen.count(kExeTypeElfX86));
  EXPECT_EQ(1U, seen.count(kExeTypeElfX64));
  EXPECT_EQ(1U, seen.count(kExeTypeElfAArch32));
  EXPECT_EQ(1U, seen.count(kExeTypeElfAArch64));
  EXPECT_EQ(DisassemblerElfX86::kVersion,
            DisassemblerVersionOfType(kExeTypeElfX86));
  EXPECT_EQ(DisassemblerElfX64::kVersion,
            DisassemblerVersionOfType(kExeTypeElfX64));
  EXPECT_EQ(DisassemblerElfAArch32::kVersion,
            DisassemblerVersionOfType(kExeTypeElfAArch32));
  EXPECT_EQ(DisassemblerElfAArch64::kVersion,
            DisassemblerVersionOfType(kExeTypeElfAArch64));
#else
  EXPECT_EQ(0U, seen.count(kExeTypeElfX86));
  EXPECT_EQ(0U, seen.count(kExeTypeElfX64));
  EXPECT_EQ(0U, seen.count(kExeTypeElfAArch32));
  EXPECT_EQ(0U, seen.count(kExeTypeElfAArch64));
  EXPECT_EQ(kInvalidVersion, DisassemblerVersionOfType(kExeTypeElfX86));
  EXPECT_EQ(kInvalidVersion, DisassemblerVersionOfType(kExeTypeElfX64));
  EXPECT_EQ(kInvalidVersion, DisassemblerVersionOfType(kExeTypeElfAArch32));
  EXPECT_EQ(kInvalidVersion, DisassemblerVersionOfType(kExeTypeElfAArch64));
#endif
#if BUILDFLAG(ENABLE_DEX)
  EXPECT_EQ(1U, seen.count(kExeTypeDex));
  EXPECT_EQ(DisassemblerDex::kVersion, DisassemblerVersionOfType(kExeTypeDex));
#else
  EXPECT_EQ(0U, seen.count(kExeTypeDex));
  EXPECT_EQ(kInvalidVersion, DisassemblerVersionOfType(kExeTypeDex));
#endif
#if BUILDFLAG(ENABLE_ZTF)
  EXPECT_EQ(1U, seen.count(kExeTypeZtf));
  EXPECT_EQ(DisassemblerZtf::kVersion, DisassemblerVersionOfType(kExeTypeZtf));
#else
  EXPECT_EQ(0U, seen.count(kExeTypeZtf));
  EXPECT_EQ(kInvalidVersion, DisassemblerVersionOfType(kExeTypeZtf));
#endif
}

}  // namespace
}  // namespace zucchini
