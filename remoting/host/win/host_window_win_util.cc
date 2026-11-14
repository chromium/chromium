// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/host_window_win_util.h"

#include <shellscalingapi.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/win/current_module.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"

namespace remoting {

namespace {

bool g_allow_capture_exclusion_failure_for_testing = false;
std::optional<bool> g_hotkey_registration_override_for_testing;

// Callback for `EnumDisplayMonitors()` that collects the virtual-screen
// bounding rectangle of each active physical display.
BOOL CALLBACK EnumMonitorsCallback(HMONITOR monitor,
                                   HDC hdc,
                                   LPRECT rect,
                                   LPARAM data) {
  if (rect) {
    auto* monitor_rects = reinterpret_cast<std::vector<RECT>*>(data);
    monitor_rects->push_back(*rect);
  }
  return TRUE;
}

// On some Windows 10 configurations (e.g. headless test VMs), calling
// `SetWindowDisplayAffinity()` while `WS_EX_LAYERED` is set fails in
// `win32kfull!GreProtectSpriteContent` with `ERROR_NOT_ENOUGH_MEMORY`.
// Temporarily clears `WS_EX_LAYERED` to apply capture exclusion, then restores
// the layered window style and attributes. Sets `*out_error` to the error code
// from the unlayered `SetWindowDisplayAffinity()` call.
bool RetryCaptureExclusionWithoutLayeredStyle(HWND hwnd,
                                              LONG_PTR ex_style,
                                              DWORD* out_error) {
  DCHECK(out_error);
  COLORREF cr_key = 0;
  BYTE alpha = 255;
  DWORD flags = LWA_ALPHA;
  if (!GetLayeredWindowAttributes(hwnd, &cr_key, &alpha, &flags)) {
    alpha = 255;
    flags = LWA_ALPHA;
  }

  SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex_style & ~WS_EX_LAYERED);
  // Clear the thread's last-error code before calling the Win32 API because
  // Windows APIs are not guaranteed to reset `GetLastError()` on success or on
  // certain internal failure paths.
  SetLastError(ERROR_SUCCESS);
  bool succeeded = SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
  *out_error = GetLastError();

  // Restore the layered window style and attributes. Restoring `WS_EX_LAYERED`
  // preserves the display affinity applied above while the window was
  // unlayered.
  SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex_style);
  SetLayeredWindowAttributes(hwnd, cr_key, alpha, flags);
  return succeeded;
}

// Measures the wrapped pixel height of `text` within `max_width` using `font`,
// returning at least `min_height` when `text` is non-empty.
int MeasureWrappedTextHeight(HDC dc,
                             HFONT font,
                             std::wstring_view text,
                             int max_width,
                             int min_height,
                             UINT draw_text_flags) {
  if (text.empty()) {
    return 0;
  }
  if (!dc || !font || max_width <= 0) {
    return min_height;
  }
  base::win::ScopedSelectObject select_font(dc, font);
  RECT calc_rect = {0, 0, max_width, 0};
  if (DrawTextW(dc, text.data(), static_cast<int>(text.size()), &calc_rect,
                draw_text_flags | DT_CALCRECT)) {
    return std::max(min_height,
                    static_cast<int>(calc_rect.bottom - calc_rect.top));
  }
  return min_height;
}

}  // namespace

int ScaleForDpi(int value, UINT dpi) {
  return MulDiv(value, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
}

UINT GetMonitorDpi(HMONITOR monitor, HWND hwnd) {
  if (monitor) {
    UINT dpi_x = USER_DEFAULT_SCREEN_DPI;
    UINT dpi_y = USER_DEFAULT_SCREEN_DPI;
    if (SUCCEEDED(
            GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y)) &&
        dpi_x > 0) {
      return dpi_x;
    }
  }
  if (hwnd) {
    UINT window_dpi = GetDpiForWindow(hwnd);
    if (window_dpi > 0) {
      return window_dpi;
    }
  }
  return USER_DEFAULT_SCREEN_DPI;
}

bool IsHighContrastEnabled() {
  HIGHCONTRASTW hc = {};
  hc.cbSize = sizeof(hc);
  if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0)) {
    return (hc.dwFlags & HCF_HIGHCONTRASTON) != 0;
  }
  return false;
}

