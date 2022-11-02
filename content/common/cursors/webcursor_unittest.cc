// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/common/cursors/webcursor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "ui/base/win/win_cursor.h"
#endif

namespace content {
namespace {

// Creates a basic bitmap for testing with the given width and height.
SkBitmap CreateTestBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorRED);
  return bitmap;
}

TEST(WebCursorTest, DefaultConstructor) {
  WebCursor webcursor;
  EXPECT_EQ(ui::mojom::CursorType::kNull, webcursor.cursor().type());
  EXPECT_TRUE(webcursor.cursor().custom_bitmap().isNull());
  EXPECT_TRUE(webcursor.cursor().custom_hotspot().IsOrigin());
  EXPECT_EQ(1.f, webcursor.cursor().image_scale_factor());
}

TEST(WebCursorTest, WebCursorCursorConstructor) {
  ui::Cursor cursor(ui::mojom::CursorType::kHand);
  WebCursor webcursor(cursor);
  EXPECT_EQ(cursor, webcursor.cursor());
}

TEST(WebCursorTest, WebCursorCursorConstructorCustom) {
  ui::Cursor cursor(ui::mojom::CursorType::kCustom);
  cursor.set_custom_bitmap(CreateTestBitmap(32, 32));
  cursor.set_custom_hotspot(gfx::Point(10, 20));
  cursor.set_image_scale_factor(2.f);
  WebCursor webcursor(cursor);
  EXPECT_EQ(cursor, webcursor.cursor());

#if defined(USE_AURA)
  // Test if the custom cursor is correctly cached and updated
  // on aura platform.
  gfx::NativeCursor native_cursor = webcursor.GetNativeCursor();
  EXPECT_EQ(gfx::Point(5, 10), native_cursor.custom_hotspot());
  EXPECT_TRUE(webcursor.has_custom_cursor_for_test());
  webcursor.SetCursor(cursor);
  EXPECT_FALSE(webcursor.has_custom_cursor_for_test());
  webcursor.GetNativeCursor();
  EXPECT_TRUE(webcursor.has_custom_cursor_for_test());

#if BUILDFLAG(IS_OZONE)
  // Test if the rotating custom cursor works correctly.
  display::Display display;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  display.set_panel_rotation(display::Display::ROTATE_90);
#endif
  webcursor.SetDisplayInfo(display);
  EXPECT_FALSE(webcursor.has_custom_cursor_for_test());
  native_cursor = webcursor.GetNativeCursor();
  EXPECT_TRUE(webcursor.has_custom_cursor_for_test());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Hotspot should be scaled & rotated.  We're using the icon created for 2.0,
  // on the display with dsf=1.0, so the host spot should be
  // ((32 - 20) / 2, 10 / 2) = (6, 5).
  EXPECT_EQ(gfx::Point(6, 5), native_cursor.custom_hotspot());
#else
  // For non-CrOS platforms, the cursor mustn't be rotated as logical and
  // physical location is the same.
  EXPECT_EQ(gfx::Point(5, 10), native_cursor.custom_hotspot());
#endif
#endif
#endif
}

TEST(WebCursorTest, ClampHotspot) {
  // Initialize a cursor with an invalid hotspot; it should be clamped.
  ui::Cursor cursor(ui::mojom::CursorType::kCustom);
  cursor.set_custom_hotspot(gfx::Point(100, 100));
  cursor.set_custom_bitmap(CreateTestBitmap(5, 7));
  WebCursor webcursor(cursor);
  EXPECT_EQ(gfx::Point(4, 6), webcursor.cursor().custom_hotspot());
  // SetCursor should also clamp the hotspot.
  EXPECT_TRUE(webcursor.SetCursor(cursor));
  EXPECT_EQ(gfx::Point(4, 6), webcursor.cursor().custom_hotspot());
}

