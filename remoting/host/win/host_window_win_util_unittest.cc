// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/host_window_win_util.h"

#include <windows.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/strings/string_util.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/current_module.h"
#include "base/win/registry.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "remoting/host/win/core_resource.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

struct HwndDeleter {
  using pointer = HWND;
  void operator()(HWND hwnd) const {
    if (hwnd) {
      DestroyWindow(hwnd);
    }
  }
};
using ScopedHwnd = std::unique_ptr<HWND, HwndDeleter>;

TEST(HostWindowWinUtilTest, ScalesValuesForDpi) {
  EXPECT_EQ(ScaleForDpi(100, USER_DEFAULT_SCREEN_DPI), 100);
  EXPECT_EQ(ScaleForDpi(100, 144), 150);
  EXPECT_EQ(ScaleForDpi(100, 192), 200);
  EXPECT_EQ(ScaleForDpi(64, 144), 96);
}

TEST(HostWindowWinUtilTest,
     ScalesValuesForDpiAndTextScaleWithSingleStepRoundingAndClamping) {
  EXPECT_EQ(ScaleForDpiAndTextScale(18, USER_DEFAULT_SCREEN_DPI, 1.0), 18);
  EXPECT_EQ(ScaleForDpiAndTextScale(18, USER_DEFAULT_SCREEN_DPI, 1.5), 27);
  EXPECT_EQ(ScaleForDpiAndTextScale(18, 192, 1.5), 54);
  // Single-step rounding (`14 * 120 * 1.25 / 96 = 21.875 -> 22`, avoiding the
  // 1px overestimate from rounding `14 * 120 / 96` to `18` first).
  EXPECT_EQ(ScaleForDpiAndTextScale(14, 120, 1.25), 22);
  EXPECT_EQ(ScaleForDpiAndTextScale(18, USER_DEFAULT_SCREEN_DPI, 0.5), 18);
  EXPECT_EQ(ScaleForDpiAndTextScale(20, USER_DEFAULT_SCREEN_DPI, 5.0), 45);
}

TEST(HostWindowWinUtilTest, OverridesAndClampsSystemTextScaleFactor) {
  EXPECT_GE(GetSystemTextScaleFactor(), kMinTextScaleFactor);
  EXPECT_LE(GetSystemTextScaleFactor(), kMaxTextScaleFactor);

  {
    base::AutoReset<std::optional<double>> override_scale =
        ScopedOverrideTextScaleFactorForTesting(1.75);
    EXPECT_DOUBLE_EQ(GetSystemTextScaleFactor(), 1.75);
  }

  {
    base::AutoReset<std::optional<double>> clamped_high =
        ScopedOverrideTextScaleFactorForTesting(10.0);
    EXPECT_DOUBLE_EQ(GetSystemTextScaleFactor(), kMaxTextScaleFactor);
  }

  {
    base::AutoReset<std::optional<double>> clamped_low =
        ScopedOverrideTextScaleFactorForTesting(0.25);
    EXPECT_DOUBLE_EQ(GetSystemTextScaleFactor(), kMinTextScaleFactor);
  }
}

