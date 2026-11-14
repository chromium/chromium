// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_HOST_WINDOW_WIN_UTIL_H_
#define REMOTING_HOST_WIN_HOST_WINDOW_WIN_UTIL_H_

#include <windows.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/auto_reset.h"
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
inline constexpr int kBaseHintFontSize = 14;
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
// screen capture. In production, returns false whenever capture exclusion fails
// so `PrivacyOverlayWin` fails closed rather than displaying an opaque curtain
// that would be captured and streamed to the remote client.
bool ApplyCaptureExclusion(HWND hwnd);

// Allows `ApplyCaptureExclusion()` to succeed in unit tests running on headless
// bots where `SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)` fails with
// `ERROR_INVALID_PARAMETER`, `ERROR_NOT_SUPPORTED`, or `ERROR_ACCESS_DENIED`.
base::AutoReset<bool> ScopedAllowCaptureExclusionFailureForTesting();

// Registers the Ctrl+Alt+Esc disconnect shortcut (`kDisconnectHotkeyId`) for
// `hwnd`. Returns false if `hwnd` is null or `RegisterHotKey()` fails.
bool RegisterDisconnectHotKey(HWND hwnd);

// Unregisters the Ctrl+Alt+Esc disconnect shortcut (`kDisconnectHotkeyId`)
// previously registered for `hwnd`.
void UnregisterDisconnectHotKey(HWND hwnd);

// Overrides `RegisterDisconnectHotKey()` and `UnregisterDisconnectHotKey()` in
// unit tests so parallel test-launcher worker processes sharing an interactive
// desktop (`WinSta0\Default`) do not contend on the desktop-global Win32
// `RegisterHotKey()` table. Because `UnregisterDisconnectHotKey()` is a no-op
// while an override is active, the returned `base::AutoReset` must span the
// full `RegisterDisconnectHotKey()` / `UnregisterDisconnectHotKey()` lifetime.
base::AutoReset<std::optional<bool>> ScopedOverrideHotKeyRegistrationForTesting(
    bool succeed);

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

// Pairs a GDI `font` handle with its localized `text` for a single centered
// text band on the privacy overlay backdrop.
struct BackdropTextBlock {
  HFONT font = nullptr;
  std::wstring_view text;
};

// Returns the `DrawTextW()` format flags used to both measure and render
// centered backdrop text blocks.
UINT GetBackdropDrawTextFlags(bool is_rtl);

// Computed layout geometry (in physical client/monitor pixels) and `DrawTextW`
// flags for the centered icon, product title, sharing message, and optional
// disconnect hotkey hint rendered on each per-monitor privacy overlay backdrop.
struct BackdropContentLayout {
  UINT draw_text_flags = 0;
  int max_text_width = 0;
  int text_left = 0;
  int text_right = 0;
  int icon_size = 0;
  int icon_left = 0;
  int icon_top = 0;
  int title_top = 0;
  int title_bottom = 0;
  int title_height = 0;
  int center_y = 0;
  int body_top = 0;
  int body_height = 0;
  int hint_gap = 0;
  int hint_top = 0;
  int hint_height = 0;
  int below_center_height = 0;
};

// Computes the centered backdrop content layout for a `client_width` x
// `client_height` surface at `dpi`. Shared by
// `PrivacyOverlayWin::PaintBackdrop` and
// `PrivacyOverlayWin::PositionDisconnectButton` (as well as unit tests) so
// Disconnect button placement and backdrop text rendering never drift out of
// sync.
BackdropContentLayout ComputeBackdropContentLayout(HDC dc,
                                                   int client_width,
                                                   int client_height,
                                                   UINT dpi,
                                                   bool is_rtl,
                                                   BackdropTextBlock title,
                                                   BackdropTextBlock body,
                                                   BackdropTextBlock hint);

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_HOST_WINDOW_WIN_UTIL_H_
