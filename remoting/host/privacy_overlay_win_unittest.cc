// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/privacy_overlay_win.h"

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/icu_test_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "remoting/base/email_utils.h"
#include "remoting/base/errors.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/disconnect_window_base.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/host_window.h"
#include "remoting/host/input_monitor/local_input_monitor.h"
#include "remoting/host/win/core_resource.h"
#include "remoting/host/win/host_window_win_util.h"
#include "remoting/host/win/input_extra_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/event.h"

namespace remoting {

namespace {

using ::testing::_;
using ::testing::ReturnRef;

// Returns true if `bitmap_dc` contains at least one pixel in `[left, right) x
// [top, bottom)` whose color differs from `bg_color` (indicating rendered icon
// or text glyphs).
bool HasNonBackgroundPixel(HDC bitmap_dc,
                           int left,
                           int top,
                           int right,
                           int bottom,
                           COLORREF bg_color) {
  for (int y = top; y < bottom; y += 2) {
    for (int x = left; x < right; x += 2) {
      COLORREF pixel = GetPixel(bitmap_dc, x, y);
      if (pixel != CLR_INVALID && pixel != bg_color) {
        return true;
      }
    }
  }
  return false;
}

// Convenience wrapper around `GetWindowTextString()` for test assertions.
std::wstring GetWindowTextOrEmpty(HWND hwnd) {
  std::wstring text;
  GetWindowTextString(hwnd, &text);
  return text;
}

// Returns true if `top_hwnd` precedes `bottom_hwnd` in the Win32 Z-order chain
// (i.e. `top_hwnd` is above `bottom_hwnd`).
bool IsWindowAbove(HWND top_hwnd, HWND bottom_hwnd) {
  for (HWND curr = top_hwnd; curr != nullptr;
       curr = GetWindow(curr, GW_HWNDNEXT)) {
    if (curr == bottom_hwnd) {
      return true;
    }
  }
  return false;
}

// Fake `ClientSessionControl` implementation that provides a `WeakPtr` and
// returns a configured client JID for string placeholder formatting.
class FakeClientSessionControl : public MockClientSessionControl {
 public:
  explicit FakeClientSessionControl(std::string client_jid)
      : client_jid_(std::move(client_jid)) {
    EXPECT_CALL(*this, client_jid()).WillRepeatedly(ReturnRef(client_jid_));
  }

  FakeClientSessionControl(const FakeClientSessionControl&) = delete;
  FakeClientSessionControl& operator=(const FakeClientSessionControl&) = delete;
  ~FakeClientSessionControl() override = default;

  base::WeakPtr<ClientSessionControl> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::string client_jid_;
  base::WeakPtrFactory<FakeClientSessionControl> weak_factory_{this};
};

// Fake `LocalInputMonitor` implementation that forwards the pointer, keyboard,
// and error callbacks registered by `PrivacyOverlayWin` to the test fixture so
// tests can invoke them deterministically without holding a `raw_ptr` to the
// monitor instance owned by `PrivacyOverlayWin`.
class FakeLocalInputMonitor : public LocalInputMonitor {
 public:
  using StartMonitoringCallback = base::OnceCallback<
      void(PointerMoveCallback, KeyPressedCallback, base::RepeatingClosure)>;

  explicit FakeLocalInputMonitor(StartMonitoringCallback on_start_monitoring)
      : on_start_monitoring_(std::move(on_start_monitoring)) {}
  FakeLocalInputMonitor(const FakeLocalInputMonitor&) = delete;
  FakeLocalInputMonitor& operator=(const FakeLocalInputMonitor&) = delete;
  ~FakeLocalInputMonitor() override = default;

  void StartMonitoringForClientSession(
      base::WeakPtr<ClientSessionControl> client_session_control) override {}

  void StartMonitoring(PointerMoveCallback on_pointer_input,
                       KeyPressedCallback on_keyboard_input,
                       base::RepeatingClosure on_error) override {
    if (on_start_monitoring_) {
      std::move(on_start_monitoring_)
          .Run(std::move(on_pointer_input), std::move(on_keyboard_input),
               std::move(on_error));
    }
  }

 private:
  StartMonitoringCallback on_start_monitoring_;
};

// Holds the Win32 window handles discovered on the current test thread for the
// active `PrivacyOverlayWin` instance.
struct OverlayThreadWindows {
  HWND button_hwnd = nullptr;
  std::vector<HWND> backdrop_hwnds;
};

// Callback for `EnumThreadWindows()` that partitions thread-owned windows into
// per-monitor privacy overlay backdrops and the Disconnect button dialog.
BOOL CALLBACK EnumOverlayThreadWindowsCallback(HWND hwnd, LPARAM lparam) {
  auto* windows = reinterpret_cast<OverlayThreadWindows*>(lparam);
  wchar_t class_name[128] = {};
  if (GetClassNameW(hwnd, class_name, std::size(class_name)) > 0 &&
      std::wstring_view(class_name) == kPrivacyOverlayClassName) {
    windows->backdrop_hwnds.push_back(hwnd);
    return TRUE;
  }

  if (GetDlgItem(hwnd, IDC_DISCONNECT) != nullptr) {
    windows->button_hwnd = hwnd;
  }
  return TRUE;
}

// Enumerates all windows owned by the current thread to locate the Disconnect
// button dialog and per-monitor backdrop windows created by
// `PrivacyOverlayWin`.
OverlayThreadWindows FindOverlayThreadWindows() {
  OverlayThreadWindows windows;
  EnumThreadWindows(GetCurrentThreadId(), &EnumOverlayThreadWindowsCallback,
                    reinterpret_cast<LPARAM>(&windows));
  return windows;
}

// Offscreen rendered snapshot and layout metrics for a `PrivacyOverlayWin`
// backdrop window at a given `text_scale`.
struct CapturedBackdropSnapshot {
  CapturedBackdropSnapshot() = default;
  CapturedBackdropSnapshot(CapturedBackdropSnapshot&&) = default;
  CapturedBackdropSnapshot& operator=(CapturedBackdropSnapshot&&) = default;
  ~CapturedBackdropSnapshot() = default;

  bool HasGlyphPixels(int left, int top, int right, int bottom) const {
    int clamped_left = std::max(0, left);
    int clamped_top = std::max(0, top);
    int clamped_right = std::min(client_width, right);
    int clamped_bottom = std::min(client_height, bottom);
    if (clamped_left >= clamped_right || clamped_top >= clamped_bottom) {
      return false;
    }
    base::win::ScopedSelectObject select_bitmap(mem_dc.get(), bitmap.get());
    return HasNonBackgroundPixel(mem_dc.get(), clamped_left, clamped_top,
                                 clamped_right, clamped_bottom,
                                 expected_bg_color);
  }

  int client_width = 0;
  int client_height = 0;
  UINT dpi = USER_DEFAULT_SCREEN_DPI;
  COLORREF expected_bg_color = kGoogleBlueColor;
  base::win::ScopedCreateDC mem_dc;
  base::win::ScopedGDIObject<HBITMAP> bitmap;
  BackdropContentLayout layout;
};

std::optional<CapturedBackdropSnapshot> CaptureBackdropSnapshot(
    HWND backdrop_hwnd,
    double text_scale) {
  RECT client_rect = {};
  if (!GetClientRect(backdrop_hwnd, &client_rect)) {
    return std::nullopt;
  }
  int client_width = client_rect.right - client_rect.left;
  int client_height = client_rect.bottom - client_rect.top;
  if (client_width <= 0 || client_height <= 0) {
    return std::nullopt;
  }

  base::win::ScopedGetDC screen_dc(nullptr);
  base::win::ScopedCreateDC mem_dc(CreateCompatibleDC(screen_dc));
  base::win::ScopedGDIObject<HBITMAP> bitmap(
      CreateCompatibleBitmap(screen_dc, client_width, client_height));
  if (!mem_dc.is_valid() || !bitmap.is_valid()) {
    return std::nullopt;
  }

  UINT dpi =
      GetMonitorDpi(MonitorFromWindow(backdrop_hwnd, MONITOR_DEFAULTTONEAREST),
                    backdrop_hwnd);
  const std::wstring product_name = LoadModuleString(IDS_PRODUCT_NAME);
  const std::wstring hotkey_hint = LoadModuleString(IDS_DISCONNECT_HOTKEY_HINT);
  const std::wstring backdrop_title = GetWindowTextOrEmpty(backdrop_hwnd);
  size_t newline_pos = backdrop_title.find(L'\n');
  std::wstring_view sharing_text =
      (newline_pos == std::wstring::npos)
          ? std::wstring_view(backdrop_title)
          : std::wstring_view(backdrop_title).substr(0, newline_pos);
  std::wstring_view hint_text = (newline_pos == std::wstring::npos)
                                    ? std::wstring_view()
                                    : std::wstring_view(hotkey_hint);

  base::win::ScopedGDIObject<HFONT> title_font = CreateOverlayFont(
      ScaleForDpiAndTextScale(kBaseTitleFontSize, dpi, text_scale),
      FW_SEMIBOLD);
  base::win::ScopedGDIObject<HFONT> body_font = CreateOverlayFont(
      ScaleForDpiAndTextScale(kBaseBodyFontSize, dpi, text_scale), FW_NORMAL);
  base::win::ScopedGDIObject<HFONT> hint_font = CreateOverlayFont(
      ScaleForDpiAndTextScale(kBaseHintFontSize, dpi, text_scale), FW_NORMAL);

  CapturedBackdropSnapshot snapshot;
  snapshot.client_width = client_width;
  snapshot.client_height = client_height;
  snapshot.dpi = dpi;
  snapshot.expected_bg_color =
      IsHighContrastEnabled() ? GetSysColor(COLOR_WINDOW) : kGoogleBlueColor;
  {
    base::win::ScopedSelectObject select_bitmap(mem_dc.get(), bitmap.get());
    SendMessage(backdrop_hwnd, WM_PRINTCLIENT,
                reinterpret_cast<WPARAM>(mem_dc.get()), PRF_CLIENT);
    snapshot.layout = ComputeBackdropContentLayout(
        mem_dc.get(), client_width, client_height, dpi, text_scale,
        /*is_rtl=*/false, {.font = title_font.get(), .text = product_name},
        {.font = body_font.get(), .text = sharing_text},
        {.font = hint_font.get(), .text = hint_text});
  }
  snapshot.mem_dc = std::move(mem_dc);
  snapshot.bitmap = std::move(bitmap);
  return snapshot;
}

}  // namespace

class PrivacyOverlayWinTest : public testing::Test {
 protected:
  void SetUp() override {
    auto input_monitor = std::make_unique<FakeLocalInputMonitor>(base::BindOnce(
        &PrivacyOverlayWinTest::OnStartMonitoring, base::Unretained(this)));
    overlay_ = HostWindow::CreatePrivacyOverlay(std::move(input_monitor));
    ASSERT_NE(overlay_, nullptr);
  }

