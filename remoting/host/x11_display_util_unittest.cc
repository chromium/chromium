// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/x11_display_util.h"

#include "remoting/proto/control.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/x/randr.h"

namespace remoting {

TEST(X11DisplayUtilTest, GetMonitorDpi) {
  EXPECT_TRUE(GetMonitorDpi({.width = 1024, .height = 768}) ==
              gfx::Vector2d({96, 96}));
  EXPECT_TRUE(GetMonitorDpi({.width = 1000,
                             .height = 500,
                             .width_in_millimeters = 100,
                             .height_in_millimeters = 100}) ==
              gfx::Vector2d({254, 127}));
}

TEST(X11DisplayUtilTest, ToVideoTrackLayout) {
  x11::RandR::MonitorInfo monitor = {.name = static_cast<x11::Atom>(123),
                                     .x = 10,
                                     .y = 20,
                                     .width = 1000,
                                     .height = 500,
                                     .width_in_millimeters = 100,
                                     .height_in_millimeters = 100};
  x11::RandRMonitorConfig layout = ToVideoTrackLayout(monitor);
  EXPECT_EQ(layout.id(), 123);
  EXPECT_EQ(layout.rect().x(), 10);
  EXPECT_EQ(layout.rect().y(), 20);
  EXPECT_EQ(layout.rect().width(), 1000);
  EXPECT_EQ(layout.rect().height(), 500);
  EXPECT_EQ(layout.dpi().x(), 254);
  EXPECT_EQ(layout.dpi().y(), 127);
}

}  // namespace remoting
