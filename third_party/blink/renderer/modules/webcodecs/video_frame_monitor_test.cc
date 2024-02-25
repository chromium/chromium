// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_frame_monitor.h"

#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class VideoFrameMonitorTest : public testing::Test {
 protected:
  static void SequenceOfOperations(const std::string& source_id) {
    VideoFrameMonitor& monitor = VideoFrameMonitor::Instance();

    monitor.OnOpenFrame(source_id, media::VideoFrame::ID::FromUnsafeValue(1));
    EXPECT_EQ(monitor.NumFrames(source_id), 1u);

    monitor.OnOpenFrame(source_id, media::VideoFrame::ID::FromUnsafeValue(10));
    EXPECT_EQ(monitor.NumFrames(source_id), 2u);

    monitor.OnOpenFrame(source_id, media::VideoFrame::ID::FromUnsafeValue(20));
    EXPECT_EQ(monitor.NumFrames(source_id), 3u);

    monitor.OnCloseFrame(source_id, media::VideoFrame::ID::FromUnsafeValue(1));
    EXPECT_EQ(monitor.NumFrames(source_id), 2u);

    monitor.OnCloseFrame(source_id, media::VideoFrame::ID::FromUnsafeValue(10));
    EXPECT_EQ(monitor.NumFrames(source_id), 1u);

    monitor.OnOpenFrame(source_id, media::VideoFrame::ID::FromUnsafeValue(30));
    EXPECT_EQ(monitor.NumFrames(source_id), 2u);

    monitor.OnOpenFrame(source_id, media::VideoFrame::ID::FromUnsafeValue(20));
    EXPECT_EQ(monitor.NumFrames(source_id), 2u);
    EXPECT_EQ(
        monitor.NumRefs(source_id, media::VideoFrame::ID::FromUnsafeValue(20)),
        2);

    // JS closes one of its VideoFrames with ID 20
    monitor.OnCloseFrame(source_id, media::VideoFrame::ID::FromUnsafeValue(20));
    EXPECT_EQ(monitor.NumFrames(source_id), 2u);
    EXPECT_EQ(
        monitor.NumRefs(source_id, media::VideoFrame::ID::FromUnsafeValue(20)),
        1);

    {
      base::AutoLock locker(monitor.GetLock());
      monitor.OnOpenFrameLocked(source_id,
                                media::VideoFrame::ID::FromUnsafeValue(30));
      EXPECT_EQ(monitor.NumFramesLocked(source_id), 2u);
      EXPECT_EQ(monitor.NumRefsLocked(
                    source_id, media::VideoFrame::ID::FromUnsafeValue(30)),
                2);

      monitor.OnCloseFrameLocked(source_id,
                                 media::VideoFrame::ID::FromUnsafeValue(20));
      EXPECT_EQ(monitor.NumFramesLocked(source_id), 1u);
      EXPECT_EQ(monitor.NumRefsLocked(
                    source_id, media::VideoFrame::ID::FromUnsafeValue(20)),
                0);

      monitor.OnCloseFrameLocked(source_id,
                                 media::VideoFrame::ID::FromUnsafeValue(30));
      EXPECT_EQ(monitor.NumFramesLocked(source_id), 1u);
      EXPECT_EQ(monitor.NumRefsLocked(
                    source_id, media::VideoFrame::ID::FromUnsafeValue(30)),
                1);
    }

    monitor.OnCloseFrame(source_id, media::VideoFrame::ID::FromUnsafeValue(30));
    EXPECT_EQ(
        monitor.NumRefs(source_id, media::VideoFrame::ID::FromUnsafeValue(30)),
        0);
    EXPECT_EQ(monitor.NumFrames(source_id), 0u);
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() {
    return platform_->GetIOTaskRunner();
  }

 private:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
};

TEST_F(VideoFrameMonitorTest, TwoDevicesOnSeparateThreads) {
  base::RunLoop loop;
  PostCrossThreadTask(*GetIOTaskRunner(), FROM_HERE,
                      CrossThreadBindOnce(
                          [](base::RunLoop* loop) {
                            SequenceOfOperations("device2");
                            loop->Quit();
                          },
                          CrossThreadUnretained(&loop)));
  SequenceOfOperations("device1");
  loop.Run();
  EXPECT_TRUE(VideoFrameMonitor::Instance().IsEmpty());
}

}  // namespace blink
