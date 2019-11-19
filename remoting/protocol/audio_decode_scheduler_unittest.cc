// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/audio_decode_scheduler.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/session_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

namespace {

const int kAudioSampleBytes = 4;
const uint8_t kDummyAudioData = 0x8B;

class FakeAudioConsumer : public AudioStub {
 public:
  FakeAudioConsumer() {}
  ~FakeAudioConsumer() override = default;

  base::WeakPtr<FakeAudioConsumer> GetWeakPtr(){
    return weak_factory_.GetWeakPtr();
  }

  // AudioStub implementation.
  void ProcessAudioPacket(std::unique_ptr<AudioPacket> packet,
                          base::OnceClosure done) override {
    if (!done.is_null())
      std::move(done).Run();
  }

 private:
  base::WeakPtrFactory<FakeAudioConsumer> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeAudioConsumer);
};

}  // namespace

class AudioDecodeSchedulerTest : public ::testing::Test {
 public:
  AudioDecodeSchedulerTest() = default;

  void SetUp() override;
  void TearDown() override;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  scoped_refptr<AutoThreadTaskRunner> audio_decode_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> main_task_runner_;
  std::unique_ptr<SessionConfig> session_config_;
};

void AudioDecodeSchedulerTest::SetUp() {
  main_task_runner_ = new AutoThreadTaskRunner(
      task_environment_.GetMainThreadTaskRunner(), run_loop_.QuitClosure());
  audio_decode_task_runner_ = AutoThread::Create("decode", main_task_runner_);
  session_config_ = SessionConfig::ForTestWithAudio();
}

void AudioDecodeSchedulerTest::TearDown() {
  // Release the task runners, so that the test can quit.
  audio_decode_task_runner_ = nullptr;
  main_task_runner_ = nullptr;

  // Run the MessageLoop until everything has torn down.
  run_loop_.Run();
}

// TODO(nicholss): Could share the following in a common class for use
// in other places.
std::unique_ptr<AudioPacket> CreatePacketWithSamplingRate_(
    AudioPacket::SamplingRate rate,
    int samples) {
  std::unique_ptr<AudioPacket> packet(new AudioPacket());
  packet->set_encoding(AudioPacket::ENCODING_RAW);
  packet->set_sampling_rate(rate);
  packet->set_bytes_per_sample(AudioPacket::BYTES_PER_SAMPLE_2);
  packet->set_channels(AudioPacket::CHANNELS_STEREO);

  // The data must be a multiple of 4 bytes (channels x bytes_per_sample).
  std::string data;
  data.resize(samples * kAudioSampleBytes, kDummyAudioData);
  packet->add_data(data);

  return packet;
}

std::unique_ptr<AudioPacket> CreatePacket44100Hz_(int samples) {
  return CreatePacketWithSamplingRate_(AudioPacket::SAMPLING_RATE_44100,
                                       samples);
}

std::unique_ptr<AudioPacket> CreatePacket48000Hz_(int samples) {
  return CreatePacketWithSamplingRate_(AudioPacket::SAMPLING_RATE_48000,
                                       samples);
}

TEST_F(AudioDecodeSchedulerTest, Shutdown) {
  std::unique_ptr<FakeAudioConsumer> audio_consumer(new FakeAudioConsumer());
  std::unique_ptr<AudioDecodeScheduler> audio_scheduler(
      new AudioDecodeScheduler(audio_decode_task_runner_,
                               audio_consumer->GetWeakPtr()));

  audio_scheduler->Initialize(*session_config_);

  audio_scheduler->ProcessAudioPacket(CreatePacket44100Hz_(1000),
                                      base::DoNothing());

  audio_scheduler.reset();
  audio_consumer.reset();
  // TODO(nicholss): This test does not really test anything. Add a way to get
  // a count of the calls to AddAudioPacket.
}

}  // namespace protocol
}  // namespace remoting
