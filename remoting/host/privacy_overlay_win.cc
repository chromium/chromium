// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/win/current_module.h"
#include "base/win/scoped_gdi_object.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/disconnect_window_base.h"
#include "remoting/host/host_window.h"
#include "remoting/host/input_monitor/local_input_monitor.h"
#include "remoting/host/win/host_window_win_util.h"

namespace remoting {

namespace {

// Registers the window class used by per-monitor privacy overlay backdrop
// windows if it is not already registered in the current module.
bool RegisterPrivacyOverlayClass(WNDPROC wnd_proc) {
  WNDCLASSEXW existing_wc = {};
  existing_wc.cbSize = sizeof(existing_wc);
  if (GetClassInfoExW(CURRENT_MODULE(), kPrivacyOverlayClassName,
                      &existing_wc)) {
    return true;
  }

  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = wnd_proc;
  wc.hInstance = CURRENT_MODULE();
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kPrivacyOverlayClassName;

  if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    PLOG(ERROR) << "Failed to register Privacy Overlay window class";
    return false;
  }

  return true;
}

// Implements the Windows "Light Curtain" Privacy Overlay (`HostWindow`).
//
// Creates one topmost, unactivated popup window per active monitor. Each
// backdrop is permanently click-through (`WS_EX_LAYERED | WS_EX_TRANSPARENT`)
// and excluded from desktop capture (`WDA_EXCLUDEFROMCAPTURE`). The local
// physical monitors display the branded Google Blue privacy screen, while
// WebRTC/DXGI desktop capture sees right through the overlay to the underlying
// desktop applications, and remote mouse/touch inputs pass straight through
// without obstruction.
class PrivacyOverlayWin : public DisconnectWindowBase {
 public:
  explicit PrivacyOverlayWin(
      std::unique_ptr<LocalInputMonitor> local_input_monitor);

  PrivacyOverlayWin(const PrivacyOverlayWin&) = delete;
  PrivacyOverlayWin& operator=(const PrivacyOverlayWin&) = delete;

  ~PrivacyOverlayWin() override;

  // `HostWindow` implementation.
  void Start(const base::WeakPtr<ClientSessionControl>& client_session_control)
      override;

 private:
  // Initializes the per-monitor privacy overlay backdrop windows.
  bool BeginOverlay();

  // Destroys all backdrop windows and releases cached GDI resources.
  void EndOverlay();

  // Recreates or resizes per-monitor privacy overlay backdrop windows across
  // all active physical displays.
  bool UpdateBackdrops();

  // Fills the privacy overlay backdrop client area into `hdc`.
  void PaintBackdrop(HWND hwnd, HDC hdc);

  // Window procedure for per-monitor backdrop windows (`backdrop_hwnds_`).
  static LRESULT CALLBACK BackdropWndProc(HWND hwnd,
                                          UINT message,
                                          WPARAM wparam,
                                          LPARAM lparam);

  // Retained for subsequent CLs that hook local input monitoring.
  std::unique_ptr<LocalInputMonitor> local_input_monitor_;

  // Re-entrancy guard preventing `EndOverlay()` from running more than once.
  bool is_ending_ = false;

  // Topmost click-through backdrop popup windows, one per active display.
  std::vector<HWND> backdrop_hwnds_;

  // Cached solid brush for the branded Google Blue background.
  base::win::ScopedGDIObject<HBRUSH> google_blue_brush_;
};

PrivacyOverlayWin::PrivacyOverlayWin(
    std::unique_ptr<LocalInputMonitor> local_input_monitor)
    : local_input_monitor_(std::move(local_input_monitor)),
      google_blue_brush_(CreateSolidBrush(kGoogleBlueColor)) {}

PrivacyOverlayWin::~PrivacyOverlayWin() {
  EndOverlay();
}

void PrivacyOverlayWin::Start(
    const base::WeakPtr<ClientSessionControl>& client_session_control) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_session_control);

  DisconnectWindowBase::Start(client_session_control);

  if (!BeginOverlay()) {
    EndOverlay();
    DisconnectSession("Failed to initialize Privacy Overlay.");
  }
}

bool PrivacyOverlayWin::BeginOverlay() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!RegisterPrivacyOverlayClass(&BackdropWndProc)) {
    return false;
  }

  return UpdateBackdrops();
}

void PrivacyOverlayWin::EndOverlay() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_ending_) {
    return;
  }
  is_ending_ = true;

  std::vector<HWND> backdrop_hwnds = std::exchange(backdrop_hwnds_, {});
  for (HWND backdrop_hwnd : backdrop_hwnds) {
    if (backdrop_hwnd && IsWindow(backdrop_hwnd)) {
      SetWindowLongPtr(backdrop_hwnd, GWLP_USERDATA, 0);
      DestroyWindow(backdrop_hwnd);
    }
  }
}

