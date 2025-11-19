// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/mouse_cursor_monitor_win.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/test/task_environment.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/protocol/mouse_cursor_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

namespace remoting {

namespace {

using testing::_;

constexpr webrtc::ScreenId kFakeScreenId = 123;
constexpr int kFakeDisplayWidth = 1920;
constexpr int kFakeDisplayHeight = 1080;

class MockMouseCursorMonitorCallback
    : public protocol::MouseCursorMonitor::Callback {
 public:
  MockMouseCursorMonitorCallback() = default;
  ~MockMouseCursorMonitorCallback() override = default;

  MOCK_METHOD(void,
              OnMouseCursor,
              (std::unique_ptr<webrtc::MouseCursor> cursor),
              (override));
  MOCK_METHOD(void,
              OnMouseCursorPosition,
              (const webrtc::DesktopVector& position),
              (override));
  MOCK_METHOD(void,
              OnMouseCursorFractionalPosition,
              (const protocol::FractionalCoordinate& fractional_position),
              (override));
};

class FakeDesktopDisplayInfoMonitor : public DesktopDisplayInfoMonitor {
 public:
  FakeDesktopDisplayInfoMonitor() {
    info_.AddDisplay(DisplayGeometry(kFakeScreenId, 0, 0, kFakeDisplayWidth,
                                     kFakeDisplayHeight, 96, 32, true, ""));
  }

  void Start() override {}
  bool IsStarted() const override { return true; }
  const DesktopDisplayInfo* GetLatestDisplayInfo() const override {
    return &info_;
  }
  void AddCallback(base::RepeatingClosure callback) override {}

 private:
  DesktopDisplayInfo info_;
};

}  // namespace

class MouseCursorMonitorWinTest : public testing::Test {
 public:
  MouseCursorMonitorWinTest() {
    auto display_monitor = std::make_unique<FakeDesktopDisplayInfoMonitor>();
    display_monitor_ = display_monitor.get();
    monitor_ =
        std::make_unique<MouseCursorMonitorWin>(std::move(display_monitor));
    DCHECK_CALLED_ON_VALID_SEQUENCE(monitor_->sequence_checker_);
    monitor_->callback_ = &callback_;
  }

 protected:
  void CallOnMouseCursor(std::unique_ptr<webrtc::MouseCursor> cursor) {
    monitor_->OnMouseCursor(std::move(cursor));
  }

  void CallOnMouseCursorPosition(const webrtc::DesktopVector& position) {
    monitor_->OnMouseCursorPosition(position);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MouseCursorMonitorWin> monitor_;
  raw_ptr<FakeDesktopDisplayInfoMonitor> display_monitor_;
  MockMouseCursorMonitorCallback callback_;
};

TEST_F(MouseCursorMonitorWinTest, CursorShape) {
  auto cursor_image =
      std::make_unique<webrtc::BasicDesktopFrame>(webrtc::DesktopSize(10, 10));
  auto cursor = std::make_unique<webrtc::MouseCursor>(
      std::move(cursor_image), webrtc::DesktopVector(1, 2));

  std::unique_ptr<webrtc::MouseCursor> captured_cursor;
  EXPECT_CALL(callback_, OnMouseCursor(_))
      .WillOnce([&](std::unique_ptr<webrtc::MouseCursor> cursor) {
        captured_cursor = std::move(cursor);
      });

  CallOnMouseCursor(std::move(cursor));

  ASSERT_TRUE(captured_cursor);
  EXPECT_TRUE(captured_cursor->image()->size().equals({10, 10}));
  EXPECT_TRUE(captured_cursor->hotspot().equals({1, 2}));
}

TEST_F(MouseCursorMonitorWinTest, CursorPosition_InDisplay) {
  const webrtc::DesktopVector position(kFakeDisplayWidth - 1,
                                       kFakeDisplayHeight - 1);

  webrtc::DesktopVector captured_position;
  EXPECT_CALL(callback_, OnMouseCursorPosition(_))
      .WillOnce([&](const webrtc::DesktopVector& p) { captured_position = p; });

  protocol::FractionalCoordinate captured_fractional;
  EXPECT_CALL(callback_, OnMouseCursorFractionalPosition(_))
      .WillOnce([&](const protocol::FractionalCoordinate& f) {
        captured_fractional = f;
      });

  CallOnMouseCursorPosition(position);

  EXPECT_TRUE(captured_position.equals(position));
  EXPECT_EQ(captured_fractional.screen_id(), kFakeScreenId);
  EXPECT_FLOAT_EQ(captured_fractional.x(), 1.f);
  EXPECT_FLOAT_EQ(captured_fractional.y(), 1.f);
}

TEST_F(MouseCursorMonitorWinTest, CursorPosition_OutsideDisplay) {
  const webrtc::DesktopVector position(kFakeDisplayWidth + 1,
                                       kFakeDisplayHeight + 1);

  webrtc::DesktopVector captured_position;
  EXPECT_CALL(callback_, OnMouseCursorPosition(_))
      .WillOnce([&](const webrtc::DesktopVector& p) { captured_position = p; });
  EXPECT_CALL(callback_, OnMouseCursorFractionalPosition(_)).Times(0);

  CallOnMouseCursorPosition(position);

  EXPECT_TRUE(captured_position.equals(position));
}

}  // namespace remoting
