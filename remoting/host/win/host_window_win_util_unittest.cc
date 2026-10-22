// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/host_window_win_util.h"

#include <windows.h>

#include <string>
#include <vector>

#include "base/win/current_module.h"
#include "base/win/scoped_gdi_object.h"
#include "remoting/host/win/core_resource.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

TEST(HostWindowWinUtilTest, ScalesValuesForDpi) {
  EXPECT_EQ(ScaleForDpi(100, USER_DEFAULT_SCREEN_DPI), 100);
  EXPECT_EQ(ScaleForDpi(100, 144), 150);
  EXPECT_EQ(ScaleForDpi(100, 192), 200);
  EXPECT_EQ(ScaleForDpi(64, 144), 96);
}

TEST(HostWindowWinUtilTest, QueriesMonitorAndWindowDpiWithFallback) {
  EXPECT_GT(GetMonitorDpi(nullptr, nullptr), 0u);
  HMONITOR primary = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
  EXPECT_GT(GetMonitorDpi(primary, nullptr), 0u);
}

TEST(HostWindowWinUtilTest, ReturnsNonEmptyActiveMonitorRects) {
  std::vector<RECT> rects = GetActiveMonitorRects();
  ASSERT_FALSE(rects.empty());
  for (const RECT& rect : rects) {
    EXPECT_GT(rect.right, rect.left);
    EXPECT_GT(rect.bottom, rect.top);
  }
}

TEST(HostWindowWinUtilTest, CreatesOverlayFontAtRequestedWeight) {
  base::win::ScopedGDIObject<HFONT> font = CreateOverlayFont(24, FW_SEMIBOLD);
  ASSERT_TRUE(font.is_valid());

  LOGFONTW lf = {};
  ASSERT_GT(GetObjectW(font.get(), sizeof(lf), &lf), 0);
  EXPECT_EQ(lf.lfHeight, -24);
  EXPECT_EQ(lf.lfWeight, FW_SEMIBOLD);
}

TEST(HostWindowWinUtilTest, ReadsFullWindowTextAndMeasuresWidth) {
  HWND hwnd =
      CreateWindowExW(0, L"STATIC", L"Sample Control Text", WS_POPUP, 0, 0, 200,
                      40, nullptr, nullptr, CURRENT_MODULE(), nullptr);
  ASSERT_NE(hwnd, nullptr);

  std::wstring text;
  EXPECT_TRUE(GetWindowTextString(hwnd, &text));
  EXPECT_EQ(text, L"Sample Control Text");

  LONG width = 0;
  EXPECT_TRUE(GetControlTextWidth(hwnd, text, /*is_rtl=*/false, &width));
  EXPECT_GT(width, 0);

  DestroyWindow(hwnd);
}

TEST(HostWindowWinUtilTest, AppliesCaptureExclusionToLayeredPopupWindow) {
  HWND hwnd =
      CreateWindowExW(WS_EX_LAYERED, L"STATIC", L"Layered Popup", WS_POPUP, 0,
                      0, 100, 100, nullptr, nullptr, CURRENT_MODULE(), nullptr);
  ASSERT_NE(hwnd, nullptr);
  ASSERT_TRUE(SetLayeredWindowAttributes(hwnd, 0, 200, LWA_ALPHA));

  EXPECT_TRUE(ApplyCaptureExclusion(hwnd));
  EXPECT_NE(GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_LAYERED, 0);

  BYTE alpha = 0;
  DWORD flags = 0;
  ASSERT_TRUE(GetLayeredWindowAttributes(hwnd, nullptr, &alpha, &flags));
  EXPECT_EQ(alpha, 200);
  EXPECT_NE(flags & LWA_ALPHA, 0u);

  DWORD affinity = 0;
  if (GetWindowDisplayAffinity(hwnd, &affinity) && affinity != 0) {
    EXPECT_EQ(affinity, static_cast<DWORD>(WDA_EXCLUDEFROMCAPTURE));
  }

  DestroyWindow(hwnd);
}

}  // namespace

}  // namespace remoting
