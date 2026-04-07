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
  EXPECT_EQ(ReplaceLfByCrLf("ab"), "ab");
  EXPECT_EQ(ReplaceLfByCrLf("\nab"), "\r\nab");
  EXPECT_EQ(ReplaceLfByCrLf("\nab\n"), "\r\nab\r\n");
  EXPECT_EQ(ReplaceLfByCrLf("\nab\ncd"), "\r\nab\r\ncd");
  EXPECT_EQ(ReplaceLfByCrLf("\nab\ncd\n"), "\r\nab\r\ncd\r\n");
  EXPECT_EQ(ReplaceLfByCrLf("\n\nab\n\ncd\n\n"),
            "\r\n\r\nab\r\n\r\ncd\r\n\r\n");
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
      EXPECT_EQ(c, 'a');
    }
    view.remove_prefix(kLineSize - 1);
    EXPECT_EQ(view[0], '\r');
    EXPECT_EQ(view[1], '\n');
    view.remove_prefix(2);
  }
}

TEST(ReplaceCrLfByLfTest, Basic) {
  EXPECT_EQ(ReplaceCrLfByLf("ab"), "ab");
  EXPECT_EQ(ReplaceCrLfByLf("\r\nab"), "\nab");
  EXPECT_EQ(ReplaceCrLfByLf("\r\nab\r\n"), "\nab\n");
  EXPECT_EQ(ReplaceCrLfByLf("\r\nab\r\ncd"), "\nab\ncd");
  EXPECT_EQ(ReplaceCrLfByLf("\r\nab\r\ncd\n"), "\nab\ncd\n");
  EXPECT_EQ(ReplaceCrLfByLf("\r\n\r\nab\r\n\r\ncd\r\n\r\n"),
            "\n\nab\n\ncd\n\n");
  EXPECT_EQ(ReplaceCrLfByLf("\rab\rcd\r"), "\rab\rcd\r");
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
      EXPECT_EQ(c, 'a');
    }
    view.remove_prefix(kLineSize - 2);
    EXPECT_EQ(view[0], '\n');
    view.remove_prefix(1);
  }
}

}  // namespace remoting
