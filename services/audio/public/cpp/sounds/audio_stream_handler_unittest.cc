// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/sounds/audio_stream_handler.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_message_loop.h"
#include "media/audio/audio_io.h"
#include "media/audio/simple_sources.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/channel_layout.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/audio/public/cpp/output_device.h"
#include "services/audio/public/cpp/sounds/test_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {

class AudioStreamHandlerTest : public testing::Test {
 public:
  AudioStreamHandlerTest() = default;
  ~AudioStreamHandlerTest() override = default;

  void SetObserverForTesting(AudioStreamHandler::TestObserver* observer) {
    AudioStreamHandler::SetObserverForTesting(observer);
  }

 private:
  base::TestMessageLoop message_loop_;
};

TEST_F(AudioStreamHandlerTest, Play) {
  base::RunLoop run_loop;
  TestObserver observer(run_loop.QuitClosure());
  base::StringPiece data(kTestAudioData, kTestAudioDataSize);
  std::unique_ptr<AudioStreamHandler> audio_stream_handler;

  SetObserverForTesting(&observer);
  audio_stream_handler =
      std::make_unique<AudioStreamHandler>(base::DoNothing(), data);

  ASSERT_TRUE(audio_stream_handler->IsInitialized());
  EXPECT_EQ(base::Microseconds(20u), audio_stream_handler->duration());

  ASSERT_TRUE(audio_stream_handler->Play());

  run_loop.Run();

  SetObserverForTesting(NULL);

  ASSERT_EQ(1, observer.num_play_requests());
  ASSERT_EQ(1, observer.num_stop_requests());
  ASSERT_EQ(4, observer.cursor());
}

TEST_F(AudioStreamHandlerTest, ConsecutivePlayRequests) {
  base::RunLoop run_loop;
  TestObserver observer(run_loop.QuitClosure());
  base::StringPiece data(kTestAudioData, kTestAudioDataSize);
  std::unique_ptr<AudioStreamHandler> audio_stream_handler;

  SetObserverForTesting(&observer);
  audio_stream_handler =
      std::make_unique<AudioStreamHandler>(base::DoNothing(), data);

  ASSERT_TRUE(audio_stream_handler->IsInitialized());
  EXPECT_EQ(base::Microseconds(20u), audio_stream_handler->duration());

  ASSERT_TRUE(audio_stream_handler->Play());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&AudioStreamHandler::Play),
                     base::Unretained(audio_stream_handler.get())),
      base::Seconds(1));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AudioStreamHandler::Stop,
                     base::Unretained(audio_stream_handler.get())),
      base::Seconds(2));

  run_loop.Run();

  SetObserverForTesting(NULL);

  ASSERT_EQ(1, observer.num_play_requests());
  ASSERT_EQ(1, observer.num_stop_requests());
}

TEST_F(AudioStreamHandlerTest, BadWavDataDoesNotInitialize) {
  // The class members and SetUp() will be ignored for this test. Create a
  // handler on the stack with some bad WAV data.
  AudioStreamHandler handler(base::DoNothing(), "RIFF1234WAVEjunkjunkjunkjunk");
  EXPECT_FALSE(handler.IsInitialized());
  EXPECT_FALSE(handler.Play());
  EXPECT_EQ(base::TimeDelta(), handler.duration());

  // Call Stop() to ensure that there is no crash.
  handler.Stop();
}

}  // namespace audio
