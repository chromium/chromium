// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include <stddef.h>

#include <optional>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/screen_base.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "ui/base/win/win_cursor.h"
#endif

namespace content {
namespace {

class TestScreen : public display::test::TestScreen {
 public:
  explicit TestScreen(display::Display display)
      : display::test::TestScreen(/*create_display=*/false,
                                  /*register_screen=*/true) {
    display_list().AddDisplay(display, display::DisplayList::Type::PRIMARY);
  }
};

TEST(WebCursorTest, DefaultConstructor) {
  WebCursor webcursor;
  EXPECT_EQ(ui::mojom::CursorType::kNull, webcursor.cursor().type());
}

TEST(WebCursorTest, WebCursorCursorConstructor) {
  ui::Cursor cursor(ui::mojom::CursorType::kHand);
  WebCursor webcursor(cursor);
  EXPECT_EQ(cursor, webcursor.cursor());
}

TEST(WebCursorTest, WebCursorCursorConstructorCustom) {
  const ui::Cursor cursor = ui::Cursor::NewCustom(
      gfx::test::CreateBitmap(/*size=*/32), gfx::Point(10, 20), 2.0f);
  WebCursor webcursor(cursor);
  EXPECT_EQ(cursor, webcursor.cursor());

#if defined(USE_AURA)
  // Test if the custom cursor is correctly cached and updated
  // on aura platform.
  EXPECT_FALSE(webcursor.has_custom_cursor_for_test());
  gfx::NativeCursor native_cursor = webcursor.GetNativeCursor();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Cursor is not scaled to device scale factor in lacros.
  EXPECT_EQ(gfx::Point(10, 20), native_cursor.custom_hotspot());
#else
  EXPECT_EQ(gfx::Point(5, 10), native_cursor.custom_hotspot());
#endif

  EXPECT_TRUE(webcursor.has_custom_cursor_for_test());
  // Test if the rotating custom cursor works correctly.
  display::Display display(/*id=*/1);
  display.set_panel_rotation(display::Display::ROTATE_90);
  TestScreen screen(display);
  webcursor.UpdateDisplayInfoForWindow(nullptr);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_FALSE(webcursor.has_custom_cursor_for_test());
#else
  EXPECT_TRUE(webcursor.has_custom_cursor_for_test());
#endif

  native_cursor = webcursor.GetNativeCursor();
  EXPECT_TRUE(webcursor.has_custom_cursor_for_test());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // In lacros rotation and scale is handled by ash. So the cursor
  // is not rotated or scaled.
  EXPECT_EQ(gfx::Point(10, 20), native_cursor.custom_hotspot());
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  // Hotspot should be scaled & rotated. We're using the icon created for 2.0,
  // on the display with dsf=1.0, so the hotspot should be
  // ((32 - 20) / 2, 10 / 2) = (6, 5).
  EXPECT_EQ(gfx::Point(6, 5), native_cursor.custom_hotspot());
#else
  // For non-CrOS platforms, the cursor mustn't be rotated as logical and
  // physical location is the same.
  EXPECT_EQ(gfx::Point(5, 10), native_cursor.custom_hotspot());
#endif

#endif  // defined(USE_AURA)
}

#if defined(USE_AURA)
TEST(WebCursorTest, CursorScaleFactor) {
  constexpr float kImageScale = 2.0f;
  constexpr float kDeviceScale = 4.2f;

  WebCursor webcursor(ui::Cursor::NewCustom(
      gfx::test::CreateBitmap(/*size=*/128), gfx::Point(0, 1), kImageScale));

  display::Display display(/*id=*/1);
  display.set_device_scale_factor(kDeviceScale);
  TestScreen screen(display);
  webcursor.UpdateDisplayInfoForWindow(nullptr);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // In Ash, the size of the cursor is capped at 64px unless the hardware
  // advertises support for bigger cursors.
  const gfx::Size kDefaultMaxSize = gfx::Size(64, 64);
  EXPECT_EQ(gfx::SkISizeToSize(
                webcursor.GetNativeCursor().custom_bitmap().dimensions()),
            kDefaultMaxSize);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // Bitmap doesn't get scaled to device scale factor in lacros.
  EXPECT_EQ(gfx::SkISizeToSize(
                webcursor.GetNativeCursor().custom_bitmap().dimensions()),
            gfx::Size(128, 128));
#else
  EXPECT_EQ(
      gfx::SkISizeToSize(
          webcursor.GetNativeCursor().custom_bitmap().dimensions()),
      gfx::ScaleToFlooredSize(gfx::Size(128, 128), kDeviceScale / kImageScale));
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // The scale factor of the cursor image should match the image scale.
  EXPECT_EQ(webcursor.GetNativeCursor().image_scale_factor(), kImageScale);
#else
  // The scale factor of the cursor image should match the device scale factor,
  // regardless of the cursor size.
  EXPECT_EQ(webcursor.GetNativeCursor().image_scale_factor(), kDeviceScale);
#endif
}
#endif  // defined(USE_AURA)

#if BUILDFLAG(IS_WIN)
void ScaleCursor(float scale, int hotspot_x, int hotspot_y) {
  WebCursor webcursor(ui::Cursor::NewCustom(
      gfx::test::CreateBitmap(/*size=*/10), gfx::Point(hotspot_x, hotspot_y)));

  display::Display display(/*id=*/1);
  display.set_device_scale_factor(scale);
  TestScreen screen(display);
  webcursor.UpdateDisplayInfoForWindow(nullptr);

  HCURSOR windows_cursor_handle =
      ui::WinCursor::FromPlatformCursor(webcursor.GetNativeCursor().platform())
          ->hcursor();
  EXPECT_NE(nullptr, windows_cursor_handle);
  ICONINFO windows_icon_info;
  EXPECT_TRUE(GetIconInfo(windows_cursor_handle, &windows_icon_info));
  EXPECT_FALSE(windows_icon_info.fIcon);
  EXPECT_EQ(static_cast<DWORD>(scale * hotspot_x), windows_icon_info.xHotspot);
  EXPECT_EQ(static_cast<DWORD>(scale * hotspot_y), windows_icon_info.yHotspot);
}

TEST(WebCursorTest, WindowsCursorScaledAtHiDpi) {
  ScaleCursor(2.0f, 4, 6);
  ScaleCursor(1.5f, 2, 8);
  ScaleCursor(1.25f, 3, 7);
}
#endif

}  // namespace
}  // namespace content
