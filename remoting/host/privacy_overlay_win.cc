// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/current_module.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/disconnect_window_base.h"
#include "remoting/host/host_window.h"
#include "remoting/host/input_monitor/local_input_monitor.h"
#include "remoting/host/win/core_resource.h"
#include "remoting/host/win/host_window_win_util.h"
#include "remoting/host/win/input_extra_info.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/event.h"

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
// Architecture:
// 1. Per-Monitor Backdrop Windows (`backdrop_hwnds_`):
//    One topmost, unactivated popup window per active monitor. Each backdrop is
//    permanently click-through (`WS_EX_LAYERED | WS_EX_TRANSPARENT`) and
//    excluded from desktop capture (`WDA_EXCLUDEFROMCAPTURE`). The local
//    physical monitors display the branded Google Blue privacy screen
//    (localized product name, localized sharing message, and localized
//    Ctrl+Alt+Esc disconnect shortcut hint), while WebRTC/DXGI desktop capture
//    sees right through the overlay to the underlying desktop applications, and
//    remote mouse/touch inputs pass straight through without obstruction.
// 2. On-Demand Floating Disconnect Button Dialog (`hwnd_`):
//    A single topmost dialog created from the localized `IDD_DISCONNECT`
//    resource and resized to wrap only the "Stop Sharing" button
//    (`IDC_DISCONNECT`). In its idle state, `hwnd_` is hidden and click-through
//    (`WS_EX_TRANSPARENT`, alpha = 0) so remote clicks at the button's screen
//    coordinates cannot accidentally disconnect the session. When local
//    physical pointer or keyboard input is detected via `local_input_monitor_`,
//    `hwnd_` becomes visible and clickable on the target monitor for
//    `kAutoHideTimeout` (10s). As defense-in-depth, any
//    `WM_COMMAND(IDC_DISCONNECT)` message tagged with `kCrdInputExtraInfo`
//    (synthesized by CRD's input injector) is ignored.
//
// Inherits from `DisconnectWindowBase` to share client email formatting,
// session disconnection callbacks, and anti-displacement
// (`WM_WINDOWPOSCHANGING`) logic.
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
  // Holds DPI-scaled GDI resources for a specific monitor DPI so multi-monitor
  // setups with mixed DPIs do not thrash or recreate icons/fonts on every
  // `WM_PAINT`.
  struct BackdropDpiResources {
    BackdropDpiResources();
    BackdropDpiResources(BackdropDpiResources&&);
    BackdropDpiResources& operator=(BackdropDpiResources&&);
    ~BackdropDpiResources();

    // Note: `base::win::ScopedGDIObject<HICON>` is specialized in
    // `base/win/scoped_gdi_object.cc` to call `::DestroyIcon()` rather than
    // `::DeleteObject()`.
    base::win::ScopedGDIObject<HICON> crd_icon;
    base::win::ScopedGDIObject<HFONT> title_font;
    base::win::ScopedGDIObject<HFONT> body_font;
    base::win::ScopedGDIObject<HFONT> hint_font;
  };

  // Dialog procedure for the floating Disconnect button dialog (`hwnd_`).
  static INT_PTR CALLBACK DialogProc(HWND hwnd,
                                     UINT message,
                                     WPARAM wparam,
                                     LPARAM lparam);

  // Handles Win32 messages dispatched to the Disconnect button dialog.
  BOOL OnDialogMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  // Creates the Disconnect button dialog, registers the Ctrl+Alt+Esc hotkey,
  // and initializes the per-monitor privacy overlay backdrop windows.
  bool BeginOverlay();

  // Destroys all backdrop and button windows, unregisters the disconnect
  // hotkey, stops the auto-hide timer, and releases cached GDI resources.
  void EndOverlay();

  // Returns `control`'s bounding rectangle in `hwnd_`'s client coordinates.
  bool GetControlRect(HWND control, RECT* rect);

  // Resizes the Disconnect button dialog (`hwnd_`), its child button control
  // (`IDC_DISCONNECT`), its DPI-scaled button font, and its rounded window
  // region to match `dpi`.
  void UpdateButtonLayoutForDpi(UINT dpi);

  // Returns the cached backdrop fonts and icon for `dpi`, creating and
  // inserting them into `backdrop_dpi_cache_` if not yet present.
  const BackdropDpiResources& GetBackdropResourcesForDpi(UINT dpi);

  // Computes the centered backdrop content layout for a `width` x `height`
  // surface at `dpi` using cached DPI fonts and current localized strings.
  BackdropContentLayout ComputeBackdropLayout(HDC dc,
                                              int width,
                                              int height,
                                              UINT dpi);

  // Draws the background and contrasting rounded border of the Disconnect
  // button dialog into `hdc`.
  void PaintButtonDialog(HWND hwnd, HDC hdc);

  // Recreates or resizes per-monitor privacy overlay backdrop windows across
  // all active physical displays.
  bool UpdateBackdrops();

  // Positions and DPI-scales the Disconnect button window on the monitor
  // containing `screen_point`. If `explicit_dpi` is non-zero and `screen_point`
  // is on `hwnd_`'s current monitor, `explicit_dpi` overrides the queried
  // monitor DPI.
  void PositionDisconnectButton(POINT screen_point, UINT explicit_dpi = 0);

  // Shows and enables click-targeting on the Disconnect button window in
  // response to local physical pointer, touch, or keyboard activity.
  void ActivateDisconnectButton(POINT screen_point);

  // Hides and restores click-through (`WS_EX_TRANSPARENT`) on the Disconnect
  // button window when the active timeout expires.
  void DeactivateDisconnectButton();

  // Permanently reveals the Disconnect button if local input monitoring fails.
  void StopAutoHideBehavior();

  // Called when a local mouse or touch event is observed by
  // `local_input_monitor_`.
  void OnLocalMouseEvent(const webrtc::DesktopVector& position,
                         ui::EventType type);

  // Called when a local keyboard event is observed by `local_input_monitor_`.
  void OnLocalKeyPressed(uint32_t usb_keycode);

  // Renders the privacy overlay backdrop content (icon, localized product
  // title, localized sharing message, and localized disconnect shortcut hint)
  // into `hdc` using an offscreen double buffer for the central content region.
  void PaintBackdrop(HWND hwnd, HDC hdc);

  // Window procedure for per-monitor backdrop windows (`backdrop_hwnds_`).
  static LRESULT CALLBACK BackdropWndProc(HWND hwnd,
                                          UINT message,
                                          WPARAM wparam,
                                          LPARAM lparam);

  // Monitors local hardware mouse, touch, and keyboard input to wake the
  // Disconnect button on demand.
  std::unique_ptr<LocalInputMonitor> local_input_monitor_;

  // Re-entrancy guard preventing `EndOverlay()` from running twice when
  // `DestroyWindow()` synchronously dispatches `WM_DESTROY`.
  bool is_ending_ = false;

  // True when `hwnd_` is in its hidden, click-through idle state.
  bool is_button_hidden_ = false;

  // True once `mouse_position_` has been initialized from either
  // `GetCursorPos()` or the first local pointer event.
  bool has_initial_mouse_position_ = false;
  // True if the localized dialog resource or process UI language uses a
  // right-to-left (RTL) reading layout.
  bool is_rtl_ = false;

  // Timer that returns the Disconnect button to its hidden click-through state
  // after `kAutoHideTimeout` of local input inactivity.
  base::OneShotTimer auto_hide_timer_;

  // The floating Disconnect button dialog created from `IDD_DISCONNECT`.
  HWND hwnd_ = nullptr;

  // The monitor on which `hwnd_` is currently positioned and its cached screen
  // rectangle (used to avoid redundant `MonitorFromPoint()` calls on every
  // pointer movement within the same monitor).
  HMONITOR active_button_monitor_ = nullptr;
  RECT active_button_monitor_rect_ = {0, 0, 0, 0};

  // The DPI for which `hwnd_`'s current size, font, border pen, and rounded
  // region were computed.
  UINT active_button_dpi_ = 0;

  // Cached DPI-scaled font for `IDC_DISCONNECT` and border pen for `hwnd_`.
  base::win::ScopedGDIObject<HFONT> button_font_;
  base::win::ScopedGDIObject<HPEN> button_border_pen_;
  bool cached_button_high_contrast_ = false;

  // Base 96-DPI dimensions of the Disconnect button and its dialog margin,
  // captured during `BeginOverlay()` before DPI scaling is applied.
  int base_button_margin_ = kDefaultControlMarginPx;
  int base_button_height_ = kDefaultButtonHeightPx;

  // Topmost click-through backdrop popup windows, one per active display.
  std::vector<HWND> backdrop_hwnds_;

  // True if the Ctrl+Alt+Esc hotkey is currently registered to `hwnd_`.
  bool hotkey_registered_ = false;

  // Cached solid brush for the branded Google Blue background.
  base::win::ScopedGDIObject<HBRUSH> google_blue_brush_;

  // Per-DPI cache of backdrop GDI resources (`HICON` and `HFONT`s), preventing
  // GDI object thrashing on mixed-DPI multi-monitor hosts.
  base::flat_map<UINT, BackdropDpiResources> backdrop_dpi_cache_;

  // Localized strings loaded from the embedded Windows resource table and
  // `IDD_DISCONNECT` dialog template.
  std::wstring product_name_text_;
  std::wstring sharing_with_text_;
  std::wstring hotkey_hint_text_;
  std::wstring accessible_backdrop_text_;
  std::wstring button_text_;

  // Last observed local cursor position in virtual-screen coordinates.
  webrtc::DesktopVector mouse_position_;

  base::WeakPtrFactory<PrivacyOverlayWin> weak_factory_{this};
};

