// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <commctrl.h>
#include <shellapi.h>
#include <stddef.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/current_module.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_window.h"
#include "remoting/host/input_monitor/local_input_monitor.h"
#include "remoting/host/win/core_resource.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/event.h"

namespace remoting {

namespace {

constexpr int DISCONNECT_HOTKEY_ID = 1000;

// Maximum length of "Your desktop is shared with ..." message in UTF-16
// characters.
constexpr size_t kMaxSharingWithTextLength = 100;

constexpr wchar_t kShellTrayWindowName[] = L"Shell_TrayWnd";
constexpr int kWindowBorderRadius = 14;

// Margin between dialog controls (in dialog units).
constexpr int kWindowTextMargin = 8;

// The amount of time to wait before allowing another position toggle.
constexpr base::TimeDelta kToggleCooldown = base::Seconds(3);

// The amount of time to wait before hiding the disconnect window.
constexpr base::TimeDelta kAutoHideTimeout = base::Seconds(10);

// The length of the hide and show animations.
constexpr DWORD kAnimationDurationMs = 200;

enum class WindowAnchor {
  kBottom,
  kTop,
};

// Remembers the last selected anchor position across dialog instances.
WindowAnchor g_current_anchor = WindowAnchor::kBottom;

class DisconnectWindowWin : public HostWindow {
 public:
  DisconnectWindowWin();

  DisconnectWindowWin(const DisconnectWindowWin&) = delete;
  DisconnectWindowWin& operator=(const DisconnectWindowWin&) = delete;

  ~DisconnectWindowWin() override;

  // Allow dialog to auto-hide after a period of time.  The dialog will be
  // reshown when local user input is detected.
  void EnableAutoHide(std::unique_ptr<LocalInputMonitor> local_input_monitor);

  // HostWindow overrides.
  void Start(const base::WeakPtr<ClientSessionControl>& client_session_control)
      override;

 private:
  static INT_PTR CALLBACK DialogProc(HWND hwnd,
                                     UINT message,
                                     WPARAM wparam,
                                     LPARAM lparam);

  BOOL OnDialogMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  // Creates the dialog window and registers the disconnect hot key.
  bool BeginDialog();

  // Closes the dialog, unregisters the hot key and invokes the disconnect
  // callback, if set.
  void EndDialog();

  // Returns |control| rectangle in the dialog coordinates.
  bool GetControlRect(HWND control, RECT* rect);

  // Positions the dialog window based on the current anchor and auto-hide
  // state.
  void SetDialogPosition();

  // Toggles the dialog anchor between top and bottom.
  void ToggleAlignment();

  // Re-enables the toggle button when cooldown expires.
  void OnCooldownExpired();

  // Updates the toggle button text according to the current anchor.
  void UpdateToggleButtonText();

  // Applies localization string and resizes the dialog.
  bool SetStrings();

  // Draws the border around the dialog window.  Can be used to draw the initial
  // border or to redraw if when the dialog is reshown.  |hwnd| is the window to
  // have the border applied.  |hdc| is the device context to draw to.
  void DrawBorder(HWND hwnd, HDC hdc);

  // Shows a previously hidden dialog using an animation.
  void ShowDialog();

  // Hides the dialog using an animation.
  void HideDialog();

  // Prevent the dialog from being hidden if local input monitoring fails.
  void StopAutoHideBehavior();

  // Called when local mouse event is seen and shows the dialog (if hidden).
  void OnLocalMouseEvent(const webrtc::DesktopVector& mouse_position,
                         ui::EventType type);

  // Called when local keyboard event is seen and shows the dialog (if hidden).
  void OnLocalKeyPressed(uint32_t usb_keycode);

  // Used to disconnect the client session.
  base::WeakPtr<ClientSessionControl> client_session_control_;

  // Used to watch for local input which will trigger the dialog to be reshown.
  std::unique_ptr<LocalInputMonitor> local_input_monitor_;

  // Specifies the remote user name.
  std::string username_;

  bool was_auto_hidden_ = false;
  base::OneShotTimer auto_hide_timer_;
  base::OneShotTimer cooldown_timer_;

  HWND hwnd_ = nullptr;
  HWND hwnd_toggle_button_ = nullptr;
  HWND hwnd_tooltip_ = nullptr;
  bool has_hotkey_ = false;
  base::win::ScopedGDIObject<HPEN> border_pen_;

  webrtc::DesktopVector mouse_position_;