bool PrivacyOverlayWin::UpdateBackdrops() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<RECT> monitor_rects = GetActiveMonitorRects();
  if (monitor_rects.empty()) {
    return false;
  }

  // Destroy any surplus backdrop windows if monitors were disconnected.
  while (backdrop_hwnds_.size() > monitor_rects.size()) {
    HWND surplus = backdrop_hwnds_.back();
    backdrop_hwnds_.pop_back();
    if (surplus && IsWindow(surplus)) {
      SetWindowLongPtr(surplus, GWLP_USERDATA, 0);
      DestroyWindow(surplus);
    }
  }

  constexpr DWORD kExStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED |
                             WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;

  for (size_t i = 0; i < monitor_rects.size(); ++i) {
    const RECT& rect = monitor_rects[i];
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    HWND backdrop_hwnd = nullptr;
    if (i < backdrop_hwnds_.size() && IsWindow(backdrop_hwnds_[i])) {
      backdrop_hwnd = backdrop_hwnds_[i];
      SetWindowPos(backdrop_hwnd, HWND_TOPMOST, rect.left, rect.top, width,
                   height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
      InvalidateRect(backdrop_hwnd, nullptr, FALSE);
    } else {
      backdrop_hwnd = CreateWindowExW(
          kExStyle, kPrivacyOverlayClassName, L"", WS_POPUP, rect.left,
          rect.top, width, height, nullptr, nullptr, CURRENT_MODULE(), this);
      if (!backdrop_hwnd) {
        PLOG(ERROR) << "Failed to create Privacy Overlay backdrop window";
        return false;
      }

      // Configure layered opacity and exclude the backdrop from screen capture
      // before showing so remote viewers never see a flash of the overlay.
      SetLayeredWindowAttributes(backdrop_hwnd, 0, 255, LWA_ALPHA);
      if (!ApplyCaptureExclusion(backdrop_hwnd)) {
        SetWindowLongPtr(backdrop_hwnd, GWLP_USERDATA, 0);
        DestroyWindow(backdrop_hwnd);
        return false;
      }
      ShowWindow(backdrop_hwnd, SW_SHOWNOACTIVATE);
      UpdateWindow(backdrop_hwnd);

      if (i < backdrop_hwnds_.size()) {
        backdrop_hwnds_[i] = backdrop_hwnd;
      } else {
        backdrop_hwnds_.push_back(backdrop_hwnd);
      }
    }
  }

  return true;
}

void PrivacyOverlayWin::PaintBackdrop(HWND hwnd, HDC hdc) {
  RECT client_rect;
  if (!GetClientRect(hwnd, &client_rect)) {
    return;
  }

  HBRUSH bg_brush = IsHighContrastEnabled() ? GetSysColorBrush(COLOR_WINDOW)
                                            : google_blue_brush_.get();
  FillRect(hdc, &client_rect, bg_brush);
}

// static
LRESULT CALLBACK PrivacyOverlayWin::BackdropWndProc(HWND hwnd,
                                                    UINT message,
                                                    WPARAM wparam,
                                                    LPARAM lparam) {
  PrivacyOverlayWin* self = nullptr;
  if (message == WM_NCCREATE) {
    auto* create_struct = reinterpret_cast<CREATESTRUCT*>(lparam);
    self = static_cast<PrivacyOverlayWin*>(create_struct->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<PrivacyOverlayWin*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }

  switch (message) {
    // Ignore close requests on backdrop windows so the privacy curtain cannot
    // be dismissed without disconnecting the session.
    case WM_CLOSE:
      return 0;

    // Never activate a backdrop window when clicked.
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;

    // Pass all mouse hit-testing through to underlying desktop windows.
    case WM_NCHITTEST:
      return HTTRANSPARENT;

    // Background erasure is handled entirely inside `PaintBackdrop()` to
    // prevent flicker.
    case WM_ERASEBKGND:
      return 1;

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      if (self) {
        self->PaintBackdrop(hwnd, hdc);
      }
      EndPaint(hwnd, &ps);
      return 0;
    }

    case WM_PRINTCLIENT: {
      HDC hdc = reinterpret_cast<HDC>(wparam);
      if (self && hdc) {
        self->PaintBackdrop(hwnd, hdc);
      }
      return 0;
    }

      // TODO(crbug.com/451238243): Handle WM_DISPLAYCHANGE, WM_DPICHANGED,
      // WM_SETTINGCHANGE, and WM_SYSCOLORCHANGE in a follow-up CL so backdrops
      // dynamically update when display topology, DPI, or High Contrast
      // settings change during an active session.
  }

  return DefWindowProc(hwnd, message, wparam, lparam);
}

}  // namespace

// static
std::unique_ptr<HostWindow> HostWindow::CreatePrivacyOverlay(
    std::unique_ptr<LocalInputMonitor> local_input_monitor) {
  return std::make_unique<PrivacyOverlayWin>(std::move(local_input_monitor));
}

}  // namespace remoting
