// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mouse_shape_pump.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/proto/coordinates.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/mouse_cursor_monitor.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

using ::remoting::protocol::MockClientStub;

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::SaveArg;

namespace remoting {

static const int kCursorWidth = 64;
static const int kCursorHeight = 32;
static const int kHotspotX = 11;
static const int kHotspotY = 12;

class TestMouseCursorMonitor : public protocol::MouseCursorMonitor {
 public:
  TestMouseCursorMonitor() : callback_(nullptr) {}

  TestMouseCursorMonitor(const TestMouseCursorMonitor&) = delete;
  TestMouseCursorMonitor& operator=(const TestMouseCursorMonitor&) = delete;

  ~TestMouseCursorMonitor() override = default;

  void Init(Callback* callback) override {
    EXPECT_FALSE(callback_);
    EXPECT_TRUE(callback);

    callback_ = callback;
  }

  void SetPreferredCaptureInterval(base::TimeDelta interval) override {
    capture_interval_ = interval;
  }

  void SendMouseCursor() {
    ASSERT_TRUE(callback_);

    auto mouse_cursor = std::make_unique<webrtc::MouseCursor>(
        new webrtc::BasicDesktopFrame(
            webrtc::DesktopSize(kCursorWidth, kCursorHeight),
            webrtc::FOURCC_ARGB),
        webrtc::DesktopVector(kHotspotX, kHotspotY));

    callback_->OnMouseCursor(std::move(mouse_cursor));
  }

  void SendFractionalCursorPosition(
      const protocol::FractionalCoordinate& position) {
    ASSERT_TRUE(callback_);

    callback_->OnMouseCursorFractionalPosition(position);
  }

  base::TimeDelta get_capture_interval() const { return capture_interval_; }

 private:
  base::TimeDelta capture_interval_;
  raw_ptr<Callback> callback_;
};

class MouseShapePumpTest : public testing::Test {
 public:
  void SetCursorShape(const protocol::CursorShapeInfo& cursor_shape);

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::RunLoop run_loop_;
  std::unique_ptr<MouseShapePump> pump_;

  MockClientStub client_stub_;
};

void MouseShapePumpTest::SetCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  EXPECT_TRUE(cursor_shape.has_width());
  EXPECT_EQ(kCursorWidth, cursor_shape.width());
  EXPECT_TRUE(cursor_shape.has_height());
  EXPECT_EQ(kCursorHeight, cursor_shape.height());
  EXPECT_TRUE(cursor_shape.has_hotspot_x());
  EXPECT_EQ(kHotspotX, cursor_shape.hotspot_x());
  EXPECT_TRUE(cursor_shape.has_hotspot_y());
  EXPECT_EQ(kHotspotY, cursor_shape.hotspot_y());
  EXPECT_TRUE(cursor_shape.has_data());
  EXPECT_EQ(kCursorWidth * kCursorHeight * webrtc::DesktopFrame::kBytesPerPixel,
            static_cast<int>(cursor_shape.data().size()));
}

// This test mocks MouseCursorMonitor and ClientStub to verify that the
// MouseShapePump sends the cursor successfully.
TEST_F(MouseShapePumpTest, OnMouseCursor_SendsCursorShape) {
  auto cursor_monitor = std::make_unique<TestMouseCursorMonitor>();
  auto unowned_cursor_monitor = cursor_monitor.get();
  // Stop the |run_loop_| once it has captured the cursor.
  EXPECT_CALL(client_stub_, SetCursorShape(_))
      .WillOnce(DoAll(Invoke(this, &MouseShapePumpTest::SetCursorShape),
                      InvokeWithoutArgs(&run_loop_, &base::RunLoop::Quit)))
      .RetiresOnSaturation();

  // Start the pump.
  pump_ = std::make_unique<MouseShapePump>(std::move(cursor_monitor),
                                           &client_stub_);
  unowned_cursor_monitor->SendMouseCursor();

  run_loop_.Run();
}

TEST_F(MouseShapePumpTest,
       OnMouseCursorFractionalPosition_SendsPositionToClient) {
  std::unique_ptr<TestMouseCursorMonitor> monitor =
      std::make_unique<TestMouseCursorMonitor>();
  TestMouseCursorMonitor* test_monitor = monitor.get();
  protocol::FractionalCoordinate position;
  position.set_screen_id(1);
  position.set_x(0.5);
  position.set_y(0.5);
  protocol::HostCursorPosition captured_position;
  EXPECT_CALL(client_stub_, SetHostCursorPosition(_))
      .WillOnce(SaveArg<0>(&captured_position));

  // Start the pump.
  pump_ = std::make_unique<MouseShapePump>(std::move(monitor), &client_stub_);
  pump_->SetSendCursorPositionToClient(true);

  // Trigger a fractional position event.
  test_monitor->SendFractionalCursorPosition(position);

  ASSERT_TRUE(captured_position.has_fractional_coordinate());
  ASSERT_EQ(captured_position.fractional_coordinate().screen_id(), 1);
  ASSERT_EQ(captured_position.fractional_coordinate().x(), 0.5);
  ASSERT_EQ(captured_position.fractional_coordinate().y(), 0.5);
}

TEST_F(MouseShapePumpTest,
       OnMouseCursorFractionalPosition_Disabled_DoesNotSendPositionToClient) {
  std::unique_ptr<TestMouseCursorMonitor> monitor =
      std::make_unique<TestMouseCursorMonitor>();
  TestMouseCursorMonitor* test_monitor = monitor.get();
  protocol::FractionalCoordinate position;
  position.set_screen_id(1);
  position.set_x(0.5);
  position.set_y(0.5);
  EXPECT_CALL(client_stub_, SetHostCursorPosition(_)).Times(0);

  // Start the pump.
  pump_ = std::make_unique<MouseShapePump>(std::move(monitor), &client_stub_);
  pump_->SetSendCursorPositionToClient(false);

  // Trigger a fractional position event.
  test_monitor->SendFractionalCursorPosition(position);
}

TEST_F(MouseShapePumpTest,
       SetSendCursorPositionToClient_FromTrueToFalse_SendsEmptyPosition) {
  // Start the pump.
  pump_ = std::make_unique<MouseShapePump>(
      std::make_unique<TestMouseCursorMonitor>(), &client_stub_);

  EXPECT_CALL(client_stub_, SetHostCursorPosition(_)).Times(0);
  pump_->SetSendCursorPositionToClient(true);

  protocol::HostCursorPosition captured_position;
  EXPECT_CALL(client_stub_, SetHostCursorPosition(_))
      .WillOnce(SaveArg<0>(&captured_position));
  pump_->SetSendCursorPositionToClient(false);

  ASSERT_FALSE(captured_position.has_fractional_coordinate());
}

}  // namespace remoting