  base::WeakPtrFactory<DisconnectWindowWin> weak_factory_{this};
};

// Returns the text for the given dialog control window.
bool GetControlText(HWND control, std::wstring* text) {
  // GetWindowText truncates the text if it is longer than can fit into
  // the buffer.
  WCHAR buffer[256];
  int result = GetWindowText(control, buffer, std::size(buffer));
  if (!result) {
    return false;
  }

  text->assign(buffer);
  return true;
}

// Returns width |text| rendered in |control| window.
bool GetControlTextWidth(HWND control, const std::wstring& text, LONG* width) {
  RECT rect = {0, 0, 0, 0};
  base::win::ScopedGetDC dc(control);
  base::win::ScopedSelectObject font(
      dc, (HFONT)SendMessage(control, WM_GETFONT, 0, 0));
  if (!DrawText(dc, text.c_str(), -1, &rect, DT_CALCRECT | DT_SINGLELINE)) {
    return false;
  }

  *width = rect.right;
  return true;
}

DisconnectWindowWin::DisconnectWindowWin()
    : border_pen_(
          CreatePen(PS_SOLID, 5, RGB(0.13 * 255, 0.69 * 255, 0.11 * 255))) {}

DisconnectWindowWin::~DisconnectWindowWin() {
  EndDialog();
}

void DisconnectWindowWin::EnableAutoHide(
    std::unique_ptr<LocalInputMonitor> local_input_monitor) {
  local_input_monitor_ = std::move(local_input_monitor);
}

void DisconnectWindowWin::Start(
    const base::WeakPtr<ClientSessionControl>& client_session_control) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!client_session_control_);
  DCHECK(client_session_control);

  client_session_control_ = client_session_control;

  std::string client_jid = client_session_control_->client_jid();
  username_ = client_jid.substr(0, client_jid.find('/'));
  if (!BeginDialog()) {
    EndDialog();
    return;
  }

  if (local_input_monitor_) {
    local_input_monitor_->StartMonitoring(
        base::BindRepeating(&DisconnectWindowWin::OnLocalMouseEvent,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&DisconnectWindowWin::OnLocalKeyPressed,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&DisconnectWindowWin::StopAutoHideBehavior,
                            weak_factory_.GetWeakPtr()));

    auto_hide_timer_.Start(FROM_HERE, kAutoHideTimeout,
                           base::BindOnce(&DisconnectWindowWin::HideDialog,
                                          base::Unretained(this)));
  }
}

INT_PTR CALLBACK DisconnectWindowWin::DialogProc(HWND hwnd,
                                                 UINT message,
                                                 WPARAM wparam,
                                                 LPARAM lparam) {
  LONG_PTR self = 0;
  if (message == WM_INITDIALOG) {
    self = lparam;

    // Store |this| to the window's user data.
    SetLastError(ERROR_SUCCESS);
    LONG_PTR result = SetWindowLongPtr(hwnd, DWLP_USER, self);
    if (result == 0 && GetLastError() != ERROR_SUCCESS) {
      reinterpret_cast<DisconnectWindowWin*>(self)->EndDialog();
    }
  } else {
    self = GetWindowLongPtr(hwnd, DWLP_USER);
  }

  if (self) {
    return reinterpret_cast<DisconnectWindowWin*>(self)->OnDialogMessage(
        hwnd, message, wparam, lparam);
  }
  return FALSE;
}

BOOL DisconnectWindowWin::OnDialogMessage(HWND hwnd,
                                          UINT message,
                                          WPARAM wparam,
                                          LPARAM lparam) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (message) {
    // Ignore close messages.
    case WM_CLOSE:
      return TRUE;

    // Handle dialog button commands.
    case WM_COMMAND:
      switch (LOWORD(wparam)) {
        case IDC_DISCONNECT:
          EndDialog();
          return TRUE;
        case IDC_TOGGLE_ALIGNMENT:
          ToggleAlignment();
          return TRUE;
      }
      return FALSE;

    // Ensure we don't try to use the HWND anymore.
    case WM_DESTROY:
      hwnd_ = nullptr;
      hwnd_toggle_button_ = nullptr;

      // Ensure that the disconnect callback is invoked even if somehow our
      // window gets destroyed.
      EndDialog();

      return TRUE;

    // Ensure the dialog stays visible if the display dimensions change.
    case WM_DISPLAYCHANGE:
      SetDialogPosition();
      return TRUE;

    // Handle the disconnect hot-key.
    case WM_HOTKEY:
      EndDialog();
      return TRUE;

    case WM_PAINT: {
      // Draw the client area after ShowWindow is used to make |hwnd_| visible.
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd_, &ps);
      DrawBorder(hwnd_, hdc);
      EndPaint(hwnd_, &ps);
      return TRUE;
    }

    case WM_PRINTCLIENT: {
      // Refresh the dialog client area.  Called after AnimateWindow is used to
      // reshow the dialog.
      HDC hdc = reinterpret_cast<HDC>(wparam);
      DrawBorder(hwnd_, hdc);
      return TRUE;
    }
  }
  return FALSE;
}

