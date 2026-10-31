// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/icu_test_util.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "remoting/base/email_utils.h"
#include "remoting/base/errors.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/host_window.h"
#include "remoting/host/input_monitor/local_input_monitor.h"
#include "remoting/host/win/core_resource.h"
#include "remoting/host/win/host_window_win_util.h"
#include "remoting/host/win/input_extra_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace

class PrivacyOverlayWinTest : public testing::Test {
 protected:
  void SetUp() override {
    overlay_ = HostWindow::CreatePrivacyOverlay(nullptr);
    ASSERT_NE(overlay_, nullptr);
  }

  void TearDown() override {
    overlay_.reset();
    SetMessageExtraInfo(0);
  }

  void StartOverlay() { overlay_->Start(client_session_control_.GetWeakPtr()); }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  testing::NiceMock<FakeClientSessionControl> client_session_control_{
      "user@domain/rest-of-jid"};
  std::unique_ptr<HostWindow> overlay_;
};

TEST_F(PrivacyOverlayWinTest,
       CreatesBackdropAndButtonWindowsInInitialIdleClickThroughState) {
  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  ASSERT_FALSE(windows.backdrop_hwnds.empty());

  // Verify every per-monitor backdrop window is visible, click-through,
  // opaque, positioned below `button_hwnd` in the `HWND_TOPMOST` Z-order band,
  // and carries the localized sharing message as its accessible window title.
  for (HWND backdrop_hwnd : windows.backdrop_hwnds) {
    EXPECT_TRUE(IsWindow(backdrop_hwnd));
    EXPECT_TRUE(IsWindowVisible(backdrop_hwnd));
    EXPECT_TRUE(IsWindowAbove(windows.button_hwnd, backdrop_hwnd));

    std::wstring backdrop_title;
    EXPECT_TRUE(GetWindowTextString(backdrop_hwnd, &backdrop_title));
    EXPECT_NE(backdrop_title.find(L"user@domain"), std::wstring::npos);

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

  // Verify the Disconnect button dialog starts in the hidden, click-through
  // state with localized control text and the sharing text control hidden
  // inside the button dialog.
  EXPECT_FALSE(IsWindowVisible(windows.button_hwnd));
  std::wstring button_window_title;
  EXPECT_TRUE(GetWindowTextString(windows.button_hwnd, &button_window_title));

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
  std::wstring disconnect_btn_text;
  EXPECT_TRUE(GetWindowTextString(disconnect_btn_hwnd, &disconnect_btn_text));

  HWND sharing_text_hwnd =
      GetDlgItem(windows.button_hwnd, IDC_DISCONNECT_SHARINGWITH);
  ASSERT_NE(sharing_text_hwnd, nullptr);
  EXPECT_FALSE(IsWindowVisible(sharing_text_hwnd));
  std::wstring sharing_text;
  EXPECT_TRUE(GetWindowTextString(sharing_text_hwnd, &sharing_text));
  EXPECT_NE(sharing_text.find(L"user@domain"), std::wstring::npos);
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
      .WillOnce([this]() { overlay_.reset(); });
  DestroyWindow(windows.button_hwnd);
  testing::Mock::VerifyAndClearExpectations(&client_session_control_);

  EXPECT_EQ(overlay_, nullptr);
  EXPECT_FALSE(IsWindow(windows.button_hwnd));
  for (HWND backdrop_hwnd : windows.backdrop_hwnds) {
    EXPECT_FALSE(IsWindow(backdrop_hwnd));
  }
}

TEST_F(PrivacyOverlayWinTest, PaintsBackdropAndButtonDialogBackgrounds) {
  StartOverlay();

  OverlayThreadWindows windows = FindOverlayThreadWindows();
  ASSERT_NE(windows.button_hwnd, nullptr);
  ASSERT_FALSE(windows.backdrop_hwnds.empty());

  EXPECT_NE(SendMessage(windows.button_hwnd, WM_CTLCOLORBTN, 0, 0), 0);
  EXPECT_NE(SendMessage(windows.button_hwnd, WM_CTLCOLORSTATIC, 0, 0), 0);

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

  EXPECT_CALL(client_session_control_, DisconnectSession(_, _, _)).Times(0);
  overlay_.reset();
  testing::Mock::VerifyAndClearExpectations(&client_session_control_);

  EXPECT_FALSE(IsWindow(windows.button_hwnd));
  for (HWND hwnd : windows.backdrop_hwnds) {
    EXPECT_FALSE(IsWindow(hwnd));
  }
}

TEST_F(PrivacyOverlayWinTest,
       ConfiguresLtrAndRtlReadingOrderAndBiDiEmailFormatting) {
  // Verify LTR locale behavior: no `WS_EX_RTLREADING` extended style and no
  // explicit LRE/PDF directional wrapping around the formatted email.
  {
    base::test::ScopedRestoreICUDefaultLocale ltr_locale("en-US");
    ASSERT_FALSE(base::i18n::IsRTL());

    StartOverlay();

    OverlayThreadWindows windows = FindOverlayThreadWindows();
    ASSERT_NE(windows.button_hwnd, nullptr);
    ASSERT_FALSE(windows.backdrop_hwnds.empty());
    EXPECT_EQ(
        GetWindowLongPtr(windows.button_hwnd, GWL_EXSTYLE) & WS_EX_RTLREADING,
        0);

    for (HWND backdrop_hwnd : windows.backdrop_hwnds) {
      LONG_PTR ex_style = GetWindowLongPtr(backdrop_hwnd, GWL_EXSTYLE);
      EXPECT_EQ(ex_style & WS_EX_RTLREADING, 0);

      std::wstring backdrop_title;
      EXPECT_TRUE(GetWindowTextString(backdrop_hwnd, &backdrop_title));
      EXPECT_NE(backdrop_title.find(L"user@domain"), std::wstring::npos);
      EXPECT_EQ(backdrop_title.find(base::i18n::kLeftToRightEmbeddingMark),
                std::wstring::npos);
    }

    overlay_.reset();
  }

  // Verify RTL locale behavior: `WS_EX_RTLREADING` is set on the button
  // dialog, button control, and every backdrop window; the formatted email is
  // wrapped in explicit LRE/PDF directional marks so `@` or leading punctuation
  // is not reordered by BiDi layout; and `WM_PRINTCLIENT` renders cleanly.
  {
    base::test::ScopedRestoreICUDefaultLocale rtl_locale("ar");
    ASSERT_TRUE(base::i18n::IsRTL());

    overlay_ = HostWindow::CreatePrivacyOverlay(nullptr);
    ASSERT_NE(overlay_, nullptr);
    StartOverlay();

    OverlayThreadWindows windows = FindOverlayThreadWindows();
    ASSERT_NE(windows.button_hwnd, nullptr);
    ASSERT_FALSE(windows.backdrop_hwnds.empty());
    EXPECT_NE(
        GetWindowLongPtr(windows.button_hwnd, GWL_EXSTYLE) & WS_EX_RTLREADING,
        0);

    HWND disconnect_btn_hwnd = GetDlgItem(windows.button_hwnd, IDC_DISCONNECT);
    ASSERT_NE(disconnect_btn_hwnd, nullptr);
    EXPECT_NE(
        GetWindowLongPtr(disconnect_btn_hwnd, GWL_EXSTYLE) & WS_EX_RTLREADING,
        0);

    const std::wstring expected_wrapped_email =
        std::wstring(
            1, static_cast<wchar_t>(base::i18n::kLeftToRightEmbeddingMark)) +
        L"user@domain" +
        std::wstring(
            1, static_cast<wchar_t>(base::i18n::kPopDirectionalFormatting));
    for (HWND backdrop_hwnd : windows.backdrop_hwnds) {
      LONG_PTR ex_style = GetWindowLongPtr(backdrop_hwnd, GWL_EXSTYLE);
      EXPECT_NE(ex_style & WS_EX_RTLREADING, 0);

      std::wstring backdrop_title;
      EXPECT_TRUE(GetWindowTextString(backdrop_hwnd, &backdrop_title));
      EXPECT_NE(backdrop_title.find(expected_wrapped_email),
                std::wstring::npos);
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

}  // namespace remoting
