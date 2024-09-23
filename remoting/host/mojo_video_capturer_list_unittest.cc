// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojo_video_capturer_list.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/protocol/fake_desktop_capturer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

using protocol::FakeDesktopCapturer;
using testing::_;
using testing::Return;

namespace {
class MockCapturerEventHandler : public mojom::VideoCapturerEventHandler {
 public:
  MockCapturerEventHandler() = default;
  MockCapturerEventHandler(const MockCapturerEventHandler&) = delete;
  MockCapturerEventHandler& operator=(const MockCapturerEventHandler&) = delete;
  ~MockCapturerEventHandler() override = default;

  MOCK_METHOD(void,
              OnSharedMemoryRegionCreated,
              (int32_t id,
               base::ReadOnlySharedMemoryRegion region,
               uint32_t size),
              (override));
  MOCK_METHOD(void, OnSharedMemoryRegionReleased, (int32_t id), (override));
  MOCK_METHOD(void, OnCaptureResult, (mojom::CaptureResultPtr), (override));
};

}  // namespace

class MojoVideoCapturerListTest : public testing::Test {
 public:
  MojoVideoCapturerListTest() = default;
  MojoVideoCapturerListTest(const MojoVideoCapturerListTest&) = delete;
  MojoVideoCapturerListTest& operator=(const MojoVideoCapturerListTest&) =
      delete;
  ~MojoVideoCapturerListTest() override = default;

 protected:
  // Helper method for calling video_capturers_.CreateVideoCapturer(...) with
  // correct parameters.
  mojom::CreateVideoCapturerResultPtr CreateVideoCapturer(webrtc::ScreenId id);

  base::test::SingleThreadTaskEnvironment task_environment_;

  MockDesktopEnvironment desktop_environment_;
  scoped_refptr<AutoThreadTaskRunner> task_runner_ =
      base::MakeRefCounted<AutoThreadTaskRunner>(
          task_environment_.GetMainThreadTaskRunner(),
          base::DoNothing());
  MockCapturerEventHandler mock_event_handler_;

  MojoVideoCapturerList video_capturers_;
};

mojom::CreateVideoCapturerResultPtr
MojoVideoCapturerListTest::CreateVideoCapturer(webrtc::ScreenId id) {
  return video_capturers_.CreateVideoCapturer(id, &desktop_environment_,
                                              task_runner_);
}

TEST_F(MojoVideoCapturerListTest, ExerciseOneCapturer) {
  auto capturer = std::make_unique<FakeDesktopCapturer>();

  EXPECT_CALL(desktop_environment_, CreateVideoCapturer(1))
      .WillOnce(Return(std::move(capturer)));
  EXPECT_CALL(mock_event_handler_, OnCaptureResult(_));

  auto result = CreateVideoCapturer(1);
  mojo::Remote<mojom::VideoCapturer> mojo_capturer(
      std::move(result->video_capturer));
  mojo::Receiver<mojom::VideoCapturerEventHandler> mojo_event_handler(
      &mock_event_handler_, std::move(result->video_capturer_event_handler));

  mojo_capturer->CaptureFrame();
  task_environment_.RunUntilIdle();
}

TEST_F(MojoVideoCapturerListTest, NonEmptyAfterCapturerCreated) {
  auto capturer = std::make_unique<FakeDesktopCapturer>();

  EXPECT_CALL(desktop_environment_, CreateVideoCapturer(1))
      .WillOnce(Return(std::move(capturer)));

  ASSERT_TRUE(video_capturers_.IsEmpty());
  auto result = CreateVideoCapturer(1);
  EXPECT_FALSE(video_capturers_.IsEmpty());
}

TEST_F(MojoVideoCapturerListTest, DisconnectingEndpointRemovesCapturer) {
  auto capturer = std::make_unique<FakeDesktopCapturer>();

  EXPECT_CALL(desktop_environment_, CreateVideoCapturer(1))
      .WillOnce(Return(std::move(capturer)));

  auto result = CreateVideoCapturer(1);
  mojo::Remote<mojom::VideoCapturer> mojo_capturer(
      std::move(result->video_capturer));
  mojo::Receiver<mojom::VideoCapturerEventHandler> mojo_event_handler(
      &mock_event_handler_, std::move(result->video_capturer_event_handler));

  ASSERT_FALSE(video_capturers_.IsEmpty());
  mojo_capturer.reset();
  mojo_event_handler.reset();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(video_capturers_.IsEmpty());
}

}  // namespace remoting
