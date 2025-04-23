// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/util.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(ReplaceLfByCrLfTest, Basic) {
  EXPECT_EQ("ab", ReplaceLfByCrLf("ab"));
  EXPECT_EQ("\r\nab", ReplaceLfByCrLf("\nab"));
  EXPECT_EQ("\r\nab\r\n", ReplaceLfByCrLf("\nab\n"));
  EXPECT_EQ("\r\nab\r\ncd", ReplaceLfByCrLf("\nab\ncd"));
  EXPECT_EQ("\r\nab\r\ncd\r\n", ReplaceLfByCrLf("\nab\ncd\n"));
  EXPECT_EQ("\r\n\r\nab\r\n\r\ncd\r\n\r\n",
            ReplaceLfByCrLf("\n\nab\n\ncd\n\n"));
}

TEST(ReplaceLfByCrLfTest, Speed) {
  constexpr size_t kLineSize = 128;
  std::string line(kLineSize, 'a');
  line.back() = '\n';
  // Make a 10M string.
  constexpr size_t kLineNum = 10 * 1024 * 1024 / kLineSize;
  std::string buffer(kLineNum * kLineSize, '\0');
  auto buffer_span = base::as_writable_byte_span(buffer);
  const auto line_span = base::as_bytes(base::span(line));
  for (size_t i = 0; i < kLineNum; ++i) {
    buffer_span.subspan(i * kLineSize, kLineSize).copy_from(line_span);
  }
  // Convert the string.
  buffer = ReplaceLfByCrLf(buffer);
  // Check the converted string.
  EXPECT_EQ(buffer.size(), (kLineSize + 1) * kLineNum);
  std::string_view view(buffer);
  for (size_t i = 0; i < kLineNum; ++i) {
    auto line_content = view.substr(0, kLineSize - 1);
    for (char c : line_content) {
      EXPECT_EQ('a', c);
    }
    view.remove_prefix(kLineSize - 1);
    EXPECT_EQ('\r', view[0]);
    EXPECT_EQ('\n', view[1]);
    view.remove_prefix(2);
  }
}

TEST(ReplaceCrLfByLfTest, Basic) {
  EXPECT_EQ("ab", ReplaceCrLfByLf("ab"));
  EXPECT_EQ("\nab", ReplaceCrLfByLf("\r\nab"));
  EXPECT_EQ("\nab\n", ReplaceCrLfByLf("\r\nab\r\n"));
  EXPECT_EQ("\nab\ncd", ReplaceCrLfByLf("\r\nab\r\ncd"));
  EXPECT_EQ("\nab\ncd\n", ReplaceCrLfByLf("\r\nab\r\ncd\n"));
  EXPECT_EQ("\n\nab\n\ncd\n\n",
            ReplaceCrLfByLf("\r\n\r\nab\r\n\r\ncd\r\n\r\n"));
  EXPECT_EQ("\rab\rcd\r", ReplaceCrLfByLf("\rab\rcd\r"));
}

TEST(ReplaceCrLfByLfTest, Speed) {
  constexpr size_t kLineSize = 128;
  std::string line(kLineSize, 'a');
  line[kLineSize - 2] = '\r';
  line[kLineSize - 1] = '\n';
  // Make a 10M string.
  constexpr size_t kLineNum = 10 * 1024 * 1024 / kLineSize;
  std::string buffer(kLineNum * kLineSize, '\0');
  auto buffer_span = base::as_writable_byte_span(buffer);
  const auto line_span = base::as_bytes(base::span(line));
  for (size_t i = 0; i < kLineNum; ++i) {
    buffer_span.subspan(i * kLineSize, kLineSize).copy_from(line_span);
  }
  // Convert the string.
  buffer = ReplaceCrLfByLf(buffer);
  // Check the converted string.
  EXPECT_EQ(buffer.size(), (kLineSize - 1) * kLineNum);
  std::string_view view(buffer);
  for (size_t i = 0; i < kLineNum; ++i) {
    auto line_content = view.substr(0, kLineSize - 2);
    for (char c : line_content) {
      EXPECT_EQ('a', c);
    }
    view.remove_prefix(kLineSize - 2);
    EXPECT_EQ('\n', view[0]);
    view.remove_prefix(1);
  }
}

}  // namespace remoting
