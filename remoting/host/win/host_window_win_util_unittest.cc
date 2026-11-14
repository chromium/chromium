// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/host_window_win_util.h"

#include <windows.h>

#include <optional>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/win/current_module.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
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
  base::AutoReset<bool> allow_capture_exclusion_failure =
      ScopedAllowCaptureExclusionFailureForTesting();

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

TEST(HostWindowWinUtilTest,
     ApplyCaptureExclusionFailsClosedByDefaultAndHonorsTestingOverride) {
  HWND parent_hwnd =
      CreateWindowExW(0, L"STATIC", L"Parent Popup", WS_POPUP, 0, 0, 100, 100,
                      nullptr, nullptr, CURRENT_MODULE(), nullptr);
  ASSERT_NE(parent_hwnd, nullptr);

  // `SetWindowDisplayAffinity()` only accepts top-level windows and
  // deterministically fails with `ERROR_INVALID_PARAMETER` on `WS_CHILD`
  // controls regardless of display driver.
  HWND child_hwnd =
      CreateWindowExW(0, L"STATIC", L"Child Control", WS_CHILD, 0, 0, 50, 50,
                      parent_hwnd, nullptr, CURRENT_MODULE(), nullptr);
  ASSERT_NE(child_hwnd, nullptr);

  EXPECT_FALSE(ApplyCaptureExclusion(child_hwnd));
  EXPECT_FALSE(ApplyCaptureExclusion(nullptr));

  {
    base::AutoReset<bool> allow_capture_exclusion_failure =
        ScopedAllowCaptureExclusionFailureForTesting();
    EXPECT_TRUE(ApplyCaptureExclusion(child_hwnd));
    // `nullptr` fails with `ERROR_INVALID_WINDOW_HANDLE`, which must still fail
    // closed even when the headless-bot override is active.
    EXPECT_FALSE(ApplyCaptureExclusion(nullptr));
  }

  EXPECT_FALSE(ApplyCaptureExclusion(child_hwnd));

  DestroyWindow(parent_hwnd);
}

TEST(HostWindowWinUtilTest,
     RegisterDisconnectHotKeyRejectsNullAndHonorsTestingOverride) {
  HWND hwnd =
      CreateWindowExW(0, L"STATIC", L"HotKey Target", WS_POPUP, 0, 0, 100, 100,
                      nullptr, nullptr, CURRENT_MODULE(), nullptr);
  ASSERT_NE(hwnd, nullptr);

  EXPECT_FALSE(RegisterDisconnectHotKey(nullptr));
  EXPECT_EQ(GetLastError(), static_cast<DWORD>(ERROR_INVALID_WINDOW_HANDLE));

  {
    base::AutoReset<std::optional<bool>> succeed_override =
        ScopedOverrideHotKeyRegistrationForTesting(true);
    EXPECT_TRUE(RegisterDisconnectHotKey(hwnd));
    EXPECT_FALSE(RegisterDisconnectHotKey(nullptr));
    UnregisterDisconnectHotKey(hwnd);
  }

  {
    base::AutoReset<std::optional<bool>> fail_override =
        ScopedOverrideHotKeyRegistrationForTesting(false);
    EXPECT_FALSE(RegisterDisconnectHotKey(hwnd));
    EXPECT_EQ(GetLastError(),
              static_cast<DWORD>(ERROR_HOTKEY_ALREADY_REGISTERED));
    UnregisterDisconnectHotKey(hwnd);
  }

  DestroyWindow(hwnd);
}

