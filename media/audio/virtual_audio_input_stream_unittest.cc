// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <list>
#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/rand_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "media/audio/audio_io.h"
#include "media/audio/simple_sources.h"
#include "media/audio/virtual_audio_input_stream.h"
#include "media/audio/virtual_audio_output_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;

namespace media {

namespace {

const AudioParameters kParams(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                              CHANNEL_LAYOUT_STEREO,
                              8000,
                              10);

class MockInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  MockInputCallback()
      : data_pushed_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                     base::WaitableEvent::InitialState::NOT_SIGNALED) {
    ON_CALL(*this, OnData(_, _, _))
        .WillByDefault(
            InvokeWithoutArgs(&data_pushed_, &base::WaitableEvent::Signal));
  }

  ~MockInputCallback() override = default;

  MOCK_METHOD3(OnData,
               void(const AudioBus* source,
                    base::TimeTicks capture_time,
                    double volume));
  MOCK_METHOD0(OnError, void());

  void WaitForDataPushes() {
    for (int i = 0; i < 3; ++i) {
      data_pushed_.Wait();
    }
  }

 private:
  base::WaitableEvent data_pushed_;

  DISALLOW_COPY_AND_ASSIGN(MockInputCallback);
};

class TestAudioSource : public SineWaveAudioSource {
 public:
  TestAudioSource()
      : SineWaveAudioSource(kParams.channel_layout(),
                            200.0,
                            kParams.sample_rate()),
        data_pulled_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                     base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  ~TestAudioSource() override = default;

  int OnMoreData(base::TimeDelta delay,
                 base::TimeTicks delay_timestamp,
                 int prior_frames_skipped,
                 AudioBus* dest) override {
    const int ret = SineWaveAudioSource::OnMoreData(delay, delay_timestamp,
                                                    prior_frames_skipped, dest);
    data_pulled_.Signal();
    return ret;
  }

  void WaitForDataPulls() {
    for (int i = 0; i < 3; ++i) {
      data_pulled_.Wait();
    }
  }

 private:
  base::WaitableEvent data_pulled_;

  DISALLOW_COPY_AND_ASSIGN(TestAudioSource);
};

}  // namespace

class VirtualAudioInputStreamTest : public testing::TestWithParam<bool> {
 public:
  VirtualAudioInputStreamTest()
      : audio_thread_(new base::Thread("AudioThread")),
        worker_thread_(new base::Thread("AudioWorkerThread")),
        stream_(NULL),
        closed_stream_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED) {
    audio_thread_->Start();
    audio_task_runner_ = audio_thread_->task_runner();
  }

  virtual ~VirtualAudioInputStreamTest() {
    SyncWithAudioThread();

    DCHECK(output_streams_.empty());
    DCHECK(stopped_output_streams_.empty());
  }

  void Create() {
    const bool worker_is_separate_thread = GetParam();
    stream_ = new VirtualAudioInputStream(
        kParams, GetWorkerTaskRunner(worker_is_separate_thread),
        base::Bind(&base::DeletePointer<VirtualAudioInputStream>));
    stream_->Open();
  }

  void Start() {
    EXPECT_CALL(input_callback_, OnData(NotNull(), _, _)).Times(AtLeast(1));

    ASSERT_TRUE(stream_);
    stream_->Start(&input_callback_);
  }

  void CreateAndStartOneOutputStream() {
    ASSERT_TRUE(stream_);
    AudioOutputStream* const output_stream = new VirtualAudioOutputStream(
        kParams,
        stream_,
        base::Bind(&base::DeletePointer<VirtualAudioOutputStream>));
    output_streams_.push_back(output_stream);

    output_stream->Open();
    output_stream->Start(&source_);
  }

  void Stop() {
    ASSERT_TRUE(stream_);
    stream_->Stop();
  }

  void Close() {
    ASSERT_TRUE(stream_);
    stream_->Close();
    stream_ = NULL;
    closed_stream_.Signal();
  }

  void WaitForDataToFlow() {
    // Wait until audio thread is idle before calling output_streams_.size().
    SyncWithAudioThread();

    const int count = output_streams_.size();
    for (int i = 0; i < count; ++i) {
      source_.WaitForDataPulls();
    }

    input_callback_.WaitForDataPushes();
  }

  void WaitUntilClosed() {
    closed_stream_.Wait();
  }

  void StopAndCloseOneOutputStream() {
    ASSERT_TRUE(!output_streams_.empty());
    AudioOutputStream* const output_stream = output_streams_.front();
    ASSERT_TRUE(output_stream);
    output_streams_.pop_front();

    output_stream->Stop();
    output_stream->Close();
  }

  void StopFirstOutputStream() {
    ASSERT_TRUE(!output_streams_.empty());
    AudioOutputStream* const output_stream = output_streams_.front();
    ASSERT_TRUE(output_stream);
    output_streams_.pop_front();
    output_stream->Stop();
    stopped_output_streams_.push_back(output_stream);
  }

  void StopSomeOutputStreams() {
    ASSERT_LE(2, static_cast<int>(output_streams_.size()));
    for (int remaning = base::RandInt(1, output_streams_.size() - 1);
         remaning > 0; --remaning) {
      StopFirstOutputStream();
    }
  }

  void RestartAllStoppedOutputStreams() {
    typedef std::list<AudioOutputStream*>::const_iterator ConstIter;
    for (ConstIter it = stopped_output_streams_.begin();
         it != stopped_output_streams_.end(); ++it) {
      (*it)->Start(&source_);
      output_streams_.push_back(*it);
    }
    stopped_output_streams_.clear();
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& audio_task_runner() const {
    return audio_task_runner_;
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& GetWorkerTaskRunner(
      bool worker_is_separate_thread) {
    if (worker_is_separate_thread) {
      if (!worker_thread_->IsRunning()) {
        worker_thread_->Start();
        worker_task_runner_ = worker_thread_->task_runner();
      }
      return worker_task_runner_;
    } else {
      return audio_task_runner_;
    }
  }

 private:
  void SyncWithAudioThread() {
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    audio_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done)));
    done.Wait();
  }

  std::unique_ptr<base::Thread> audio_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;
  std::unique_ptr<base::Thread> worker_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner_;

  VirtualAudioInputStream* stream_;
  MockInputCallback input_callback_;
  base::WaitableEvent closed_stream_;

  std::list<AudioOutputStream*> output_streams_;
  std::list<AudioOutputStream*> stopped_output_streams_;
  TestAudioSource source_;

  DISALLOW_COPY_AND_ASSIGN(VirtualAudioInputStreamTest);
};

