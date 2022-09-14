// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>

#include "gtest/gtest.h"

#include "nacl_io/kernel_proxy.h"
#include "nacl_io/path.h"

using namespace nacl_io;

TEST(PathTest, Empty) {
  Path p;
  EXPECT_FALSE(p.IsAbsolute());
  EXPECT_FALSE(p.IsRoot());
  EXPECT_EQ(0, p.Size());
  EXPECT_EQ("", p.Basename());
  EXPECT_EQ("", p.Join());
  EXPECT_EQ("", p.Range(0, 0));
  EXPECT_EQ("", p.Parent().Join());
}

TEST(PathTest, Dot) {
  Path p(".");
  EXPECT_FALSE(p.IsAbsolute());
  EXPECT_FALSE(p.IsRoot());
  EXPECT_EQ(1, p.Size());
  EXPECT_EQ(".", p.Part(0));
  EXPECT_EQ(".", p.Basename());
  EXPECT_EQ(".", p.Join());
  EXPECT_EQ(".", p.Range(0, 1));
  EXPECT_EQ(".", p.Parent().Join());  // TODO(binji): this is unexpected.
}

TEST(PathTest, Root) {
  Path p("/");
  EXPECT_TRUE(p.IsAbsolute());
  EXPECT_TRUE(p.IsRoot());
  EXPECT_EQ(1, p.Size());
  EXPECT_EQ("/", p.Part(0));
  EXPECT_EQ("/", p.Basename());
  EXPECT_EQ("/", p.Join());
  EXPECT_EQ("/", p.Range(0, 1));
  EXPECT_EQ("/", p.Parent().Join());
}

TEST(PathTest, OnePart_Relative) {
  Path p("foo");
  EXPECT_FALSE(p.IsAbsolute());
  EXPECT_FALSE(p.IsRoot());
  EXPECT_EQ(1, p.Size());
  EXPECT_EQ("foo", p.Part(0));
  EXPECT_EQ("foo", p.Basename());
  EXPECT_EQ("foo", p.Join());
  EXPECT_EQ("foo", p.Range(0, 1));
  EXPECT_EQ("foo", p.Parent().Join());
}

TEST(PathTest, OnePart_Absolute) {
  Path p("/foo");
  EXPECT_TRUE(p.IsAbsolute());
  EXPECT_FALSE(p.IsRoot());
  EXPECT_EQ(2, p.Size());
  EXPECT_EQ("/", p.Part(0));
  EXPECT_EQ("foo", p.Part(1));
  EXPECT_EQ("foo", p.Basename());
  EXPECT_EQ("/foo", p.Join());
  EXPECT_EQ("/", p.Range(0, 1));
  EXPECT_EQ("foo", p.Range(1, 2));
  EXPECT_EQ("/foo", p.Range(0, 2));
  EXPECT_EQ("", p.Range(2, 2));
  EXPECT_EQ("/", p.Parent().Join());
}

TEST(PathTest, TwoPart_Relative) {
  Path p("foo/bar");
  EXPECT_FALSE(p.IsAbsolute());
  EXPECT_FALSE(p.IsRoot());
  EXPECT_EQ(2, p.Size());
  EXPECT_EQ("foo", p.Part(0));
  EXPECT_EQ("bar", p.Part(1));
  EXPECT_EQ("bar", p.Basename());
  EXPECT_EQ("foo/bar", p.Join());
  EXPECT_EQ("foo", p.Range(0, 1));
  EXPECT_EQ("bar", p.Range(1, 2));
  EXPECT_EQ("foo/bar", p.Range(0, 2));
  EXPECT_EQ("", p.Range(2, 2));
  EXPECT_EQ("foo", p.Parent().Join());
}

TEST(PathTest, MakeRelative) {
  EXPECT_EQ("", Path("/").MakeRelative().Join());
  EXPECT_EQ("foo/bar/baz", Path("/foo/bar/baz").MakeRelative().Join());
  EXPECT_EQ("foo/bar/baz", Path("foo/bar/baz").MakeRelative().Join());
}

TEST(PathTest, Normalize_EmptyComponent) {
  EXPECT_EQ("foo/bar", Path("foo//bar").Join());
  EXPECT_EQ("/blah", Path("//blah").Join());
  EXPECT_EQ("/a/b/c", Path("//a//b//c").Join());
  EXPECT_EQ("path/to/dir", Path("path/to/dir/").Join());
}

TEST(PathTest, Normalize_Dot) {
  EXPECT_EQ(".", Path(".").Join());
  EXPECT_EQ("foo", Path("foo/.").Join());
  EXPECT_EQ("foo/bar", Path("foo/./bar").Join());
  EXPECT_EQ("blah", Path("./blah").Join());
  EXPECT_EQ("stuff", Path("stuff/./.").Join());
  EXPECT_EQ("/", Path("/.").Join());
}

