// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "remoting/base/errors.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/host_window.h"
#include "remoting/host/input_monitor/local_input_monitor.h"
#include "remoting/host/win/host_window_win_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using ::testing::_;
using ::testing::ReturnRef;

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

// Callback for `EnumThreadWindows()` that collects per-monitor privacy overlay
// backdrop windows owned by the current thread.
BOOL CALLBACK EnumBackdropWindowsCallback(HWND hwnd, LPARAM lparam) {
  auto* backdrop_hwnds = reinterpret_cast<std::vector<HWND>*>(lparam);
  wchar_t class_name[128] = {};
  if (GetClassNameW(hwnd, class_name, std::size(class_name)) > 0 &&
      std::wstring_view(class_name) == kPrivacyOverlayClassName) {
    backdrop_hwnds->push_back(hwnd);
  }
  return TRUE;
}

// Enumerates all privacy overlay backdrop windows owned by the current thread.
std::vector<HWND> FindBackdropWindows() {
  std::vector<HWND> backdrop_hwnds;
  EnumThreadWindows(GetCurrentThreadId(), &EnumBackdropWindowsCallback,
                    reinterpret_cast<LPARAM>(&backdrop_hwnds));
  return backdrop_hwnds;
}

}  // namespace

class PrivacyOverlayWinTest : public testing::Test {
 protected:
  void SetUp() override {
    overlay_ = HostWindow::CreatePrivacyOverlay(nullptr);
    ASSERT_NE(overlay_, nullptr);
  }

  void TearDown() override { overlay_.reset(); }

  void StartOverlay() { overlay_->Start(client_session_control_.GetWeakPtr()); }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  testing::NiceMock<FakeClientSessionControl> client_session_control_{
      "user@domain/rest-of-jid"};
  std::unique_ptr<HostWindow> overlay_;
};

TEST_F(PrivacyOverlayWinTest, CreatesClickThroughCaptureExcludedBackdrops) {
  StartOverlay();

  std::vector<HWND> backdrop_hwnds = FindBackdropWindows();
  ASSERT_FALSE(backdrop_hwnds.empty());

  for (HWND backdrop_hwnd : backdrop_hwnds) {
    EXPECT_TRUE(IsWindow(backdrop_hwnd));
    EXPECT_TRUE(IsWindowVisible(backdrop_hwnd));

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

TEST_F(PrivacyOverlayWinTest, PaintsBackdropBackgroundAndDestroysCleanly) {
  StartOverlay();

  std::vector<HWND> backdrop_hwnds = FindBackdropWindows();
  ASSERT_FALSE(backdrop_hwnds.empty());

  HWND backdrop_hwnd = backdrop_hwnds.front();
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

  EXPECT_CALL(client_session_control_, DisconnectSession(_, _, _)).Times(0);
  overlay_.reset();
  testing::Mock::VerifyAndClearExpectations(&client_session_control_);

  for (HWND hwnd : backdrop_hwnds) {
    EXPECT_FALSE(IsWindow(hwnd));
  }
}

}  // namespace remoting
