// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webrtc_mouse_cursor_monitor_adaptor.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "remoting/protocol/mouse_cursor_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

using ::testing::_;

namespace remoting {

namespace {

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
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  MockMouseCursorMonitorCallback callback_;
};

TEST_F(WebrtcMouseCursorMonitorAdaptorTest, CaptureCursorShape) {
  auto fake_monitor = std::make_unique<FakeMouseCursorMonitor>();
  base::RunLoop run_loop;
  std::unique_ptr<webrtc::MouseCursor> cursor_to_send =
      std::make_unique<webrtc::MouseCursor>(
          std::make_unique<webrtc::BasicDesktopFrame>(
              webrtc::DesktopSize{64, 32}),
          webrtc::DesktopVector{11, 12});
  fake_monitor->set_cursor_to_send(std::move(cursor_to_send));
  std::unique_ptr<webrtc::MouseCursor> captured_cursor;
  EXPECT_CALL(callback_, OnMouseCursor(_))
      .WillOnce([&](std::unique_ptr<webrtc::MouseCursor> owned_cursor) {
        captured_cursor = std::move(owned_cursor);
        run_loop.Quit();
      });

  WebrtcMouseCursorMonitorAdaptor adaptor(std::move(fake_monitor));
  adaptor.Init(&callback_);
  run_loop.Run();

  ASSERT_TRUE(captured_cursor);
  ASSERT_TRUE(captured_cursor->image()->size().equals({64, 32}));
  ASSERT_TRUE(captured_cursor->hotspot().equals({11, 12}));
}

TEST_F(WebrtcMouseCursorMonitorAdaptorTest, CaptureCursorPosition) {
  auto fake_monitor = std::make_unique<FakeMouseCursorMonitor>();
  fake_monitor->set_position_to_send(webrtc::DesktopVector{100, 200});
  base::RunLoop run_loop;
  webrtc::DesktopVector captured_position;
  EXPECT_CALL(callback_, OnMouseCursorPosition(_))
      .WillOnce([&](const webrtc::DesktopVector& position) {
        captured_position = position;
        run_loop.Quit();
      });

  WebrtcMouseCursorMonitorAdaptor adaptor(std::move(fake_monitor));
  adaptor.Init(&callback_);
  run_loop.Run();

  ASSERT_TRUE(captured_position.equals({100, 200}));
}

TEST_F(WebrtcMouseCursorMonitorAdaptorTest, DefaultCaptureInterval) {
  auto fake_monitor = std::make_unique<FakeMouseCursorMonitor>();
  FakeMouseCursorMonitor* fake_monitor_ptr = fake_monitor.get();

  WebrtcMouseCursorMonitorAdaptor adaptor(std::move(fake_monitor));
  adaptor.Init(&callback_);
  ASSERT_EQ(fake_monitor_ptr->get_capture_call_count(), 0);

  task_environment_.FastForwardBy(
      WebrtcMouseCursorMonitorAdaptor::GetDefaultCaptureInterval() -
      base::Milliseconds(1));
  ASSERT_EQ(fake_monitor_ptr->get_capture_call_count(), 0);

  task_environment_.FastForwardBy(base::Milliseconds(1));
  ASSERT_EQ(fake_monitor_ptr->get_capture_call_count(), 1);

  task_environment_.FastForwardBy(
      WebrtcMouseCursorMonitorAdaptor::GetDefaultCaptureInterval());
  ASSERT_EQ(fake_monitor_ptr->get_capture_call_count(), 2);
}

TEST_F(WebrtcMouseCursorMonitorAdaptorTest, SetPreferredCaptureInterval) {
  auto fake_monitor = std::make_unique<FakeMouseCursorMonitor>();
  FakeMouseCursorMonitor* fake_monitor_ptr = fake_monitor.get();

  WebrtcMouseCursorMonitorAdaptor adaptor(std::move(fake_monitor));
  adaptor.Init(&callback_);

  base::TimeDelta test_capture_interval = base::Milliseconds(15);
  adaptor.SetPreferredCaptureInterval(test_capture_interval);
  ASSERT_EQ(fake_monitor_ptr->get_capture_call_count(), 0);

  task_environment_.FastForwardBy(test_capture_interval -
                                  base::Milliseconds(1));
  ASSERT_EQ(fake_monitor_ptr->get_capture_call_count(), 0);

  task_environment_.FastForwardBy(base::Milliseconds(2));
  ASSERT_EQ(fake_monitor_ptr->get_capture_call_count(), 1);

  task_environment_.FastForwardBy(test_capture_interval);
  ASSERT_EQ(fake_monitor_ptr->get_capture_call_count(), 2);
}

}  // namespace remoting
