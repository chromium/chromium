// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/named_pages_mapper.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

TEST(NamedPagesMapperTest, Test) {
  NamedPagesMapper mapper;
  EXPECT_EQ(mapper.NamedPageAtIndex(0), AtomicString());
  EXPECT_EQ(mapper.NamedPageAtIndex(100), AtomicString());

  mapper.AddNamedPage("foo", 7);
  EXPECT_EQ(mapper.NamedPageAtIndex(7), "foo");
  EXPECT_EQ(mapper.NamedPageAtIndex(6), AtomicString());
  EXPECT_EQ(mapper.NamedPageAtIndex(100), "foo");

  mapper.AddNamedPage("bar", 8);
  EXPECT_EQ(mapper.NamedPageAtIndex(8), "bar");
  EXPECT_EQ(mapper.NamedPageAtIndex(7), "foo");
  EXPECT_EQ(mapper.NamedPageAtIndex(6), AtomicString());
  EXPECT_EQ(mapper.NamedPageAtIndex(100), "bar");

  mapper.AddNamedPage("foo", 10);
  EXPECT_EQ(mapper.NamedPageAtIndex(10), "foo");
  EXPECT_EQ(mapper.NamedPageAtIndex(9), "bar");
  EXPECT_EQ(mapper.NamedPageAtIndex(8), "bar");
  EXPECT_EQ(mapper.NamedPageAtIndex(7), "foo");
  EXPECT_EQ(mapper.NamedPageAtIndex(6), AtomicString());
  EXPECT_EQ(mapper.NamedPageAtIndex(100), "foo");

  mapper.AddNamedPage(AtomicString(), 11);
  EXPECT_EQ(mapper.NamedPageAtIndex(11), AtomicString());
  EXPECT_EQ(mapper.NamedPageAtIndex(10), "foo");
  EXPECT_EQ(mapper.NamedPageAtIndex(9), "bar");
  EXPECT_EQ(mapper.NamedPageAtIndex(8), "bar");
  EXPECT_EQ(mapper.NamedPageAtIndex(7), "foo");
  EXPECT_EQ(mapper.NamedPageAtIndex(6), AtomicString());
  EXPECT_EQ(mapper.NamedPageAtIndex(100), AtomicString());

  mapper.AddNamedPage("FOO", 13);
  EXPECT_EQ(mapper.NamedPageAtIndex(13), "FOO");
  EXPECT_EQ(mapper.NamedPageAtIndex(12), AtomicString());
  EXPECT_EQ(mapper.NamedPageAtIndex(11), AtomicString());
  EXPECT_EQ(mapper.NamedPageAtIndex(10), "foo");
  EXPECT_EQ(mapper.NamedPageAtIndex(9), "bar");
  EXPECT_EQ(mapper.NamedPageAtIndex(8), "bar");
  EXPECT_EQ(mapper.NamedPageAtIndex(7), "foo");
  EXPECT_EQ(mapper.NamedPageAtIndex(6), AtomicString());
  EXPECT_EQ(mapper.NamedPageAtIndex(100), "FOO");

  // Go back to page 9. This will clear everything after.
  mapper.AddNamedPage("surprise", 9);
  EXPECT_EQ(mapper.NamedPageAtIndex(13), "surprise");
  EXPECT_EQ(mapper.NamedPageAtIndex(12), "surprise");
  EXPECT_EQ(mapper.NamedPageAtIndex(11), "surprise");
  EXPECT_EQ(mapper.NamedPageAtIndex(10), "surprise");
  EXPECT_EQ(mapper.NamedPageAtIndex(9), "surprise");
  EXPECT_EQ(mapper.NamedPageAtIndex(8), "bar");
  EXPECT_EQ(mapper.NamedPageAtIndex(7), "foo");
  EXPECT_EQ(mapper.NamedPageAtIndex(6), AtomicString());
  EXPECT_EQ(mapper.NamedPageAtIndex(100), "surprise");

  mapper.AddNamedPage("lol", 13);
  EXPECT_EQ(mapper.NamedPageAtIndex(13), "lol");
  EXPECT_EQ(mapper.NamedPageAtIndex(12), "surprise");
  EXPECT_EQ(mapper.NamedPageAtIndex(11), "surprise");
  EXPECT_EQ(mapper.NamedPageAtIndex(10), "surprise");
  EXPECT_EQ(mapper.NamedPageAtIndex(9), "surprise");
  EXPECT_EQ(mapper.NamedPageAtIndex(8), "bar");
  EXPECT_EQ(mapper.NamedPageAtIndex(7), "foo");
  EXPECT_EQ(mapper.NamedPageAtIndex(6), AtomicString());
  EXPECT_EQ(mapper.NamedPageAtIndex(100), "lol");

  mapper.AddNamedPage("page2", 2);
  EXPECT_EQ(mapper.NamedPageAtIndex(0), AtomicString());
  EXPECT_EQ(mapper.NamedPageAtIndex(1), AtomicString());
  EXPECT_EQ(mapper.NamedPageAtIndex(2), "page2");
  EXPECT_EQ(mapper.NamedPageAtIndex(100), "page2");

  mapper.AddNamedPage("page1", 1);
  EXPECT_EQ(mapper.NamedPageAtIndex(0), AtomicString());
  EXPECT_EQ(mapper.NamedPageAtIndex(1), "page1");
  EXPECT_EQ(mapper.NamedPageAtIndex(2), "page1");
  EXPECT_EQ(mapper.NamedPageAtIndex(100), "page1");
}

