// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media/audio/audio_renderer_sink_cache.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {
const char* const kDefaultDeviceId =
    media::AudioDeviceDescription::kDefaultDeviceId;
const char kAnotherDeviceId[] = "another-device-id";
const char kUnhealthyDeviceId[] = "i-am-sick";
const LocalFrameToken kFrameToken;
constexpr base::TimeDelta kDeleteTimeout = base::Milliseconds(500);
}  // namespace

class AudioRendererSinkCacheTest : public testing::Test {
 public:
  AudioRendererSinkCacheTest()
      : task_runner_(base::MakeRefCounted<base::TestMockTimeTaskRunner>(
            base::Time::Now(),
            base::TimeTicks::Now())),
        task_runner_context_(
            std::make_unique<base::TestMockTimeTaskRunner::ScopedContext>(
                task_runner_)),
        cache_(std::make_unique<AudioRendererSinkCache>(
            task_runner_,
            base::BindRepeating(&AudioRendererSinkCacheTest::CreateSink,
                                base::Unretained(this)),
            kDeleteTimeout)) {}

  AudioRendererSinkCacheTest(const AudioRendererSinkCacheTest&) = delete;
  AudioRendererSinkCacheTest& operator=(const AudioRendererSinkCacheTest&) =
      delete;

  ~AudioRendererSinkCacheTest() override {
    task_runner_->FastForwardUntilNoTasksRemain();
  }

 protected:
  size_t sink_count() {
    DCHECK(task_runner_->BelongsToCurrentThread());
    return cache_->GetCacheSizeForTesting();
  }

  scoped_refptr<media::AudioRendererSink> CreateSink(
      const LocalFrameToken& frame_token,
      const std::string& device_id) {
    return new testing::NiceMock<media::MockAudioRendererSink>(
        device_id, (device_id == kUnhealthyDeviceId)
                       ? media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL
                       : media::OUTPUT_DEVICE_STATUS_OK);
  }

  void ExpectNotToStop(media::AudioRendererSink* sink) {
    // The sink must be stoped before deletion.
    EXPECT_CALL(*static_cast<media::MockAudioRendererSink*>(sink), Stop())
        .Times(0);
  }

  // Posts the task to the specified thread and runs current message loop until
  // the task is completed.
  void PostAndWaitUntilDone(const base::Thread& thread,
                            base::OnceClosure task) {
    base::WaitableEvent e{base::WaitableEvent::ResetPolicy::MANUAL,
                          base::WaitableEvent::InitialState::NOT_SIGNALED};

    thread.task_runner()->PostTask(FROM_HERE, std::move(task));
    thread.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&e)));

    e.Wait();
  }

  void DropSinksForFrame(const LocalFrameToken& frame_token) {
    cache_->DropSinksForFrame(frame_token);
  }

  test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  // Ensure all things run on |task_runner_| instead of the default task
  // runner initialized by blink_unittests.
  std::unique_ptr<base::TestMockTimeTaskRunner::ScopedContext>
      task_runner_context_;

  std::unique_ptr<AudioRendererSinkCache> cache_;
};

// Verify that the sink created with GetSinkInfo() is reused when possible.
TEST_F(AudioRendererSinkCacheTest, GetDeviceInfo) {
  EXPECT_EQ(0u, sink_count());
  media::OutputDeviceInfo device_info =
      cache_->GetSinkInfo(kFrameToken, kDefaultDeviceId);
  EXPECT_EQ(1u, sink_count());

  // The info on the same device is requested, so no new sink is created.
  media::OutputDeviceInfo one_more_device_info =
      cache_->GetSinkInfo(kFrameToken, kDefaultDeviceId);
  EXPECT_EQ(1u, sink_count());
  EXPECT_EQ(device_info.device_id(), one_more_device_info.device_id());
}

// Verify that the sink created with GetSinkInfo() is deleted.
TEST_F(AudioRendererSinkCacheTest, GarbageCollection) {
  EXPECT_EQ(0u, sink_count());

  media::OutputDeviceInfo device_info =
      cache_->GetSinkInfo(kFrameToken, kDefaultDeviceId);
  EXPECT_EQ(1u, sink_count());

  media::OutputDeviceInfo another_device_info =
      cache_->GetSinkInfo(kFrameToken, kAnotherDeviceId);
  EXPECT_EQ(2u, sink_count());

  // Wait for garbage collection. Doesn't actually sleep, just advances the mock
  // clock.
  task_runner_->FastForwardBy(kDeleteTimeout);

  // All the sinks should be garbage-collected by now.
  EXPECT_EQ(0u, sink_count());
}

// Verify that the sink created with GetSinkInfo() is not cached if it is
// unhealthy.
TEST_F(AudioRendererSinkCacheTest, UnhealthySinkIsNotCached) {
  EXPECT_EQ(0u, sink_count());
  media::OutputDeviceInfo device_info =
      cache_->GetSinkInfo(kFrameToken, kUnhealthyDeviceId);
  EXPECT_EQ(0u, sink_count());
}

// Verify that a sink created with GetSinkInfo() is stopped even if it's
// unhealthy.
TEST_F(AudioRendererSinkCacheTest, UnhealthySinkIsStopped) {
  scoped_refptr<media::MockAudioRendererSink> sink =
      new media::MockAudioRendererSink(
          kUnhealthyDeviceId, media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);

  cache_.reset();  // Destruct first so there's only one cache at a time.
  cache_ = std::make_unique<AudioRendererSinkCache>(
      task_runner_,
      base::BindRepeating(
          [](scoped_refptr<media::AudioRendererSink> sink,
             const LocalFrameToken& frame_token, const std::string& device_id) {
            EXPECT_EQ(kFrameToken, frame_token);
            EXPECT_EQ(kUnhealthyDeviceId, device_id);
            return sink;
          },
          sink),
      kDeleteTimeout);

  EXPECT_CALL(*sink, Stop());

  media::OutputDeviceInfo device_info =
      cache_->GetSinkInfo(kFrameToken, kUnhealthyDeviceId);
}

// Check that a sink created on one thread in response to GetSinkInfo can be
// used on another thread.
TEST_F(AudioRendererSinkCacheTest, MultithreadedAccess) {
  EXPECT_EQ(0u, sink_count());

  base::Thread thread1("thread1");
  thread1.Start();

  base::Thread thread2("thread2");
  thread2.Start();

  // Request device information on the first thread.
  PostAndWaitUntilDone(
      thread1,
      base::BindOnce(base::IgnoreResult(&AudioRendererSinkCache::GetSinkInfo),
                     base::Unretained(cache_.get()), kFrameToken,
                     kDefaultDeviceId));

  EXPECT_EQ(1u, sink_count());

  // Request the device information again on the second thread.
  PostAndWaitUntilDone(
      thread2,
      base::BindOnce(base::IgnoreResult(&AudioRendererSinkCache::GetSinkInfo),
                     base::Unretained(cache_.get()), kFrameToken,
                     kDefaultDeviceId));
}

}  // namespace blink