TEST(HostWindowWinUtilTest,
     ReadsAndClampsTextScaleFactorFromCurrentUserRegistry) {
  registry_util::RegistryOverrideManager registry_override;
  ASSERT_NO_FATAL_FAILURE(
      registry_override.OverrideRegistry(HKEY_CURRENT_USER));

  // With an empty overridden `HKEY_CURRENT_USER` (no `Accessibility` subkey),
  // `GetSystemTextScaleFactor()` falls back cleanly within bounds.
  EXPECT_GE(GetSystemTextScaleFactor(), kMinTextScaleFactor);
  EXPECT_LE(GetSystemTextScaleFactor(), kMaxTextScaleFactor);

  base::win::RegKey accessibility_key;
  ASSERT_EQ(accessibility_key.Create(HKEY_CURRENT_USER,
                                     L"Software\\Microsoft\\Accessibility",
                                     KEY_SET_VALUE),
            ERROR_SUCCESS);

  ASSERT_EQ(
      accessibility_key.WriteValue(L"TextScaleFactor", static_cast<DWORD>(0)),
      ERROR_SUCCESS);
  EXPECT_GE(GetSystemTextScaleFactor(), kMinTextScaleFactor);
  EXPECT_LE(GetSystemTextScaleFactor(), kMaxTextScaleFactor);

  ASSERT_EQ(
      accessibility_key.WriteValue(L"TextScaleFactor", static_cast<DWORD>(175)),
      ERROR_SUCCESS);
  EXPECT_DOUBLE_EQ(GetSystemTextScaleFactor(), 1.75);

  // An explicit `100` (1.0x) in `TextScaleFactor` takes precedence over any
  // logon-time `SPI_GETNONCLIENTMETRICS` message font scale.
  ASSERT_EQ(
      accessibility_key.WriteValue(L"TextScaleFactor", static_cast<DWORD>(100)),
      ERROR_SUCCESS);
  EXPECT_DOUBLE_EQ(GetSystemTextScaleFactor(), 1.0);

  ASSERT_EQ(
      accessibility_key.WriteValue(L"TextScaleFactor", static_cast<DWORD>(300)),
      ERROR_SUCCESS);
  EXPECT_DOUBLE_EQ(GetSystemTextScaleFactor(), kMaxTextScaleFactor);

  ASSERT_EQ(
      accessibility_key.WriteValue(L"TextScaleFactor", static_cast<DWORD>(50)),
      ERROR_SUCCESS);
  EXPECT_DOUBLE_EQ(GetSystemTextScaleFactor(), kMinTextScaleFactor);
}

TEST(HostWindowWinUtilTest,
     ComputesNonClientMessageFontScaleGuardingCellHeightAndFaceName) {
  LOGFONTW segoe_default = {};
  segoe_default.lfHeight = -12;
  base::wcslcpy(segoe_default.lfFaceName, L"Segoe UI");
  EXPECT_DOUBLE_EQ(ComputeNonClientMessageFontScale(segoe_default), 1.0);

  LOGFONTW segoe_enlarged = {};
  segoe_enlarged.lfHeight = -18;
  base::wcslcpy(segoe_enlarged.lfFaceName, L"segoe ui");
  EXPECT_DOUBLE_EQ(ComputeNonClientMessageFontScale(segoe_enlarged), 1.5);

  LOGFONTW segoe_clamped = {};
  segoe_clamped.lfHeight = -48;
  base::wcslcpy(segoe_clamped.lfFaceName, L"Segoe UI");
  EXPECT_DOUBLE_EQ(ComputeNonClientMessageFontScale(segoe_clamped),
                   kMaxTextScaleFactor);

  // Positive `lfHeight` specifies cell height (including internal leading) and
  // must not overestimate character height.
  LOGFONTW positive_cell_height = segoe_default;
  positive_cell_height.lfHeight = 16;
  EXPECT_DOUBLE_EQ(ComputeNonClientMessageFontScale(positive_cell_height), 1.0);

  // Non-`Segoe UI` locale/OEM message fonts with baselines > 12px must remain
  // at 1.0x rather than enlarging the overlay at 100% Text size.
  LOGFONTW non_segoe_font = {};
  non_segoe_font.lfHeight = -16;
  base::wcslcpy(non_segoe_font.lfFaceName, L"Yu Gothic UI");
  EXPECT_DOUBLE_EQ(ComputeNonClientMessageFontScale(non_segoe_font), 1.0);
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
  ScopedHwnd hwnd(CreateWindowExW(0, L"STATIC", L"Sample Control Text",
                                  WS_POPUP, 0, 0, 200, 40, nullptr, nullptr,
                                  CURRENT_MODULE(), nullptr));
  ASSERT_NE(hwnd.get(), nullptr);

  std::wstring text;
  EXPECT_TRUE(GetWindowTextString(hwnd.get(), &text));
  EXPECT_EQ(text, L"Sample Control Text");

  LONG width = 0;
  EXPECT_TRUE(GetControlTextWidth(hwnd.get(), text, /*is_rtl=*/false, &width));
  EXPECT_GT(width, 0);
}