PrivacyOverlayWin::BackdropDpiResources::BackdropDpiResources() = default;
PrivacyOverlayWin::BackdropDpiResources::BackdropDpiResources(
    BackdropDpiResources&&) = default;
PrivacyOverlayWin::BackdropDpiResources&
PrivacyOverlayWin::BackdropDpiResources::operator=(BackdropDpiResources&&) =
    default;
PrivacyOverlayWin::BackdropDpiResources::~BackdropDpiResources() = default;

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
    return;
  }

  if (local_input_monitor_) {
    local_input_monitor_->StartMonitoring(
        base::BindRepeating(&PrivacyOverlayWin::OnLocalMouseEvent,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&PrivacyOverlayWin::OnLocalKeyPressed,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&PrivacyOverlayWin::StopAutoHideBehavior,
                            weak_factory_.GetWeakPtr()));
  } else {
    StopAutoHideBehavior();
  }
}

INT_PTR CALLBACK PrivacyOverlayWin::DialogProc(HWND hwnd,
                                               UINT message,
                                               WPARAM wparam,
                                               LPARAM lparam) {
  LONG_PTR self = 0;
  if (message == WM_INITDIALOG) {
    self = lparam;

    // Store `this` in the dialog window's user data slot. Do not call
    // `DestroyWindow(hwnd)` synchronously inside `WM_INITDIALOG` while
    // `CreateDialogParam()` is mid-initialization; `BeginOverlay()` verifies
    // `DWLP_USER` after `CreateDialogParam()` returns.
    SetLastError(ERROR_SUCCESS);
    LONG_PTR result = SetWindowLongPtr(hwnd, DWLP_USER, self);
    if (result == 0 && GetLastError() != ERROR_SUCCESS) {
      return FALSE;
    }
  } else {
    self = GetWindowLongPtr(hwnd, DWLP_USER);
  }

  if (self) {
    return reinterpret_cast<PrivacyOverlayWin*>(self)->OnDialogMessage(
        hwnd, message, wparam, lparam);
  }
  return FALSE;
}

