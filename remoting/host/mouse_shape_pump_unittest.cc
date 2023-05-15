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
#include "remoting/host/host_mock_objects.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

using ::remoting::protocol::MockClientStub;

using ::testing::_;
using ::testing::InvokeWithoutArgs;

namespace remoting {

static const int kCursorWidth = 64;
static const int kCursorHeight = 32;
static const int kHotspotX = 11;
static const int kHotspotY = 12;

class TestMouseCursorMonitor : public webrtc::MouseCursorMonitor {
 public:
  TestMouseCursorMonitor() : callback_(nullptr) {}

  TestMouseCursorMonitor(const TestMouseCursorMonitor&) = delete;
  TestMouseCursorMonitor& operator=(const TestMouseCursorMonitor&) = delete;

  ~TestMouseCursorMonitor() override = default;

  void Init(Callback* callback, Mode mode) override {
    EXPECT_FALSE(callback_);
    EXPECT_TRUE(callback);

    callback_ = callback;
  }

  void Capture() override {
    ASSERT_TRUE(callback_);
    capture_call_count_++;

    std::unique_ptr<webrtc::MouseCursor> mouse_cursor(new webrtc::MouseCursor(
        new webrtc::BasicDesktopFrame(
            webrtc::DesktopSize(kCursorWidth, kCursorHeight)),
        webrtc::DesktopVector(kHotspotX, kHotspotY)));

    callback_->OnMouseCursor(mouse_cursor.release());
  }

  int get_capture_call_count() const { return capture_call_count_; }

 private:
  int capture_call_count_ = 0;
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
TEST_F(MouseShapePumpTest, FirstCursor) {
  // Stop the |run_loop_| once it has captured the cursor.
  EXPECT_CALL(client_stub_, SetCursorShape(_))
      .WillOnce(DoAll(Invoke(this, &MouseShapePumpTest::SetCursorShape),
                      InvokeWithoutArgs(&run_loop_, &base::RunLoop::Quit)))
      .RetiresOnSaturation();

  // Start the pump.
  pump_ = std::make_unique<MouseShapePump>(
      base::WrapUnique(new TestMouseCursorMonitor()), &client_stub_);

  run_loop_.Run();
}

TEST_F(MouseShapePumpTest, DefaultCaptureInterval) {
  EXPECT_CALL(client_stub_, SetCursorShape(_)).Times(2);

  std::unique_ptr<TestMouseCursorMonitor> monitor =
      std::make_unique<TestMouseCursorMonitor>();
  TestMouseCursorMonitor* test_monitor = monitor.get();

  // Start the pump.
  pump_ = std::make_unique<MouseShapePump>(std::move(monitor), &client_stub_);
  // Default capture interval is 100ms.
  base::TimeDelta default_capure_interval = base::Milliseconds(100);

  task_environment_.FastForwardBy(default_capure_interval -
                                  base::Milliseconds(1));
  ASSERT_EQ(test_monitor->get_capture_call_count(), 0);

  task_environment_.FastForwardBy(base::Milliseconds(2));
  ASSERT_EQ(test_monitor->get_capture_call_count(), 1);

  task_environment_.FastForwardBy(default_capure_interval);
  ASSERT_EQ(test_monitor->get_capture_call_count(), 2);
}

TEST_F(MouseShapePumpTest, UpdatedCaptureInterval) {
  EXPECT_CALL(client_stub_, SetCursorShape(_)).Times(2);

  std::unique_ptr<TestMouseCursorMonitor> monitor =
      std::make_unique<TestMouseCursorMonitor>();
  TestMouseCursorMonitor* test_monitor = monitor.get();

  // Start the pump.
  pump_ = std::make_unique<MouseShapePump>(std::move(monitor), &client_stub_);
  base::TimeDelta test_capture_interval = base::Milliseconds(15);
  pump_->SetCursorCaptureInterval(test_capture_interval);

  task_environment_.FastForwardBy(test_capture_interval -
                                  base::Milliseconds(1));
  ASSERT_EQ(test_monitor->get_capture_call_count(), 0);

  task_environment_.FastForwardBy(base::Milliseconds(2));
  ASSERT_EQ(test_monitor->get_capture_call_count(), 1);

  task_environment_.FastForwardBy(test_capture_interval);
  ASSERT_EQ(test_monitor->get_capture_call_count(), 2);
}

}  // namespace remoting
