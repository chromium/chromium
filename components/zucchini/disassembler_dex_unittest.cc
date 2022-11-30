// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/disassembler_dex.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <set>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

template <typename T>
size_t CountDistinct(const std::vector<T>& v) {
  return std::set<T>(v.begin(), v.end()).size();
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

}  // namespace zucchini
