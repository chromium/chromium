// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mouse_cursor_monitor_proxy.h"

#include <memory>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "remoting/host/mouse_cursor_monitor_proxy.h"
#include "remoting/protocol/mouse_cursor_monitor.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

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

class ThreadCheckMouseCursorMonitor : public protocol::MouseCursorMonitor {
 public:
  explicit ThreadCheckMouseCursorMonitor(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner), callback_(nullptr) {}

  ThreadCheckMouseCursorMonitor(const ThreadCheckMouseCursorMonitor&) = delete;
  ThreadCheckMouseCursorMonitor& operator=(
      const ThreadCheckMouseCursorMonitor&) = delete;

  ~ThreadCheckMouseCursorMonitor() override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  }

  void Init(Callback* callback) override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    EXPECT_FALSE(callback_);
    EXPECT_TRUE(callback);

    callback_ = callback;
  }

  void SetPreferredCaptureInterval(base::TimeDelta interval) override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    capture_interval_ = interval;
  }

  void Capture() {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    ASSERT_TRUE(callback_);

    auto mouse_cursor = std::make_unique<webrtc::MouseCursor>(
        new webrtc::BasicDesktopFrame(
            webrtc::DesktopSize(kCursorWidth, kCursorHeight),
            webrtc::FOURCC_ARGB),
        webrtc::DesktopVector(kHotspotX, kHotspotY));

    callback_->OnMouseCursor(std::move(mouse_cursor));
  }

  base::TimeDelta capture_interval() const { return capture_interval_; }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  raw_ptr<Callback> callback_;
  base::TimeDelta capture_interval_;
};

class MouseCursorMonitorProxyTest
    : public testing::Test,
      public protocol::MouseCursorMonitor::Callback {
 public:
  MouseCursorMonitorProxyTest() : capture_thread_("test capture thread") {
    capture_thread_.Start();
  }

  ~MouseCursorMonitorProxyTest() override {
    proxy_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // MouseCursorMonitor::Callback implementation.
  void OnMouseCursor(
      std::unique_ptr<webrtc::MouseCursor> mouse_cursor) override;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  base::Thread capture_thread_;
  std::unique_ptr<MouseCursorMonitorProxy> proxy_;

  MockClientStub client_stub_;
};

void MouseCursorMonitorProxyTest::OnMouseCursor(
    std::unique_ptr<webrtc::MouseCursor> mouse_cursor) {
  DCHECK(task_environment_.GetMainThreadTaskRunner()->BelongsToCurrentThread());

  EXPECT_EQ(kCursorWidth, mouse_cursor->image()->size().width());
  EXPECT_EQ(kCursorHeight, mouse_cursor->image()->size().height());
  EXPECT_EQ(kHotspotX, mouse_cursor->hotspot().x());
  EXPECT_EQ(kHotspotY, mouse_cursor->hotspot().y());

  run_loop_.Quit();
}

TEST_F(MouseCursorMonitorProxyTest, CursorShape) {
  // Initialize the proxy.
  auto monitor = std::make_unique<ThreadCheckMouseCursorMonitor>(
      capture_thread_.task_runner());
  ThreadCheckMouseCursorMonitor* unowned_monitor = monitor.get();
  proxy_ = std::make_unique<MouseCursorMonitorProxy>(
      capture_thread_.task_runner(),
      base::ReturnValueOnce<std::unique_ptr<protocol::MouseCursorMonitor>>(
          std::move(monitor)));
  proxy_->Init(this);
  capture_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ThreadCheckMouseCursorMonitor::Capture,
                                base::Unretained(unowned_monitor)));

  // |run_loop_| will be stopped when the first cursor is received.
  run_loop_.Run();
}

TEST_F(MouseCursorMonitorProxyTest, PreferredCaptureInterval) {
  auto monitor = std::make_unique<ThreadCheckMouseCursorMonitor>(
      capture_thread_.task_runner());
  ThreadCheckMouseCursorMonitor* unowned_monitor = monitor.get();
  proxy_ = std::make_unique<MouseCursorMonitorProxy>(
      capture_thread_.task_runner(),
      base::ReturnValueOnce<std::unique_ptr<protocol::MouseCursorMonitor>>(
          std::move(monitor)));
  proxy_->Init(this);
  proxy_->SetPreferredCaptureInterval(base::Milliseconds(16));

  base::RunLoop run_loop;
  capture_thread_.task_runner()->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                                  run_loop.QuitClosure());
  run_loop.Run();
  ASSERT_EQ(unowned_monitor->capture_interval(), base::Milliseconds(16));
}

}  // namespace remoting