BOOL PrivacyOverlayWin::OnDialogMessage(HWND hwnd,
                                        UINT message,
                                        WPARAM wparam,
                                        LPARAM lparam) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (message) {
    // Ignore close messages so external callers or Alt+F4 cannot dismiss the
    // Disconnect button without ending the session.
    case WM_CLOSE:
      return TRUE;

    // Handle Disconnect button clicks.
    case WM_COMMAND:
      if (LOWORD(wparam) == IDC_DISCONNECT && HIWORD(wparam) == BN_CLICKED) {
        // Defense-in-depth: ignore clicks synthesized by CRD remote input
        // injection if local and remote clicks race while the button is active.
        if (static_cast<uint32_t>(GetMessageExtraInfo()) ==
            kCrdInputExtraInfo) {
          return TRUE;
        }
        EndOverlay();
        DisconnectSession("Disconnect window closed.");
        return TRUE;
      }
      return FALSE;

    // Ensure we don't try to use the HWND anymore and tear down the session if
    // the dialog window is destroyed externally.
    case WM_DESTROY:
      SetWindowLongPtr(hwnd, DWLP_USER, 0);
      hwnd_ = nullptr;
      EndOverlay();
      DisconnectSession("Disconnect window closed.");
      return TRUE;

    // Intercept position changes to keep the button window anchored on its
    // target monitor.
    case WM_WINDOWPOSCHANGING: {
      WINDOWPOS* pos = reinterpret_cast<WINDOWPOS*>(lparam);
      if (pos && !(pos->flags & SWP_NOMOVE) && expected_x().has_value() &&
          expected_y().has_value()) {
        if (ShouldRepositionOnDisplacement(pos->x, pos->y)) {
          pos->x = *expected_x();
          pos->y = *expected_y();
        }
      }
      return FALSE;
    }

    // Block attempts to move or minimize the window via system commands.
    case WM_SYSCOMMAND:
      if ((wparam & 0xFFF0) == SC_MOVE || (wparam & 0xFFF0) == SC_MINIMIZE) {
        return TRUE;
      }
      return FALSE;

    // Restore the window if it somehow gets minimized (e.g. via Show Desktop /
    // Win+D). If the button is currently in its idle hidden state, clear
    // `WS_MINIMIZE` first via `SW_SHOWNOACTIVATE` (safe without visual flash
    // while `LWA_ALPHA` is 0 and `WS_EX_TRANSPARENT | WS_EX_NOACTIVATE` are
    // set) and then hide the window again.
    case WM_SIZE:
      if (wparam == SIZE_MINIMIZED) {
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        if (is_button_hidden_) {
          ShowWindow(hwnd_, SW_HIDE);
        }
        ResetRepositionAttempts();
        active_button_monitor_ = nullptr;
        active_button_dpi_ = 0;
        POINT pt = {mouse_position_.x(), mouse_position_.y()};
        PositionDisconnectButton(pt);
        if (!is_button_hidden_) {
          SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return TRUE;
      }
      return FALSE;

    // Update per-monitor privacy overlays and button geometry if the display
    // dimensions, monitor topology, or per-monitor DPI change.
    case WM_DISPLAYCHANGE:
    case WM_DPICHANGED: {
      ResetRepositionAttempts();
      UINT explicit_dpi =
          (message == WM_DPICHANGED) ? static_cast<UINT>(LOWORD(wparam)) : 0;
      if (!UpdateBackdrops()) {
        EndOverlay();
        DisconnectSession("Failed to update Privacy Overlay backdrops.");
        return TRUE;
      }
      POINT pt = {mouse_position_.x(), mouse_position_.y()};
      PositionDisconnectButton(pt, explicit_dpi);
      if (!is_button_hidden_) {
        SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
      }
      return TRUE;
    }

    // Refresh the overlay and invalidate cached fonts/pens if the user toggles
    // Windows High Contrast mode or updates system display/font metrics.
    case WM_SETTINGCHANGE:
    case WM_SYSCOLORCHANGE: {
      backdrop_dpi_cache_.clear();
      UINT dpi_to_restore = active_button_dpi_;
      active_button_monitor_ = nullptr;
      active_button_dpi_ = 0;
      button_border_pen_.reset();
      POINT pt = {mouse_position_.x(), mouse_position_.y()};
      PositionDisconnectButton(pt, dpi_to_restore);
      for (HWND backdrop_hwnd : backdrop_hwnds_) {
        if (backdrop_hwnd && IsWindow(backdrop_hwnd)) {
          InvalidateRect(backdrop_hwnd, nullptr, FALSE);
        }
      }
      if (hwnd_ && IsWindow(hwnd_)) {
        InvalidateRect(hwnd_, nullptr, FALSE);
      }
      return TRUE;
    }

    // Handle the Ctrl+Alt+Esc disconnect hot-key.
    case WM_HOTKEY:
      if (static_cast<int>(wparam) == kDisconnectHotkeyId) {
        EndOverlay();
        DisconnectSession("Disconnect window closed.");
        return TRUE;
      }
      return FALSE;

    // Return the dialog background brush for child controls so the standard
    // Win32 pushbutton does not paint a rectangular gray frame around its
    // rounded parent dialog.
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC: {
      if (IsHighContrastEnabled()) {
        return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW));
      }
      return reinterpret_cast<INT_PTR>(google_blue_brush_.get());
    }

    case WM_ERASEBKGND:
      return TRUE;

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd_, &ps);
      PaintButtonDialog(hwnd_, hdc);
      EndPaint(hwnd_, &ps);
      return TRUE;
    }

    case WM_PRINTCLIENT: {
      HDC hdc = reinterpret_cast<HDC>(wparam);
      if (hdc) {
        PaintButtonDialog(hwnd_, hdc);
      }
      return TRUE;
    }
  }
  return FALSE;
}

