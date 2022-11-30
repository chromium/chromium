// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_layout_util.h"

#include <vector>

#include "build/build_config.h"
#include "remoting/proto/control.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

namespace protocol {

std::ostream& operator<<(std::ostream& os,
                         const protocol::VideoTrackLayout& layout) {
  if (layout.has_screen_id()) {
    os << layout.screen_id() << ": ";
  } else {
    os << "(no screen id): ";
  }
  return os << layout.position_x() << "," << layout.position_y() << " : "
            << layout.width() << "x" << layout.height() << " @ "
            << layout.x_dpi() << "x" << layout.y_dpi();
}

bool operator==(const protocol::VideoTrackLayout& a,
                const protocol::VideoTrackLayout& b) {
  return a.has_screen_id() == b.has_screen_id() &&
         a.screen_id() == b.screen_id() && a.position_x() == b.position_x() &&
         a.position_y() == b.position_y() && a.width() == b.width() &&
         a.height() == b.height() && a.x_dpi() == b.x_dpi() &&
         a.y_dpi() == b.y_dpi();
}

}  // namespace protocol

namespace {

protocol::VideoTrackLayout MakeLayout(
    int position_x,
    int position_y,
    int width,
    int height,
    int x_dpi,
    int y_dpi,
    absl::optional<webrtc::ScreenId> screen_id) {
  protocol::VideoTrackLayout layout;
  if (screen_id) {
    layout.set_screen_id(*screen_id);
  }
  layout.set_position_x(position_x);
  layout.set_position_y(position_y);
  layout.set_width(width);
  layout.set_height(height);
  layout.set_x_dpi(x_dpi);
  layout.set_y_dpi(y_dpi);
  return layout;
}

void* ContextOf(int i) {
  return reinterpret_cast<void*>(i);
}

}  // namespace

std::ostream& operator<<(
    std::ostream& os,
    const VideoTrackLayoutWithContext& layout_with_context) {
  return os << "{layout=" << layout_with_context.layout
            << ", context=" << layout_with_context.context << "}";
}

bool operator==(const VideoTrackLayoutWithContext& a,
                const VideoTrackLayoutWithContext& b) {
  return a.layout == b.layout && a.context == b.context;
}

TEST(DesktopDisplayLayoutUtilTest, CalculateDisplayLayoutDiff) {
  std::vector<VideoTrackLayoutWithContext> current_displays = {
      {.layout = MakeLayout(0, 0, 1230, 1230, 96, 96, 123),
       .context = ContextOf(1)},
      {.layout = MakeLayout(1230, 0, 2340, 2340, 192, 192, 234),
       .context = ContextOf(2)},
      {.layout = MakeLayout(0, 1230, 3450, 3450, 96, 96, 345),
       .context = ContextOf(3)}};
  protocol::VideoLayout new_layout;
  // Updated.
  new_layout.add_video_track()->CopyFrom(
      MakeLayout(3450, 1230, 2340, 2000, 100, 96, 234));
  // Unchanged.
  new_layout.add_video_track()->CopyFrom(
      MakeLayout(0, 1230, 3450, 3450, 96, 96, 345));
  // New.
  new_layout.add_video_track()->CopyFrom(
      MakeLayout(3450, 3450, 4560, 4560, 192, 192, {}));
  auto diff = CalculateDisplayLayoutDiff(current_displays, new_layout);

  std::vector<protocol::VideoTrackLayout> expected_new_displays = {
      MakeLayout(3450, 3450, 4560, 4560, 192, 192, {})};
  EXPECT_EQ(diff.new_displays, expected_new_displays);

  std::vector<VideoTrackLayoutWithContext> expected_updated_displays = {
      {.layout = MakeLayout(3450, 1230, 2340, 2000, 100, 96, 234),
       .context = ContextOf(2)}};
  EXPECT_EQ(diff.updated_displays, expected_updated_displays);

  std::vector<VideoTrackLayoutWithContext> expected_removed_displays = {
      {.layout = MakeLayout(0, 0, 1230, 1230, 96, 96, 123),
       .context = ContextOf(1)}};
  EXPECT_EQ(diff.removed_displays, expected_removed_displays);
}

}  // namespace remoting
