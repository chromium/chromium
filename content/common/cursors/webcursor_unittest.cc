// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "build/build_config.h"
#include "content/common/cursors/webcursor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if defined(OS_WIN)
#include <windows.h>
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
  WebCursor cursor;
  EXPECT_EQ(ui::CursorType::kPointer, cursor.info().type);
  EXPECT_TRUE(cursor.info().custom_image.isNull());
  EXPECT_TRUE(cursor.info().hotspot.IsOrigin());
  EXPECT_EQ(1.f, cursor.info().image_scale_factor);
}

TEST(WebCursorTest, CursorInfoConstructor) {
  CursorInfo info(ui::CursorType::kHand);
  WebCursor cursor(info);
  EXPECT_EQ(info, cursor.info());
}

TEST(WebCursorTest, CursorInfoConstructorCustom) {
  CursorInfo info(ui::CursorType::kCustom);
  info.custom_image = CreateTestBitmap(32, 32);
  info.hotspot = gfx::Point(10, 20);
  info.image_scale_factor = 1.5f;
  WebCursor cursor(info);
  EXPECT_EQ(info, cursor.info());
}

TEST(WebCursorTest, CopyConstructorType) {
  CursorInfo info(ui::CursorType::kHand);
  WebCursor cursor(info);
  WebCursor copy(cursor);
  EXPECT_EQ(cursor, copy);
}

TEST(WebCursorTest, CopyConstructorCustom) {
  CursorInfo info(ui::CursorType::kCustom);
  info.custom_image = CreateTestBitmap(32, 32);
  info.hotspot = gfx::Point(10, 20);
  info.image_scale_factor = 1.5f;
  WebCursor cursor(info);
  WebCursor copy(cursor);
  EXPECT_EQ(cursor, copy);
}

TEST(WebCursorTest, ClampHotspot) {
  // Initialize a cursor with an invalid hotspot; it should be clamped.
  CursorInfo info(ui::CursorType::kCustom);
  info.hotspot = gfx::Point(100, 100);
  info.custom_image = CreateTestBitmap(5, 7);
  WebCursor cursor(info);
  EXPECT_EQ(gfx::Point(4, 6), cursor.info().hotspot);
  // SetInfo should also clamp the hotspot.
  EXPECT_TRUE(cursor.SetInfo(info));
  EXPECT_EQ(gfx::Point(4, 6), cursor.info().hotspot);
}

TEST(WebCursorTest, SetInfo) {
  WebCursor cursor;
  EXPECT_TRUE(cursor.SetInfo(CursorInfo()));
  EXPECT_TRUE(cursor.SetInfo(CursorInfo(ui::CursorType::kHand)));
  EXPECT_TRUE(cursor.SetInfo(CursorInfo(ui::CursorType::kCustom)));

  CursorInfo info(ui::CursorType::kCustom);
  info.custom_image = CreateTestBitmap(32, 32);
  info.hotspot = gfx::Point(10, 20);
  info.image_scale_factor = 1.5f;
  EXPECT_TRUE(cursor.SetInfo(info));

  // SetInfo should return false when the scale factor is too small.
  info.image_scale_factor = 0.001f;
  EXPECT_FALSE(cursor.SetInfo(info));

  // SetInfo should return false when the scale factor is too large.
  info.image_scale_factor = 1000.f;
  EXPECT_FALSE(cursor.SetInfo(info));

  // SetInfo should return false when the image width is too large.
  info.image_scale_factor = 1.f;
  info.custom_image = CreateTestBitmap(1025, 3);
  EXPECT_FALSE(cursor.SetInfo(info));

  // SetInfo should return false when the image height is too large.
  info.custom_image = CreateTestBitmap(3, 1025);
  EXPECT_FALSE(cursor.SetInfo(info));

  // SetInfo should return false when the scaled image width is too large.
  info.image_scale_factor = 0.02f;
  info.custom_image = CreateTestBitmap(50, 5);
  EXPECT_FALSE(cursor.SetInfo(info));

  // SetInfo should return false when the scaled image height is too large.
  info.image_scale_factor = 0.1f;
  info.custom_image = CreateTestBitmap(5, 200);
  EXPECT_FALSE(cursor.SetInfo(info));
}

#if defined(USE_AURA)
TEST(WebCursorTest, CursorScaleFactor) {
  CursorInfo info;
  info.type = ui::CursorType::kCustom;
  info.hotspot = gfx::Point(0, 1);
  info.image_scale_factor = 2.0f;
  info.custom_image = CreateTestBitmap(128, 128);
  WebCursor cursor(info);

  display::Display display;
  display.set_device_scale_factor(4.2f);
  cursor.SetDisplayInfo(display);

#if defined(USE_OZONE)
  // For Ozone cursors, the size of the cursor is capped at 64px, and this is
  // enforce through the calculated scale factor.
  EXPECT_EQ(0.5f, cursor.GetNativeCursor().device_scale_factor());
#else
  EXPECT_EQ(2.1f, cursor.GetNativeCursor().device_scale_factor());
#endif

  // Test that the Display dsf is copied.
  WebCursor copy(cursor);
  EXPECT_EQ(cursor.GetNativeCursor().device_scale_factor(),
            copy.GetNativeCursor().device_scale_factor());
}

TEST(WebCursorTest, UnscaledImageCopy) {
  CursorInfo info;
  info.type = ui::CursorType::kCustom;
  info.hotspot = gfx::Point(0, 1);
  info.custom_image = CreateTestBitmap(2, 2);
  WebCursor cursor(info);

  SkBitmap copy;
  gfx::Point hotspot;
  float dsf = 0.f;
  cursor.CreateScaledBitmapAndHotspotFromCustomData(&copy, &hotspot, &dsf);
  EXPECT_EQ(1.f, dsf);
  EXPECT_EQ(2, copy.width());
  EXPECT_EQ(2, copy.height());
  EXPECT_EQ(0, hotspot.x());
  EXPECT_EQ(1, hotspot.y());
}
#endif

#if defined(OS_WIN)
void ScaleCursor(float scale, int hotspot_x, int hotspot_y) {
  CursorInfo info;
  info.type = ui::CursorType::kCustom;
  info.hotspot = gfx::Point(hotspot_x, hotspot_y);
  info.custom_image = CreateTestBitmap(10, 10);
  WebCursor cursor(info);

  display::Display display;
  display.set_device_scale_factor(scale);
  cursor.SetDisplayInfo(display);

  HCURSOR windows_cursor_handle = cursor.GetNativeCursor().platform();
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