bool PrivacyOverlayWin::BeginOverlay() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!hwnd_);

  if (!RegisterPrivacyOverlayClass(&BackdropWndProc)) {
    return false;
  }

  product_name_text_ = LoadModuleString(IDS_PRODUCT_NAME);
  if (product_name_text_.empty()) {
    LOG(ERROR) << "Failed to load IDS_PRODUCT_NAME for Privacy Overlay";
    return false;
  }

  hwnd_ =
      CreateDialogParam(CURRENT_MODULE(), MAKEINTRESOURCE(IDD_DISCONNECT),
                        nullptr, DialogProc, reinterpret_cast<LPARAM>(this));
  if (!hwnd_) {
    PLOG(ERROR) << "Failed to create IDD_DISCONNECT dialog for Privacy Overlay";
    return false;
  }

  // Verify that `DialogProc` succeeded in storing `this` in `DWLP_USER` during
  // `WM_INITDIALOG`.
  if (GetWindowLongPtr(hwnd_, DWLP_USER) != reinterpret_cast<LONG_PTR>(this)) {
    return false;
  }

  // Exclude the Disconnect button dialog from screen capture immediately before
  // it is ever shown so remote viewers never see it.
  if (!ApplyCaptureExclusion(hwnd_)) {
    return false;
  }

  HWND hwnd_button = GetDlgItem(hwnd_, IDC_DISCONNECT);
  HWND hwnd_message = GetDlgItem(hwnd_, IDC_DISCONNECT_SHARINGWITH);
  HWND hwnd_toggle_button = GetDlgItem(hwnd_, IDC_TOGGLE_ALIGNMENT);
  if (!hwnd_button || !hwnd_message) {
    return false;
  }

  std::wstring message_text;
  if (!GetWindowTextString(hwnd_message, &message_text) ||
      !GetWindowTextString(hwnd_button, &button_text_)) {
    return false;
  }

  if (!hotkey_registered_) {
    if (RegisterDisconnectHotKey(hwnd_)) {
      hotkey_registered_ = true;
    } else {
      PLOG(WARNING) << "Failed to register Ctrl+Alt+Esc disconnect hotkey";
    }
  }

  // Only advertise the Ctrl+Alt+Esc shortcut when the hotkey was actually
  // registered, and degrade gracefully if loading the hint string fails.
  if (hotkey_registered_) {
    hotkey_hint_text_ = LoadModuleString(IDS_DISCONNECT_HOTKEY_HINT);
    if (hotkey_hint_text_.empty()) {
      LOG(WARNING)
          << "Failed to load IDS_DISCONNECT_HOTKEY_HINT for Privacy Overlay";
    }
  } else {
    hotkey_hint_text_.clear();
  }

  // Detect whether the localized dialog resource, message text, or process UI
  // language uses a right-to-left (RTL) reading layout.
  LONG_PTR ex_style = GetWindowLongPtr(hwnd_, GWL_EXSTYLE);
  is_rtl_ = (ex_style & (WS_EX_LAYOUTRTL | WS_EX_RTLREADING)) != 0 ||
            base::i18n::IsRTL() ||
            base::i18n::StringContainsStrongRTLChars(
                base::AsStringPiece16(message_text)) ||
            base::i18n::StringContainsStrongRTLChars(
                base::AsStringPiece16(hotkey_hint_text_));

  // Configure extended styles for the Disconnect button window:
  // - Strip `WS_EX_LAYOUTRTL` so GDI coordinates in `PaintButtonDialog()` are
  //   not mirrored relative to the unmirrored `CreateRoundRectRgn()` region.
  // - `WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE`: stays above
  //   desktop windows without stealing focus and is never cloaked across
  //   virtual desktops (Win+Ctrl+D).
  // - `WS_EX_LAYERED | WS_EX_TRANSPARENT`: starts in the idle click-through
  //   state with alpha = 0 so remote clicks pass straight through.
  ex_style &= ~(WS_EX_APPWINDOW | WS_EX_LAYOUTRTL);
  ex_style |= WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED |
              WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
  if (is_rtl_) {
    ex_style |= WS_EX_RTLREADING;
    LONG_PTR btn_ex_style = GetWindowLongPtr(hwnd_button, GWL_EXSTYLE);
    SetWindowLongPtr(hwnd_button, GWL_EXSTYLE, btn_ex_style | WS_EX_RTLREADING);
  }
  SetWindowLongPtr(hwnd_, GWL_EXSTYLE, ex_style);
  SetLayeredWindowAttributes(hwnd_, 0, 0, LWA_ALPHA);

  // Wrap the formatted client email in explicit LTR directional formatting
  // marks (LRE/PDF) when rendering in an RTL context so neutral characters
  // (such as a leading ellipsis or `@`) are not reordered by the BiDi
  // algorithm.
  std::u16string email_for_display = formatted_email();
  if (is_rtl_) {
    base::i18n::WrapStringWithLTRFormatting(&email_for_display);
  }
  sharing_with_text_ = base::AsWString(base::ReplaceStringPlaceholders(
      base::AsString16(message_text), email_for_display, nullptr));
  if (sharing_with_text_.empty()) {
    return false;
  }

  accessible_backdrop_text_ =
      hotkey_hint_text_.empty()
          ? sharing_with_text_
          : (sharing_with_text_ + L"\n" + hotkey_hint_text_);

  // Write the formatted text back to the control and dialog window caption so
  // screen readers have full accessible context.
  SetWindowTextW(hwnd_message, accessible_backdrop_text_.c_str());
  SetWindowTextW(hwnd_, product_name_text_.c_str());

  // Hide the alignment toggle and sharing text controls inside the button
  // dialog so only the Stop Sharing button is presented in the floating button
  // window.
  if (hwnd_toggle_button) {
    ShowWindow(hwnd_toggle_button, SW_HIDE);
  }
  ShowWindow(hwnd_message, SW_HIDE);

  // Measure the base control dimensions and normalize them to 96-DPI logical
  // units so `UpdateButtonLayoutForDpi()` can scale them cleanly to any
  // monitor.
  UINT initial_dpi = GetMonitorDpi(nullptr, hwnd_);
  RECT margin_rect = {0, 0, kWindowTextMargin, 0};
  int margin_px = kDefaultControlMarginPx;
  if (MapDialogRect(hwnd_, &margin_rect) && margin_rect.right > 0) {
    margin_px = margin_rect.right;
  }

  RECT button_rect;
  if (!GetControlRect(hwnd_button, &button_rect)) {
    return false;
  }

  int button_height_px = button_rect.bottom - button_rect.top;
  if (button_height_px <= 0) {
    button_height_px = ScaleForDpi(kDefaultButtonHeightPx, initial_dpi);
  }

  base_button_margin_ =
      std::max(1, MulDiv(margin_px, USER_DEFAULT_SCREEN_DPI, initial_dpi));
  base_button_height_ = std::max(
      1, MulDiv(button_height_px, USER_DEFAULT_SCREEN_DPI, initial_dpi));

  UpdateButtonLayoutForDpi(initial_dpi);

  if (!UpdateBackdrops()) {
    return false;
  }

  POINT cursor_pos = {0, 0};
  if (GetCursorPos(&cursor_pos)) {
    mouse_position_ = webrtc::DesktopVector(cursor_pos.x, cursor_pos.y);
    has_initial_mouse_position_ = true;
  }

  PositionDisconnectButton(cursor_pos);
  DeactivateDisconnectButton();
  return true;
}

