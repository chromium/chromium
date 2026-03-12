// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mouse_cursor_monitor_proxy.h"

#include <memory>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "remoting/host/mouse_cursor_monitor_proxy.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
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

class ThreadCheckMouseCursorMonitor : public webrtc::MouseCursorMonitor {
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

  raw_ptr<Callback> callback_;
};

class MouseCursorMonitorProxyTest
    : public testing::Test,
      public webrtc::MouseCursorMonitor::Callback {
 public:
  MouseCursorMonitorProxyTest() {
    capture_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner({});
  }

  ~MouseCursorMonitorProxyTest() override {
    proxy_.reset();
    base::test::TestFuture<void> future;
    task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                          future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  // webrtc::MouseCursorMonitor::Callback implementation.
  void OnMouseCursor(webrtc::MouseCursor* mouse_cursor) override;

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::TestFuture<void> future_;
  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;
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

  future_.SetValue();
}

TEST_F(MouseCursorMonitorProxyTest, CursorShape) {
  // Initialize the proxy.
  proxy_ = std::make_unique<MouseCursorMonitorProxy>(
      capture_task_runner_,
      base::ReturnValueOnce<std::unique_ptr<webrtc::MouseCursorMonitor>>(
          std::make_unique<ThreadCheckMouseCursorMonitor>(
              capture_task_runner_)));
  proxy_->Init(this, webrtc::MouseCursorMonitor::SHAPE_ONLY);
  proxy_->Capture();

  // |future_| will be satisfied when the first cursor is received.
  ASSERT_TRUE(future_.Wait());
}

}  // namespace remoting