bool ApplyCaptureExclusion(HWND hwnd) {
  // Clear the thread's last-error code before calling
  // `SetWindowDisplayAffinity` because Win32 APIs do not always clear
  // `GetLastError()` on success, and we inspect `GetLastError()` below to
  // distinguish benign headless-bot errors from real display driver failures.
  SetLastError(ERROR_SUCCESS);
  if (SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)) {
    return true;
  }

  DWORD error = GetLastError();
  LONG_PTR ex_style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
  if (error == ERROR_NOT_ENOUGH_MEMORY && (ex_style & WS_EX_LAYERED) != 0) {
    if (RetryCaptureExclusionWithoutLayeredStyle(hwnd, ex_style, &error)) {
      return true;
    }
  }

  SetLastError(error);
  PLOG(ERROR) << "SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE) failed";

  if (g_allow_capture_exclusion_failure_for_testing &&
      (error == ERROR_INVALID_PARAMETER || error == ERROR_NOT_SUPPORTED ||
       error == ERROR_ACCESS_DENIED)) {
    return true;
  }
  return false;
}

base::AutoReset<bool> ScopedAllowCaptureExclusionFailureForTesting() {
  return base::AutoReset<bool>(&g_allow_capture_exclusion_failure_for_testing,
                               true);
}

bool RegisterDisconnectHotKey(HWND hwnd) {
  if (!hwnd) {
    SetLastError(ERROR_INVALID_WINDOW_HANDLE);
    return false;
  }
  if (g_hotkey_registration_override_for_testing.has_value()) {
    if (!*g_hotkey_registration_override_for_testing) {
      SetLastError(ERROR_HOTKEY_ALREADY_REGISTERED);
    }
    return *g_hotkey_registration_override_for_testing;
  }
  return RegisterHotKey(hwnd, kDisconnectHotkeyId, MOD_ALT | MOD_CONTROL,
                        VK_ESCAPE) != FALSE;
}

void UnregisterDisconnectHotKey(HWND hwnd) {
  if (!hwnd || g_hotkey_registration_override_for_testing.has_value()) {
    return;
  }
  UnregisterHotKey(hwnd, kDisconnectHotkeyId);
}

base::AutoReset<std::optional<bool>> ScopedOverrideHotKeyRegistrationForTesting(
    bool succeed) {
  return base::AutoReset<std::optional<bool>>(
      &g_hotkey_registration_override_for_testing, succeed);
}

std::vector<RECT> GetActiveMonitorRects() {
  std::vector<RECT> monitor_rects;
  EnumDisplayMonitors(nullptr, nullptr, &EnumMonitorsCallback,
                      reinterpret_cast<LPARAM>(&monitor_rects));
  if (monitor_rects.empty()) {
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (vw <= 0 || vh <= 0) {
      vx = 0;
      vy = 0;
      vw = GetSystemMetrics(SM_CXSCREEN);
      vh = GetSystemMetrics(SM_CYSCREEN);
    }
    if (vw > 0 && vh > 0) {
      RECT virtual_rect = {vx, vy, vx + vw, vy + vh};
      monitor_rects.push_back(virtual_rect);
    }
  }
  return monitor_rects;
}

bool GetWindowTextString(HWND hwnd, std::wstring* text) {
  DCHECK(text);
  int length = GetWindowTextLengthW(hwnd);
  if (length <= 0) {
    return false;
  }

  std::wstring buffer(static_cast<size_t>(length) + 1, L'\0');
  int copied = GetWindowTextW(hwnd, buffer.data(), length + 1);
  if (copied <= 0) {
    return false;
  }

  buffer.resize(static_cast<size_t>(copied));
  *text = std::move(buffer);
  return true;
}

bool GetControlTextWidth(HWND control,
                         std::wstring_view text,
                         bool is_rtl,
                         LONG* width) {
  DCHECK(width);
  if (text.empty()) {
    return false;
  }

  RECT rect = {0, 0, 0, 0};
  base::win::ScopedGetDC dc(control);
  if (!dc) {
    return false;
  }

  HFONT font = reinterpret_cast<HFONT>(SendMessage(control, WM_GETFONT, 0, 0));
  std::optional<base::win::ScopedSelectObject> select_font;
  if (font) {
    select_font.emplace(dc, font);
  }

  UINT flags = DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX;
  if (is_rtl) {
    flags |= DT_RTLREADING;
  }
  if (!DrawTextW(dc, text.data(), static_cast<int>(text.size()), &rect,
                 flags)) {
    return false;
  }

  *width = rect.right - rect.left;
  return true;
}

