// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/util.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

namespace {

const webrtc::DesktopSize kFullHdLandscape(1920, 1080);
const webrtc::DesktopSize k4kLandscape(3840, 2160);
const webrtc::DesktopSize k4kPortrait(2160, 3840);
const webrtc::DesktopSize k5kPortrait(2880, 5120);
const webrtc::DesktopSize kUhdPlus16By10(3840, 2400);

}  // namespace

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

TEST(ScaleSizeToFitTest, EmptyInputs) {
  EXPECT_TRUE(ScaleSizeToFit(webrtc::DesktopSize(), k4kLandscape).is_empty());
  EXPECT_TRUE(
      ScaleSizeToFit(kFullHdLandscape, webrtc::DesktopSize()).is_empty());
  EXPECT_TRUE(
      ScaleSizeToFit(webrtc::DesktopSize(-100, 100), k4kLandscape).is_empty());
}

TEST(ScaleSizeToFitTest, AlreadyFits) {
  EXPECT_TRUE(
      ScaleSizeToFit(kFullHdLandscape, k4kLandscape).equals(kFullHdLandscape));
  EXPECT_TRUE(ScaleSizeToFit(k4kLandscape, k4kLandscape).equals(k4kLandscape));
  EXPECT_TRUE(ScaleSizeToFit(k4kPortrait, k4kPortrait).equals(k4kPortrait));
}

TEST(ScaleSizeToFitTest, ScalesDownProportionally) {
  // Width-limited (5120x2744 -> 3840x2058).
  EXPECT_TRUE(ScaleSizeToFit(webrtc::DesktopSize(5120, 2744), k4kLandscape)
                  .equals(webrtc::DesktopSize(3840, 2058)));

  // Width-limited with rounding (6016x3258 -> 3840x2080).
  EXPECT_TRUE(ScaleSizeToFit(webrtc::DesktopSize(6016, 3258), k4kLandscape)
                  .equals(webrtc::DesktopSize(3840, 2080)));

  // Height-limited 16:10 (3840x2400 -> 3456x2160).
  EXPECT_TRUE(ScaleSizeToFit(kUhdPlus16By10, k4kLandscape)
                  .equals(webrtc::DesktopSize(3456, 2160)));

  // Portrait 5K (2880x5120 -> 2160x3840).
  EXPECT_TRUE(ScaleSizeToFit(k5kPortrait, k4kPortrait).equals(k4kPortrait));

  // Square exceeding short side (3000x3000 -> 2160x2160).
  EXPECT_TRUE(ScaleSizeToFit(webrtc::DesktopSize(3000, 3000), k4kLandscape)
                  .equals(webrtc::DesktopSize(2160, 2160)));
}

TEST(ScaleSizeToFitTest, ExtremeAspectRatioAndLargeValues) {
  // Never scales a non-empty dimension down to 0.
  EXPECT_TRUE(ScaleSizeToFit(webrtc::DesktopSize(100000, 1), k4kLandscape)
                  .equals(webrtc::DesktopSize(3840, 1)));
  EXPECT_TRUE(ScaleSizeToFit(webrtc::DesktopSize(1, 100000), k4kPortrait)
                  .equals(webrtc::DesktopSize(1, 3840)));

  // Handles INT32_MAX without 64-bit integer overflow.
  const int32_t kMaxInt = std::numeric_limits<int32_t>::max();
  EXPECT_TRUE(
      ScaleSizeToFit(webrtc::DesktopSize(kMaxInt, kMaxInt), k4kLandscape)
          .equals(webrtc::DesktopSize(2160, 2160)));
}

}  // namespace remoting