  void TearDown() override {
    DestroyOverlay();
    SetMessageExtraInfo(0);
  }

  void DestroyOverlay() {
    pointer_callback_.Reset();
    key_pressed_callback_.Reset();
    error_callback_.Reset();
    overlay_.reset();
  }

  void StartOverlay() {
    overlay_->Start(client_session_control_.GetWeakPtr());
    ASSERT_TRUE(pointer_callback_);
    ASSERT_TRUE(key_pressed_callback_);
    ASSERT_TRUE(error_callback_);
  }

  void ResetAndStartOverlay() {
    DestroyOverlay();
    auto input_monitor = std::make_unique<FakeLocalInputMonitor>(base::BindOnce(
        &PrivacyOverlayWinTest::OnStartMonitoring, base::Unretained(this)));
    overlay_ = HostWindow::CreatePrivacyOverlay(std::move(input_monitor));
    ASSERT_NE(overlay_, nullptr);
    StartOverlay();
  }

  void OnStartMonitoring(
      LocalInputMonitor::PointerMoveCallback on_pointer_input,
      LocalInputMonitor::KeyPressedCallback on_keyboard_input,
      base::RepeatingClosure on_error) {
    pointer_callback_ = std::move(on_pointer_input);
    key_pressed_callback_ = std::move(on_keyboard_input);
    error_callback_ = std::move(on_error);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::AutoReset<bool> allow_capture_exclusion_failure_ =
      ScopedAllowCaptureExclusionFailureForTesting();
  base::AutoReset<std::optional<bool>> hotkey_registration_override_ =
      ScopedOverrideHotKeyRegistrationForTesting(true);
  base::AutoReset<std::optional<double>> text_scale_override_ =
      ScopedOverrideTextScaleFactorForTesting(1.0);
  testing::NiceMock<FakeClientSessionControl> client_session_control_{
      "user@domain/rest-of-jid"};
  LocalInputMonitor::PointerMoveCallback pointer_callback_;
  LocalInputMonitor::KeyPressedCallback key_pressed_callback_;
  base::RepeatingClosure error_callback_;
  std::unique_ptr<HostWindow> overlay_;
};

TEST_F(PrivacyOverlayWinTest,
       CreatesBackdropWindowsWithExpectedStylesAndAttributes) {
  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  ASSERT_FALSE(windows.backdrop_hwnds.empty());

  const std::wstring expected_hotkey_hint =
      LoadModuleString(IDS_DISCONNECT_HOTKEY_HINT);
  ASSERT_FALSE(expected_hotkey_hint.empty());

  // Verify every per-monitor backdrop window is visible, click-through,
  // opaque, positioned below `button_hwnd` in the `HWND_TOPMOST` Z-order band,
  // and carries the localized sharing message and disconnect hotkey hint as its
  // accessible window title.
  for (HWND backdrop_hwnd : windows.backdrop_hwnds) {
    EXPECT_TRUE(IsWindow(backdrop_hwnd));
    EXPECT_TRUE(IsWindowVisible(backdrop_hwnd));
    EXPECT_TRUE(IsWindowAbove(windows.button_hwnd, backdrop_hwnd));

    std::wstring backdrop_title = GetWindowTextOrEmpty(backdrop_hwnd);
    EXPECT_FALSE(backdrop_title.empty());
    EXPECT_NE(backdrop_title.find(L"user@domain"), std::wstring::npos);
    EXPECT_NE(backdrop_title.find(L"\n" + expected_hotkey_hint),
              std::wstring::npos);

    LONG_PTR ex_style = GetWindowLongPtr(backdrop_hwnd, GWL_EXSTYLE);
    EXPECT_NE(ex_style & WS_EX_TOPMOST, 0);
    EXPECT_NE(ex_style & WS_EX_TOOLWINDOW, 0);
    EXPECT_NE(ex_style & WS_EX_LAYERED, 0);
    EXPECT_NE(ex_style & WS_EX_TRANSPARENT, 0);
    EXPECT_NE(ex_style & WS_EX_NOACTIVATE, 0);

    BYTE alpha = 0;
    DWORD flags = 0;
    ASSERT_TRUE(
        GetLayeredWindowAttributes(backdrop_hwnd, nullptr, &alpha, &flags));
    EXPECT_EQ(alpha, 255);
    EXPECT_NE(flags & LWA_ALPHA, 0u);

    EXPECT_EQ(SendMessage(backdrop_hwnd, WM_NCHITTEST, 0, 0), HTTRANSPARENT);
    EXPECT_EQ(SendMessage(backdrop_hwnd, WM_MOUSEACTIVATE, 0, 0),
              MA_NOACTIVATE);
    EXPECT_EQ(SendMessage(backdrop_hwnd, WM_CLOSE, 0, 0), 0);
    EXPECT_TRUE(IsWindow(backdrop_hwnd));

    DWORD affinity = 0;
    if (GetWindowDisplayAffinity(backdrop_hwnd, &affinity) && affinity != 0) {
      EXPECT_EQ(affinity, static_cast<DWORD>(WDA_EXCLUDEFROMCAPTURE));
    }
  }
}

TEST_F(PrivacyOverlayWinTest,
       CreatesButtonWindowInInitialIdleClickThroughState) {
  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);

  // Verify the Disconnect button dialog starts in the hidden, click-through
  // state with localized control text and the sharing text control hidden
  // inside the button dialog.
  EXPECT_FALSE(IsWindowVisible(windows.button_hwnd));
  EXPECT_FALSE(GetWindowTextOrEmpty(windows.button_hwnd).empty());

  LONG_PTR button_ex_style = GetWindowLongPtr(windows.button_hwnd, GWL_EXSTYLE);
  EXPECT_NE(button_ex_style & WS_EX_TOPMOST, 0);
  EXPECT_NE(button_ex_style & WS_EX_TOOLWINDOW, 0);
  EXPECT_NE(button_ex_style & WS_EX_LAYERED, 0);
  EXPECT_NE(button_ex_style & WS_EX_TRANSPARENT, 0);
  EXPECT_NE(button_ex_style & WS_EX_NOACTIVATE, 0);
  EXPECT_EQ(button_ex_style & WS_EX_LAYOUTRTL, 0);

  BYTE button_alpha = 255;
  DWORD button_flags = 0;
  ASSERT_TRUE(GetLayeredWindowAttributes(windows.button_hwnd, nullptr,
                                         &button_alpha, &button_flags));
  EXPECT_EQ(button_alpha, 0);
  EXPECT_NE(button_flags & LWA_ALPHA, 0u);

  HWND disconnect_btn_hwnd = GetDlgItem(windows.button_hwnd, IDC_DISCONNECT);
  ASSERT_NE(disconnect_btn_hwnd, nullptr);
  EXPECT_FALSE(GetWindowTextOrEmpty(disconnect_btn_hwnd).empty());

  HWND sharing_text_hwnd =
      GetDlgItem(windows.button_hwnd, IDC_DISCONNECT_SHARINGWITH);
  ASSERT_NE(sharing_text_hwnd, nullptr);
  EXPECT_FALSE(IsWindowVisible(sharing_text_hwnd));
  const std::wstring sharing_control_text =
      GetWindowTextOrEmpty(sharing_text_hwnd);
  EXPECT_NE(sharing_control_text.find(L"user@domain"), std::wstring::npos);
  EXPECT_NE(sharing_control_text.find(
                L"\n" + LoadModuleString(IDS_DISCONNECT_HOTKEY_HINT)),
            std::wstring::npos);