void PrivacyOverlayWin::EndOverlay() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_ending_) {
    return;
  }
  is_ending_ = true;

  auto_hide_timer_.Stop();
  local_input_monitor_.reset();

  if (hotkey_registered_ && hwnd_) {
    UnregisterDisconnectHotKey(hwnd_);
    hotkey_registered_ = false;
  }

  std::vector<HWND> backdrop_hwnds = std::exchange(backdrop_hwnds_, {});
  for (HWND backdrop_hwnd : backdrop_hwnds) {
    if (backdrop_hwnd && IsWindow(backdrop_hwnd)) {
      SetWindowLongPtr(backdrop_hwnd, GWLP_USERDATA, 0);
      DestroyWindow(backdrop_hwnd);
    }
  }
  active_button_monitor_ = nullptr;
  active_button_monitor_rect_ = {0, 0, 0, 0};
  active_button_dpi_ = 0;

  backdrop_dpi_cache_.clear();
  button_font_.reset();
  button_border_pen_.reset();

  if (hwnd_) {
    HWND hwnd = std::exchange(hwnd_, nullptr);
    SetWindowLongPtr(hwnd, DWLP_USER, 0);
    DestroyWindow(hwnd);
  }
}

bool PrivacyOverlayWin::GetControlRect(HWND control, RECT* rect) {
  if (!GetWindowRect(control, rect)) {
    return false;
  }
  SetLastError(ERROR_SUCCESS);
  int result =
      MapWindowPoints(HWND_DESKTOP, hwnd_, reinterpret_cast<LPPOINT>(rect), 2);
  if (!result && GetLastError() != ERROR_SUCCESS) {
    return false;
  }

  return true;
}

void PrivacyOverlayWin::UpdateButtonLayoutForDpi(UINT dpi) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!hwnd_ || dpi == 0 || dpi == active_button_dpi_) {
    return;
  }

  HWND hwnd_button = GetDlgItem(hwnd_, IDC_DISCONNECT);
  if (!hwnd_button) {
    return;
  }

  active_button_dpi_ = dpi;
  button_border_pen_.reset();

  // Scale and assign the system UI font for the Disconnect button at `dpi`, and
  // re-measure the localized button text width at the target DPI so the label
  // is never clipped when moving across mixed-DPI monitors.
  int button_font_height = ScaleForDpi(kBaseButtonFontSize, dpi);
  button_font_ = CreateOverlayFont(button_font_height, FW_NORMAL);
  if (button_font_.is_valid()) {
    SendMessage(hwnd_button, WM_SETFONT,
                reinterpret_cast<WPARAM>(button_font_.get()), TRUE);
  }

  LONG button_text_width = 0;
  if (!GetControlTextWidth(hwnd_button, button_text_, is_rtl_,
                           &button_text_width) ||
      button_text_width <= 0) {
    button_text_width = ScaleForDpi(kDefaultButtonTextWidthPx, dpi);
  }

  int margin = ScaleForDpi(base_button_margin_, dpi);
  int button_width = button_text_width + margin * 2;
  int button_height = ScaleForDpi(base_button_height_, dpi);
  int dialog_width = button_width + margin * 2;
  int dialog_height = button_height + margin * 2;
  int border_radius = ScaleForDpi(kBaseWindowBorderRadius, dpi);

  SetWindowPos(hwnd_button, nullptr, margin, margin, button_width,
               button_height, SWP_NOZORDER | SWP_NOACTIVATE);
  SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, dialog_width, dialog_height,
               SWP_NOMOVE | SWP_NOACTIVATE);

  // `CreateRoundRectRgn` excludes the bottom and right edges from the region,
  // so add 1 pixel to width and height to keep the rounded corners symmetric.
  // Use `ScopedGDIObject<HRGN>` and release ownership only after `SetWindowRgn`
  // succeeds.
  base::win::ScopedGDIObject<HRGN> rgn(CreateRoundRectRgn(
      0, 0, dialog_width + 1, dialog_height + 1, border_radius, border_radius));
  if (rgn.is_valid()) {
    if (SetWindowRgn(hwnd_, rgn.get(), TRUE)) {
      std::ignore = rgn.release();
    }
  }
}