std::wstring LoadModuleString(UINT resource_id) {
  const wchar_t* string_ptr = nullptr;
  int length = LoadStringW(CURRENT_MODULE(), resource_id,
                           reinterpret_cast<wchar_t*>(&string_ptr), 0);
  if (length > 0 && string_ptr) {
    return std::wstring(string_ptr, static_cast<size_t>(length));
  }
  return std::wstring();
}

base::win::ScopedGDIObject<HFONT> CreateOverlayFont(int pixel_height,
                                                    LONG weight) {
  NONCLIENTMETRICSW metrics = {};
  metrics.cbSize = sizeof(metrics);
  LOGFONTW logfont = {};
  if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics,
                            0)) {
    logfont = metrics.lfMessageFont;
  } else {
    base::wcslcpy(logfont.lfFaceName, kFallbackFontName,
                  std::size(logfont.lfFaceName));
  }
  logfont.lfHeight = -std::abs(pixel_height);
  logfont.lfWidth = 0;
  logfont.lfEscapement = 0;
  logfont.lfOrientation = 0;
  logfont.lfWeight = weight;
  logfont.lfItalic = FALSE;
  logfont.lfUnderline = FALSE;
  logfont.lfStrikeOut = FALSE;
  logfont.lfCharSet = DEFAULT_CHARSET;
  logfont.lfQuality = CLEARTYPE_QUALITY;
  return base::win::ScopedGDIObject<HFONT>(CreateFontIndirectW(&logfont));
}

UINT GetBackdropDrawTextFlags(bool is_rtl) {
  UINT flags = DT_CENTER | DT_WORDBREAK | DT_EDITCONTROL | DT_EXTERNALLEADING |
               DT_NOPREFIX;
  if (is_rtl) {
    flags |= DT_RTLREADING;
  }
  return flags;
}

BackdropContentLayout ComputeBackdropContentLayout(HDC dc,
                                                   int client_width,
                                                   int client_height,
                                                   UINT dpi,
                                                   bool is_rtl,
                                                   BackdropTextBlock title,
                                                   BackdropTextBlock body,
                                                   BackdropTextBlock hint) {
  if (dpi == 0) {
    dpi = USER_DEFAULT_SCREEN_DPI;
  }

  BackdropContentLayout layout;
  layout.draw_text_flags = GetBackdropDrawTextFlags(is_rtl);
  int spacing = ScaleForDpi(kBaseVerticalSpacing, dpi);
  layout.icon_size = ScaleForDpi(kBaseIconSize, dpi);

  int center_x = client_width / 2;
  int center_y = client_height / 2;
  layout.max_text_width = std::max(
      1, (client_width * kMaxTextWidthNumerator) / kMaxTextWidthDenominator);
  layout.text_left = center_x - layout.max_text_width / 2;
  layout.text_right = layout.text_left + layout.max_text_width;

  // Use the unpadded font pixel size as the fallback minimum height for each
  // non-empty block so single-line and multi-line sharing messages maintain a
  // uniform `spacing / 2` visual gap above `hint.text`.
  layout.title_height = MeasureWrappedTextHeight(
      dc, title.font, title.text, layout.max_text_width,
      ScaleForDpi(kBaseTitleFontSize, dpi), layout.draw_text_flags);
  layout.body_height = MeasureWrappedTextHeight(
      dc, body.font, body.text, layout.max_text_width,
      ScaleForDpi(kBaseBodyFontSize, dpi), layout.draw_text_flags);
  layout.hint_height = MeasureWrappedTextHeight(
      dc, hint.font, hint.text, layout.max_text_width,
      ScaleForDpi(kBaseHintFontSize, dpi), layout.draw_text_flags);
  layout.hint_gap =
      (layout.body_height > 0 && layout.hint_height > 0) ? (spacing / 2) : 0;
  layout.below_center_height =
      layout.body_height + layout.hint_gap + layout.hint_height;

  int title_bottom = center_y - spacing;
  int icon_top =
      title_bottom - layout.title_height - spacing - layout.icon_size;
  if (client_height > 0 && icon_top < spacing) {
    int shift_down = spacing - icon_top;
    icon_top = spacing;
    title_bottom += shift_down;
    center_y += shift_down;
  }

  layout.icon_left = center_x - layout.icon_size / 2;
  layout.icon_top = icon_top;
  layout.title_bottom = title_bottom;
  layout.title_top = title_bottom - layout.title_height;
  layout.center_y = center_y;
  layout.body_top = center_y;
  layout.hint_top = center_y + layout.body_height + layout.hint_gap;
  return layout;
}

}  // namespace remoting
