// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio/audio_renderer_sink_cache_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
const char* const kDefaultDeviceId =
    media::AudioDeviceDescription::kDefaultDeviceId;
const char kAnotherDeviceId[] = "another-device-id";
const char kUnhealthyDeviceId[] = "i-am-sick";
const int kRenderFrameId = 124;
constexpr base::TimeDelta kDeleteTimeout =
    base::TimeDelta::FromMilliseconds(500);
}  // namespace

class AudioRendererSinkCacheTest : public testing::Test {
 public:
  AudioRendererSinkCacheTest()
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME,
                  base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
        cache_(std::make_unique<AudioRendererSinkCacheImpl>(
            task_env_.GetMainThreadTaskRunner(),
            base::BindRepeating(&AudioRendererSinkCacheTest::CreateSink,
                                base::Unretained(this)),
            kDeleteTimeout)) {}
  ~AudioRendererSinkCacheTest() override {
    task_env_.FastForwardUntilNoTasksRemain();
  }

  void GetSink(int render_frame_id,
               const std::string& device_id,
               media::AudioRendererSink** sink) {
    *sink = cache_->GetSink(render_frame_id, device_id).get();
  }

 protected:
  int sink_count() {
    DCHECK(task_env_.GetMainThreadTaskRunner()->BelongsToCurrentThread());
    return cache_->GetCacheSizeForTesting();
  }

