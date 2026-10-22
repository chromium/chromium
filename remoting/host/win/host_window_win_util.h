// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_HOST_WINDOW_WIN_UTIL_H_
#define REMOTING_HOST_WIN_HOST_WINDOW_WIN_UTIL_H_

#include <windows.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/time/time.h"
#include "base/win/scoped_gdi_object.h"

namespace remoting {

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

// Identifier registered with `RegisterHotKey()` for the Ctrl+Alt+Esc shortcut.
inline constexpr int kDisconnectHotkeyId = 1000;

// Window class name used for per-monitor privacy overlay backdrop windows.
inline constexpr wchar_t kPrivacyOverlayClassName[] =
    L"ChromotingPrivacyOverlay";

// Fallback font face name if `SPI_GETNONCLIENTMETRICS` fails.
inline constexpr wchar_t kFallbackFontName[] = L"Segoe UI";

// Branded Google Blue background color and white foreground color used when
// Windows High Contrast mode is not active.
inline constexpr COLORREF kGoogleBlueColor = RGB(26, 115, 232);
inline constexpr COLORREF kWhiteColor = RGB(255, 255, 255);

// Corner radius (in 96-DPI base pixels) for the Disconnect window and button
// dialog.
inline constexpr int kBaseWindowBorderRadius = 14;

// Border stroke width (in 96-DPI base pixels) around the Disconnect button
// dialog so it stands out against the backdrop.
inline constexpr int kBaseButtonBorderWidth = 2;

// Margin between dialog controls (in dialog units) and fallback pixel values
// at 96 DPI when dialog font metrics cannot be queried.
inline constexpr int kWindowTextMargin = 8;
inline constexpr int kDefaultControlMarginPx = 12;
inline constexpr int kDefaultButtonTextWidthPx = 100;
inline constexpr int kDefaultButtonHeightPx = 24;
inline constexpr int kBaseButtonFontSize = 14;

// Layout constants (in 96-DPI base pixels) for the privacy overlay backdrop
// and floating Disconnect button dialog.
inline constexpr int kBaseIconSize = 64;
inline constexpr int kBaseTitleFontSize = 28;
inline constexpr int kBaseBodyFontSize = 18;
inline constexpr int kBaseVerticalSpacing = 16;
inline constexpr int kBaseButtonTopOffset = 72;

// Maximum width ratio (numerator / denominator) of the monitor width allocated
// to centered text blocks on the backdrop before word-wrapping.
inline constexpr int kMaxTextWidthNumerator = 4;
inline constexpr int kMaxTextWidthDenominator = 5;

// Minimum movement threshold (in physical screen pixels) required before local
// pointer activity reveals the Disconnect window or button, filtering out 1px
// hardware or software cursor jitter.
inline constexpr int kMouseJitterThresholdPx = 1;

// The amount of time to wait before hiding the Disconnect window or button
// after local input stops.
inline constexpr base::TimeDelta kAutoHideTimeout = base::Seconds(10);

// Scales a 96-DPI base pixel measurement to the specified `dpi`.
int ScaleForDpi(int value, UINT dpi);

// Queries the effective per-monitor DPI for `monitor`, falling back to the
// window DPI of `hwnd` or `USER_DEFAULT_SCREEN_DPI` (96) if unavailable.
UINT GetMonitorDpi(HMONITOR monitor, HWND hwnd);

// Queries whether a Windows High Contrast accessibility theme is currently
// active. When active, system colors (`COLOR_WINDOW` / `COLOR_WINDOWTEXT`) must
// be used instead of hardcoded branding colors.
bool IsHighContrastEnabled();

// Applies `WDA_EXCLUDEFROMCAPTURE` to `hwnd` so the window is excluded from
// screen capture on a real display driver. On headless virtual display drivers
// (such as Windows CQ test bots) where `SetWindowDisplayAffinity()` returns
// FALSE with `ERROR_INVALID_PARAMETER`, `ERROR_NOT_SUPPORTED`, or
// `ERROR_ACCESS_DENIED`, logs an error and allows the window to proceed; on
// real display drivers where exclusion unexpectedly fails, returns false to
// fail closed rather than capturing the opaque curtain.
bool ApplyCaptureExclusion(HWND hwnd);

// Enumerates the virtual-screen bounding rectangles of all active physical
// displays, falling back to `GetSystemMetrics` virtual/primary screen metrics
// if `EnumDisplayMonitors()` returns no monitors (e.g. on headless bots).
std::vector<RECT> GetActiveMonitorRects();

// Retrieves the full localized text of `hwnd` without fixed-buffer truncation.
bool GetWindowTextString(HWND hwnd, std::wstring* text);

// Measures the single-line pixel width of `text` when rendered in `control`'s
// current font.
bool GetControlTextWidth(HWND control,
                         std::wstring_view text,
                         bool is_rtl,
                         LONG* width);

// Loads a localized string resource from the current module's `STRINGTABLE`
// using a zero-copy read-only resource pointer.
std::wstring LoadModuleString(UINT resource_id);

// Creates a GDI font derived from the system UI message font (via
// `SPI_GETNONCLIENTMETRICS`) at the specified pixel height and weight so CJK,
// Indic, and RTL scripts use the appropriate localized system font family and
// `DEFAULT_CHARSET` GDI font linking.
base::win::ScopedGDIObject<HFONT> CreateOverlayFont(int pixel_height,
                                                    LONG weight);

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_HOST_WINDOW_WIN_UTIL_H_