#define RUN_ON_AUDIO_THREAD(method)                                   \
  audio_task_runner()->PostTask(                                      \
      FROM_HERE, base::BindOnce(&VirtualAudioInputStreamTest::method, \
                                base::Unretained(this)))

TEST_P(VirtualAudioInputStreamTest, CreateAndClose) {
  RUN_ON_AUDIO_THREAD(Create);
  RUN_ON_AUDIO_THREAD(Close);
  WaitUntilClosed();
}

TEST_P(VirtualAudioInputStreamTest, NoOutputs) {
  RUN_ON_AUDIO_THREAD(Create);
  RUN_ON_AUDIO_THREAD(Start);
  WaitForDataToFlow();
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
  WaitUntilClosed();
}

TEST_P(VirtualAudioInputStreamTest, SingleOutput) {
  RUN_ON_AUDIO_THREAD(Create);
  RUN_ON_AUDIO_THREAD(Start);
  RUN_ON_AUDIO_THREAD(CreateAndStartOneOutputStream);
  WaitForDataToFlow();
  RUN_ON_AUDIO_THREAD(StopAndCloseOneOutputStream);
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
  WaitUntilClosed();
}

TEST_P(VirtualAudioInputStreamTest, SingleOutputPausedAndRestarted) {
  RUN_ON_AUDIO_THREAD(Create);
  RUN_ON_AUDIO_THREAD(Start);
  RUN_ON_AUDIO_THREAD(CreateAndStartOneOutputStream);
  WaitForDataToFlow();
  RUN_ON_AUDIO_THREAD(StopFirstOutputStream);
  RUN_ON_AUDIO_THREAD(RestartAllStoppedOutputStreams);
  WaitForDataToFlow();
  RUN_ON_AUDIO_THREAD(StopAndCloseOneOutputStream);
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(Close);
  WaitUntilClosed();
}

TEST_P(VirtualAudioInputStreamTest, MultipleOutputs) {
  RUN_ON_AUDIO_THREAD(Create);
  RUN_ON_AUDIO_THREAD(Start);
  RUN_ON_AUDIO_THREAD(CreateAndStartOneOutputStream);
  WaitForDataToFlow();
  RUN_ON_AUDIO_THREAD(CreateAndStartOneOutputStream);
  RUN_ON_AUDIO_THREAD(CreateAndStartOneOutputStream);
  WaitForDataToFlow();
  RUN_ON_AUDIO_THREAD(StopFirstOutputStream);
  RUN_ON_AUDIO_THREAD(StopFirstOutputStream);
  WaitForDataToFlow();
  RUN_ON_AUDIO_THREAD(StopFirstOutputStream);
  RUN_ON_AUDIO_THREAD(RestartAllStoppedOutputStreams);
  WaitForDataToFlow();
  RUN_ON_AUDIO_THREAD(StopAndCloseOneOutputStream);
  RUN_ON_AUDIO_THREAD(StopAndCloseOneOutputStream);
  RUN_ON_AUDIO_THREAD(Stop);
  RUN_ON_AUDIO_THREAD(StopAndCloseOneOutputStream);
  RUN_ON_AUDIO_THREAD(Close);
  WaitUntilClosed();
}

// A combination of all of the above tests with many output streams.
TEST_P(VirtualAudioInputStreamTest, ComprehensiveTest) {
  static const int kNumOutputs = 8;
  static const int kHalfNumOutputs = kNumOutputs / 2;
  static const int kPauseIterations = 5;

  RUN_ON_AUDIO_THREAD(Create);
  for (int i = 0; i < kHalfNumOutputs; ++i) {
    RUN_ON_AUDIO_THREAD(CreateAndStartOneOutputStream);
  }
  RUN_ON_AUDIO_THREAD(Start);
  WaitForDataToFlow();
  for (int i = 0; i < kHalfNumOutputs; ++i) {
    RUN_ON_AUDIO_THREAD(CreateAndStartOneOutputStream);
  }
  WaitForDataToFlow();
  for (int i = 0; i < kPauseIterations; ++i) {
    RUN_ON_AUDIO_THREAD(StopSomeOutputStreams);
    WaitForDataToFlow();
    RUN_ON_AUDIO_THREAD(RestartAllStoppedOutputStreams);
    WaitForDataToFlow();
  }
  for (int i = 0; i < kHalfNumOutputs; ++i) {
    RUN_ON_AUDIO_THREAD(StopAndCloseOneOutputStream);
  }
  RUN_ON_AUDIO_THREAD(Stop);
  for (int i = 0; i < kHalfNumOutputs; ++i) {
    RUN_ON_AUDIO_THREAD(StopAndCloseOneOutputStream);
  }
  RUN_ON_AUDIO_THREAD(Close);
  WaitUntilClosed();
}

INSTANTIATE_TEST_CASE_P(SingleVersusMultithreaded,
                        VirtualAudioInputStreamTest,
                        ::testing::Values(false, true));

}  // namespace media
