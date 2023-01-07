// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_error.h"

#include <gtest/gtest.h>

namespace crazy {

TEST(Error, ConstructEmpty) {
  Error error;
  EXPECT_STREQ("", error.c_str());
}

TEST(Error, ConstructWithString) {
  Error error("Foo Bar");
  EXPECT_STREQ("Foo Bar", error.c_str());
}

TEST(Error, CopyConstructor) {
  Error error("FooFoo");
  Error error2(error);

  EXPECT_STREQ("FooFoo", error2.c_str());
}

TEST(Error, Set) {
  Error error;
  error.Set("BarFoo");
  EXPECT_STREQ("BarFoo", error.c_str());
  error.Set("FooBar");
  EXPECT_STREQ("FooBar", error.c_str());
}

TEST(Error, Append) {
  Error error("Foo");
  error.Append("Bar");
  EXPECT_STREQ("FooBar", error.c_str());
}

TEST(Error, Format) {
  Error error;
  error.Format("%s %s!", "Hi", "Cowboy");
  EXPECT_STREQ("Hi Cowboy!", error.c_str());
}

TEST(Error, AppendFormat) {
  Error error("Hi");
  error.AppendFormat(" there %s!", "Cowboy");
  EXPECT_STREQ("Hi there Cowboy!", error.c_str());
}

}  // namespace crazy