  // Verify the Disconnect button dialog is horizontally centered on its target
  // monitor and positioned vertically below the centered backdrop text.
  HMONITOR monitor =
      MonitorFromWindow(windows.button_hwnd, MONITOR_DEFAULTTONEAREST);
  MONITORINFO monitor_info = {sizeof(monitor_info)};
  RECT button_rect = {};
  ASSERT_TRUE(GetMonitorInfo(monitor, &monitor_info));
  ASSERT_TRUE(GetWindowRect(windows.button_hwnd, &button_rect));

  int button_width = button_rect.right - button_rect.left;
  int monitor_width =
      monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
  int monitor_height =
      monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;
  int expected_left =
      monitor_info.rcMonitor.left + (monitor_width - button_width) / 2;
  int center_y = monitor_info.rcMonitor.top + monitor_height / 2;
  EXPECT_EQ(button_rect.left, expected_left);
  EXPECT_GT(button_rect.top, center_y);
  EXPECT_LE(button_rect.bottom, monitor_info.rcMonitor.bottom);
}

TEST_F(PrivacyOverlayWinTest,
       ActivatesButtonOnLocalMouseMoveAndAutoHidesAfterTimeout) {
  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  EXPECT_FALSE(IsWindowVisible(windows.button_hwnd));

  // Establish initial mouse position and move > 1px to guarantee activation
  // regardless of the initial cursor position on the test bot.
  pointer_callback_.Run(webrtc::DesktopVector(100, 100),
                        ui::EventType::kMouseMoved);
  pointer_callback_.Run(webrtc::DesktopVector(250, 250),
                        ui::EventType::kMouseMoved);

  EXPECT_TRUE(IsWindowVisible(windows.button_hwnd));
  EXPECT_EQ(
      GetWindowLongPtr(windows.button_hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT,
      0);
  BYTE alpha = 0;
  DWORD flags = 0;
  ASSERT_TRUE(
      GetLayeredWindowAttributes(windows.button_hwnd, nullptr, &alpha, &flags));
  EXPECT_EQ(alpha, 255);

  // Just before the 10-second timeout expires, the button must remain active.
  task_environment_.FastForwardBy(kAutoHideTimeout - base::Seconds(1));
  EXPECT_TRUE(IsWindowVisible(windows.button_hwnd));
  EXPECT_EQ(
      GetWindowLongPtr(windows.button_hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT,
      0);

  // Once the 10-second timeout expires, the button hides and restores
  // `WS_EX_TRANSPARENT`.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(IsWindowVisible(windows.button_hwnd));
  EXPECT_NE(
      GetWindowLongPtr(windows.button_hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT,
      0);
  ASSERT_TRUE(
      GetLayeredWindowAttributes(windows.button_hwnd, nullptr, &alpha, &flags));
  EXPECT_EQ(alpha, 0);
}

TEST_F(PrivacyOverlayWinTest,
       IgnoresOnePixelCursorDriftAndWakesOnTwoPixelMoveOrKeyboardInput) {
  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);

  // Establish baseline position at (200, 200) and let the button auto-hide.
  pointer_callback_.Run(webrtc::DesktopVector(200, 200),
                        ui::EventType::kMouseMoved);
  pointer_callback_.Run(webrtc::DesktopVector(210, 210),
                        ui::EventType::kMouseMoved);
  task_environment_.FastForwardBy(kAutoHideTimeout);
  ASSERT_FALSE(IsWindowVisible(windows.button_hwnd));

  // Movements of <= 1px in X and/or Y must not wake the button.
  pointer_callback_.Run(webrtc::DesktopVector(210, 210),
                        ui::EventType::kMouseMoved);
  EXPECT_FALSE(IsWindowVisible(windows.button_hwnd));

  pointer_callback_.Run(webrtc::DesktopVector(211, 211),
                        ui::EventType::kMouseMoved);
  EXPECT_FALSE(IsWindowVisible(windows.button_hwnd));

  pointer_callback_.Run(webrtc::DesktopVector(209, 209),
                        ui::EventType::kMouseMoved);
  EXPECT_FALSE(IsWindowVisible(windows.button_hwnd));

  // A movement of 2px from (210, 210) wakes the button and resets the timer.
  pointer_callback_.Run(webrtc::DesktopVector(212, 210),
                        ui::EventType::kMouseMoved);
  EXPECT_TRUE(IsWindowVisible(windows.button_hwnd));
  EXPECT_EQ(
      GetWindowLongPtr(windows.button_hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT,
      0);

  // Let the button auto-hide again, then verify local keyboard activity also
  // wakes the button for keyboard-only / screen-reader accessibility.
  task_environment_.FastForwardBy(kAutoHideTimeout);
  ASSERT_FALSE(IsWindowVisible(windows.button_hwnd));

  key_pressed_callback_.Run(0x04);
  EXPECT_TRUE(IsWindowVisible(windows.button_hwnd));
  EXPECT_EQ(
      GetWindowLongPtr(windows.button_hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT,
      0);
}

TEST_F(PrivacyOverlayWinTest,
       InputMonitorErrorStopsAutoHideAndKeepsButtonVisible) {
  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  ASSERT_FALSE(IsWindowVisible(windows.button_hwnd));

  // Simulate a local input monitor failure; the button must become permanently
  // visible and clickable so the local user can still disconnect.
  error_callback_.Run();

  EXPECT_TRUE(IsWindowVisible(windows.button_hwnd));
  EXPECT_EQ(
      GetWindowLongPtr(windows.button_hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT,
      0);

  task_environment_.FastForwardBy(kAutoHideTimeout * 3);
  EXPECT_TRUE(IsWindowVisible(windows.button_hwnd));
  EXPECT_EQ(
      GetWindowLongPtr(windows.button_hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT,
      0);
}

TEST_F(PrivacyOverlayWinTest, NullInputMonitorFailsOpenAndKeepsButtonVisible) {
  DestroyOverlay();
  overlay_ = HostWindow::CreatePrivacyOverlay(nullptr);
  ASSERT_NE(overlay_, nullptr);
  overlay_->Start(client_session_control_.GetWeakPtr());

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  EXPECT_TRUE(IsWindowVisible(windows.button_hwnd));
  EXPECT_EQ(
      GetWindowLongPtr(windows.button_hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT,
      0);
}

TEST_F(PrivacyOverlayWinTest,
       IgnoresRemoteInjectedClickAndDisconnectsOnLocalClick) {
  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);

  // Defense-in-depth: if a click arrives tagged with `kCrdInputExtraInfo`, it
  // must be ignored without disconnecting the session.
  EXPECT_CALL(client_session_control_, DisconnectSession(_, _, _)).Times(0);
  SetMessageExtraInfo(static_cast<LPARAM>(kCrdInputExtraInfo));
  SendMessage(windows.button_hwnd, WM_COMMAND,
              MAKEWPARAM(IDC_DISCONNECT, BN_CLICKED), 0);
  testing::Mock::VerifyAndClearExpectations(&client_session_control_);
  EXPECT_TRUE(IsWindow(windows.button_hwnd));

  // Non-click notifications (such as `BN_SETFOCUS`) on `IDC_DISCONNECT` must be
  // ignored without disconnecting the session.
  EXPECT_CALL(client_session_control_, DisconnectSession(_, _, _)).Times(0);
  SetMessageExtraInfo(0);
  SendMessage(windows.button_hwnd, WM_COMMAND,
              MAKEWPARAM(IDC_DISCONNECT, BN_SETFOCUS), 0);
  testing::Mock::VerifyAndClearExpectations(&client_session_control_);
  EXPECT_TRUE(IsWindow(windows.button_hwnd));

  // A genuine local click (extra info == 0) disconnects the session and
  // destroys all overlay windows.
  EXPECT_CALL(client_session_control_, DisconnectSession(ErrorCode::OK, _, _))
      .Times(1);
  SetMessageExtraInfo(0);
  SendMessage(windows.button_hwnd, WM_COMMAND,
              MAKEWPARAM(IDC_DISCONNECT, BN_CLICKED), 0);
  testing::Mock::VerifyAndClearExpectations(&client_session_control_);

  EXPECT_FALSE(IsWindow(windows.button_hwnd));
  for (HWND backdrop_hwnd : windows.backdrop_hwnds) {
    EXPECT_FALSE(IsWindow(backdrop_hwnd));
  }
}

TEST_F(PrivacyOverlayWinTest, HotkeyDisconnectsSessionAndDestroysWindows) {
  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);

  // Unrecognized hotkey IDs should be ignored.
  EXPECT_CALL(client_session_control_, DisconnectSession(_, _, _)).Times(0);
  SendMessage(windows.button_hwnd, WM_HOTKEY, kDisconnectHotkeyId + 1, 0);
  testing::Mock::VerifyAndClearExpectations(&client_session_control_);
  EXPECT_TRUE(IsWindow(windows.button_hwnd));

  EXPECT_CALL(client_session_control_, DisconnectSession(ErrorCode::OK, _, _))
      .Times(1);
  SendMessage(windows.button_hwnd, WM_HOTKEY, kDisconnectHotkeyId, 0);
  testing::Mock::VerifyAndClearExpectations(&client_session_control_);

  EXPECT_FALSE(IsWindow(windows.button_hwnd));
  for (HWND backdrop_hwnd : windows.backdrop_hwnds) {
    EXPECT_FALSE(IsWindow(backdrop_hwnd));
  }
}

TEST_F(PrivacyOverlayWinTest,
       ExternalDestroyOfButtonDialogDisconnectsSessionAndCleansUpBackdrops) {
  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  ASSERT_FALSE(windows.backdrop_hwnds.empty());

  // Simulate `DisconnectSession()` synchronously tearing down the session and
  // deleting `overlay_` while `DestroyWindow()` is still dispatching
  // `WM_DESTROY` / `WM_NCDESTROY`.
  EXPECT_CALL(client_session_control_, DisconnectSession(ErrorCode::OK, _, _))
      .WillOnce([this]() { DestroyOverlay(); });
  DestroyWindow(windows.button_hwnd);
  testing::Mock::VerifyAndClearExpectations(&client_session_control_);

  EXPECT_EQ(overlay_, nullptr);
  EXPECT_FALSE(IsWindow(windows.button_hwnd));
  for (HWND backdrop_hwnd : windows.backdrop_hwnds) {
    EXPECT_FALSE(IsWindow(backdrop_hwnd));
  }
}

TEST_F(PrivacyOverlayWinTest,
       DestroyingOverlayDestroysWindowsWithoutDisconnectingSession) {
  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  ASSERT_FALSE(windows.backdrop_hwnds.empty());

  EXPECT_CALL(client_session_control_, DisconnectSession(_, _, _)).Times(0);
  DestroyOverlay();
  testing::Mock::VerifyAndClearExpectations(&client_session_control_);

  EXPECT_FALSE(IsWindow(windows.button_hwnd));
  for (HWND backdrop_hwnd : windows.backdrop_hwnds) {
    EXPECT_FALSE(IsWindow(backdrop_hwnd));
  }
}

TEST_F(PrivacyOverlayWinTest,
       BlocksSystemMoveMinimizeAndRestoresFromMinimizedState) {
  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  ASSERT_FALSE(windows.backdrop_hwnds.empty());

  // System move/minimize and close requests must be blocked.
  EXPECT_EQ(SendMessage(windows.button_hwnd, WM_SYSCOMMAND, SC_MOVE, 0), 0);
  EXPECT_EQ(SendMessage(windows.button_hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0), 0);
  EXPECT_FALSE(IsIconic(windows.button_hwnd));
  EXPECT_EQ(SendMessage(windows.button_hwnd, WM_CLOSE, 0, 0), 0);
  EXPECT_TRUE(IsWindow(windows.button_hwnd));

  // Minimizing while the button is hidden clears `WS_MINIMIZE`, preserves the
  // non-iconic dialog dimensions, and keeps the button hidden.
  ShowWindow(windows.button_hwnd, SW_MINIMIZE);
  EXPECT_FALSE(IsIconic(windows.button_hwnd));
  EXPECT_FALSE(IsWindowVisible(windows.button_hwnd));

  // Wake the button via local keyboard input and verify `WM_SIZE` with
  // `SIZE_MINIMIZED` while visible keeps the button visible and above all
  // backdrops.
  key_pressed_callback_.Run(0x04);
  ASSERT_TRUE(IsWindowVisible(windows.button_hwnd));
  EXPECT_FALSE(IsIconic(windows.button_hwnd));
  EXPECT_EQ(SendMessage(windows.button_hwnd, WM_SIZE, SIZE_MINIMIZED, 0), 0);
  EXPECT_TRUE(IsWindowVisible(windows.button_hwnd));
  EXPECT_FALSE(IsIconic(windows.button_hwnd));
  for (HWND backdrop_hwnd : windows.backdrop_hwnds) {
    EXPECT_TRUE(IsWindowAbove(windows.button_hwnd, backdrop_hwnd));
  }
}

TEST_F(
    PrivacyOverlayWinTest,
    RepositionsOnExternalDisplacementUpToMaxAttemptsAndResetsOnDisplayChange) {
  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  ASSERT_FALSE(windows.backdrop_hwnds.empty());

  // External displacement via `WM_WINDOWPOSCHANGING` snaps back to the expected
  // position up to `kMaxRepositionAttempts` (3) times, then yields on the 4th.
  RECT expected_rect = {};
  ASSERT_TRUE(GetWindowRect(windows.button_hwnd, &expected_rect));

  for (int attempt = 0; attempt < DisconnectWindowBase::kMaxRepositionAttempts;
       ++attempt) {
    WINDOWPOS pos = {};
    pos.hwnd = windows.button_hwnd;
    pos.x = expected_rect.left + 100;
    pos.y = expected_rect.top + 100;
    pos.flags = SWP_NOSIZE | SWP_NOZORDER;
    SendMessage(windows.button_hwnd, WM_WINDOWPOSCHANGING, 0,
                reinterpret_cast<LPARAM>(&pos));
    EXPECT_EQ(pos.x, expected_rect.left);
    EXPECT_EQ(pos.y, expected_rect.top);
  }

  WINDOWPOS fourth_pos = {};
  fourth_pos.hwnd = windows.button_hwnd;
  fourth_pos.x = expected_rect.left + 100;
  fourth_pos.y = expected_rect.top + 100;
  fourth_pos.flags = SWP_NOSIZE | SWP_NOZORDER;
  SendMessage(windows.button_hwnd, WM_WINDOWPOSCHANGING, 0,
              reinterpret_cast<LPARAM>(&fourth_pos));
  EXPECT_EQ(fourth_pos.x, expected_rect.left + 100);
  EXPECT_EQ(fourth_pos.y, expected_rect.top + 100);

  // `WM_DISPLAYCHANGE` resets the reposition counter, refreshes backdrops, and
  // keeps `button_hwnd` above all backdrops in the `HWND_TOPMOST` band.
  EXPECT_EQ(SendMessage(windows.button_hwnd, WM_DISPLAYCHANGE, 32,
                        MAKELPARAM(1920, 1080)),
            0);
  for (HWND backdrop_hwnd : windows.backdrop_hwnds) {
    EXPECT_TRUE(IsWindowAbove(windows.button_hwnd, backdrop_hwnd));
  }

  WINDOWPOS post_display_change_pos = {};
  post_display_change_pos.hwnd = windows.button_hwnd;
  post_display_change_pos.x = expected_rect.left + 100;
  post_display_change_pos.y = expected_rect.top + 100;
  post_display_change_pos.flags = SWP_NOSIZE | SWP_NOZORDER;
  SendMessage(windows.button_hwnd, WM_WINDOWPOSCHANGING, 0,
              reinterpret_cast<LPARAM>(&post_display_change_pos));
  EXPECT_EQ(post_display_change_pos.x, expected_rect.left);
  EXPECT_EQ(post_display_change_pos.y, expected_rect.top);
}

TEST_F(PrivacyOverlayWinTest, ScalesAndRecentersButtonOnDpiChanged) {
  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  ASSERT_FALSE(windows.backdrop_hwnds.empty());
  HWND disconnect_btn_hwnd = GetDlgItem(windows.button_hwnd, IDC_DISCONNECT);
  ASSERT_NE(disconnect_btn_hwnd, nullptr);

  RECT rect_96_dpi = {};
  EXPECT_EQ(
      SendMessage(windows.button_hwnd, WM_DPICHANGED, MAKEWPARAM(96, 96), 0),
      0);
  ASSERT_TRUE(GetWindowRect(windows.button_hwnd, &rect_96_dpi));

  RECT rect_192_dpi = {};
  EXPECT_EQ(
      SendMessage(windows.button_hwnd, WM_DPICHANGED, MAKEWPARAM(192, 192), 0),
      0);
  ASSERT_TRUE(GetWindowRect(windows.button_hwnd, &rect_192_dpi));
  EXPECT_GT(rect_192_dpi.right - rect_192_dpi.left,
            rect_96_dpi.right - rect_96_dpi.left);
  EXPECT_GT(rect_192_dpi.bottom - rect_192_dpi.top,
            rect_96_dpi.bottom - rect_96_dpi.top);

  // Verify the button control still holds a live GDI font handle at the
  // updated 192-DPI height after repeated `UpdateButtonLayoutForDpi()` calls.
  HFONT font_192_dpi = reinterpret_cast<HFONT>(
      SendMessage(disconnect_btn_hwnd, WM_GETFONT, 0, 0));
  ASSERT_NE(font_192_dpi, nullptr);
  LOGFONTW lf_192_dpi = {};
  ASSERT_GT(GetObjectW(font_192_dpi, sizeof(lf_192_dpi), &lf_192_dpi), 0);
  EXPECT_EQ(lf_192_dpi.lfHeight,
            -ScaleForDpiAndTextScale(kBaseButtonFontSize, 192, 1.0));

  HMONITOR dpi_monitor =
      MonitorFromWindow(windows.button_hwnd, MONITOR_DEFAULTTONEAREST);
  MONITORINFO dpi_monitor_info = {sizeof(dpi_monitor_info)};
  ASSERT_TRUE(GetMonitorInfo(dpi_monitor, &dpi_monitor_info));
  int monitor_width =
      dpi_monitor_info.rcMonitor.right - dpi_monitor_info.rcMonitor.left;
  int width_192 = rect_192_dpi.right - rect_192_dpi.left;
  EXPECT_EQ(rect_192_dpi.left,
            dpi_monitor_info.rcMonitor.left + (monitor_width - width_192) / 2);

  for (HWND backdrop_hwnd : windows.backdrop_hwnds) {
    EXPECT_TRUE(IsWindowAbove(windows.button_hwnd, backdrop_hwnd));
  }
}

TEST_F(PrivacyOverlayWinTest, HandlesSysColorChangeAndControlColorMessages) {
  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  ASSERT_FALSE(windows.backdrop_hwnds.empty());

  // Reveal `button_hwnd` and clear pending update regions so
  // `WM_SYSCOLORCHANGE` invalidation can be observed on both the button dialog
  // and backdrops.
  key_pressed_callback_.Run(0);
  ASSERT_TRUE(IsWindowVisible(windows.button_hwnd));
  ValidateRect(windows.button_hwnd, nullptr);
  ASSERT_FALSE(GetUpdateRect(windows.button_hwnd, nullptr, FALSE));
  for (HWND backdrop_hwnd : windows.backdrop_hwnds) {
    ValidateRect(backdrop_hwnd, nullptr);
    ASSERT_FALSE(GetUpdateRect(backdrop_hwnd, nullptr, FALSE));
  }

  EXPECT_EQ(SendMessage(windows.button_hwnd, WM_SYSCOLORCHANGE, 0, 0), 0);
  EXPECT_TRUE(GetUpdateRect(windows.button_hwnd, nullptr, FALSE));
  for (HWND backdrop_hwnd : windows.backdrop_hwnds) {
    EXPECT_TRUE(GetUpdateRect(backdrop_hwnd, nullptr, FALSE));
  }

  EXPECT_NE(SendMessage(windows.button_hwnd, WM_CTLCOLORBTN, 0, 0), 0);
  EXPECT_NE(SendMessage(windows.button_hwnd, WM_CTLCOLORSTATIC, 0, 0), 0);
}

TEST_F(PrivacyOverlayWinTest, PaintsBackdropAndButtonWindowsOnPrintClient) {
  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  ASSERT_FALSE(windows.backdrop_hwnds.empty());

  HWND backdrop_hwnd = windows.backdrop_hwnds.front();
  RECT client_rect = {};
  ASSERT_TRUE(GetClientRect(backdrop_hwnd, &client_rect));
  int client_width = client_rect.right - client_rect.left;
  int client_height = client_rect.bottom - client_rect.top;
  ASSERT_GT(client_width, 0);
  ASSERT_GT(client_height, 0);

  base::win::ScopedGetDC screen_dc(nullptr);
  base::win::ScopedCreateDC mem_dc(CreateCompatibleDC(screen_dc));
  base::win::ScopedGDIObject<HBITMAP> bitmap(
      CreateCompatibleBitmap(screen_dc, client_width, client_height));
  base::win::ScopedSelectObject select_bitmap(mem_dc.get(), bitmap.get());

  const COLORREF expected_bg_color =
      IsHighContrastEnabled() ? GetSysColor(COLOR_WINDOW) : kGoogleBlueColor;
  SendMessage(backdrop_hwnd, WM_PRINTCLIENT,
              reinterpret_cast<WPARAM>(mem_dc.get()), PRF_CLIENT);
  EXPECT_EQ(GetPixel(mem_dc.get(), 10, 10), expected_bg_color);

  // Verify the central content area renders non-background pixels (the
  // centered icon and localized text glyphs).
  int center_x = client_width / 2;
  int center_y = client_height / 2;
  EXPECT_TRUE(HasNonBackgroundPixel(
      mem_dc.get(), std::max(0, center_x - 160), std::max(0, center_y - 160),
      std::min(client_width, center_x + 160),
      std::min(client_height, center_y + 120), expected_bg_color));

  SendMessage(windows.button_hwnd, WM_PRINTCLIENT,
              reinterpret_cast<WPARAM>(mem_dc.get()), PRF_CLIENT);
  EXPECT_EQ(GetPixel(mem_dc.get(), 10, 10), expected_bg_color);
}

TEST_F(PrivacyOverlayWinTest,
       ConfiguresLtrReadingOrderWithoutBiDiEmailWrapping) {
  base::test::ScopedRestoreICUDefaultLocale ltr_locale("en-US");
  ASSERT_FALSE(base::i18n::IsRTL());

  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  ASSERT_FALSE(windows.backdrop_hwnds.empty());
  EXPECT_EQ(
      GetWindowLongPtr(windows.button_hwnd, GWL_EXSTYLE) & WS_EX_RTLREADING, 0);

  for (HWND backdrop_hwnd : windows.backdrop_hwnds) {
    LONG_PTR ex_style = GetWindowLongPtr(backdrop_hwnd, GWL_EXSTYLE);
    EXPECT_EQ(ex_style & WS_EX_RTLREADING, 0);

    std::wstring backdrop_title;
    EXPECT_TRUE(GetWindowTextString(backdrop_hwnd, &backdrop_title));
    EXPECT_NE(backdrop_title.find(L"user@domain"), std::wstring::npos);
    EXPECT_EQ(backdrop_title.find(base::i18n::kLeftToRightEmbeddingMark),
              std::wstring::npos);
  }
}

TEST_F(PrivacyOverlayWinTest,
       ConfiguresRtlReadingOrderAndWrapsEmailWithLtrMarks) {
  base::test::ScopedRestoreICUDefaultLocale rtl_locale("ar");
  ASSERT_TRUE(base::i18n::IsRTL());

  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  ASSERT_FALSE(windows.backdrop_hwnds.empty());
  EXPECT_NE(
      GetWindowLongPtr(windows.button_hwnd, GWL_EXSTYLE) & WS_EX_RTLREADING, 0);

  HWND disconnect_btn_hwnd = GetDlgItem(windows.button_hwnd, IDC_DISCONNECT);
  ASSERT_NE(disconnect_btn_hwnd, nullptr);
  EXPECT_NE(
      GetWindowLongPtr(disconnect_btn_hwnd, GWL_EXSTYLE) & WS_EX_RTLREADING, 0);

  const std::wstring expected_wrapped_email =
      std::wstring(
          1, static_cast<wchar_t>(base::i18n::kLeftToRightEmbeddingMark)) +
      L"user@domain" +
      std::wstring(1,
                   static_cast<wchar_t>(base::i18n::kPopDirectionalFormatting));
  for (HWND backdrop_hwnd : windows.backdrop_hwnds) {
    LONG_PTR ex_style = GetWindowLongPtr(backdrop_hwnd, GWL_EXSTYLE);
    EXPECT_NE(ex_style & WS_EX_RTLREADING, 0);

    std::wstring backdrop_title;
    EXPECT_TRUE(GetWindowTextString(backdrop_hwnd, &backdrop_title));
    EXPECT_NE(backdrop_title.find(expected_wrapped_email), std::wstring::npos);
  }

  HWND backdrop_hwnd = windows.backdrop_hwnds.front();
  base::win::ScopedGetDC screen_dc(nullptr);
  base::win::ScopedCreateDC mem_dc(CreateCompatibleDC(screen_dc));
  base::win::ScopedGDIObject<HBITMAP> bitmap(
      CreateCompatibleBitmap(screen_dc, 64, 64));
  base::win::ScopedSelectObject select_bitmap(mem_dc.get(), bitmap.get());

  const COLORREF expected_bg_color =
      IsHighContrastEnabled() ? GetSysColor(COLOR_WINDOW) : kGoogleBlueColor;
  SendMessage(backdrop_hwnd, WM_PRINTCLIENT,
              reinterpret_cast<WPARAM>(mem_dc.get()), PRF_CLIENT);
  EXPECT_EQ(GetPixel(mem_dc.get(), 10, 10), expected_bg_color);
}

TEST_F(PrivacyOverlayWinTest, TruncatesOverlongClientEmailWithEllipsis) {
  // Verify that an overlong client JID (> 36 characters before `/`) is elided
  // with ellipsis by `FormatEmailForDisplay()` and applied to the backdrop
  // window title and sharing text control.
  constexpr std::string_view kLongEmail =
      "verylongusername1234567890@verylongdomainexample.com";
  testing::NiceMock<FakeClientSessionControl> long_email_control(
      std::string(kLongEmail) + "/chromoting-resource");
  overlay_->Start(long_email_control.GetWeakPtr());

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  ASSERT_FALSE(windows.backdrop_hwnds.empty());

  std::wstring backdrop_title;
  ASSERT_TRUE(
      GetWindowTextString(windows.backdrop_hwnds.front(), &backdrop_title));
  const std::wstring expected_elided_email =
      base::AsWString(FormatEmailForDisplay(kLongEmail));
  EXPECT_NE(expected_elided_email.find(L"\x2026"), std::wstring::npos);
  EXPECT_NE(backdrop_title.find(expected_elided_email), std::wstring::npos);
}

TEST_F(PrivacyOverlayWinTest,
       RendersHotkeyHintBelowSharingMessageAndAboveDisconnectButton) {
  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  ASSERT_FALSE(windows.backdrop_hwnds.empty());

  const std::wstring hotkey_hint = LoadModuleString(IDS_DISCONNECT_HOTKEY_HINT);
  ASSERT_FALSE(hotkey_hint.empty());

  // Locate the backdrop window on the monitor where `button_hwnd` is anchored.
  HMONITOR button_monitor =
      MonitorFromWindow(windows.button_hwnd, MONITOR_DEFAULTTONEAREST);
  HWND target_backdrop = windows.backdrop_hwnds.front();
  for (HWND backdrop_hwnd : windows.backdrop_hwnds) {
    if (MonitorFromWindow(backdrop_hwnd, MONITOR_DEFAULTTONEAREST) ==
        button_monitor) {
      target_backdrop = backdrop_hwnd;
      break;
    }
  }

  RECT client_rect = {};
  ASSERT_TRUE(GetClientRect(target_backdrop, &client_rect));
  int client_width = client_rect.right - client_rect.left;
  int client_height = client_rect.bottom - client_rect.top;
  ASSERT_GT(client_width, 0);
  ASSERT_GT(client_height, 0);

  base::win::ScopedGetDC screen_dc(nullptr);
  base::win::ScopedCreateDC mem_dc(CreateCompatibleDC(screen_dc));
  base::win::ScopedGDIObject<HBITMAP> bitmap(
      CreateCompatibleBitmap(screen_dc, client_width, client_height));
  base::win::ScopedSelectObject select_bitmap(mem_dc.get(), bitmap.get());

  const COLORREF expected_bg_color =
      IsHighContrastEnabled() ? GetSysColor(COLOR_WINDOW) : kGoogleBlueColor;
  SendMessage(target_backdrop, WM_PRINTCLIENT,
              reinterpret_cast<WPARAM>(mem_dc.get()), PRF_CLIENT);

  // Query the shared `ComputeBackdropContentLayout()` helper for
  // `target_backdrop` and verify glyphs are rendered in both the sharing band
  // and the hotkey hint band, and that `button_hwnd` sits strictly below the
  // hotkey hint band (unless clamped at the bottom of an unusually tiny
  // screen).
  UINT dpi = GetMonitorDpi(button_monitor, target_backdrop);
  const double text_scale = GetSystemTextScaleFactor();
  int spacing = ScaleForDpi(kBaseVerticalSpacing, dpi);
  base::win::ScopedGDIObject<HFONT> title_font = CreateOverlayFont(
      ScaleForDpiAndTextScale(kBaseTitleFontSize, dpi, text_scale),
      FW_SEMIBOLD);
  base::win::ScopedGDIObject<HFONT> body_font = CreateOverlayFont(
      ScaleForDpiAndTextScale(kBaseBodyFontSize, dpi, text_scale), FW_NORMAL);
  base::win::ScopedGDIObject<HFONT> hint_font = CreateOverlayFont(
      ScaleForDpiAndTextScale(kBaseHintFontSize, dpi, text_scale), FW_NORMAL);

  std::wstring product_name = LoadModuleString(IDS_PRODUCT_NAME);
  std::wstring backdrop_title = GetWindowTextOrEmpty(target_backdrop);
  size_t newline_pos = backdrop_title.find(L'\n');
  ASSERT_NE(newline_pos, std::wstring::npos);
  EXPECT_EQ(backdrop_title.substr(newline_pos + 1), hotkey_hint);
  std::wstring_view sharing_only =
      std::wstring_view(backdrop_title).substr(0, newline_pos);

  BackdropContentLayout layout = ComputeBackdropContentLayout(
      mem_dc.get(), client_width, client_height, dpi, text_scale,
      /*is_rtl=*/false, {.font = title_font.get(), .text = product_name},
      {.font = body_font.get(), .text = sharing_only},
      {.font = hint_font.get(), .text = hotkey_hint});
  ASSERT_GT(layout.body_height, 0);
  ASSERT_GT(layout.hint_height, 0);
  EXPECT_EQ(layout.hint_gap, spacing / 2);

  int body_bottom =
      std::min(client_height, layout.body_top + layout.body_height);
  int hint_bottom =
      std::min(client_height, layout.hint_top + layout.hint_height);
  ASSERT_LT(layout.body_top, body_bottom);
  ASSERT_LT(body_bottom, layout.hint_top);
  ASSERT_LT(layout.hint_top, hint_bottom);

  EXPECT_TRUE(HasNonBackgroundPixel(mem_dc.get(), std::max(0, layout.text_left),
                                    std::max(0, layout.body_top),
                                    std::min(client_width, layout.text_right),
                                    body_bottom, expected_bg_color));
  EXPECT_TRUE(HasNonBackgroundPixel(mem_dc.get(), std::max(0, layout.text_left),
                                    std::max(0, layout.hint_top),
                                    std::min(client_width, layout.text_right),
                                    hint_bottom, expected_bg_color));

  RECT backdrop_screen_rect = {};
  RECT button_screen_rect = {};
  ASSERT_TRUE(GetWindowRect(target_backdrop, &backdrop_screen_rect));
  ASSERT_TRUE(GetWindowRect(windows.button_hwnd, &button_screen_rect));
  int button_height = button_screen_rect.bottom - button_screen_rect.top;
  if (hint_bottom + spacing + button_height + spacing <= client_height) {
    int button_top_in_backdrop =
        button_screen_rect.top - backdrop_screen_rect.top;
    EXPECT_GE(button_top_in_backdrop, hint_bottom + spacing);
  }
}

TEST_F(PrivacyOverlayWinTest, OmitsHotkeyHintWhenHotkeyRegistrationFails) {
  base::AutoReset<std::optional<bool>> fail_hotkey_registration =
      ScopedOverrideHotKeyRegistrationForTesting(false);

  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  ASSERT_FALSE(windows.backdrop_hwnds.empty());

  const std::wstring hotkey_hint = LoadModuleString(IDS_DISCONNECT_HOTKEY_HINT);
  ASSERT_FALSE(hotkey_hint.empty());

  HWND sharing_text_hwnd =
      GetDlgItem(windows.button_hwnd, IDC_DISCONNECT_SHARINGWITH);
  ASSERT_NE(sharing_text_hwnd, nullptr);
  const std::wstring sharing_control_text =
      GetWindowTextOrEmpty(sharing_text_hwnd);
  EXPECT_NE(sharing_control_text.find(L"user@domain"), std::wstring::npos);
  EXPECT_EQ(sharing_control_text.find(L'\n'), std::wstring::npos);
  EXPECT_EQ(sharing_control_text.find(hotkey_hint), std::wstring::npos);

  for (HWND backdrop_hwnd : windows.backdrop_hwnds) {
    std::wstring backdrop_title = GetWindowTextOrEmpty(backdrop_hwnd);
    EXPECT_NE(backdrop_title.find(L"user@domain"), std::wstring::npos);
    EXPECT_EQ(backdrop_title.find(L'\n'), std::wstring::npos);
    EXPECT_EQ(backdrop_title.find(hotkey_hint), std::wstring::npos);
  }

  // Verify `PaintBackdrop()` also omits the hotkey hint glyphs below the
  // sharing message band.
  HWND backdrop_hwnd = windows.backdrop_hwnds.front();
  RECT client_rect = {};
  ASSERT_TRUE(GetClientRect(backdrop_hwnd, &client_rect));
  int client_width = client_rect.right - client_rect.left;
  int client_height = client_rect.bottom - client_rect.top;
  ASSERT_GT(client_width, 0);
  ASSERT_GT(client_height, 0);

  base::win::ScopedGetDC screen_dc(nullptr);
  base::win::ScopedCreateDC mem_dc(CreateCompatibleDC(screen_dc));
  base::win::ScopedGDIObject<HBITMAP> bitmap(
      CreateCompatibleBitmap(screen_dc, client_width, client_height));
  base::win::ScopedSelectObject select_bitmap(mem_dc.get(), bitmap.get());

  const COLORREF expected_bg_color =
      IsHighContrastEnabled() ? GetSysColor(COLOR_WINDOW) : kGoogleBlueColor;
  SendMessage(backdrop_hwnd, WM_PRINTCLIENT,
              reinterpret_cast<WPARAM>(mem_dc.get()), PRF_CLIENT);

  HMONITOR monitor = MonitorFromWindow(backdrop_hwnd, MONITOR_DEFAULTTONEAREST);
  UINT dpi = GetMonitorDpi(monitor, backdrop_hwnd);
  const double text_scale = GetSystemTextScaleFactor();
  base::win::ScopedGDIObject<HFONT> title_font = CreateOverlayFont(
      ScaleForDpiAndTextScale(kBaseTitleFontSize, dpi, text_scale),
      FW_SEMIBOLD);
  base::win::ScopedGDIObject<HFONT> body_font = CreateOverlayFont(
      ScaleForDpiAndTextScale(kBaseBodyFontSize, dpi, text_scale), FW_NORMAL);
  base::win::ScopedGDIObject<HFONT> hint_font = CreateOverlayFont(
      ScaleForDpiAndTextScale(kBaseHintFontSize, dpi, text_scale), FW_NORMAL);

  const std::wstring product_name = LoadModuleString(IDS_PRODUCT_NAME);
  BackdropContentLayout layout_if_shown = ComputeBackdropContentLayout(
      mem_dc.get(), client_width, client_height, dpi, text_scale,
      /*is_rtl=*/false, {.font = title_font.get(), .text = product_name},
      {.font = body_font.get(), .text = sharing_control_text},
      {.font = hint_font.get(), .text = hotkey_hint});
  int hint_bottom = std::min(
      client_height, layout_if_shown.hint_top + layout_if_shown.hint_height);
  if (layout_if_shown.hint_top < hint_bottom) {
    EXPECT_FALSE(HasNonBackgroundPixel(
        mem_dc.get(), std::max(0, layout_if_shown.text_left),
        std::max(0, layout_if_shown.hint_top),
        std::min(client_width, layout_if_shown.text_right), hint_bottom,
        expected_bg_color));
  }
}

TEST_F(PrivacyOverlayWinTest,
       ScalesDisconnectButtonFontAndSizeWithSystemTextScale) {
  StartOverlay();

  OverlayThreadWindows windows_1x = FindOverlayThreadWindows();
  ASSERT_NE(windows_1x.button_hwnd, nullptr);
  HWND disconnect_btn_1x = GetDlgItem(windows_1x.button_hwnd, IDC_DISCONNECT);
  ASSERT_NE(disconnect_btn_1x, nullptr);

  RECT rect_1x = {};
  ASSERT_TRUE(GetWindowRect(windows_1x.button_hwnd, &rect_1x));

  HFONT font_1x =
      reinterpret_cast<HFONT>(SendMessage(disconnect_btn_1x, WM_GETFONT, 0, 0));
  ASSERT_NE(font_1x, nullptr);
  LOGFONTW lf_1x = {};
  ASSERT_GT(GetObjectW(font_1x, sizeof(lf_1x), &lf_1x), 0);

  base::AutoReset<std::optional<double>> enlarged_text_scale =
      ScopedOverrideTextScaleFactorForTesting(1.75);
  ResetAndStartOverlay();

  OverlayThreadWindows windows_175x = FindOverlayThreadWindows();
  ASSERT_NE(windows_175x.button_hwnd, nullptr);
  HWND disconnect_btn_175x =
      GetDlgItem(windows_175x.button_hwnd, IDC_DISCONNECT);
  ASSERT_NE(disconnect_btn_175x, nullptr);

  HMONITOR monitor =
      MonitorFromWindow(windows_175x.button_hwnd, MONITOR_DEFAULTTONEAREST);
  UINT dpi = GetMonitorDpi(monitor, windows_175x.button_hwnd);

  HFONT font_175x = reinterpret_cast<HFONT>(
      SendMessage(disconnect_btn_175x, WM_GETFONT, 0, 0));
  ASSERT_NE(font_175x, nullptr);
  LOGFONTW lf_175x = {};
  ASSERT_GT(GetObjectW(font_175x, sizeof(lf_175x), &lf_175x), 0);
  EXPECT_EQ(lf_175x.lfHeight,
            -ScaleForDpiAndTextScale(kBaseButtonFontSize, dpi, 1.75));
  EXPECT_LT(lf_175x.lfHeight, lf_1x.lfHeight);

  RECT rect_175x = {};
  ASSERT_TRUE(GetWindowRect(windows_175x.button_hwnd, &rect_175x));
  EXPECT_GT(rect_175x.right - rect_175x.left, rect_1x.right - rect_1x.left);
  EXPECT_GT(rect_175x.bottom - rect_175x.top, rect_1x.bottom - rect_1x.top);
}

TEST_F(PrivacyOverlayWinTest, ScalesBackdropRenderedTextWithSystemTextScale) {
  StartOverlay();

  OverlayThreadWindows windows_1x = FindOverlayThreadWindows();
  ASSERT_NE(windows_1x.button_hwnd, nullptr);
  ASSERT_FALSE(windows_1x.backdrop_hwnds.empty());

  std::optional<CapturedBackdropSnapshot> snapshot_1x =
      CaptureBackdropSnapshot(windows_1x.backdrop_hwnds.front(),
                              /*text_scale=*/1.0);
  ASSERT_TRUE(snapshot_1x.has_value());

  // Use `kMaxTextScaleFactor` (2.25x) so the enlarged sharing message and gap
  // push `layout_225x.hint_top` completely below `hint_bottom_1x` on standard
  // displays, avoiding overlap with the 1.0x hint band.
  base::AutoReset<std::optional<double>> enlarged_text_scale =
      ScopedOverrideTextScaleFactorForTesting(kMaxTextScaleFactor);
  ResetAndStartOverlay();

  OverlayThreadWindows windows_225x = FindOverlayThreadWindows();
  ASSERT_NE(windows_225x.button_hwnd, nullptr);
  ASSERT_FALSE(windows_225x.backdrop_hwnds.empty());

  std::optional<CapturedBackdropSnapshot> snapshot_225x =
      CaptureBackdropSnapshot(windows_225x.backdrop_hwnds.front(),
                              /*text_scale=*/kMaxTextScaleFactor);
  ASSERT_TRUE(snapshot_225x.has_value());

  const BackdropContentLayout& layout_1x = snapshot_1x->layout;
  const BackdropContentLayout& layout_225x = snapshot_225x->layout;
  if (layout_225x.hint_top + layout_225x.hint_height >
      snapshot_225x->client_height) {
    GTEST_SKIP() << "Display height (" << snapshot_225x->client_height
                 << "px) is too short for unclamped 2.25x backdrop layout.";
  }

  int hint_bottom_1x = std::min(snapshot_1x->client_height,
                                layout_1x.hint_top + layout_1x.hint_height);
  int body_bottom_225x =
      std::min(snapshot_225x->client_height,
               layout_225x.body_top + layout_225x.body_height);
  int hint_bottom_225x =
      std::min(snapshot_225x->client_height,
               layout_225x.hint_top + layout_225x.hint_height);
  ASSERT_LT(layout_225x.body_top, body_bottom_225x);
  ASSERT_LT(layout_225x.hint_top, hint_bottom_225x);
  ASSERT_LT(hint_bottom_1x, hint_bottom_225x);

  int non_overlapping_hint_top = std::max(hint_bottom_1x, layout_225x.hint_top);
  ASSERT_LT(non_overlapping_hint_top, hint_bottom_225x);

  EXPECT_TRUE(
      snapshot_225x->HasGlyphPixels(layout_225x.text_left, layout_225x.body_top,
                                    layout_225x.text_right, body_bottom_225x));
  EXPECT_TRUE(
      snapshot_225x->HasGlyphPixels(layout_225x.text_left, layout_225x.hint_top,
                                    layout_225x.text_right, hint_bottom_225x));
  EXPECT_FALSE(snapshot_1x->HasGlyphPixels(
      layout_225x.text_left, non_overlapping_hint_top, layout_225x.text_right,
      hint_bottom_225x));
  EXPECT_TRUE(snapshot_225x->HasGlyphPixels(
      layout_225x.text_left, non_overlapping_hint_top, layout_225x.text_right,
      hint_bottom_225x));
}

TEST_F(PrivacyOverlayWinTest,
       WrapsDisconnectButtonHeightWhenClampedToNarrowMonitorAndRestores) {
  // Override the button label with an explicit multi-word string so
  // `DT_WORDBREAK` height growth is deterministic even on host locales whose
  // `STOP_SHARING_BUTTON` translation is a single word.
  constexpr std::wstring_view kMultiWordLabel = L"Stop Sharing";
  auto label_override =
      ScopedOverrideDisconnectButtonTextForTesting(kMultiWordLabel);

  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);

  HWND disconnect_btn_hwnd = GetDlgItem(windows.button_hwnd, IDC_DISCONNECT);
  ASSERT_NE(disconnect_btn_hwnd, nullptr);
  EXPECT_NE(GetWindowLongPtr(disconnect_btn_hwnd, GWL_STYLE) & BS_MULTILINE, 0);
  EXPECT_EQ(GetWindowTextOrEmpty(disconnect_btn_hwnd),
            std::wstring(kMultiWordLabel));

  RECT unclamped_rect = {};
  ASSERT_TRUE(GetWindowRect(windows.button_hwnd, &unclamped_rect));
  const int unclamped_width = unclamped_rect.right - unclamped_rect.left;
  const int unclamped_height = unclamped_rect.bottom - unclamped_rect.top;
  ASSERT_GT(unclamped_width, 0);
  ASSERT_GT(unclamped_height, 0);

  RECT unclamped_btn_rect = {};
  ASSERT_TRUE(GetWindowRect(disconnect_btn_hwnd, &unclamped_btn_rect));
  const int unclamped_btn_width =
      unclamped_btn_rect.right - unclamped_btn_rect.left;
  const int unclamped_btn_height =
      unclamped_btn_rect.bottom - unclamped_btn_rect.top;
  ASSERT_GT(unclamped_btn_width, 0);
  ASSERT_GT(unclamped_btn_height, 0);

  // Constrain `max_dialog_width` below the single-line width so
  // `IDC_DISCONNECT` wraps its label onto multiple lines while keeping its full
  // window/accessible text intact.
  const int narrow_max_width = std::max(40, (unclamped_width * 2) / 3);
  ASSERT_LT(narrow_max_width, unclamped_width);
  {
    auto narrow_width_override =
        ScopedOverrideMaxButtonDialogWidthForTesting(narrow_max_width);
    EXPECT_EQ(SendMessage(windows.button_hwnd, WM_DISPLAYCHANGE, 32,
                          MAKELPARAM(1920, 1080)),
              0);

    RECT clamped_rect = {};
    ASSERT_TRUE(GetWindowRect(windows.button_hwnd, &clamped_rect));
    EXPECT_EQ(clamped_rect.right - clamped_rect.left, narrow_max_width);
    EXPECT_GT(clamped_rect.bottom - clamped_rect.top, unclamped_height);
    EXPECT_EQ(GetWindowTextOrEmpty(disconnect_btn_hwnd),
              std::wstring(kMultiWordLabel));

    RECT clamped_btn_rect = {};
    ASSERT_TRUE(GetWindowRect(disconnect_btn_hwnd, &clamped_btn_rect));
    EXPECT_LT(clamped_btn_rect.right - clamped_btn_rect.left,
              unclamped_btn_width);
    EXPECT_GT(clamped_btn_rect.bottom - clamped_btn_rect.top,
              unclamped_btn_height);
    EXPECT_GT(clamped_btn_rect.left, clamped_rect.left);
    EXPECT_GT(clamped_btn_rect.top, clamped_rect.top);
    EXPECT_LT(clamped_btn_rect.right, clamped_rect.right);
    EXPECT_LT(clamped_btn_rect.bottom, clamped_rect.bottom);

    HMONITOR monitor =
        MonitorFromWindow(windows.button_hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info = {sizeof(monitor_info)};
    ASSERT_TRUE(GetMonitorInfo(monitor, &monitor_info));
    int monitor_width =
        monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
    EXPECT_EQ(clamped_rect.left, monitor_info.rcMonitor.left +
                                     (monitor_width - narrow_max_width) / 2);
  }

  // Restoring the monitor width budget returns the dialog and child button to
  // their single-line width and height.
  EXPECT_EQ(SendMessage(windows.button_hwnd, WM_DISPLAYCHANGE, 32,
                        MAKELPARAM(1920, 1080)),
            0);
  RECT restored_rect = {};
  ASSERT_TRUE(GetWindowRect(windows.button_hwnd, &restored_rect));
  EXPECT_EQ(restored_rect.right - restored_rect.left, unclamped_width);
  EXPECT_EQ(restored_rect.bottom - restored_rect.top, unclamped_height);
  RECT restored_btn_rect = {};
  ASSERT_TRUE(GetWindowRect(disconnect_btn_hwnd, &restored_btn_rect));
  EXPECT_EQ(restored_btn_rect.right - restored_btn_rect.left,
            unclamped_btn_width);
  EXPECT_EQ(restored_btn_rect.bottom - restored_btn_rect.top,
            unclamped_btn_height);
  EXPECT_EQ(GetWindowTextOrEmpty(disconnect_btn_hwnd),
            std::wstring(kMultiWordLabel));
}

TEST_F(PrivacyOverlayWinTest,
       WidensDisconnectButtonDialogUpToHardMaxForSingleUnbrokenWord) {
  constexpr std::wstring_view kSingleWordLabel =
      L"UnbrokenSingleWordDisconnectButtonLabel";
  auto label_override =
      ScopedOverrideDisconnectButtonTextForTesting(kSingleWordLabel);

  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  HWND disconnect_btn_hwnd = GetDlgItem(windows.button_hwnd, IDC_DISCONNECT);
  ASSERT_NE(disconnect_btn_hwnd, nullptr);
  EXPECT_EQ(GetWindowTextOrEmpty(disconnect_btn_hwnd),
            std::wstring(kSingleWordLabel));

  RECT unclamped_rect = {};
  ASSERT_TRUE(GetWindowRect(windows.button_hwnd, &unclamped_rect));
  const int unclamped_width = unclamped_rect.right - unclamped_rect.left;
  const int unclamped_height = unclamped_rect.bottom - unclamped_rect.top;
  ASSERT_GT(unclamped_width, 60);
  ASSERT_GT(unclamped_height, 0);

  // When `max_width` is narrower than the single unbroken word but
  // `hard_max_width` (representing the full monitor width) is large enough to
  // fit it, `UpdateButtonLayoutForDpi()` reclaims outer margins rather than
  // clipping horizontally, while keeping single-line height.
  const int soft_max_width = unclamped_width / 2;
  {
    auto width_override = ScopedOverrideMaxButtonDialogWidthForTesting(
        soft_max_width, /*hard_max_width=*/unclamped_width + 40);
    EXPECT_EQ(SendMessage(windows.button_hwnd, WM_DISPLAYCHANGE, 32,
                          MAKELPARAM(1920, 1080)),
              0);

    RECT widened_rect = {};
    ASSERT_TRUE(GetWindowRect(windows.button_hwnd, &widened_rect));
    EXPECT_GT(widened_rect.right - widened_rect.left, soft_max_width);
    EXPECT_EQ(widened_rect.right - widened_rect.left, unclamped_width);
    EXPECT_EQ(widened_rect.bottom - widened_rect.top, unclamped_height);
  }

  // When `hard_max_width` itself is narrower than the single unbroken word,
  // `dialog_width` widens past `soft_max_width` and clamps at `hard_max_width`.
  const int capped_hard_max_width = (unclamped_width * 3) / 4;
  ASSERT_GT(capped_hard_max_width, soft_max_width);
  ASSERT_LT(capped_hard_max_width, unclamped_width);
  {
    auto width_override = ScopedOverrideMaxButtonDialogWidthForTesting(
        soft_max_width, capped_hard_max_width);
    EXPECT_EQ(SendMessage(windows.button_hwnd, WM_DISPLAYCHANGE, 32,
                          MAKELPARAM(1920, 1080)),
              0);

    RECT capped_rect = {};
    ASSERT_TRUE(GetWindowRect(windows.button_hwnd, &capped_rect));
    EXPECT_EQ(capped_rect.right - capped_rect.left, capped_hard_max_width);
    EXPECT_EQ(capped_rect.bottom - capped_rect.top, unclamped_height);
  }
}

TEST_F(PrivacyOverlayWinTest,
       CapsWrappedDisconnectButtonDialogHeightToMaxHeightBudget) {
  constexpr std::wstring_view kMultiWordLabel = L"Stop Sharing";
  auto label_override =
      ScopedOverrideDisconnectButtonTextForTesting(kMultiWordLabel);

  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  HWND disconnect_btn_hwnd = GetDlgItem(windows.button_hwnd, IDC_DISCONNECT);
  ASSERT_NE(disconnect_btn_hwnd, nullptr);

  RECT unclamped_rect = {};
  ASSERT_TRUE(GetWindowRect(windows.button_hwnd, &unclamped_rect));
  const int unclamped_width = unclamped_rect.right - unclamped_rect.left;
  const int unclamped_height = unclamped_rect.bottom - unclamped_rect.top;
  ASSERT_GT(unclamped_width, 0);
  ASSERT_GT(unclamped_height, 0);

  const int narrow_max_width = std::max(40, (unclamped_width * 2) / 3);
  ASSERT_LT(narrow_max_width, unclamped_width);

  int uncapped_wrapped_height = 0;
  {
    auto narrow_width_override =
        ScopedOverrideMaxButtonDialogWidthForTesting(narrow_max_width);
    EXPECT_EQ(SendMessage(windows.button_hwnd, WM_DISPLAYCHANGE, 32,
                          MAKELPARAM(1920, 1080)),
              0);
    RECT wrapped_rect = {};
    ASSERT_TRUE(GetWindowRect(windows.button_hwnd, &wrapped_rect));
    uncapped_wrapped_height = wrapped_rect.bottom - wrapped_rect.top;
    ASSERT_GT(uncapped_wrapped_height, unclamped_height);
  }

  // Cap `max_height` below `uncapped_wrapped_height` and verify both the dialog
  // height cap and child `IDC_DISCONNECT` containment.
  const int capped_max_height = unclamped_height;
  {
    auto capped_override = ScopedOverrideMaxButtonDialogWidthForTesting(
        narrow_max_width, /*hard_max_width=*/narrow_max_width,
        capped_max_height);
    EXPECT_EQ(SendMessage(windows.button_hwnd, WM_DISPLAYCHANGE, 32,
                          MAKELPARAM(1920, 1080)),
              0);

    RECT capped_rect = {};
    ASSERT_TRUE(GetWindowRect(windows.button_hwnd, &capped_rect));
    EXPECT_EQ(capped_rect.right - capped_rect.left, narrow_max_width);
    EXPECT_EQ(capped_rect.bottom - capped_rect.top, capped_max_height);

    RECT capped_btn_rect = {};
    ASSERT_TRUE(GetWindowRect(disconnect_btn_hwnd, &capped_btn_rect));
    EXPECT_GT(capped_btn_rect.left, capped_rect.left);
    EXPECT_GT(capped_btn_rect.top, capped_rect.top);
    EXPECT_LT(capped_btn_rect.right, capped_rect.right);
    EXPECT_LT(capped_btn_rect.bottom, capped_rect.bottom);
  }

  EXPECT_EQ(SendMessage(windows.button_hwnd, WM_DISPLAYCHANGE, 32,
                        MAKELPARAM(1920, 1080)),
            0);
  RECT restored_rect = {};
  ASSERT_TRUE(GetWindowRect(windows.button_hwnd, &restored_rect));
  EXPECT_EQ(restored_rect.right - restored_rect.left, unclamped_width);
  EXPECT_EQ(restored_rect.bottom - restored_rect.top, unclamped_height);
}

}  // namespace remoting