const PrivacyOverlayWin::BackdropDpiResources&
PrivacyOverlayWin::GetBackdropResourcesForDpi(UINT dpi) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (dpi == 0) {
    dpi = USER_DEFAULT_SCREEN_DPI;
  }

  auto it = backdrop_dpi_cache_.find(dpi);
  if (it != backdrop_dpi_cache_.end()) {
    return it->second;
  }

  BackdropDpiResources resources;
  int icon_size = ScaleForDpi(kBaseIconSize, dpi);
  int title_height = ScaleForDpi(kBaseTitleFontSize, dpi);
  int body_height = ScaleForDpi(kBaseBodyFontSize, dpi);
  int hint_height = ScaleForDpi(kBaseHintFontSize, dpi);

  resources.crd_icon.reset(static_cast<HICON>(
      LoadImageW(CURRENT_MODULE(), MAKEINTRESOURCEW(IDI_CHROME_REMOTE_DESKTOP),
                 IMAGE_ICON, icon_size, icon_size, LR_DEFAULTCOLOR)));
  resources.title_font = CreateOverlayFont(title_height, FW_SEMIBOLD);
  resources.body_font = CreateOverlayFont(body_height, FW_NORMAL);
  resources.hint_font = CreateOverlayFont(hint_height, FW_NORMAL);

  auto [inserted_it, _] =
      backdrop_dpi_cache_.emplace(dpi, std::move(resources));
  return inserted_it->second;
}

BackdropContentLayout PrivacyOverlayWin::ComputeBackdropLayout(HDC dc,
                                                               int width,
                                                               int height,
                                                               UINT dpi) {
  const BackdropDpiResources& resources = GetBackdropResourcesForDpi(dpi);
  return ComputeBackdropContentLayout(
      dc, width, height, dpi, is_rtl_,
      {.font = resources.title_font.get(), .text = product_name_text_},
      {.font = resources.body_font.get(), .text = sharing_with_text_},
      {.font = resources.hint_font.get(), .text = hotkey_hint_text_});
}

void PrivacyOverlayWin::PaintButtonDialog(HWND hwnd, HDC hdc) {
  RECT rect;
  if (!GetClientRect(hwnd, &rect)) {
    return;
  }

  const bool high_contrast = IsHighContrastEnabled();
  HBRUSH bg_brush =
      high_contrast ? GetSysColorBrush(COLOR_WINDOW) : google_blue_brush_.get();
  COLORREF border_color =
      high_contrast ? GetSysColor(COLOR_WINDOWTEXT) : kWhiteColor;

  FillRect(hdc, &rect, bg_brush);

  UINT dpi = active_button_dpi_ > 0
                 ? active_button_dpi_
                 : GetMonitorDpi(active_button_monitor_, hwnd);
  int border_width = std::max(1, ScaleForDpi(kBaseButtonBorderWidth, dpi));
  int border_radius = ScaleForDpi(kBaseWindowBorderRadius, dpi);

  if (!button_border_pen_.is_valid() ||
      cached_button_high_contrast_ != high_contrast) {
    cached_button_high_contrast_ = high_contrast;
    button_border_pen_.reset(
        CreatePen(PS_INSIDEFRAME, border_width, border_color));
  }

  // Inset the border rectangle by `border_width / 2` and use `PS_INSIDEFRAME`
  // so the full stroke thickness remains inside `CreateRoundRectRgn` without
  // clipping along the top/left edges or rounded corners.
  int inset = border_width / 2;
  base::win::ScopedSelectObject select_pen(hdc, button_border_pen_.get());
  base::win::ScopedSelectObject select_brush(hdc, GetStockObject(NULL_BRUSH));
  RoundRect(hdc, rect.left + inset, rect.top + inset, rect.right - inset,
            rect.bottom - inset, border_radius, border_radius);
}

bool PrivacyOverlayWin::UpdateBackdrops() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Invalidate the cached active button monitor so `PositionDisconnectButton()`
  // recomputes the button coordinates for updated monitor geometry or DPI.
  active_button_monitor_ = nullptr;
  active_button_monitor_rect_ = {0, 0, 0, 0};

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

  DWORD ex_style = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED |
                   WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
  if (is_rtl_) {
    ex_style |= WS_EX_RTLREADING;
  }

  // Set the accessible window title of each backdrop to the localized sharing
  // message and (when active) the disconnect shortcut hint separated by a
  // newline so each line forms an independent BiDi paragraph and screen readers
  // pause naturally between them.
  const std::wstring& backdrop_title = !accessible_backdrop_text_.empty()
                                           ? accessible_backdrop_text_
                                           : product_name_text_;

  for (size_t i = 0; i < monitor_rects.size(); ++i) {
    const RECT& rect = monitor_rects[i];
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    HWND backdrop_hwnd = nullptr;
    if (i < backdrop_hwnds_.size() && IsWindow(backdrop_hwnds_[i])) {
      backdrop_hwnd = backdrop_hwnds_[i];
      SetWindowTextW(backdrop_hwnd, backdrop_title.c_str());
      SetWindowPos(backdrop_hwnd, HWND_TOPMOST, rect.left, rect.top, width,
                   height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
      InvalidateRect(backdrop_hwnd, nullptr, FALSE);
    } else {
      backdrop_hwnd = CreateWindowExW(ex_style, kPrivacyOverlayClassName,
                                      backdrop_title.c_str(), WS_POPUP,
                                      rect.left, rect.top, width, height,
                                      nullptr, nullptr, CURRENT_MODULE(), this);
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
      NotifyWinEvent(EVENT_OBJECT_SHOW, backdrop_hwnd, OBJID_WINDOW,
                     CHILDID_SELF);

      if (i < backdrop_hwnds_.size()) {
        backdrop_hwnds_[i] = backdrop_hwnd;
      } else {
        backdrop_hwnds_.push_back(backdrop_hwnd);
      }
    }
  }

  // Raising `backdrop_hwnds_` with `HWND_TOPMOST` places them at the top of the
  // topmost Z-order band. Re-assert `HWND_TOPMOST` on `hwnd_` so the Disconnect
  // button dialog always remains above all backdrop windows.
  if (hwnd_ && IsWindow(hwnd_)) {
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }

  return true;
}

