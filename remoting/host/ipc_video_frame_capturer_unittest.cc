// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_video_frame_capturer.h"

#include <cstdint>
#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/desktop_session_proxy.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

namespace {

using testing::InSequence;
using testing::StrictMock;

class MockVideoCapturer : public mojom::VideoCapturer {
 public:
  MockVideoCapturer() = default;
  MockVideoCapturer(const MockVideoCapturer&) = delete;
  MockVideoCapturer& operator=(const MockVideoCapturer&) = delete;
  ~MockVideoCapturer() override = default;

  MOCK_METHOD(void, Start, (), (override));
  MOCK_METHOD(void, SetComposeEnabled, (bool enabled), (override));
  MOCK_METHOD(void, SetMaxFrameRate, (uint32_t max_frame_rate), (override));
  MOCK_METHOD(void, Pause, (bool pause), (override));
  MOCK_METHOD(void,
              BoostCaptureRate,
              (base::TimeDelta capture_interval, base::TimeDelta duration),
              (override));
};

class MockCapturerCallback : public webrtc::DesktopCapturer::Callback {
 public:
  MockCapturerCallback() = default;
  MockCapturerCallback(const MockCapturerCallback&) = delete;
  MockCapturerCallback& operator=(const MockCapturerCallback&) = delete;
  ~MockCapturerCallback() override = default;

  MOCK_METHOD(void, OnFrameCaptureStart, (), (override));
  MOCK_METHOD(void,
              OnCaptureResult,
              (webrtc::DesktopCapturer::Result result,
               std::unique_ptr<webrtc::DesktopFrame> frame),
              (override));
};

// The Desktop process side of a capturer's Mojo endpoints.
class RemoteCapturer {
 public:
  RemoteCapturer() = default;
  RemoteCapturer(const RemoteCapturer&) = delete;
  RemoteCapturer& operator=(const RemoteCapturer&) = delete;
  ~RemoteCapturer() = default;

  mojom::CreateVideoCapturerResultPtr CreateMojoEndpoints() {
    return mojom::CreateVideoCapturerResult::New(
        receiver_.BindNewPipeAndPassRemote(),
        event_handler_.BindNewPipeAndPassReceiver());
  }

  StrictMock<MockVideoCapturer>& mock() { return mock_; }
  mojo::Receiver<mojom::VideoCapturer>& receiver() { return receiver_; }

 private:
  StrictMock<MockVideoCapturer> mock_;
  mojo::Receiver<mojom::VideoCapturer> receiver_{&mock_};
  mojo::Remote<mojom::VideoCapturerEventHandler> event_handler_;
};

}  // namespace

class IpcVideoFrameCapturerTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  StrictMock<MockCapturerCallback> callback_;
  // `DesktopSessionProxy` is only used by SelectSource(), which isn't tested.
  IpcVideoFrameCapturer capturer_{nullptr};
};

TEST_F(IpcVideoFrameCapturerTest, SettingsSetBeforeStartAreSentAfterStart) {
  RemoteCapturer remote;
  capturer_.SetMaxFrameRate(30);
  capturer_.SetComposeEnabled(true);
  capturer_.Pause(true);
  capturer_.OnCreateVideoCapturerResult(remote.CreateMojoEndpoints());

  base::test::TestFuture<void> settings_received;
  {
    InSequence s;
    EXPECT_CALL(remote.mock(), Start());
    EXPECT_CALL(remote.mock(), SetComposeEnabled(true));
    EXPECT_CALL(remote.mock(), SetMaxFrameRate(30));
    EXPECT_CALL(remote.mock(), Pause(true))
        .WillOnce(base::test::RunOnceClosure(settings_received.GetCallback()));
  }
  capturer_.Start(&callback_);
  EXPECT_TRUE(settings_received.Wait());
}

TEST_F(IpcVideoFrameCapturerTest, SettingsAreNotSentBeforeStart) {
  RemoteCapturer remote;
  capturer_.OnCreateVideoCapturerResult(remote.CreateMojoEndpoints());

  // `StrictMock` fails the test if any of these are sent.
  capturer_.SetMaxFrameRate(30);
  capturer_.SetComposeEnabled(true);
  capturer_.Pause(false);
  remote.receiver().FlushForTesting();
}

TEST_F(IpcVideoFrameCapturerTest, SettingsAreSentToNewCapturerAfterReattach) {
  RemoteCapturer old_remote;
  capturer_.OnCreateVideoCapturerResult(old_remote.CreateMojoEndpoints());

  base::test::TestFuture<void> old_settings_received;
  {
    InSequence s;
    EXPECT_CALL(old_remote.mock(), Start());
    EXPECT_CALL(old_remote.mock(), SetMaxFrameRate(30));
    EXPECT_CALL(old_remote.mock(), SetComposeEnabled(true));
    EXPECT_CALL(old_remote.mock(), Pause(false))
        .WillOnce(
            base::test::RunOnceClosure(old_settings_received.GetCallback()));
  }
  capturer_.Start(&callback_);
  capturer_.SetMaxFrameRate(30);
  capturer_.SetComposeEnabled(true);
  capturer_.Pause(false);
  EXPECT_TRUE(old_settings_received.Wait());

  // Simulate a new Desktop process providing a new capturer.
  base::test::TestFuture<void> old_remote_disconnected;
  old_remote.receiver().set_disconnect_handler(
      old_remote_disconnected.GetCallback());
  RemoteCapturer new_remote;
  base::test::TestFuture<void> new_settings_received;
  {
    InSequence s;
    EXPECT_CALL(new_remote.mock(), Start());
    EXPECT_CALL(new_remote.mock(), SetComposeEnabled(true));
    EXPECT_CALL(new_remote.mock(), SetMaxFrameRate(30));
    EXPECT_CALL(new_remote.mock(), Pause(false))
        .WillOnce(
            base::test::RunOnceClosure(new_settings_received.GetCallback()));
  }
  capturer_.OnCreateVideoCapturerResult(new_remote.CreateMojoEndpoints());
  EXPECT_TRUE(new_settings_received.Wait());
  EXPECT_TRUE(old_remote_disconnected.Wait());
}

}  // namespace remoting
