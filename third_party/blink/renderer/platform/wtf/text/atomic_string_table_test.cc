// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/atomic_string_table.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

TEST(WeakResultTest, BasicOperations) {
  AtomicStringTable::WeakResult null;
  EXPECT_TRUE(null.IsNull());

  EXPECT_TRUE(null == AtomicStringTable::WeakResult());

  AtomicString s("astring");
  AtomicStringTable::WeakResult not_null(s.Impl());
  AtomicStringTable::WeakResult not_null2(s.Impl());

  EXPECT_TRUE(not_null == not_null2);
  EXPECT_FALSE(not_null == null);
  EXPECT_FALSE(not_null.IsNull());

  EXPECT_TRUE(not_null == s);
  EXPECT_TRUE(s == not_null);

  String s2(s);
  EXPECT_TRUE(s2 == not_null);
}

}  // namespace WTF
