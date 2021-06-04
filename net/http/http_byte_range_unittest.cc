// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_byte_range.h"
#include "base/cxx17_backports.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(HttpByteRangeTest, ValidRanges) {
  const struct {
    int64_t first_byte_position;
    int64_t last_byte_position;
    int64_t suffix_length;
    bool valid;
  } tests[] = {
    {  -1, -1,  0, false },
    {   0,  0,  0, true  },
    { -10,  0,  0, false },
    {  10,  0,  0, false },
    {  10, -1,  0, true  },
    {  -1, -1, -1, false },
    {  -1, 50,  0, false },
    {  10, 10000, 0, true },
    {  -1, -1, 100000, true },
  };

  for (size_t i = 0; i < base::size(tests); ++i) {
    HttpByteRange range;
    range.set_first_byte_position(tests[i].first_byte_position);
    range.set_last_byte_position(tests[i].last_byte_position);
    range.set_suffix_length(tests[i].suffix_length);
    EXPECT_EQ(tests[i].valid, range.IsValid());
  }
}

TEST(HttpByteRangeTest, SetInstanceSize) {
  const struct {
    int64_t first_byte_position;
    int64_t last_byte_position;
    int64_t suffix_length;
    int64_t instance_size;
    bool expected_return_value;
    int64_t expected_lower_bound;
    int64_t expected_upper_bound;
  } tests[] = {
    { -10,  0,  -1,   0, false,  -1,  -1 },
    {  10,  0,  -1,   0, false,  -1,  -1 },
    // Zero instance size is valid, this is the case that user has to handle.
    {  -1, -1,  -1,   0,  true,   0,  -1 },
    {  -1, -1, 500,   0,  true,   0,  -1 },
    {  -1, 50,  -1,   0, false,  -1,  -1 },
    {  -1, -1, 500, 300,  true,   0, 299 },
    {  -1, -1,  -1, 100,  true,   0,  99 },
    {  10, -1,  -1, 100,  true,  10,  99 },
    {  -1, -1, 500, 1000, true, 500, 999 },
    {  10, 10000, -1, 1000000, true, 10, 10000 },
  };

  for (size_t i = 0; i < base::size(tests); ++i) {
    HttpByteRange range;
    range.set_first_byte_position(tests[i].first_byte_position);
    range.set_last_byte_position(tests[i].last_byte_position);
    range.set_suffix_length(tests[i].suffix_length);

    bool return_value = range.ComputeBounds(tests[i].instance_size);
    EXPECT_EQ(tests[i].expected_return_value, return_value);
    if (return_value) {
      EXPECT_EQ(tests[i].expected_lower_bound, range.first_byte_position());
      EXPECT_EQ(tests[i].expected_upper_bound, range.last_byte_position());

      // Try to call SetInstanceSize the second time.
      EXPECT_FALSE(range.ComputeBounds(tests[i].instance_size));
      // And expect there's no side effect.
      EXPECT_EQ(tests[i].expected_lower_bound, range.first_byte_position());
      EXPECT_EQ(tests[i].expected_upper_bound, range.last_byte_position());
      EXPECT_EQ(tests[i].suffix_length, range.suffix_length());
    }
  }
}

TEST(HttpByteRangeTest, GetHeaderValue) {
  static const struct {
    HttpByteRange range;
    const char* expected;
  } tests[] = {
      {HttpByteRange::Bounded(0, 0), "bytes=0-0"},
      {HttpByteRange::Bounded(0, 100), "bytes=0-100"},
      {HttpByteRange::Bounded(0, -1), "bytes=0-"},
      {HttpByteRange::RightUnbounded(100), "bytes=100-"},
      {HttpByteRange::Suffix(100), "bytes=-100"},
  };
  for (size_t i = 0; i < base::size(tests); ++i) {
    EXPECT_EQ(tests[i].expected, tests[i].range.GetHeaderValue());
  }
}

}  // namespace

}  // namespace net
