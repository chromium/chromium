// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webrtc_mouse_cursor_monitor_adaptor.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/proto/coordinates.pb.h"
#include "remoting/protocol/mouse_cursor_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

using ::testing::_;
using ::testing::Return;

namespace remoting {

namespace {

constexpr webrtc::ScreenId kFakeScreenId = 123;
constexpr int kFakeDisplayWidth = 1920;
constexpr int kFakeDisplayHeight = 1080;

class MockMouseCursorMonitorCallback
    : public protocol::MouseCursorMonitor::Callback {
 public:
  MOCK_METHOD(void,
              OnMouseCursor,
              (std::unique_ptr<webrtc::MouseCursor> mouse_cursor),
              (override));
  MOCK_METHOD(void,
              OnMouseCursorPosition,
              (const webrtc::DesktopVector& position),
              (override));
  MOCK_METHOD(void,
              OnMouseCursorFractionalPosition,
              (const protocol::FractionalCoordinate& position),
              (override));
};

class FakeDesktopDisplayInfoMonitor : public DesktopDisplayInfoMonitor {
 public:
  FakeDesktopDisplayInfoMonitor() {
    info.emplace();
    info->AddDisplay(DisplayGeometry(kFakeScreenId, 0, 0, kFakeDisplayWidth,
                                     kFakeDisplayHeight, 96, 32, true, ""));
  }

  void Start() override { started = true; }

  bool IsStarted() const override { return started; }

  const DesktopDisplayInfo* GetLatestDisplayInfo() const override {
    return info ? &info.value() : nullptr;
  }

  void AddCallback(base::RepeatingClosure callback) override { NOTREACHED(); }

  bool started = false;
  std::optional<DesktopDisplayInfo> info;
};

class FakeMouseCursorMonitor : public webrtc::MouseCursorMonitor {
 public:
  FakeMouseCursorMonitor() = default;
  ~FakeMouseCursorMonitor() override = default;

  void Init(Callback* callback, Mode mode) override {
    EXPECT_FALSE(callback_);
    EXPECT_TRUE(callback);

    callback_ = callback;
    mode_ = mode;
  }

  void Capture() override {
    ASSERT_TRUE(callback_);
    capture_call_count_++;

    if (cursor_to_send_) {
      callback_->OnMouseCursor(cursor_to_send_.release());
    }

    if (position_to_send_) {
      callback_->OnMouseCursorPosition(*position_to_send_);
      position_to_send_.reset();
    }
  }

  void set_cursor_to_send(std::unique_ptr<webrtc::MouseCursor> cursor) {
    cursor_to_send_ = std::move(cursor);
  }

  void set_position_to_send(std::optional<webrtc::DesktopVector> position) {
    position_to_send_ = std::move(position);
  }

  int get_capture_call_count() const { return capture_call_count_; }