TEST(WebCursorTest, SetCursor) {
  WebCursor webcursor;
  EXPECT_TRUE(webcursor.SetCursor(ui::Cursor()));
  EXPECT_TRUE(webcursor.SetCursor(ui::Cursor(ui::mojom::CursorType::kHand)));
  EXPECT_TRUE(webcursor.SetCursor(ui::Cursor(ui::mojom::CursorType::kCustom)));

  ui::Cursor cursor(ui::mojom::CursorType::kCustom);
  cursor.set_custom_bitmap(CreateTestBitmap(32, 32));
  cursor.set_custom_hotspot(gfx::Point(10, 20));
  cursor.set_image_scale_factor(1.5f);
  EXPECT_TRUE(webcursor.SetCursor(cursor));

  // SetCursor should return false when the scale factor is too small.
  cursor.set_image_scale_factor(0.001f);
  EXPECT_FALSE(webcursor.SetCursor(cursor));

  // SetCursor should return false when the scale factor is too large.
  cursor.set_image_scale_factor(1000.f);
  EXPECT_FALSE(webcursor.SetCursor(cursor));

  // SetCursor should return false when the unscaled bitmap width is too large.
  cursor.set_image_scale_factor(10.f);
  cursor.set_custom_bitmap(CreateTestBitmap(1025, 5));
  EXPECT_FALSE(webcursor.SetCursor(cursor));

  // SetCursor should return false when the unscaled bitmap height is too large.
  cursor.set_custom_bitmap(CreateTestBitmap(5, 1025));
  EXPECT_FALSE(webcursor.SetCursor(cursor));

  // SetCursor should return false when the 1x scaled image width is too large.
  cursor.set_image_scale_factor(1.f);
  cursor.set_custom_bitmap(CreateTestBitmap(151, 3));
  EXPECT_FALSE(webcursor.SetCursor(cursor));

  // SetCursor should return false when the 1x scaled image height is too large.
  cursor.set_custom_bitmap(CreateTestBitmap(3, 151));
  EXPECT_FALSE(webcursor.SetCursor(cursor));

  // SetCursor should return false when the scaled image width is too large.
  cursor.set_image_scale_factor(0.02f);
  cursor.set_custom_bitmap(CreateTestBitmap(50, 5));
  EXPECT_FALSE(webcursor.SetCursor(cursor));

  // SetCursor should return false when the scaled image height is too large.
  cursor.set_image_scale_factor(0.1f);
  cursor.set_custom_bitmap(CreateTestBitmap(5, 20));
  EXPECT_FALSE(webcursor.SetCursor(cursor));
}

#if defined(USE_AURA)
TEST(WebCursorTest, CursorScaleFactor) {
  constexpr float kImageScale = 2.0f;
  constexpr float kDeviceScale = 4.2f;

  ui::Cursor cursor(ui::mojom::CursorType::kCustom);
  cursor.set_custom_hotspot(gfx::Point(0, 1));
  cursor.set_image_scale_factor(kImageScale);
  cursor.set_custom_bitmap(CreateTestBitmap(128, 128));
  WebCursor webcursor(cursor);

  display::Display display;
  display.set_device_scale_factor(kDeviceScale);
  webcursor.SetDisplayInfo(display);

#if BUILDFLAG(IS_OZONE)
  // In Ozone, the size of the cursor is capped at 64px unless the hardware
  // advertises support for bigger cursors.
  const gfx::Size kDefaultMaxSize = gfx::Size(64, 64);
  EXPECT_EQ(gfx::SkISizeToSize(
                webcursor.GetNativeCursor().custom_bitmap().dimensions()),
            kDefaultMaxSize);
#else
  EXPECT_EQ(
      gfx::SkISizeToSize(
          webcursor.GetNativeCursor().custom_bitmap().dimensions()),
      gfx::ScaleToFlooredSize(gfx::Size(128, 128), kDeviceScale / kImageScale));
#endif

  // The scale factor of the cursor image should match the device scale factor,
  // regardless of the cursor size.
  EXPECT_EQ(webcursor.GetNativeCursor().image_scale_factor(), kDeviceScale);
}

TEST(WebCursorTest, UnscaledImageCopy) {
  ui::Cursor cursor(ui::mojom::CursorType::kCustom);
  cursor.set_custom_hotspot(gfx::Point(0, 1));
  cursor.set_custom_bitmap(CreateTestBitmap(2, 2));
  WebCursor webcursor(cursor);

  SkBitmap copy;
  gfx::Point hotspot;
  float dsf = 0.f;
  webcursor.CreateScaledBitmapAndHotspotFromCustomData(&copy, &hotspot, &dsf);
  EXPECT_EQ(1.f, dsf);
  EXPECT_EQ(2, copy.width());
  EXPECT_EQ(2, copy.height());
  EXPECT_EQ(0, hotspot.x());
  EXPECT_EQ(1, hotspot.y());
}
#endif

#if BUILDFLAG(IS_WIN)
void ScaleCursor(float scale, int hotspot_x, int hotspot_y) {
  ui::Cursor cursor(ui::mojom::CursorType::kCustom);
  cursor.set_custom_hotspot(gfx::Point(hotspot_x, hotspot_y));
  cursor.set_custom_bitmap(CreateTestBitmap(10, 10));
  WebCursor webcursor(cursor);

  display::Display display;
  display.set_device_scale_factor(scale);
  webcursor.SetDisplayInfo(display);

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
