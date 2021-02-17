// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mouse_shape_pump.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
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

class TestMouseCursorMonitor : public webrtc::MouseCursorMonitor  {
 public:
  TestMouseCursorMonitor() : callback_(nullptr) {}
  ~TestMouseCursorMonitor() override = default;

  void Init(Callback* callback, Mode mode) override {
    EXPECT_FALSE(callback_);
    EXPECT_TRUE(callback);

    callback_ = callback;
  }

  void Capture() override {
    ASSERT_TRUE(callback_);

    std::unique_ptr<webrtc::MouseCursor> mouse_cursor(new webrtc::MouseCursor(
        new webrtc::BasicDesktopFrame(
            webrtc::DesktopSize(kCursorWidth, kCursorHeight)),
        webrtc::DesktopVector(kHotspotX, kHotspotY)));

    callback_->OnMouseCursor(mouse_cursor.release());
  }

 private:
  Callback* callback_;

  DISALLOW_COPY_AND_ASSIGN(TestMouseCursorMonitor);
};

class MouseShapePumpTest : public testing::Test {
 public:
  void SetCursorShape(const protocol::CursorShapeInfo& cursor_shape);

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
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
      .WillOnce(DoAll(
          Invoke(this, &MouseShapePumpTest::SetCursorShape),
          InvokeWithoutArgs(&run_loop_, &base::RunLoop::Quit)))
      .RetiresOnSaturation();

  // Start the pump.
  pump_.reset(new MouseShapePump(base::WrapUnique(new TestMouseCursorMonitor()),
                                 &client_stub_));

  run_loop_.Run();
}

}  // namespace remoting