 private:
  int capture_call_count_ = 0;
  raw_ptr<Callback> callback_ = nullptr;
  Mode mode_ = SHAPE_AND_POSITION;
  std::unique_ptr<webrtc::MouseCursor> cursor_to_send_;
  std::optional<webrtc::DesktopVector> position_to_send_;
};

}  // namespace

class WebrtcMouseCursorMonitorAdaptorTest : public testing::Test {
 public:
  WebrtcMouseCursorMonitorAdaptorTest();

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  MockMouseCursorMonitorCallback callback_;
  std::unique_ptr<WebrtcMouseCursorMonitorAdaptor> adaptor_;
  raw_ptr<FakeMouseCursorMonitor> mouse_monitor_;
  raw_ptr<FakeDesktopDisplayInfoMonitor> display_info_monitor_;
};

WebrtcMouseCursorMonitorAdaptorTest::WebrtcMouseCursorMonitorAdaptorTest() {
  auto mouse_monitor = std::make_unique<FakeMouseCursorMonitor>();
  auto display_info_monitor = std::make_unique<FakeDesktopDisplayInfoMonitor>();
  mouse_monitor_ = mouse_monitor.get();
  display_info_monitor_ = display_info_monitor.get();
  adaptor_ = std::make_unique<WebrtcMouseCursorMonitorAdaptor>(
      std::move(mouse_monitor), std::move(display_info_monitor));
}

TEST_F(WebrtcMouseCursorMonitorAdaptorTest, CaptureCursorShape) {
  base::RunLoop run_loop;
  std::unique_ptr<webrtc::MouseCursor> cursor_to_send =
      std::make_unique<webrtc::MouseCursor>(
          std::make_unique<webrtc::BasicDesktopFrame>(
              webrtc::DesktopSize{64, 32}),
          webrtc::DesktopVector{11, 12});
  mouse_monitor_->set_cursor_to_send(std::move(cursor_to_send));
  std::unique_ptr<webrtc::MouseCursor> captured_cursor;
  EXPECT_CALL(callback_, OnMouseCursor(_))
      .WillOnce([&](std::unique_ptr<webrtc::MouseCursor> owned_cursor) {
        captured_cursor = std::move(owned_cursor);
        run_loop.Quit();
      });

  adaptor_->Init(&callback_);
  run_loop.Run();

  ASSERT_TRUE(captured_cursor);
  ASSERT_TRUE(captured_cursor->image()->size().equals({64, 32}));
  ASSERT_TRUE(captured_cursor->hotspot().equals({11, 12}));
}

TEST_F(WebrtcMouseCursorMonitorAdaptorTest,
       CaptureCursorPosition_NoDisplayInfo_OnlyReportsAbsolutePosition) {
  display_info_monitor_->info.reset();
  mouse_monitor_->set_position_to_send(
      webrtc::DesktopVector{kFakeDisplayWidth - 1, kFakeDisplayHeight - 1});
  base::RunLoop run_loop;
  webrtc::DesktopVector captured_position;
  EXPECT_CALL(callback_, OnMouseCursorPosition(_))
      .WillOnce([&](const webrtc::DesktopVector& position) {
        captured_position = position;
        run_loop.Quit();
      });
  EXPECT_CALL(callback_, OnMouseCursorFractionalPosition(_)).Times(0);

  adaptor_->Init(&callback_);
  run_loop.Run();

  ASSERT_TRUE(display_info_monitor_->IsStarted());
  ASSERT_TRUE(captured_position.equals(
      {kFakeDisplayWidth - 1, kFakeDisplayHeight - 1}));
}

TEST_F(WebrtcMouseCursorMonitorAdaptorTest,
       CaptureCursorPosition_CursorNotInDisplay_OnlyReportsAbsolutePosition) {
  mouse_monitor_->set_position_to_send(
      webrtc::DesktopVector{kFakeDisplayWidth + 1, kFakeDisplayHeight + 1});
  base::RunLoop run_loop;
  webrtc::DesktopVector captured_position;
  EXPECT_CALL(callback_, OnMouseCursorPosition(_))
      .WillOnce([&](const webrtc::DesktopVector& position) {
        captured_position = position;
        run_loop.Quit();
      });
  EXPECT_CALL(callback_, OnMouseCursorFractionalPosition(_)).Times(0);

  adaptor_->Init(&callback_);
  run_loop.Run();

  ASSERT_TRUE(display_info_monitor_->IsStarted());
  ASSERT_TRUE(captured_position.equals(
      {kFakeDisplayWidth + 1, kFakeDisplayHeight + 1}));
}

TEST_F(
    WebrtcMouseCursorMonitorAdaptorTest,
    CaptureCursorPosition_CursorInDisplay_ReportsAbsoluteAndFractionalPositions) {
  mouse_monitor_->set_position_to_send(
      webrtc::DesktopVector{kFakeDisplayWidth - 1, kFakeDisplayHeight - 1});
  base::RunLoop run_loop_1;
  webrtc::DesktopVector captured_position;
  EXPECT_CALL(callback_, OnMouseCursorPosition(_))
      .WillOnce([&](const webrtc::DesktopVector& position) {
        captured_position = position;
        run_loop_1.Quit();
      });
  base::RunLoop run_loop_2;
  protocol::FractionalCoordinate captured_fractional_position;
  EXPECT_CALL(callback_, OnMouseCursorFractionalPosition(_))
      .WillOnce([&](const protocol::FractionalCoordinate& position) {
        captured_fractional_position = position;
        run_loop_2.Quit();
      });

  adaptor_->Init(&callback_);
  run_loop_1.Run();
  run_loop_2.Run();

  ASSERT_TRUE(display_info_monitor_->IsStarted());
  ASSERT_TRUE(captured_position.equals(
      {kFakeDisplayWidth - 1, kFakeDisplayHeight - 1}));
  ASSERT_EQ(captured_fractional_position.screen_id(), kFakeScreenId);
  ASSERT_FLOAT_EQ(captured_fractional_position.x(), 1.f);
  ASSERT_FLOAT_EQ(captured_fractional_position.y(), 1.f);
}

TEST_F(WebrtcMouseCursorMonitorAdaptorTest, DefaultCaptureInterval) {
  adaptor_->Init(&callback_);
  ASSERT_EQ(mouse_monitor_->get_capture_call_count(), 0);

  task_environment_.FastForwardBy(
      WebrtcMouseCursorMonitorAdaptor::GetDefaultCaptureInterval() -
      base::Milliseconds(1));
  ASSERT_EQ(mouse_monitor_->get_capture_call_count(), 0);

  task_environment_.FastForwardBy(base::Milliseconds(1));
  ASSERT_EQ(mouse_monitor_->get_capture_call_count(), 1);

  task_environment_.FastForwardBy(
      WebrtcMouseCursorMonitorAdaptor::GetDefaultCaptureInterval());
  ASSERT_EQ(mouse_monitor_->get_capture_call_count(), 2);
}

TEST_F(WebrtcMouseCursorMonitorAdaptorTest, SetPreferredCaptureInterval) {
  adaptor_->Init(&callback_);

  base::TimeDelta test_capture_interval = base::Milliseconds(15);
  adaptor_->SetPreferredCaptureInterval(test_capture_interval);
  ASSERT_EQ(mouse_monitor_->get_capture_call_count(), 0);

  task_environment_.FastForwardBy(test_capture_interval -
                                  base::Milliseconds(1));
  ASSERT_EQ(mouse_monitor_->get_capture_call_count(), 0);

  task_environment_.FastForwardBy(base::Milliseconds(2));
  ASSERT_EQ(mouse_monitor_->get_capture_call_count(), 1);

  task_environment_.FastForwardBy(test_capture_interval);
  ASSERT_EQ(mouse_monitor_->get_capture_call_count(), 2);
}

}  // namespace remoting
