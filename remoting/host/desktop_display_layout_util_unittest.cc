// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_layout_util.h"

#include <optional>
#include <vector>

#include "build/build_config.h"
#include "remoting/host/desktop_geometry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace remoting {

std::ostream& operator<<(std::ostream& os, const DesktopLayout& layout) {
  if (layout.screen_id().has_value()) {
    os << *layout.screen_id() << ": ";
  } else {
    os << "(no screen id): ";
  }
  return os << layout.position_x() << "," << layout.position_y() << " : "
            << layout.width() << "x" << layout.height() << " @ "
            << layout.dpi().x() << "x" << layout.dpi().y();
}

namespace {

remoting::DesktopLayout MakeLayout(int position_x,
                                   int position_y,
                                   int width,
                                   int height,
                                   int x_dpi,
                                   int y_dpi,
                                   std::optional<int64_t> screen_id) {
  return remoting::DesktopLayout(
      screen_id, gfx::Rect(position_x, position_y, width, height),
      gfx::Vector2d(x_dpi, y_dpi));
}

void* ContextOf(int i) {
  return reinterpret_cast<void*>(i);
}

}  // namespace

std::ostream& operator<<(
    std::ostream& os,
    const remoting::DesktopLayoutWithContext& layout_with_context) {
  return os << "{layout=" << layout_with_context.layout
            << ", context=" << layout_with_context.context << "}";
}

bool operator==(const remoting::DesktopLayoutWithContext& a,
                const remoting::DesktopLayoutWithContext& b) {
  return a.layout == b.layout && a.context == b.context;
}

TEST(DesktopDisplayLayoutUtilTest, CalculateDisplayLayoutDiff) {
  std::vector<DesktopLayoutWithContext> current_displays = {
      {.layout = MakeLayout(0, 0, 1230, 1230, 96, 96, 123),
       .context = ContextOf(1)},
      {.layout = MakeLayout(1230, 0, 2340, 2340, 192, 192, 234),
       .context = ContextOf(2)},
      {.layout = MakeLayout(0, 1230, 3450, 3450, 96, 96, 345),
       .context = ContextOf(3)}};
  DesktopLayoutSet new_layout(
      {// Updated.
       MakeLayout(3450, 1230, 2340, 2000, 100, 96, 234),
       // Unchanged.
       MakeLayout(0, 1230, 3450, 3450, 96, 96, 345),
       // New.
       MakeLayout(3450, 3450, 4560, 4560, 192, 192, {})});
  auto diff = CalculateDisplayLayoutDiff(current_displays, new_layout);

  DesktopLayoutSet expected_new_displays;
  expected_new_displays.layouts.push_back(
      MakeLayout(3450, 3450, 4560, 4560, 192, 192, {}));
  EXPECT_EQ(diff.new_displays, expected_new_displays);

  std::vector<DesktopLayoutWithContext> expected_updated_displays = {
      {.layout = MakeLayout(3450, 1230, 2340, 2000, 100, 96, 234),
       .context = ContextOf(2)}};
  EXPECT_EQ(diff.updated_displays, expected_updated_displays);

  std::vector<DesktopLayoutWithContext> expected_removed_displays = {
      {.layout = MakeLayout(0, 0, 1230, 1230, 96, 96, 123),
       .context = ContextOf(1)}};
  EXPECT_EQ(diff.removed_displays, expected_removed_displays);
}

}  // namespace remoting