bool DisconnectWindowWin::BeginDialog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!hwnd_);

  hwnd_ =
      CreateDialogParam(CURRENT_MODULE(), MAKEINTRESOURCE(IDD_DISCONNECT),
                        nullptr, DialogProc, reinterpret_cast<LPARAM>(this));
  if (!hwnd_) {
    return false;
  }

  // Set up handler for Ctrl-Alt-Esc shortcut.
  if (!has_hotkey_ && RegisterHotKey(hwnd_, DISCONNECT_HOTKEY_ID,
                                     MOD_ALT | MOD_CONTROL, VK_ESCAPE)) {
    has_hotkey_ = true;
  }

  if (!SetStrings()) {
    return false;
  }

  SetDialogPosition();
  ShowWindow(hwnd_, SW_SHOW);
  return IsWindowVisible(hwnd_) != FALSE;
}

void DisconnectWindowWin::EndDialog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (has_hotkey_) {
    UnregisterHotKey(hwnd_, DISCONNECT_HOTKEY_ID);
    has_hotkey_ = false;
  }

  if (hwnd_tooltip_) {
    DestroyWindow(hwnd_tooltip_);
    hwnd_tooltip_ = nullptr;
  }

  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
    hwnd_toggle_button_ = nullptr;
  }

  // Disable auto-hide and cooldown events since the window has been destroyed.
  auto_hide_timer_.Stop();
  cooldown_timer_.Stop();

  if (client_session_control_) {
    client_session_control_->DisconnectSession(
        ErrorCode::OK, "Disconnect window closed.", FROM_HERE);
  }
}

void DisconnectWindowWin::ShowDialog() {
  // Always reset the hide timer when this method is called.
  if (local_input_monitor_) {
    auto_hide_timer_.Start(FROM_HERE, kAutoHideTimeout,
                           base::BindOnce(&DisconnectWindowWin::HideDialog,
                                          base::Unretained(this)));
  }

  if (!was_auto_hidden_) {
    return;
  }

  // Make sure the dialog is positioned at the current anchor when it is
  // reshown.
  SetDialogPosition();

  if (!AnimateWindow(hwnd_, kAnimationDurationMs, AW_BLEND)) {
    PLOG(ERROR) << "AnimateWindow() failed to show dialog: ";
    ShowWindow(hwnd_, SW_SHOW);

    // If the window still isn't visible, then disconnect the session.
    if (!IsWindowVisible(hwnd_)) {
      client_session_control_->DisconnectSession(
          ErrorCode::OK, "Disconnect window is invisible.", FROM_HERE);
    }
  }
  was_auto_hidden_ = false;
}

void DisconnectWindowWin::HideDialog() {
  if (was_auto_hidden_ || !local_input_monitor_ || !hwnd_) {
    return;
  }

  if (!AnimateWindow(hwnd_, kAnimationDurationMs, AW_BLEND | AW_HIDE)) {
    PLOG(ERROR) << "AnimateWindow() failed to hide dialog: ";
  } else {
    was_auto_hidden_ = true;
  }
}

void DisconnectWindowWin::StopAutoHideBehavior() {
  auto_hide_timer_.Stop();
  local_input_monitor_.reset();

  ShowDialog();
}

void DisconnectWindowWin::OnLocalMouseEvent(
    const webrtc::DesktopVector& position,
    ui::EventType type) {
  // Don't show the dialog if the position changes by ~1px in any direction.
  // This will prevent the dialog from being reshown due to small movements
  // caused by hardware/software issues which cause cursor drift or small
  // vibrations in the environment around the remote host.
  if (std::abs(position.x() - mouse_position_.x()) > 1 ||
      std::abs(position.y() - mouse_position_.y()) > 1) {
    ShowDialog();
  }

  mouse_position_ = position;
}

void DisconnectWindowWin::OnLocalKeyPressed(uint32_t usb_keycode) {
  ShowDialog();
}

void DisconnectWindowWin::DrawBorder(HWND hwnd, HDC hdc) {
  RECT rect;
  GetClientRect(hwnd, &rect);
  base::win::ScopedSelectObject border(hdc, border_pen_.get());
  base::win::ScopedSelectObject brush(hdc, GetStockObject(NULL_BRUSH));
  RoundRect(hdc, rect.left, rect.top, rect.right - 1, rect.bottom - 1,
            kWindowBorderRadius, kWindowBorderRadius);
}

