// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/element_detection.h"

#include <map>
#include <vector>

#include "base/functional/bind.h"
#include "components/zucchini/buffer_view.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  ElementVector TestElementFinder(std::vector<uint8_t> buffer) {
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
            exe_map_, image));
    std::vector<Element> elements;
    for (auto element = finder.GetNext(); element; element = finder.GetNext()) {
      elements.push_back(*element);
    }
    return elements;
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
      }));
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
}

}  // namespace
}  // namespace zucchini