TEST(HostWindowWinUtilTest, MeasuresWrappedAndUnbrokenControlText) {
  const std::wstring text = L"Sample Control Text";
  ScopedHwnd hwnd(CreateWindowExW(0, L"STATIC", text.c_str(), WS_POPUP, 0, 0,
                                  200, 40, nullptr, nullptr, CURRENT_MODULE(),
                                  nullptr));
  ASSERT_NE(hwnd.get(), nullptr);

  LONG width = 0;
  ASSERT_TRUE(GetControlTextWidth(hwnd.get(), text, /*is_rtl=*/false, &width));
  ASSERT_GT(width, 0);

  SIZE single_line_size = {};
  ASSERT_TRUE(MeasureControlWrappedText(hwnd.get(), text, /*is_rtl=*/false,
                                        width, &single_line_size));
  EXPECT_GT(single_line_size.cx, 0);
  EXPECT_LE(single_line_size.cx, width);
  EXPECT_GT(single_line_size.cy, 0);

  // Constraining `max_width` below `width` wraps multi-word text onto multiple
  // lines, strictly increasing the measured height.
  SIZE wrapped_size = {};
  ASSERT_TRUE(MeasureControlWrappedText(hwnd.get(), text, /*is_rtl=*/false,
                                        std::max<LONG>(1, width / 2),
                                        &wrapped_size));
  EXPECT_GT(wrapped_size.cy, single_line_size.cy);

  // A single unbroken word wider than `max_width` cannot break under
  // `DT_WORDBREAK` and reports `cx > max_width` so callers can widen up to the
  // full monitor bounds.
  const std::wstring single_word = L"UnbrokenSingleWordButtonLabel";
  LONG single_word_width = 0;
  ASSERT_TRUE(GetControlTextWidth(hwnd.get(), single_word, /*is_rtl=*/false,
                                  &single_word_width));
  SIZE unbroken_size = {};
  ASSERT_TRUE(MeasureControlWrappedText(
      hwnd.get(), single_word,
      /*is_rtl=*/false, std::max<LONG>(1, single_word_width / 2),
      &unbroken_size));
  EXPECT_EQ(unbroken_size.cx, single_word_width);

  SIZE unused_size = {};
  EXPECT_FALSE(MeasureControlWrappedText(nullptr, text, /*is_rtl=*/false, width,
                                         &unused_size));
  EXPECT_FALSE(MeasureControlWrappedText(hwnd.get(), L"", /*is_rtl=*/false,
                                         width, &unused_size));
  EXPECT_FALSE(MeasureControlWrappedText(hwnd.get(), text, /*is_rtl=*/false,
                                         /*max_width=*/0, &unused_size));
}

TEST(HostWindowWinUtilTest, AppliesCaptureExclusionToLayeredPopupWindow) {
  base::AutoReset<bool> allow_capture_exclusion_failure =
      ScopedAllowCaptureExclusionFailureForTesting();

  ScopedHwnd hwnd(CreateWindowExW(WS_EX_LAYERED, L"STATIC", L"Layered Popup",
                                  WS_POPUP, 0, 0, 100, 100, nullptr, nullptr,
                                  CURRENT_MODULE(), nullptr));
  ASSERT_NE(hwnd.get(), nullptr);
  ASSERT_TRUE(SetLayeredWindowAttributes(hwnd.get(), 0, 200, LWA_ALPHA));

  EXPECT_TRUE(ApplyCaptureExclusion(hwnd.get()));
  EXPECT_NE(GetWindowLongPtr(hwnd.get(), GWL_EXSTYLE) & WS_EX_LAYERED, 0);

  BYTE alpha = 0;
  DWORD flags = 0;
  ASSERT_TRUE(GetLayeredWindowAttributes(hwnd.get(), nullptr, &alpha, &flags));
  EXPECT_EQ(alpha, 200);
  EXPECT_NE(flags & LWA_ALPHA, 0u);

  DWORD affinity = 0;
  if (GetWindowDisplayAffinity(hwnd.get(), &affinity) && affinity != 0) {
    EXPECT_EQ(affinity, static_cast<DWORD>(WDA_EXCLUDEFROMCAPTURE));
  }
}

TEST(HostWindowWinUtilTest,
     ApplyCaptureExclusionFailsClosedByDefaultAndHonorsTestingOverride) {
  ScopedHwnd parent_hwnd(CreateWindowExW(0, L"STATIC", L"Parent Popup",
                                         WS_POPUP, 0, 0, 100, 100, nullptr,
                                         nullptr, CURRENT_MODULE(), nullptr));
  ASSERT_NE(parent_hwnd.get(), nullptr);

  // `SetWindowDisplayAffinity()` only accepts top-level windows and
  // deterministically fails with `ERROR_INVALID_PARAMETER` on `WS_CHILD`
  // controls regardless of display driver.
  HWND child_hwnd =
      CreateWindowExW(0, L"STATIC", L"Child Control", WS_CHILD, 0, 0, 50, 50,
                      parent_hwnd.get(), nullptr, CURRENT_MODULE(), nullptr);
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
}