// Returns |control| rectangle in the dialog coordinates.
bool DisconnectWindowWin::GetControlRect(HWND control, RECT* rect) {
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

void DisconnectWindowWin::ToggleAlignment() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cooldown_timer_.IsRunning()) {
    return;
  }

  g_current_anchor = (g_current_anchor == WindowAnchor::kBottom)
                         ? WindowAnchor::kTop
                         : WindowAnchor::kBottom;
  UpdateToggleButtonText();
  if (hwnd_toggle_button_) {
    EnableWindow(hwnd_toggle_button_, FALSE);
    cooldown_timer_.Start(
        FROM_HERE, kToggleCooldown,
        base::BindOnce(&DisconnectWindowWin::OnCooldownExpired,
                       weak_factory_.GetWeakPtr()));
  }
  SetDialogPosition();
}

void DisconnectWindowWin::OnCooldownExpired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (hwnd_toggle_button_) {
    EnableWindow(hwnd_toggle_button_, TRUE);
  }
}

void DisconnectWindowWin::UpdateToggleButtonText() {
  if (!hwnd_toggle_button_) {
    return;
  }

  SetWindowText(hwnd_toggle_button_,
                (g_current_anchor == WindowAnchor::kBottom) ? L"▲" : L"▼");

  int string_id = (g_current_anchor == WindowAnchor::kBottom)
                      ? IDS_MOVE_TO_TOP_BUTTON
                      : IDS_MOVE_TO_BOTTOM_BUTTON;
  const wchar_t* string_ptr = nullptr;
  int string_length = LoadStringW(CURRENT_MODULE(), string_id,
                                  reinterpret_cast<wchar_t*>(&string_ptr), 0);
  std::wstring tooltip_text;
  if (string_length > 0 && string_ptr) {
    tooltip_text.assign(string_ptr, string_length);
  }

  if (hwnd_tooltip_) {
    TOOLINFO tool_info = {sizeof(tool_info)};
    tool_info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    tool_info.hwnd = hwnd_;
    tool_info.uId = reinterpret_cast<UINT_PTR>(hwnd_toggle_button_);
    tool_info.lpszText = tooltip_text.data();
    SendMessage(hwnd_tooltip_, TTM_UPDATETIPTEXT, 0,
                reinterpret_cast<LPARAM>(&tool_info));
  }
}