TEST(NamedPagesMapperTest, FirstPageIsNamed) {
  NamedPagesMapper mapper;
  mapper.AddNamedPage("named", 0);
  EXPECT_EQ(mapper.NamedPageAtIndex(0), "named");
  EXPECT_EQ(mapper.NamedPageAtIndex(100), "named");

  mapper.AddNamedPage("overwrite", 0);
  EXPECT_EQ(mapper.NamedPageAtIndex(0), "overwrite");
  EXPECT_EQ(mapper.NamedPageAtIndex(100), "overwrite");

  mapper.AddNamedPage("foo", 1);
  EXPECT_EQ(mapper.NamedPageAtIndex(0), "overwrite");
  EXPECT_EQ(mapper.NamedPageAtIndex(1), "foo");
  EXPECT_EQ(mapper.NamedPageAtIndex(100), "foo");

  mapper.AddNamedPage("xxx", 0);
  EXPECT_EQ(mapper.NamedPageAtIndex(0), "xxx");
  EXPECT_EQ(mapper.NamedPageAtIndex(1), "xxx");
  EXPECT_EQ(mapper.NamedPageAtIndex(100), "xxx");
}

TEST(NamedPagesMapperTest, NameFirstPage) {
  NamedPagesMapper mapper;
  mapper.AddNamedPage("named", 2);
  mapper.AddNamedPage("another", 3);
  EXPECT_EQ(mapper.NamedPageAtIndex(0), AtomicString());
  EXPECT_EQ(mapper.NamedPageAtIndex(1), AtomicString());
  EXPECT_EQ(mapper.NamedPageAtIndex(2), "named");
  EXPECT_EQ(mapper.NamedPageAtIndex(3), "another");
  EXPECT_EQ(mapper.NamedPageAtIndex(100), "another");

  mapper.NameFirstPage("rootname");
  EXPECT_EQ(mapper.NamedPageAtIndex(0), "rootname");
  EXPECT_EQ(mapper.NamedPageAtIndex(1), "rootname");
  EXPECT_EQ(mapper.NamedPageAtIndex(2), "named");
  EXPECT_EQ(mapper.NamedPageAtIndex(3), "another");
  EXPECT_EQ(mapper.NamedPageAtIndex(100), "another");
}

}  // anonymous namespace
}  // namespace blink