void PrivacyOverlayWin::PositionDisconnectButton(POINT screen_point,
                                                 UINT explicit_dpi) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!hwnd_) {
    return;
  }

  HMONITOR monitor = MonitorFromPoint(screen_point, MONITOR_DEFAULTTONEAREST);
  UINT dpi = (explicit_dpi > 0 &&
              monitor == MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST))
                 ? explicit_dpi
                 : GetMonitorDpi(monitor, hwnd_);
  if (monitor && monitor == active_button_monitor_ &&
      dpi == active_button_dpi_) {
    return;
  }

  UpdateButtonLayoutForDpi(dpi);

  MONITORINFO monitor_info = {sizeof(monitor_info)};
  RECT window_rect;
  if (!GetMonitorInfo(monitor, &monitor_info) ||
      !GetWindowRect(hwnd_, &window_rect)) {
    return;
  }

  active_button_monitor_ = monitor;
  active_button_monitor_rect_ = monitor_info.rcMonitor;

  int window_width = window_rect.right - window_rect.left;
  int window_height = window_rect.bottom - window_rect.top;
  int monitor_width =
      monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
  int monitor_height =
      monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;

  int left = monitor_info.rcMonitor.left + (monitor_width - window_width) / 2;
  int spacing = ScaleForDpi(kBaseVerticalSpacing, dpi);
  int min_top = monitor_info.rcMonitor.top + (monitor_height / 2) +
                ScaleForDpi(kBaseButtonTopOffset, dpi);
  base::win::ScopedGetDC screen_dc(hwnd_);
  BackdropContentLayout layout =
      ComputeBackdropLayout(screen_dc, monitor_width, monitor_height, dpi);
  int below_text_top = monitor_info.rcMonitor.top + layout.center_y +
                       layout.below_center_height + spacing;
  int top = std::max(min_top, below_text_top);

  // Clamp `top` so the button never clips off the bottom of the monitor.
  int max_top = std::max(static_cast<int>(monitor_info.rcMonitor.top),
                         static_cast<int>(monitor_info.rcMonitor.bottom) -
                             window_height - spacing);
  top = std::min(top, max_top);

  SetExpectedPosition(left, top);
  SetWindowPos(hwnd_, HWND_TOPMOST, left, top, 0, 0,
               SWP_NOSIZE | SWP_NOACTIVATE);
}

void PrivacyOverlayWin::ActivateDisconnectButton(POINT screen_point) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!hwnd_) {
    return;
  }

  if (is_button_hidden_ || !active_button_monitor_ ||
      !PtInRect(&active_button_monitor_rect_, screen_point)) {
    PositionDisconnectButton(screen_point);
  }

  if (is_button_hidden_) {
    LONG_PTR ex_style = GetWindowLongPtr(hwnd_, GWL_EXSTYLE);
    ex_style &= ~WS_EX_TRANSPARENT;
    ex_style |=
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE;
    SetWindowLongPtr(hwnd_, GWL_EXSTYLE, ex_style);
    SetLayeredWindowAttributes(hwnd_, 0, 255, LWA_ALPHA);
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    is_button_hidden_ = false;
    NotifyWinEvent(EVENT_OBJECT_SHOW, hwnd_, OBJID_WINDOW, CHILDID_SELF);
  } else {
    // Ensure `hwnd_` stays at the top of the `HWND_TOPMOST` Z-order band above
    // all backdrop windows and any newly-opened topmost windows.
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }

  if (local_input_monitor_) {
    // `base::Unretained` is safe because `auto_hide_timer_` is a member owned
    // by `this` and is stopped in `EndOverlay()` and on destruction.
    auto_hide_timer_.Start(
        FROM_HERE, kAutoHideTimeout,
        base::BindOnce(&PrivacyOverlayWin::DeactivateDisconnectButton,
                       base::Unretained(this)));
  }
}

void PrivacyOverlayWin::DeactivateDisconnectButton() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!hwnd_) {
    return;
  }

  LONG_PTR ex_style = GetWindowLongPtr(hwnd_, GWL_EXSTYLE);
  ex_style |= WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED |
              WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
  SetWindowLongPtr(hwnd_, GWL_EXSTYLE, ex_style);
  SetLayeredWindowAttributes(hwnd_, 0, 0, LWA_ALPHA);
  ShowWindow(hwnd_, SW_HIDE);
  is_button_hidden_ = true;
}

void PrivacyOverlayWin::StopAutoHideBehavior() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto_hide_timer_.Stop();
  local_input_monitor_.reset();

  // Reveal the Disconnect button permanently since `local_input_monitor_` has
  // been reset above and `ActivateDisconnectButton()` only starts
  // `auto_hide_timer_` when `local_input_monitor_` is non-null.
  POINT pt = {mouse_position_.x(), mouse_position_.y()};
  ActivateDisconnectButton(pt);
}

void PrivacyOverlayWin::OnLocalMouseEvent(const webrtc::DesktopVector& position,
                                          ui::EventType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!has_initial_mouse_position_) {
    has_initial_mouse_position_ = true;
    mouse_position_ = position;
    POINT pt = {position.x(), position.y()};
    ActivateDisconnectButton(pt);
    return;
  }

  // Ignore movements of <= `kMouseJitterThresholdPx` (1px) in both directions
  // to avoid showing the Disconnect button due to hardware/software cursor
  // drift or small vibrations in the environment around the remote host.
  if (std::abs(position.x() - mouse_position_.x()) > kMouseJitterThresholdPx ||
      std::abs(position.y() - mouse_position_.y()) > kMouseJitterThresholdPx) {
    mouse_position_ = position;
    POINT pt = {position.x(), position.y()};
    ActivateDisconnectButton(pt);
  }
}