TEST(HostWindowWinUtilTest,
     ComputesBackdropContentLayoutWithWrappingHintOmissionAndSmallScreenShift) {
  base::win::ScopedGetDC screen_dc(nullptr);
  ASSERT_NE(static_cast<HDC>(screen_dc), nullptr);

  constexpr UINT kDpi = USER_DEFAULT_SCREEN_DPI;
  base::win::ScopedGDIObject<HFONT> title_font =
      CreateOverlayFont(ScaleForDpi(kBaseTitleFontSize, kDpi), FW_SEMIBOLD);
  base::win::ScopedGDIObject<HFONT> body_font =
      CreateOverlayFont(ScaleForDpi(kBaseBodyFontSize, kDpi), FW_NORMAL);
  base::win::ScopedGDIObject<HFONT> hint_font =
      CreateOverlayFont(ScaleForDpi(kBaseHintFontSize, kDpi), FW_NORMAL);
  ASSERT_TRUE(title_font.is_valid());
  ASSERT_TRUE(body_font.is_valid());
  ASSERT_TRUE(hint_font.is_valid());

  const std::wstring title = L"Chrome Remote Desktop";
  const std::wstring body = L"Your desktop is currently shared with user@host.";
  const std::wstring hint = L"Press Ctrl+Alt+Esc to disconnect";

  const BackdropTextBlock title_block = {.font = title_font.get(),
                                         .text = title};
  const BackdropTextBlock body_block = {.font = body_font.get(), .text = body};
  const BackdropTextBlock hint_block = {.font = hint_font.get(), .text = hint};

  // Standard 1920x1080 layout with both sharing text and hotkey hint.
  BackdropContentLayout with_hint = ComputeBackdropContentLayout(
      screen_dc, 1920, 1080, kDpi, /*is_rtl=*/false, title_block, body_block,
      hint_block);
  EXPECT_EQ(with_hint.draw_text_flags,
            GetBackdropDrawTextFlags(/*is_rtl=*/false));
  EXPECT_EQ(with_hint.draw_text_flags & DT_RTLREADING, 0u);
  EXPECT_NE(GetBackdropDrawTextFlags(/*is_rtl=*/true) & DT_RTLREADING, 0u);
  EXPECT_EQ(with_hint.center_y, 540);
  EXPECT_EQ(with_hint.body_top, 540);
  EXPECT_GT(with_hint.title_height, 0);
  EXPECT_GT(with_hint.body_height, 0);
  EXPECT_GT(with_hint.hint_height, 0);
  EXPECT_EQ(with_hint.hint_gap, kBaseVerticalSpacing / 2);
  EXPECT_EQ(with_hint.hint_top,
            with_hint.body_top + with_hint.body_height + with_hint.hint_gap);
  EXPECT_EQ(with_hint.below_center_height,
            with_hint.body_height + with_hint.hint_gap + with_hint.hint_height);

  // Omitting `hint.text` collapses both `hint_height` and `hint_gap` to 0.
  BackdropContentLayout without_hint = ComputeBackdropContentLayout(
      screen_dc, 1920, 1080, kDpi, /*is_rtl=*/false, title_block, body_block,
      {.font = hint_font.get(), .text = L""});
  EXPECT_EQ(without_hint.body_height, with_hint.body_height);
  EXPECT_EQ(without_hint.hint_height, 0);
  EXPECT_EQ(without_hint.hint_gap, 0);
  EXPECT_EQ(without_hint.below_center_height, without_hint.body_height);

  // Multi-line wrapping on a narrow monitor shifts `hint_top` downward below
  // the wrapped sharing text without overlap.
  const std::wstring long_body =
      L"Your desktop is currently shared with user@host. This additional "
      L"sentence forces multi-line word wrapping on narrow displays.";
  BackdropContentLayout wrapped = ComputeBackdropContentLayout(
      screen_dc, 360, 1080, kDpi, /*is_rtl=*/false, title_block,
      {.font = body_font.get(), .text = long_body}, hint_block);
  EXPECT_GT(wrapped.body_height, with_hint.body_height);
  EXPECT_EQ(wrapped.hint_top,
            wrapped.body_top + wrapped.body_height + wrapped.hint_gap);

  // An unusually short monitor shifts `icon_top`, `title_bottom`, and
  // `center_y` downward so the icon never clips above `kBaseVerticalSpacing`.
  BackdropContentLayout short_screen =
      ComputeBackdropContentLayout(screen_dc, 1920, 160, kDpi, /*is_rtl=*/true,
                                   title_block, body_block, hint_block);
  EXPECT_EQ(short_screen.draw_text_flags,
            GetBackdropDrawTextFlags(/*is_rtl=*/true));
  EXPECT_EQ(short_screen.icon_top, kBaseVerticalSpacing);
  EXPECT_GT(short_screen.center_y, 80);
  EXPECT_EQ(short_screen.body_top, short_screen.center_y);
}

}  // namespace

}  // namespace remoting
