// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_serializers.h"

#include <memory>

#include "base/json/json_writer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

std::string ToString(const base::Value& value) {
  if (value.is_string()) {
    return value.GetString();
  }
  std::string output_str;
  base::JSONWriter::Write(value, &output_str);
  return output_str;
}

TEST(MediaSerializersTest, BaseTypes) {
  int a = 1;
  int64_t b = 2;
  bool c = false;
  double d = 100;
  float e = 4523;
  std::string f = "foo";
  const char* g = "bar";

  ASSERT_EQ(ToString(MediaSerialize(a)), "1");
  ASSERT_EQ(ToString(MediaSerialize(b)), "0x2");
  ASSERT_EQ(ToString(MediaSerialize(c)), "false");
  ASSERT_EQ(ToString(MediaSerialize(d)), "100.0");
  ASSERT_EQ(ToString(MediaSerialize(e)), "4523.0");
  ASSERT_EQ(ToString(MediaSerialize(f)), "foo");
  ASSERT_EQ(ToString(MediaSerialize(g)), "bar");

  ASSERT_EQ(ToString(MediaSerialize("raw string")), "raw string");
}

TEST(MediaSerializersTest, Optional) {
  std::optional<int> foo;
  ASSERT_EQ(ToString(MediaSerialize(foo)), "unset");

  foo = 1;
  ASSERT_EQ(ToString(MediaSerialize(foo)), "1");
}

TEST(MediaSerializersTest, Vector) {
  std::vector<int> foo = {1, 2, 3, 6, 78, 8};
  ASSERT_EQ(ToString(MediaSerialize(foo)), "[1,2,3,6,78,8]");

  std::vector<std::string> bar = {"1", "3"};
  ASSERT_EQ(ToString(MediaSerialize(bar)), "[\"1\",\"3\"]");
}

}  // namespace media
