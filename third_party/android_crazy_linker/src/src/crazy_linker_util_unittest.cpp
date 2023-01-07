// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_util.h"

#include <gtest/gtest.h>

#include <utility>

namespace crazy {

TEST(GetBaseNamePtr, Test) {
  const char kString[] = "/tmp/foo";
  EXPECT_EQ(kString + 5, GetBaseNamePtr(kString));
}

TEST(String, Empty) {
  String s;
  EXPECT_TRUE(s.IsEmpty());
  EXPECT_EQ(0u, s.size());
  EXPECT_EQ('\0', s.c_str()[0]);
}

TEST(String, CStringConstructor) {
  String s("Simple");
  EXPECT_STREQ("Simple", s.c_str());
  EXPECT_EQ(6U, s.size());
}

TEST(String, CStringConstructorWithLength) {
  String s2("Simple", 3);
  EXPECT_STREQ("Sim", s2.c_str());
  EXPECT_EQ(3U, s2.size());
}

TEST(String, CopyConstructor) {
  String s1("Simple");
  String s2(s1);

  EXPECT_EQ(6U, s2.size());
  EXPECT_STREQ(s2.c_str(), s1.c_str());
}

TEST(String, MoveConstructor) {
  String s1("Source");
  String s2(std::move(s1));

  EXPECT_TRUE(s1.IsEmpty());
  EXPECT_EQ(6U, s2.size());
  EXPECT_STREQ(s2.c_str(), "Source");
}

TEST(String, CharConstructor) {
  String s('H');
  EXPECT_EQ(1U, s.size());
  EXPECT_STREQ("H", s.c_str());
}

TEST(String, CopyAssign) {
  String s1("Source");
  String s2("Destination");

  s1 = s2;
  EXPECT_STREQ(s1.c_str(), s2.c_str());
}

TEST(String, MoveAssign) {
  String s1("Source");
  String s2("Destination");

  s2 = std::move(s1);

  EXPECT_TRUE(s1.IsEmpty());
  EXPECT_STREQ("Source", s2.c_str());
}

TEST(String, AppendCString) {
  String s("Foo");
  s += "Bar";
  EXPECT_STREQ("FooBar", s.c_str());
  s += "Zoo";
  EXPECT_STREQ("FooBarZoo", s.c_str());
}

TEST(String, AppendOther) {
  String s("Foo");
  String s2("Bar");
  s += s2;
  EXPECT_STREQ("FooBar", s.c_str());
}

TEST(String, ArrayAccess) {
  String s("FooBar");
  EXPECT_EQ('F', s[0]);
  EXPECT_EQ('o', s[1]);
  EXPECT_EQ('o', s[2]);
  EXPECT_EQ('B', s[3]);
  EXPECT_EQ('a', s[4]);
  EXPECT_EQ('r', s[5]);
  EXPECT_EQ('\0', s[6]);
}

TEST(String, EqualityOperators) {
  String a("Foo");
  String b("Bar");

  EXPECT_TRUE(a == String("Foo"));
  EXPECT_TRUE(a == "Foo");

  EXPECT_TRUE(b != String("Foo"));
  EXPECT_TRUE(b != "Foo");

  EXPECT_TRUE(a != b);

  EXPECT_TRUE(b == String("Bar"));
  EXPECT_TRUE(b == "Bar");

  EXPECT_FALSE(a == "Foo ");
  EXPECT_FALSE(b == " Bar");
  EXPECT_FALSE(a == "foo");

  EXPECT_TRUE(a != "Foo ");
  EXPECT_TRUE(b != " Bar");
  EXPECT_TRUE(a != "foo");
}

TEST(String, Resize) {
  String s("A very long string to have fun");
  s.Resize(10);
  EXPECT_EQ(10U, s.size());
  EXPECT_STREQ("A very lon", s.c_str());
}

TEST(String, ResizeToZero) {
  String s("Long string to force a dynamic allocation");
  s.Resize(0);
  EXPECT_EQ(0U, s.size());
  EXPECT_STREQ("", s.c_str());
}

TEST(Vector, IsEmpty) {
  Vector<void*> v;
  EXPECT_TRUE(v.IsEmpty());
}

TEST(Vector, PushBack) {
  Vector<int> v;
  v.PushBack(12345);
  EXPECT_FALSE(v.IsEmpty());
  EXPECT_EQ(1U, v.GetCount());
  EXPECT_EQ(12345, v[0]);
}

TEST(Vector, PushBack2) {
  const int kMaxCount = 500;
  Vector<int> v;
  for (int n = 0; n < kMaxCount; ++n)
    v.PushBack(n * 100);

  EXPECT_FALSE(v.IsEmpty());
  EXPECT_EQ(static_cast<size_t>(kMaxCount), v.GetCount());
}

TEST(Vector, MoveConstructor) {
  const int kMaxCount = 500;
  Vector<int> v1;
  for (int n = 0; n < kMaxCount; ++n)
    v1.PushBack(n * 100);

  Vector<int> v2(std::move(v1));

  EXPECT_TRUE(v1.IsEmpty());
  EXPECT_FALSE(v2.IsEmpty());

  EXPECT_EQ(static_cast<size_t>(kMaxCount), v2.GetCount());
  for (int n = 0; n < kMaxCount; ++n) {
    EXPECT_EQ(n * 100, v2[n]) << "Checking v[" << n << "]";
  }
}

TEST(Vector, MoveAssign) {
  const int kMaxCount = 500;
  Vector<int> v1;
  for (int n = 0; n < kMaxCount; ++n)
    v1.PushBack(n * 100);

  Vector<int> v2;

  v2 = std::move(v1);

  EXPECT_TRUE(v1.IsEmpty());
  EXPECT_FALSE(v2.IsEmpty());

  EXPECT_EQ(static_cast<size_t>(kMaxCount), v2.GetCount());
  for (int n = 0; n < kMaxCount; ++n) {
    EXPECT_EQ(n * 100, v2[n]) << "Checking v[" << n << "]";
  }
}

TEST(Vector, At) {
  const int kMaxCount = 500;
  Vector<int> v;
  for (int n = 0; n < kMaxCount; ++n)
    v.PushBack(n * 100);

  for (int n = 0; n < kMaxCount; ++n) {
    EXPECT_EQ(n * 100, v[n]) << "Checking v[" << n << "]";
  }
}

TEST(Vector, Find) {
  const int kMaxCount = 500;
  Vector<int> v;
  for (int n = 0; n < kMaxCount; ++n)
    v.PushBack(n * 100);

  for (int n = 0; n < kMaxCount; ++n) {
    SearchResult r = v.Find(n * 100);
    EXPECT_TRUE(r.found) << "Looking for " << n * 100;
    EXPECT_EQ(n, r.pos) << "Looking for " << n * 100;
  }
}

TEST(Vector, ForRangeLoop) {
  const int kMaxCount = 500;
  Vector<int> v;
  for (int n = 0; n < kMaxCount; ++n)
    v.PushBack(n * 100);

  int n = 0;
  for (const int& value : v) {
    EXPECT_EQ(n * 100, value) << "Checking v[" << n << "]";
    n++;
  }
}

TEST(Vector, InsertAt) {
  const int kMaxCount = 500;

  for (size_t k = 0; k < kMaxCount; ++k) {
    Vector<int> v;
    for (int n = 0; n < kMaxCount; ++n)
      v.PushBack(n * 100);

    v.InsertAt(k, -1000);

    EXPECT_EQ(kMaxCount + 1, v.GetCount());
    for (int n = 0; n < v.GetCount(); ++n) {
      int expected;
      if (n < k)
        expected = n * 100;
      else if (n == k)
        expected = -1000;
      else
        expected = (n - 1) * 100;
      EXPECT_EQ(expected, v[n]) << "Checking v[" << n << "]";
    }
  }
}

TEST(Vector, RemoveAt) {
  const int kMaxCount = 500;

  for (size_t k = 0; k < kMaxCount; ++k) {
    Vector<int> v;
    for (int n = 0; n < kMaxCount; ++n)
      v.PushBack(n * 100);

    v.RemoveAt(k);

    EXPECT_EQ(kMaxCount - 1, v.GetCount());
    for (int n = 0; n < kMaxCount - 1; ++n) {
      int expected = (n < k) ? (n * 100) : ((n + 1) * 100);
      EXPECT_EQ(expected, v[n]) << "Checking v[" << n << "]";
    }
  }
}

TEST(Vector, PopFirst) {
  const int kMaxCount = 500;
  Vector<int> v;
  for (int n = 0; n < kMaxCount; ++n)
    v.PushBack(n * 100);

  for (int n = 0; n < kMaxCount; ++n) {
    int first = v.PopFirst();
    EXPECT_EQ(n * 100, first) << "Checking " << n << "-th PopFirst()";
    EXPECT_EQ(kMaxCount - 1 - n, v.GetCount())
        << "Checking " << n << "-th PopFirst()";
  }
  EXPECT_EQ(0u, v.GetCount());
  EXPECT_TRUE(v.IsEmpty());
}

}  // namespace crazy