  scoped_refptr<media::AudioRendererSink> CreateSink(
      int render_frame_id,
      const media::AudioSinkParameters& params) {
    return new testing::NiceMock<media::MockAudioRendererSink>(
        params.device_id, (params.device_id == kUnhealthyDeviceId)
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

  void DropSinksForFrame(int frame_id) { cache_->DropSinksForFrame(frame_id); }

  base::test::TaskEnvironment task_env_;
  std::unique_ptr<AudioRendererSinkCacheImpl> cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioRendererSinkCacheTest);
};

// Verify that normal get/release sink sequence works.
TEST_F(AudioRendererSinkCacheTest, GetReleaseSink) {
  // Verify that a new sink is successfully created.
  EXPECT_EQ(0, sink_count());
  scoped_refptr<media::AudioRendererSink> sink =
      cache_->GetSink(kRenderFrameId, kDefaultDeviceId).get();
  ExpectNotToStop(sink.get());  // Cache should not stop sinks marked as used.
  EXPECT_EQ(kDefaultDeviceId, sink->GetOutputDeviceInfo().device_id());
  EXPECT_EQ(1, sink_count());

  // Verify that another sink with the same key is successfully created
  scoped_refptr<media::AudioRendererSink> another_sink =
      cache_->GetSink(kRenderFrameId, kDefaultDeviceId).get();
  ExpectNotToStop(another_sink.get());
  EXPECT_EQ(kDefaultDeviceId, another_sink->GetOutputDeviceInfo().device_id());
  EXPECT_EQ(2, sink_count());
  EXPECT_NE(sink, another_sink);

  // Verify that another sink with a different kay is successfully created.
  scoped_refptr<media::AudioRendererSink> yet_another_sink =
      cache_->GetSink(kRenderFrameId, kAnotherDeviceId).get();
  ExpectNotToStop(yet_another_sink.get());
  EXPECT_EQ(kAnotherDeviceId,
            yet_another_sink->GetOutputDeviceInfo().device_id());
  EXPECT_EQ(3, sink_count());
  EXPECT_NE(sink, yet_another_sink);
  EXPECT_NE(another_sink, yet_another_sink);

  // Verify that the first sink is successfully deleted.
  cache_->ReleaseSink(sink.get());
  EXPECT_EQ(2, sink_count());
  sink = nullptr;

  // Make sure we deleted the right sink, and the memory for the rest is not
  // corrupted.
  EXPECT_EQ(kDefaultDeviceId, another_sink->GetOutputDeviceInfo().device_id());
  EXPECT_EQ(kAnotherDeviceId,
            yet_another_sink->GetOutputDeviceInfo().device_id());

  // Verify that the second sink is successfully deleted.
  cache_->ReleaseSink(another_sink.get());
  EXPECT_EQ(1, sink_count());
  EXPECT_EQ(kAnotherDeviceId,
            yet_another_sink->GetOutputDeviceInfo().device_id());

  cache_->ReleaseSink(yet_another_sink.get());
  EXPECT_EQ(0, sink_count());
}

// Verify that the sink created with GetSinkInfo() is reused when possible.
TEST_F(AudioRendererSinkCacheTest, GetDeviceInfo) {
  EXPECT_EQ(0, sink_count());
  media::OutputDeviceInfo device_info = cache_->GetSinkInfo(
      kRenderFrameId, base::UnguessableToken(), kDefaultDeviceId);
  EXPECT_EQ(1, sink_count());

  // The info on the same device is requested, so no new sink is created.
  media::OutputDeviceInfo one_more_device_info = cache_->GetSinkInfo(
      kRenderFrameId, base::UnguessableToken(), kDefaultDeviceId);
  EXPECT_EQ(1, sink_count());
  EXPECT_EQ(device_info.device_id(), one_more_device_info.device_id());

  // Aquire the sink that was created on GetSinkInfo().
  scoped_refptr<media::AudioRendererSink> sink =
      cache_->GetSink(kRenderFrameId, kDefaultDeviceId).get();
  EXPECT_EQ(1, sink_count());
  EXPECT_EQ(device_info.device_id(), sink->GetOutputDeviceInfo().device_id());

  // Now the sink is in used, but we can still get the device info out of it, no
  // new sink is created.
  one_more_device_info = cache_->GetSinkInfo(
      kRenderFrameId, base::UnguessableToken(), kDefaultDeviceId);
  EXPECT_EQ(1, sink_count());
  EXPECT_EQ(device_info.device_id(), one_more_device_info.device_id());

  // Request sink for the same device. The first sink is in use, so a new one
  // should be created.
  scoped_refptr<media::AudioRendererSink> another_sink =
      cache_->GetSink(kRenderFrameId, kDefaultDeviceId).get();
  EXPECT_EQ(2, sink_count());
  EXPECT_EQ(device_info.device_id(),
            another_sink->GetOutputDeviceInfo().device_id());
}

// Verify that the sink created with GetSinkInfo() is deleted if unused.
TEST_F(AudioRendererSinkCacheTest, GarbageCollection) {
  EXPECT_EQ(0, sink_count());

  media::OutputDeviceInfo device_info = cache_->GetSinkInfo(
      kRenderFrameId, base::UnguessableToken(), kDefaultDeviceId);
  EXPECT_EQ(1, sink_count());

  media::OutputDeviceInfo another_device_info = cache_->GetSinkInfo(
      kRenderFrameId, base::UnguessableToken(), kAnotherDeviceId);
  EXPECT_EQ(2, sink_count());

  // Wait for garbage collection. Doesn't actually sleep, just advances the mock
  // clock.
  task_env_.FastForwardBy(kDeleteTimeout);

  // All the sinks should be garbage-collected by now.
  EXPECT_EQ(0, sink_count());
}

// Verify that the sink created with GetSinkInfo() is not deleted if used within
// the timeout.
TEST_F(AudioRendererSinkCacheTest, NoGarbageCollectionForUsedSink) {
  EXPECT_EQ(0, sink_count());

  media::OutputDeviceInfo device_info = cache_->GetSinkInfo(
      kRenderFrameId, base::UnguessableToken(), kDefaultDeviceId);
  EXPECT_EQ(1, sink_count());

  // Wait less than garbage collection timeout.
  base::TimeDelta wait_a_bit =
      kDeleteTimeout - base::TimeDelta::FromMilliseconds(1);
  task_env_.FastForwardBy(wait_a_bit);

  // Sink is not deleted yet.
  EXPECT_EQ(1, sink_count());

  // Request it:
  scoped_refptr<media::AudioRendererSink> sink =
      cache_->GetSink(kRenderFrameId, kDefaultDeviceId).get();
  EXPECT_EQ(kDefaultDeviceId, sink->GetOutputDeviceInfo().device_id());
  EXPECT_EQ(1, sink_count());

  // Wait more to hit garbage collection timeout.
  task_env_.FastForwardBy(kDeleteTimeout);

  // The sink is still in place.
  EXPECT_EQ(1, sink_count());
}

// Verify that the sink created with GetSinkInfo() is not cached if it is
// unhealthy.
TEST_F(AudioRendererSinkCacheTest, UnhealthySinkIsNotCached) {
  EXPECT_EQ(0, sink_count());
  media::OutputDeviceInfo device_info = cache_->GetSinkInfo(
      kRenderFrameId, base::UnguessableToken(), kUnhealthyDeviceId);
  EXPECT_EQ(0, sink_count());
  scoped_refptr<media::AudioRendererSink> sink =
      cache_->GetSink(kRenderFrameId, kUnhealthyDeviceId).get();
  EXPECT_EQ(0, sink_count());
}

// Verify that a sink created with GetSinkInfo() is stopped even if it's
// unhealthy.
TEST_F(AudioRendererSinkCacheTest, UnhealthySinkIsStopped) {
  scoped_refptr<media::MockAudioRendererSink> sink =
      new media::MockAudioRendererSink(
          kUnhealthyDeviceId, media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);

  cache_.reset();  // Destruct first so there's only one cache at a time.
  cache_ = std::make_unique<AudioRendererSinkCacheImpl>(
      task_env_.GetMainThreadTaskRunner(),
      base::BindRepeating(
          [](scoped_refptr<media::AudioRendererSink> sink, int render_frame_id,
             const media::AudioSinkParameters& params) {
            EXPECT_EQ(kRenderFrameId, render_frame_id);
            EXPECT_TRUE(params.session_id.is_empty());
            EXPECT_EQ(kUnhealthyDeviceId, params.device_id);
            return sink;
          },
          sink),
      kDeleteTimeout);

  EXPECT_CALL(*sink, Stop());

  media::OutputDeviceInfo device_info = cache_->GetSinkInfo(
      kRenderFrameId, base::UnguessableToken(), kUnhealthyDeviceId);
}

// Verify that a sink created with GetSinkInfo() is stopped even if it's
// unhealthy.
TEST_F(AudioRendererSinkCacheTest, UnhealthySinkUsingSessionIdIsStopped) {
  scoped_refptr<media::MockAudioRendererSink> sink =
      new media::MockAudioRendererSink(
          kUnhealthyDeviceId, media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);

  cache_.reset();  // Destruct first so there's only one cache at a time.
  cache_ = std::make_unique<AudioRendererSinkCacheImpl>(
      task_env_.GetMainThreadTaskRunner(),
      base::BindRepeating(
          [](scoped_refptr<media::AudioRendererSink> sink, int render_frame_id,
             const media::AudioSinkParameters& params) {
            EXPECT_EQ(kRenderFrameId, render_frame_id);
            EXPECT_TRUE(!params.session_id.is_empty());
            EXPECT_TRUE(params.device_id.empty());
            return sink;
          },
          sink),
      kDeleteTimeout);

  EXPECT_CALL(*sink, Stop());

  media::OutputDeviceInfo device_info = cache_->GetSinkInfo(
      kRenderFrameId, base::UnguessableToken::Create(), std::string());
}

// Verify that cache works fine if a sink scheduled for deletion is acquired and
// released before deletion timeout elapses.
TEST_F(AudioRendererSinkCacheTest, ReleaseSinkBeforeScheduledDeletion) {
  EXPECT_EQ(0, sink_count());

  base::Thread thread("timeout_thread");
  thread.Start();

  media::OutputDeviceInfo device_info = cache_->GetSinkInfo(
      kRenderFrameId, base::UnguessableToken(), kDefaultDeviceId);
  EXPECT_EQ(1, sink_count());  // This sink is scheduled for deletion now.

  // Request it:
  scoped_refptr<media::AudioRendererSink> sink =
      cache_->GetSink(kRenderFrameId, kDefaultDeviceId).get();
  ExpectNotToStop(sink.get());
  EXPECT_EQ(1, sink_count());

  // Release it:
  cache_->ReleaseSink(sink.get());
  EXPECT_EQ(0, sink_count());

  media::OutputDeviceInfo another_device_info = cache_->GetSinkInfo(
      kRenderFrameId, base::UnguessableToken(), kAnotherDeviceId);
  EXPECT_EQ(1, sink_count());  // This sink is scheduled for deletion now.

  task_env_.FastForwardBy(kDeleteTimeout);

  // Nothing crashed and the second sink deleted on schedule.
  EXPECT_EQ(0, sink_count());
}

// Check that a sink created on one thread in response to GetSinkInfo can be
// used on another thread.
TEST_F(AudioRendererSinkCacheTest, MultithreadedAccess) {
  EXPECT_EQ(0, sink_count());

  base::Thread thread1("thread1");
  thread1.Start();

  base::Thread thread2("thread2");
  thread2.Start();

  // Request device information on the first thread.
  PostAndWaitUntilDone(
      thread1, base::BindOnce(
                   base::IgnoreResult(&AudioRendererSinkCacheImpl::GetSinkInfo),
                   base::Unretained(cache_.get()), kRenderFrameId,
                   base::UnguessableToken(), kDefaultDeviceId));

  EXPECT_EQ(1, sink_count());

  // Request the sink on the second thread.
  media::AudioRendererSink* sink;

  PostAndWaitUntilDone(thread2,
                       base::BindOnce(&AudioRendererSinkCacheTest::GetSink,
                                      base::Unretained(this), kRenderFrameId,
                                      kDefaultDeviceId, &sink));

  EXPECT_EQ(kDefaultDeviceId, sink->GetOutputDeviceInfo().device_id());
  EXPECT_EQ(1, sink_count());

  // Request device information on the first thread again.
  PostAndWaitUntilDone(
      thread1, base::BindOnce(
                   base::IgnoreResult(&AudioRendererSinkCacheImpl::GetSinkInfo),
                   base::Unretained(cache_.get()), kRenderFrameId,
                   base::UnguessableToken(), kDefaultDeviceId));
  EXPECT_EQ(1, sink_count());

  // Release the sink on the second thread.
  PostAndWaitUntilDone(
      thread2,
      base::BindOnce(&AudioRendererSinkCache::ReleaseSink,
                     base::Unretained(cache_.get()), base::RetainedRef(sink)));

  EXPECT_EQ(0, sink_count());
}

TEST_F(AudioRendererSinkCacheTest, StopsAndDropsSinks) {
  EXPECT_EQ(0, sink_count());
  scoped_refptr<media::AudioRendererSink> sink1 =
      cache_->GetSink(kRenderFrameId, "device1").get();
  scoped_refptr<media::AudioRendererSink> sink2 =
      cache_->GetSink(kRenderFrameId, "device2").get();
  EXPECT_EQ(2, sink_count());

  EXPECT_CALL(*static_cast<media::MockAudioRendererSink*>(sink1.get()), Stop());
  EXPECT_CALL(*static_cast<media::MockAudioRendererSink*>(sink2.get()), Stop());
  DropSinksForFrame(kRenderFrameId);
  EXPECT_EQ(0, sink_count());
}

TEST_F(AudioRendererSinkCacheTest, StopsAndDropsCorrectSinks) {
  EXPECT_EQ(0, sink_count());
  scoped_refptr<media::AudioRendererSink> sink1 =
      cache_->GetSink(kRenderFrameId, "device1").get();
  scoped_refptr<media::AudioRendererSink> another_sink =
      cache_->GetSink(kRenderFrameId + 1, "device1").get();
  scoped_refptr<media::AudioRendererSink> sink2 =
      cache_->GetSink(kRenderFrameId, "device2").get();
  EXPECT_EQ(3, sink_count());

  EXPECT_CALL(*static_cast<media::MockAudioRendererSink*>(sink1.get()), Stop());
  EXPECT_CALL(*static_cast<media::MockAudioRendererSink*>(sink2.get()), Stop());
  DropSinksForFrame(kRenderFrameId);
  EXPECT_EQ(1, sink_count());
  EXPECT_CALL(*static_cast<media::MockAudioRendererSink*>(another_sink.get()),
              Stop());
}

}  // namespace content