void PrivacyOverlayWin::OnLocalKeyPressed(uint32_t usb_keycode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Reveal the Disconnect button on local keyboard activity (matching
  // `DisconnectWindowWin::OnLocalKeyPressed`) so keyboard-only and
  // screen-reader users can discover and interact with the Disconnect button.
  POINT pt = {mouse_position_.x(), mouse_position_.y()};
  ActivateDisconnectButton(pt);
}

void PrivacyOverlayWin::PaintBackdrop(HWND hwnd, HDC hdc) {
  RECT client_rect;
  if (!GetClientRect(hwnd, &client_rect)) {
    return;
  }

  int client_width = client_rect.right - client_rect.left;
  int client_height = client_rect.bottom - client_rect.top;
  if (client_width <= 0 || client_height <= 0) {
    return;
  }

  const bool high_contrast = IsHighContrastEnabled();
  HBRUSH bg_brush =
      high_contrast ? GetSysColorBrush(COLOR_WINDOW) : google_blue_brush_.get();
  COLORREF text_color =
      high_contrast ? GetSysColor(COLOR_WINDOWTEXT) : kWhiteColor;

  HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  UINT dpi = GetMonitorDpi(monitor, hwnd);
  const BackdropDpiResources& resources = GetBackdropResourcesForDpi(dpi);

  // Compute the centered content layout first so we only need to allocate a
  // compact offscreen double-buffer bitmap for the central content region
  // (rather than a ~33 MB full-screen 4K bitmap) while filling the surrounding
  // solid background bands directly without flicker.
  const BackdropContentLayout layout =
      ComputeBackdropLayout(hdc, client_width, client_height, dpi);

  int content_left = std::max(0, std::min(layout.text_left, layout.icon_left));
  int content_top = std::max(0, layout.icon_top);
  int content_right = std::min(
      client_width,
      std::max(layout.text_right, layout.icon_left + layout.icon_size));
  int content_bottom =
      std::min(client_height, layout.center_y + layout.below_center_height);
  int content_width = std::max(0, content_right - content_left);
  int content_height = std::max(0, content_bottom - content_top);

  if (content_width <= 0 || content_height <= 0) {
    FillRect(hdc, &client_rect, bg_brush);
    return;
  }

  // Fill the four exterior background bands around the central content box so
  // the solid background never flickers.
  if (content_top > 0) {
    RECT top_band = {0, 0, client_width, content_top};
    FillRect(hdc, &top_band, bg_brush);
  }
  if (content_bottom < client_height) {
    RECT bottom_band = {0, content_bottom, client_width, client_height};
    FillRect(hdc, &bottom_band, bg_brush);
  }
  if (content_left > 0) {
    RECT left_band = {0, content_top, content_left, content_bottom};
    FillRect(hdc, &left_band, bg_brush);
  }
  if (content_right < client_width) {
    RECT right_band = {content_right, content_top, client_width,
                       content_bottom};
    FillRect(hdc, &right_band, bg_brush);
  }

  // Render the central content box into an offscreen memory DC and `BitBlt` in
  // a single pass. If offscreen allocation fails, fall back to drawing directly
  // into `hdc`.
  base::win::ScopedCreateDC mem_dc(CreateCompatibleDC(hdc));
  base::win::ScopedGDIObject<HBITMAP> mem_bitmap;
  std::optional<base::win::ScopedSelectObject> select_bitmap;
  HDC target_dc = hdc;
  int offset_x = 0;
  int offset_y = 0;
  if (mem_dc.is_valid()) {
    mem_bitmap.reset(
        CreateCompatibleBitmap(hdc, content_width, content_height));
    if (mem_bitmap.is_valid()) {
      select_bitmap.emplace(mem_dc.get(), mem_bitmap.get());
      target_dc = mem_dc.get();
      offset_x = -content_left;
      offset_y = -content_top;
    }
  }

  RECT target_bg_rect = {content_left + offset_x, content_top + offset_y,
                         content_right + offset_x, content_bottom + offset_y};
  FillRect(target_dc, &target_bg_rect, bg_brush);

  SetBkMode(target_dc, TRANSPARENT);
  SetTextColor(target_dc, text_color);

  if (resources.crd_icon.is_valid()) {
    DrawIconEx(target_dc, layout.icon_left + offset_x,
               layout.icon_top + offset_y, resources.crd_icon.get(),
               layout.icon_size, layout.icon_size, 0, nullptr,
               DI_NORMAL | DI_NOMIRROR);
  }

  if (layout.title_height > 0 && resources.title_font.is_valid()) {
    base::win::ScopedSelectObject select_font(target_dc,
                                              resources.title_font.get());
    RECT title_rect = {layout.text_left + offset_x, layout.title_top + offset_y,
                       layout.text_right + offset_x,
                       layout.title_bottom + offset_y};
    DrawTextW(target_dc, product_name_text_.c_str(), -1, &title_rect,
              layout.draw_text_flags);
  }

  if (layout.body_height > 0 && resources.body_font.is_valid()) {
    base::win::ScopedSelectObject select_font(target_dc,
                                              resources.body_font.get());
    RECT sharing_rect = {layout.text_left + offset_x,
                         layout.body_top + offset_y,
                         layout.text_right + offset_x,
                         layout.body_top + layout.body_height + offset_y};
    DrawTextW(target_dc, sharing_with_text_.c_str(), -1, &sharing_rect,
              layout.draw_text_flags);
  }

  if (layout.hint_height > 0 && resources.hint_font.is_valid()) {
    base::win::ScopedSelectObject select_font(target_dc,
                                              resources.hint_font.get());
    RECT hint_rect = {layout.text_left + offset_x, layout.hint_top + offset_y,
                      layout.text_right + offset_x,
                      layout.hint_top + layout.hint_height + offset_y};
    DrawTextW(target_dc, hotkey_hint_text_.c_str(), -1, &hint_rect,
              layout.draw_text_flags);
  }

  if (target_dc != hdc) {
    BitBlt(hdc, content_left, content_top, content_width, content_height,
           target_dc, 0, 0, SRCCOPY);
  }
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

    // Forward per-monitor DPI changes on backdrop windows to `hwnd_` as an
    // asynchronous `WM_DISPLAYCHANGE` notification (avoiding forwarding
    // `WM_DPICHANGED`'s stack-allocated `RECT*` in `lparam` via `PostMessage`).
    // `WM_DISPLAYCHANGE` itself is already broadcast by Windows to all
    // top-level windows including `hwnd_`.
    case WM_DPICHANGED:
      InvalidateRect(hwnd, nullptr, FALSE);
      if (self && self->hwnd_) {
        PostMessage(self->hwnd_, WM_DISPLAYCHANGE, 0, 0);
      }
      return 0;
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
