// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_screen.h"

#include <memory>

#include "extensions/shell/test/shell_test_base_aura.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace extensions {

using ShellScreenTest = ShellTestBaseAura;

// Basic sanity tests for ShellScreen.
TEST_F(ShellScreenTest, ShellScreen) {
  ShellScreen screen(nullptr, gfx::Size(640, 480));

  // There is only one display.
  EXPECT_EQ(1, screen.GetNumDisplays());
  EXPECT_EQ(1u, screen.GetAllDisplays().size());
  EXPECT_EQ(0, screen.GetAllDisplays()[0].id());
  EXPECT_EQ(0, screen.GetPrimaryDisplay().id());
  EXPECT_EQ("640x480", screen.GetPrimaryDisplay().size().ToString());

  // Tests that reshaping the host window reshapes the display.
  std::unique_ptr<aura::WindowTreeHost> host =
      aura::WindowTreeHost::Create(ui::PlatformWindowInitProperties{
          gfx::Rect(screen.GetPrimaryDisplay().GetSizeInPixel())});
  host->AddObserver(&screen);
  host->InitHost();
  EXPECT_TRUE(host->window());

  host->SetBoundsInPixels(gfx::Rect(0, 0, 800, 600));
  EXPECT_EQ("800x600", screen.GetPrimaryDisplay().size().ToString());

  host->RemoveObserver(&screen);
}

}  // namespace extensions