TEST(PathTest, Normalize_DotDot_Relative) {
  EXPECT_EQ("..", Path("..").Join());
  EXPECT_EQ("../..", Path("../..").Join());
  EXPECT_EQ(".", Path("foo/..").Join());
  EXPECT_EQ("foo", Path("foo/bar/..").Join());
  EXPECT_EQ("bar", Path("foo/../bar").Join());
  EXPECT_EQ("foo/baz", Path("foo/bar/../baz").Join());
}

TEST(PathTest, Normalize_DotDot_Absolute) {
  EXPECT_EQ("/", Path("/..").Join());
  EXPECT_EQ("/", Path("/../..").Join());
  EXPECT_EQ("/", Path("/foo/..").Join());
  EXPECT_EQ("/foo", Path("/foo/bar/..").Join());
  EXPECT_EQ("/bar", Path("/foo/../bar").Join());
  EXPECT_EQ("/foo/baz", Path("/foo/bar/../baz").Join());
}

TEST(PathTest, Append) {
  EXPECT_EQ(".", Path("").Append(Path("")).Join());
  EXPECT_EQ("foo", Path("").Append(Path("foo")).Join());
  EXPECT_EQ(".", Path(".").Append(Path("")).Join());
  EXPECT_EQ("foo", Path(".").Append(Path("foo")).Join());
  EXPECT_EQ("foo/bar", Path(".").Append(Path("foo/bar")).Join());
  EXPECT_EQ("foo", Path("foo").Append(Path("")).Join());
  EXPECT_EQ("foo/bar", Path("foo").Append(Path("bar")).Join());
  EXPECT_EQ("foo/bar/quux", Path("foo").Append(Path("bar/quux")).Join());
  EXPECT_EQ("foo/and", Path("foo/and/more").Append(Path("..")).Join());
}

TEST(PathTest, Append_Absolute) {
  EXPECT_EQ("/", Path("").Append(Path("/")).Join());
  EXPECT_EQ("/hello/world", Path("").Append(Path("/hello/world")).Join());
  EXPECT_EQ("/", Path(".").Append(Path("/")).Join());
  EXPECT_EQ("/goodbye", Path(".").Append(Path("/goodbye")).Join());
  EXPECT_EQ("/foo/bar/baz", Path("/a/b").Append(Path("/foo/bar/baz")).Join());
}

TEST(PathTest, Append_Overflow) {
  std::string big(PATH_MAX - 5, 'A');
  Path p(big.c_str());
  p.Append(Path("0123456789"));

  std::string part(p.Join());
  EXPECT_EQ(PATH_MAX - 1, part.size());
}

TEST(PathTest, Set) {
  Path p("/random/path");
  EXPECT_EQ("something/else", p.Set("something/else").Join());
  // Set should change p, not just return a copy.
  EXPECT_EQ("something/else", p.Join());
}

TEST(PathTest, Set_Overflow) {
  std::string big(PATH_MAX * 2, 'A');
  Path p(big.c_str());
  EXPECT_EQ(PATH_MAX - 1, p.Part(0).size());
}

TEST(PathTest, Range_Empty) {
  EXPECT_EQ("", Path("/").Range(1, 1));
}

TEST(PathTest, Range_Relative) {
  Path p("a/relative/path");

  EXPECT_EQ("a", p.Range(0, 1));
  EXPECT_EQ("a/relative", p.Range(0, 2));
  EXPECT_EQ("a/relative/path", p.Range(0, 3));

  EXPECT_EQ("relative", p.Range(1, 2));
  EXPECT_EQ("relative/path", p.Range(1, 3));

  EXPECT_EQ("path", p.Range(2, 3));

  EXPECT_EQ("path", p.Range(2, 100));
  EXPECT_EQ("", p.Range(42, 67));
}

TEST(PathTest, Range_Absolute) {
  Path p("/an/absolute/path");

  EXPECT_EQ("/", p.Range(0, 1));
  EXPECT_EQ("/an", p.Range(0, 2));
  EXPECT_EQ("/an/absolute", p.Range(0, 3));
  EXPECT_EQ("/an/absolute/path", p.Range(0, 4));

  EXPECT_EQ("an", p.Range(1, 2));
  EXPECT_EQ("an/absolute", p.Range(1, 3));
  EXPECT_EQ("an/absolute/path", p.Range(1, 4));

  EXPECT_EQ("absolute", p.Range(2, 3));
  EXPECT_EQ("absolute/path", p.Range(2, 4));

  EXPECT_EQ("path", p.Range(3, 4));

  EXPECT_EQ("absolute/path", p.Range(2, 100));
  EXPECT_EQ("", p.Range(42, 67));
}

TEST(PathTest, Assign) {
  Path p;

  p = "foo/bar";
  EXPECT_EQ("foo/bar", p.Join());

  // Should normalize.
  p = "/foo/../bar";
  EXPECT_EQ("/bar", p.Join());

  p = Path("hi/planet");
  EXPECT_EQ("hi/planet", p.Join());
}

TEST(PathTest, Equals) {
  EXPECT_TRUE(Path("/foo") == Path("/foo"));
  EXPECT_TRUE(Path("foo/../bar") == Path("bar"));
  EXPECT_TRUE(Path("one/path") != Path("another/path"));
}