void DisconnectWindowWin::SetDialogPosition() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Try to center the window above the task-bar. If that fails, use the
  // primary monitor. If that fails (very unlikely), use the default position.
  HWND taskbar = FindWindow(kShellTrayWindowName, nullptr);
  HMONITOR monitor = MonitorFromWindow(taskbar, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO monitor_info = {sizeof(monitor_info)};
  RECT window_rect;
  if (!GetMonitorInfo(monitor, &monitor_info) ||
      !GetWindowRect(hwnd_, &window_rect)) {
    return;
  }

  int window_width = window_rect.right - window_rect.left;
  int window_height = window_rect.bottom - window_rect.top;

  int top = 0;
  if (g_current_anchor == WindowAnchor::kTop) {
    top = monitor_info.rcWork.top;
    // Check if the taskbar is at the top of the monitor (even if auto-hidden).
    APPBARDATA abd = {sizeof(abd)};
    abd.hWnd = taskbar;
    if (taskbar && SHAppBarMessage(ABM_GETTASKBARPOS, &abd) &&
        abd.uEdge == ABE_TOP) {
      top = std::max(top, static_cast<int>(abd.rc.bottom));
    }
  } else {
    top = monitor_info.rcWork.bottom - window_height;
    // Check if the taskbar is at the bottom of the monitor (even if
    // auto-hidden).
    APPBARDATA abd = {sizeof(abd)};
    abd.hWnd = taskbar;
    if (taskbar && SHAppBarMessage(ABM_GETTASKBARPOS, &abd) &&
        abd.uEdge == ABE_BOTTOM) {
      top = std::min(top, static_cast<int>(abd.rc.top - window_height));
    }
  }

  int left =
      (monitor_info.rcWork.right + monitor_info.rcWork.left - window_width) / 2;

  SetWindowPos(hwnd_, nullptr, left, top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

bool DisconnectWindowWin::SetStrings() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  HWND hwnd_button = GetDlgItem(hwnd_, IDC_DISCONNECT);
  HWND hwnd_message = GetDlgItem(hwnd_, IDC_DISCONNECT_SHARINGWITH);
  hwnd_toggle_button_ = GetDlgItem(hwnd_, IDC_TOGGLE_ALIGNMENT);
  if (!hwnd_button || !hwnd_message || !hwnd_toggle_button_) {
    return false;
  }

  if (!hwnd_tooltip_) {
    hwnd_tooltip_ =
        CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr,
                       WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT,
                       CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd_,
                       nullptr, CURRENT_MODULE(), nullptr);
    if (hwnd_tooltip_) {
      TOOLINFO tool_info = {sizeof(tool_info)};
      tool_info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
      tool_info.hwnd = hwnd_;
      tool_info.uId = reinterpret_cast<UINT_PTR>(hwnd_toggle_button_);
      SendMessage(hwnd_tooltip_, TTM_ADDTOOL, 0,
                  reinterpret_cast<LPARAM>(&tool_info));
    }
  }

  UpdateToggleButtonText();

  std::wstring button_text;
  std::wstring message_text;
  if (!GetControlText(hwnd_button, &button_text) ||
      !GetControlText(hwnd_message, &message_text)) {
    return false;
  }

  // Format and truncate "Your desktop is shared with ..." message.
  message_text = base::AsWString(base::ReplaceStringPlaceholders(
      base::AsString16(message_text), base::UTF8ToUTF16(username_), nullptr));
  if (message_text.length() > kMaxSharingWithTextLength) {
    message_text.erase(kMaxSharingWithTextLength);
  }

  if (!SetWindowText(hwnd_message, message_text.c_str())) {
    return false;
  }

  // Calculate the margin between controls in pixels.
  RECT rect = {0};
  rect.right = kWindowTextMargin;
  if (!MapDialogRect(hwnd_, &rect)) {
    return false;
  }
  int margin = rect.right;

  // Position toggle button at left margin.
  RECT toggle_rect;
  if (!GetControlRect(hwnd_toggle_button_, &toggle_rect)) {
    return false;
  }
  int toggle_width = toggle_rect.right - toggle_rect.left;
  int toggle_height = toggle_rect.bottom - toggle_rect.top;
  if (!SetWindowPos(hwnd_toggle_button_, nullptr, margin, toggle_rect.top,
                    toggle_width, toggle_height, SWP_NOZORDER)) {
    return false;
  }

  // Resize |hwnd_message| so that the text is not clipped.
  RECT message_rect;
  if (!GetControlRect(hwnd_message, &message_rect)) {
    return false;
  }

  LONG control_width;
  if (!GetControlTextWidth(hwnd_message, message_text, &control_width)) {
    return false;
  }
  message_rect.left = margin + toggle_width + margin;
  message_rect.right = message_rect.left + control_width;

  if (!SetWindowPos(hwnd_message, nullptr, message_rect.left, message_rect.top,
                    control_width, message_rect.bottom - message_rect.top,
                    SWP_NOZORDER)) {
    return false;
  }

  // Reposition and resize |hwnd_button| as well.
  RECT button_rect;
  if (!GetControlRect(hwnd_button, &button_rect)) {
    return false;
  }

  if (!GetControlTextWidth(hwnd_button, button_text, &control_width)) {
    return false;
  }

  button_rect.left = message_rect.right + margin;
  button_rect.right = button_rect.left + control_width + margin * 2;
  if (!SetWindowPos(hwnd_button, nullptr, button_rect.left, button_rect.top,
                    button_rect.right - button_rect.left,
                    button_rect.bottom - button_rect.top, SWP_NOZORDER)) {
    return false;
  }

  // Resize the whole window to fit the resized controls.
  RECT window_rect;
  if (!GetWindowRect(hwnd_, &window_rect)) {
    return false;
  }
  int width = button_rect.right + margin;
  int height = window_rect.bottom - window_rect.top;
  if (!SetWindowPos(hwnd_, nullptr, 0, 0, width, height,
                    SWP_NOMOVE | SWP_NOZORDER)) {
    return false;
  }

  // Make the corners of the disconnect window rounded.
  HRGN rgn = CreateRoundRectRgn(0, 0, width, height, kWindowBorderRadius,
                                kWindowBorderRadius);
  if (!rgn) {
    return false;
  }
  if (!SetWindowRgn(hwnd_, rgn, TRUE)) {
    return false;
  }

  return true;
}

}  // namespace

// static
std::unique_ptr<HostWindow> HostWindow::CreateDisconnectWindow() {
  return std::make_unique<DisconnectWindowWin>();
}

std::unique_ptr<HostWindow> HostWindow::CreateAutoHidingDisconnectWindow(
    std::unique_ptr<LocalInputMonitor> local_input_monitor) {
  auto disconnect_window = std::make_unique<DisconnectWindowWin>();
  disconnect_window->EnableAutoHide(std::move(local_input_monitor));

  return disconnect_window;
}

}  // namespace remoting
