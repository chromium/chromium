// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/statement_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sql {

namespace {

class SqlStatementIdTest : public testing::Test {
 protected:
  // This test takes advantage of the fact that statement IDs are logically
  // (file, line) tuples, and are compared lexicographically.
  SqlStatementIdTest()
      : id1_(SQL_FROM_HERE),   // file statement_id_unittest.cc, line L
        id2_(SQL_FROM_HERE),   // file statement_id_unittest.cc, line L+1
        id3_(SQL_FROM_HERE) {  // file statement_id_unittest.cc, line L+2
  }

  StatementID id1_;
  StatementID id2_;
  StatementID id3_;
};

TEST_F(SqlStatementIdTest, LessThan) {
  EXPECT_FALSE(id1_ < id1_);
  EXPECT_FALSE(id2_ < id2_);
  EXPECT_FALSE(id3_ < id3_);

  EXPECT_LT(id1_, id2_);
  EXPECT_LT(id1_, id3_);
  EXPECT_LT(id2_, id3_);

  EXPECT_FALSE(id2_ < id1_);
  EXPECT_FALSE(id3_ < id1_);
  EXPECT_FALSE(id3_ < id2_);
}

TEST_F(SqlStatementIdTest, CopyConstructor) {
  StatementID id2_copy = id2_;

  EXPECT_FALSE(id2_copy < id2_);
  EXPECT_FALSE(id2_ < id2_copy);

  EXPECT_LT(id1_, id2_copy);
  EXPECT_LT(id2_copy, id3_);
}

TEST_F(SqlStatementIdTest, CopyAssignment) {
  StatementID id2_copy(SQL_FROM_HERE);

  // The new ID should be different from ID2.
  EXPECT_TRUE(id2_copy < id2_ || id2_ < id2_copy);

  id2_copy = id2_;

  EXPECT_FALSE(id2_copy < id2_);
  EXPECT_FALSE(id2_ < id2_copy);

  EXPECT_LT(id1_, id2_copy);
  EXPECT_LT(id2_copy, id3_);
}

}  // namespace

}  // namespace sql