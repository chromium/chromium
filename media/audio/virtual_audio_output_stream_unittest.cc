// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "media/audio/audio_manager.h"
#include "media/audio/simple_sources.h"
#include "media/audio/virtual_audio_input_stream.h"
#include "media/audio/virtual_audio_output_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace media {

namespace {
const AudioParameters kParams(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                              CHANNEL_LAYOUT_MONO,
                              8000,
                              128);
}

class MockVirtualAudioInputStream : public VirtualAudioInputStream {
 public:
  explicit MockVirtualAudioInputStream(
      const scoped_refptr<base::SingleThreadTaskRunner>& worker_task_runner)
      : VirtualAudioInputStream(
            kParams,
            worker_task_runner,
            base::Bind(&base::DeletePointer<VirtualAudioInputStream>)) {}
  ~MockVirtualAudioInputStream() override = default;

  MOCK_METHOD2(AddInputProvider,
               void(AudioConverter::InputCallback* input,
                    const AudioParameters& params));
  MOCK_METHOD2(RemoveInputProvider,
               void(AudioConverter::InputCallback* input,
                    const AudioParameters& params));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVirtualAudioInputStream);
};

class MockAudioDeviceListener : public AudioManager::AudioDeviceListener {
 public:
  MOCK_METHOD0(OnDeviceChange, void());
};

class VirtualAudioOutputStreamTest : public testing::Test {
 public:
  VirtualAudioOutputStreamTest()
      : audio_thread_(new base::Thread("AudioThread")) {
    audio_thread_->Start();
    audio_task_runner_ = audio_thread_->task_runner();
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& audio_task_runner() const {
    return audio_task_runner_;
  }

  void SyncWithAudioThread() {
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    audio_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&done)));
    done.Wait();
  }

 private:
  std::unique_ptr<base::Thread> audio_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(VirtualAudioOutputStreamTest);
};

TEST_F(VirtualAudioOutputStreamTest, StartStopStartStop) {
  static const int kCycles = 3;

  MockVirtualAudioInputStream* const input_stream =
      new MockVirtualAudioInputStream(audio_task_runner());
  audio_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&MockVirtualAudioInputStream::Open),
                     base::Unretained(input_stream)));

  VirtualAudioOutputStream* const output_stream = new VirtualAudioOutputStream(
      kParams,
      input_stream,
      base::Bind(&base::DeletePointer<VirtualAudioOutputStream>));

  EXPECT_CALL(*input_stream, AddInputProvider(output_stream, _)).Times(kCycles);
  EXPECT_CALL(*input_stream, RemoveInputProvider(output_stream, _))
      .Times(kCycles);

  audio_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&VirtualAudioOutputStream::Open),
                     base::Unretained(output_stream)));
  SineWaveAudioSource source(CHANNEL_LAYOUT_STEREO, 200.0, 128);
  for (int i = 0; i < kCycles; ++i) {
    audio_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&VirtualAudioOutputStream::Start,
                                  base::Unretained(output_stream), &source));
    audio_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&VirtualAudioOutputStream::Stop,
                                  base::Unretained(output_stream)));
  }
  audio_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VirtualAudioOutputStream::Close,
                                base::Unretained(output_stream)));

  audio_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockVirtualAudioInputStream::Close,
                                base::Unretained(input_stream)));

  SyncWithAudioThread();
}

}  // namespace media
