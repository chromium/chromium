// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mouse_cursor_monitor_proxy.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "remoting/host/mouse_cursor_monitor_proxy.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

using ::remoting::protocol::MockClientStub;

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;

namespace remoting {

static const int kCursorWidth = 64;
static const int kCursorHeight = 32;
static const int kHotspotX = 11;
static const int kHotspotY = 12;

class ThreadCheckMouseCursorMonitor : public webrtc::MouseCursorMonitor  {
 public:
  ThreadCheckMouseCursorMonitor(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner), callback_(nullptr) {
  }
  ~ThreadCheckMouseCursorMonitor() override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  }

  void Init(Callback* callback, Mode mode) override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    EXPECT_FALSE(callback_);
    EXPECT_TRUE(callback);

    callback_ = callback;
  }

  void Capture() override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    ASSERT_TRUE(callback_);

    std::unique_ptr<webrtc::MouseCursor> mouse_cursor(new webrtc::MouseCursor(
        new webrtc::BasicDesktopFrame(
            webrtc::DesktopSize(kCursorWidth, kCursorHeight)),
        webrtc::DesktopVector(kHotspotX, kHotspotY)));

    callback_->OnMouseCursor(mouse_cursor.release());
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  Callback* callback_;

  DISALLOW_COPY_AND_ASSIGN(ThreadCheckMouseCursorMonitor);
};

class MouseCursorMonitorProxyTest
    : public testing::Test,
      public webrtc::MouseCursorMonitor::Callback {
 public:
  MouseCursorMonitorProxyTest() : capture_thread_("test capture thread") {
    capture_thread_.Start();
  }

  ~MouseCursorMonitorProxyTest() override {
    proxy_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // webrtc::MouseCursorMonitor::Callback implementation.
  void OnMouseCursor(webrtc::MouseCursor* mouse_cursor) override;
  void OnMouseCursorPosition(webrtc::MouseCursorMonitor::CursorState state,
                             const webrtc::DesktopVector& position) override;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  base::Thread capture_thread_;
  std::unique_ptr<MouseCursorMonitorProxy> proxy_;

  MockClientStub client_stub_;
};

void MouseCursorMonitorProxyTest::OnMouseCursor(
    webrtc::MouseCursor* mouse_cursor) {
  DCHECK(task_environment_.GetMainThreadTaskRunner()->BelongsToCurrentThread());

  EXPECT_EQ(kCursorWidth, mouse_cursor->image()->size().width());
  EXPECT_EQ(kCursorHeight, mouse_cursor->image()->size().height());
  EXPECT_EQ(kHotspotX, mouse_cursor->hotspot().x());
  EXPECT_EQ(kHotspotY, mouse_cursor->hotspot().y());
  delete mouse_cursor;

  run_loop_.Quit();
}

void MouseCursorMonitorProxyTest::OnMouseCursorPosition(
    webrtc::MouseCursorMonitor::CursorState state,
    const webrtc::DesktopVector& position) {
  NOTREACHED();
}

TEST_F(MouseCursorMonitorProxyTest, CursorShape) {
  // Initialize the proxy.
  proxy_.reset(new MouseCursorMonitorProxy(
      capture_thread_.task_runner(),
      webrtc::DesktopCaptureOptions::CreateDefault()));
  proxy_->SetMouseCursorMonitorForTests(
      std::make_unique<ThreadCheckMouseCursorMonitor>(
          capture_thread_.task_runner()));
  proxy_->Init(this, webrtc::MouseCursorMonitor::SHAPE_ONLY);
  proxy_->Capture();

  // |run_loop_| will be stopped when the first cursor is received.
  run_loop_.Run();
}

}  // namespace remoting
