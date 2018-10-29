// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/qpack/qpack_header_table.h"

#include "net/third_party/quic/core/qpack/qpack_static_table.h"
#include "net/third_party/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/spdy/core/hpack/hpack_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quic {
namespace test {
namespace {

class QpackHeaderTableTest : public QuicTest {
 protected:
  QpackHeaderTable table_;
};

TEST_F(QpackHeaderTableTest, LookupEntry) {
  const auto* entry = table_.LookupEntry(0);
  EXPECT_EQ(":authority", entry->name());
  EXPECT_EQ("", entry->value());

  entry = table_.LookupEntry(1);
  EXPECT_EQ(":path", entry->name());
  EXPECT_EQ("/", entry->value());

  // 98 is the last entry.
  entry = table_.LookupEntry(98);
  EXPECT_EQ("x-frame-options", entry->name());
  EXPECT_EQ("sameorigin", entry->value());

  assert(QUIC_ARRAYSIZE(kQpackStaticTable) == 99);
  entry = table_.LookupEntry(99);
  EXPECT_FALSE(entry);
}

TEST_F(QpackHeaderTableTest, FindHeaderField) {
  // A header name that has multiple entries with different values.
  size_t index = 0;
  QpackHeaderTable::MatchType matchtype =
      table_.FindHeaderField(":method", "GET", &index);
  EXPECT_EQ(QpackHeaderTable::MatchType::kNameAndValue, matchtype);
  EXPECT_EQ(17u, index);

  matchtype = table_.FindHeaderField(":method", "POST", &index);
  EXPECT_EQ(QpackHeaderTable::MatchType::kNameAndValue, matchtype);
  EXPECT_EQ(20u, index);

  matchtype = table_.FindHeaderField(":method", "TRACE", &index);
  EXPECT_EQ(QpackHeaderTable::MatchType::kName, matchtype);
  EXPECT_EQ(15u, index);

  // A header name that has a single entry with non-empty value.
  matchtype =
      table_.FindHeaderField("accept-encoding", "gzip, deflate, br", &index);
  EXPECT_EQ(QpackHeaderTable::MatchType::kNameAndValue, matchtype);
  EXPECT_EQ(31u, index);

  matchtype = table_.FindHeaderField("accept-encoding", "compress", &index);
  EXPECT_EQ(QpackHeaderTable::MatchType::kName, matchtype);
  EXPECT_EQ(31u, index);

  matchtype = table_.FindHeaderField("accept-encoding", "", &index);
  EXPECT_EQ(QpackHeaderTable::MatchType::kName, matchtype);
  EXPECT_EQ(31u, index);

  // A header name that has a single entry with empty value.
  matchtype = table_.FindHeaderField("location", "", &index);
  EXPECT_EQ(QpackHeaderTable::MatchType::kNameAndValue, matchtype);
  EXPECT_EQ(12u, index);

  matchtype = table_.FindHeaderField("location", "foo", &index);
  EXPECT_EQ(QpackHeaderTable::MatchType::kName, matchtype);
  EXPECT_EQ(12u, index);

  // No matching header name.
  matchtype = table_.FindHeaderField("foo", "", &index);
  EXPECT_EQ(QpackHeaderTable::MatchType::kNoMatch, matchtype);

  matchtype = table_.FindHeaderField("foo", "bar", &index);
  EXPECT_EQ(QpackHeaderTable::MatchType::kNoMatch, matchtype);
}

}  // namespace
}  // namespace test
}  // namespace quic
