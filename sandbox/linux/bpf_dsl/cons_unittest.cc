// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/bpf_dsl/cons.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace {

std::string Join(cons::List<char> char_list) {
  std::string res;
  for (const char& ch : char_list) {
    res.push_back(ch);
  }
  return res;
}

TEST(ConsListTest, Basic) {
  cons::List<char> ba = Cons('b', Cons('a', cons::List<char>()));
  EXPECT_EQ("ba", Join(ba));

  cons::List<char> cba = Cons('c', ba);
  cons::List<char> dba = Cons('d', ba);
  EXPECT_EQ("cba", Join(cba));
  EXPECT_EQ("dba", Join(dba));
}

}  // namespace
}  // namespace sandbox
