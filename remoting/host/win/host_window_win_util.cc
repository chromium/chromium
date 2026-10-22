// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/host_window_win_util.h"

#include <shellscalingapi.h>

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

  if (error == ERROR_INVALID_PARAMETER || error == ERROR_NOT_SUPPORTED ||
      error == ERROR_ACCESS_DENIED) {
    return true;
  }
  return false;
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

}  // namespace remoting