TEST(HostWindowWinUtilTest,
     RegisterDisconnectHotKeyRejectsNullAndHonorsTestingOverride) {
  ScopedHwnd hwnd(CreateWindowExW(0, L"STATIC", L"HotKey Target", WS_POPUP, 0,
                                  0, 100, 100, nullptr, nullptr,
                                  CURRENT_MODULE(), nullptr));
  ASSERT_NE(hwnd.get(), nullptr);

  EXPECT_FALSE(RegisterDisconnectHotKey(nullptr));
  EXPECT_EQ(GetLastError(), static_cast<DWORD>(ERROR_INVALID_WINDOW_HANDLE));

  {
    base::AutoReset<std::optional<bool>> succeed_override =
        ScopedOverrideHotKeyRegistrationForTesting(true);
    EXPECT_TRUE(RegisterDisconnectHotKey(hwnd.get()));
    EXPECT_FALSE(RegisterDisconnectHotKey(nullptr));
    UnregisterDisconnectHotKey(hwnd.get());
  }

  {
    base::AutoReset<std::optional<bool>> fail_override =
        ScopedOverrideHotKeyRegistrationForTesting(false);
    EXPECT_FALSE(RegisterDisconnectHotKey(hwnd.get()));
    EXPECT_EQ(GetLastError(),
              static_cast<DWORD>(ERROR_HOTKEY_ALREADY_REGISTERED));
    UnregisterDisconnectHotKey(hwnd.get());
  }
}

TEST(HostWindowWinUtilTest,
     ComputesBackdropContentLayoutWithWrappingHintOmissionAndSmallScreenShift) {
  base::win::ScopedGetDC screen_dc(nullptr);
  ASSERT_NE(static_cast<HDC>(screen_dc), nullptr);

  constexpr UINT kDpi = USER_DEFAULT_SCREEN_DPI;
  constexpr double kBaselineScale = 1.0;
  base::win::ScopedGDIObject<HFONT> title_font = CreateOverlayFont(
      ScaleForDpiAndTextScale(kBaseTitleFontSize, kDpi, kBaselineScale),
      FW_SEMIBOLD);
  base::win::ScopedGDIObject<HFONT> body_font = CreateOverlayFont(
      ScaleForDpiAndTextScale(kBaseBodyFontSize, kDpi, kBaselineScale),
      FW_NORMAL);
  base::win::ScopedGDIObject<HFONT> hint_font = CreateOverlayFont(
      ScaleForDpiAndTextScale(kBaseHintFontSize, kDpi, kBaselineScale),
      FW_NORMAL);
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
      screen_dc, 1920, 1080, kDpi, kBaselineScale, /*is_rtl=*/false,
      title_block, body_block, hint_block);
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
      screen_dc, 1920, 1080, kDpi, kBaselineScale, /*is_rtl=*/false,
      title_block, body_block, {.font = hint_font.get(), .text = L""});
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
      screen_dc, 360, 1080, kDpi, kBaselineScale, /*is_rtl=*/false, title_block,
      {.font = body_font.get(), .text = long_body}, hint_block);
  EXPECT_GT(wrapped.body_height, with_hint.body_height);
  EXPECT_EQ(wrapped.hint_top,
            wrapped.body_top + wrapped.body_height + wrapped.hint_gap);

  // An unusually short monitor shifts `icon_top`, `title_bottom`, and
  // `center_y` downward so the icon never clips above `kBaseVerticalSpacing`.
  BackdropContentLayout short_screen = ComputeBackdropContentLayout(
      screen_dc, 1920, 160, kDpi, kBaselineScale, /*is_rtl=*/true, title_block,
      body_block, hint_block);
  EXPECT_EQ(short_screen.draw_text_flags,
            GetBackdropDrawTextFlags(/*is_rtl=*/true));
  EXPECT_EQ(short_screen.icon_top, kBaseVerticalSpacing);
  EXPECT_GT(short_screen.center_y, 80);
  EXPECT_EQ(short_screen.body_top, short_screen.center_y);
}

}  // namespace

}  // namespace remoting
