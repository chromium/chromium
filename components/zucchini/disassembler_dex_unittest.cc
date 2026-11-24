// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/disassembler_dex.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <set>
#include <vector>

#include "base/containers/span.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/type_dex.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

template <typename T>
size_t CountDistinct(const std::vector<T>& v) {
  return std::set<T>(v.begin(), v.end()).size();
}

void AppendBytes(std::vector<uint8_t>& out, base::span<const uint8_t> bytes) {
  out.insert(out.end(), bytes.begin(), bytes.end());
}

// Builds a minimal DEX image containing a single code_item.
std::vector<uint8_t> BuildDexWithCodeItem(
    uint32_t insns_size,
    base::span<const uint16_t> insns,
    uint16_t tries_size,
    base::span<const uint8_t> trailing_bytes) {
  std::vector<uint8_t> bytes(sizeof(dex::HeaderItem), 0);

  const uint32_t code_off = static_cast<uint32_t>(bytes.size());
  dex::CodeItem code_item = {};
  code_item.registers_size = 1;
  code_item.tries_size = tries_size;
  code_item.insns_size = insns_size;
  AppendBytes(bytes, base::byte_span_from_ref(code_item));
  AppendBytes(bytes, base::as_bytes(insns));
  AppendBytes(bytes, trailing_bytes);

  while (bytes.size() % 4 != 0) {
    bytes.push_back(0);
  }
  const uint32_t map_off = static_cast<uint32_t>(bytes.size());
  const std::array<dex::MapItem, 3> map_items = {{
      {dex::kTypeHeaderItem, 0, 1, 0},
      {dex::kTypeCodeItem, 0, 1, code_off},
      {dex::kTypeMapList, 0, 1, map_off},
  }};
  const uint32_t map_size = static_cast<uint32_t>(map_items.size());
  AppendBytes(bytes, base::byte_span_from_ref(map_size));
  AppendBytes(bytes, base::as_byte_span(map_items));

  dex::HeaderItem header = {};
  header.magic = {'d', 'e', 'x', '\n', '0', '3', '5', '\0'};
  header.file_size = static_cast<uint32_t>(bytes.size());
  header.header_size = sizeof(dex::HeaderItem);
  header.endian_tag = 0x12345678;
  header.map_off = map_off;
  base::as_writable_byte_span(bytes)
      .first(sizeof(header))
      .copy_from(base::byte_span_from_ref(header));
  return bytes;
}

}  // namespace

// Ensures that ReferenceGroups from DisassemblerDex::MakeReferenceGroups()
// cover each non-sentinel element in ReferenceType in order, exactly once. Also
// ensures that the ReferenceType elements are grouped by ReferencePool, and
// listed in increasing order.
TEST(DisassemblerDexTest, ReferenceGroups) {
  std::vector<uint32_t> pool_list;
  std::vector<uint32_t> type_list;
  DisassemblerDex dis;
  for (ReferenceGroup group : dis.MakeReferenceGroups()) {
    pool_list.push_back(static_cast<uint32_t>(group.pool_tag().value()));
    type_list.push_back(static_cast<uint32_t>(group.type_tag().value()));
  }

  // Check ReferenceByte coverage.
  constexpr size_t kNumTypes = DisassemblerDex::kNumTypes;
  EXPECT_EQ(kNumTypes, type_list.size());
  EXPECT_EQ(kNumTypes, CountDistinct(type_list));
  EXPECT_TRUE(std::is_sorted(type_list.begin(), type_list.end()));

  // Check that ReferenceType elements are grouped by ReferencePool. Note that
  // repeats can occur, and pools can be skipped.
  EXPECT_TRUE(std::is_sorted(pool_list.begin(), pool_list.end()));
}

TEST(DisassemblerDexTest, CodeItemParserInvalidInputsRejected) {
  const std::array<uint16_t, 2> kReturnVoidInsns = {0x000E, 0x0000};

  // Positive baseline: valid code_item with no tries parses cleanly.
  EXPECT_TRUE(Disassembler::Make<DisassemblerDex>(ConstBufferView(
      BuildDexWithCodeItem(kReturnVoidInsns.size(), kReturnVoidInsns, 0, {}))));

  // Positive baseline: valid tries_size = 1 with negative
  // encoded_catch_handler_size (-1 in SLEB128 = 0x7F: 1 handler pair + 1
  // catch_all_addr).
  {
    std::vector<uint8_t> trailing(sizeof(dex::TryItem), 0);
    trailing.insert(trailing.end(), {0x01, 0x7F, 0x00, 0x00, 0x00});
    std::vector<uint8_t> dex = BuildDexWithCodeItem(
        kReturnVoidInsns.size(), kReturnVoidInsns, 1, trailing);
    EXPECT_TRUE(Disassembler::Make<DisassemblerDex>(ConstBufferView(dex)));
  }

  // INT32_MIN encoded_catch_handler_size in SLEB128: {0x80, 0x80, 0x80, 0x80,
  // 0x78}.
  {
    std::vector<uint8_t> trailing(sizeof(dex::TryItem), 0);
    trailing.push_back(1);  // handlers_size = 1 (uleb128)
    trailing.insert(trailing.end(), {0x80, 0x80, 0x80, 0x80, 0x78});
    std::vector<uint8_t> dex = BuildDexWithCodeItem(
        kReturnVoidInsns.size(), kReturnVoidInsns, 1, trailing);
    EXPECT_FALSE(Disassembler::Make<DisassemblerDex>(ConstBufferView(dex)));
  }

  // Overflowing insns_size (0x80000001 * 2 wraps to 2 in 32-bit arithmetic).
  {
    std::vector<uint8_t> dex =
        BuildDexWithCodeItem(0x80000001U, kReturnVoidInsns, 0, {});
    EXPECT_FALSE(Disassembler::Make<DisassemblerDex>(ConstBufferView(dex)));
  }
}

}  // namespace zucchini
