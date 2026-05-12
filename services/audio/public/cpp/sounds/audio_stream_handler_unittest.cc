// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/sounds/audio_stream_handler.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "media/audio/audio_io.h"
#include "media/audio/simple_sources.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_codecs.h"
#include "media/base/channel_layout.h"
#include "media/base/test_data_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/audio/public/cpp/output_device.h"
#include "services/audio/public/cpp/sounds/test_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace audio {

namespace {

const char kTestBadWavAudioData[] = "RIFF1234WAVEjunkjunkjunkjunk";
const size_t kTestBadWavAudioDataSize = std::size(kTestBadWavAudioData) - 1;

struct TestParams {
  const media::AudioCodec codec;
  const bool is_bad;
  const bool is_file;
  const char* const source;
  const size_t data_size = 0;
};

constexpr TestParams kTestParamsWav[] = {
    {media::AudioCodec::kPCM, /*is_bad=*/false, /*is_file=*/false,
     kTestAudioData, kTestAudioDataSize},
    {media::AudioCodec::kPCM, /*is_bad=*/true, /*is_file=*/false,
     kTestBadWavAudioData, kTestBadWavAudioDataSize},
};

constexpr TestParams kTestParamsFlac[] = {
    {media::AudioCodec::kFLAC, /*is_bad=*/false, /*is_file=*/true, "bear.flac"},
};

}  // namespace

class AudioStreamHandlerTest : public ::testing::TestWithParam<TestParams> {
 public:
  AudioStreamHandlerTest()
      : codec_(GetParam().codec),
        is_bad_(GetParam().is_bad),
        is_file_(GetParam().is_file),
        source_(GetParam().source),
        data_size_(GetParam().data_size) {}

  ~AudioStreamHandlerTest() override = default;

  bool is_bad() const { return is_bad_; }
  AudioStreamHandler* handler() { return audio_stream_handler_.get(); }

  void SetUp() override {
    CreateHandler();
    DCHECK(audio_stream_handler_);
  }

  void CreateHandler(bool loop = false) {
    if (is_file_) {
      const base::FilePath file_path = media::GetTestDataFilePath(source_);
      EXPECT_TRUE(base::ReadFileToString(file_path, &bitstream_));
      audio_stream_handler_ = std::make_unique<AudioStreamHandler>(
          base::DoNothing(), bitstream_, codec_, loop);
    } else {
      std::string_view data(source_, data_size_);
      audio_stream_handler_ = std::make_unique<AudioStreamHandler>(
          base::DoNothing(), data, codec_, loop);
    }
  }

  void SetObserverForTesting(AudioStreamHandler::TestObserver* observer) {
    AudioStreamHandler::SetObserverForTesting(observer);
  }

 private:
  base::test::TaskEnvironment env_;
  const media::AudioCodec codec_;
  const bool is_bad_;
  const bool is_file_;
  const char* const source_;
  const size_t data_size_;
  std::string bitstream_;
  std::unique_ptr<AudioStreamHandler> audio_stream_handler_;
};

TEST_P(AudioStreamHandlerTest, Play) {
  if (is_bad()) {
    return;
  }

  base::RunLoop run_loop;
  TestObserver observer(run_loop.QuitClosure());
  SetObserverForTesting(&observer);

  ASSERT_TRUE(handler()->IsInitialized());
  ASSERT_TRUE(handler()->Play());

  run_loop.Run();

  SetObserverForTesting(nullptr);

  ASSERT_EQ(1, observer.num_play_requests());
  ASSERT_EQ(1, observer.num_stop_requests());
}

TEST_P(AudioStreamHandlerTest, ConsecutivePlayRequests) {
  if (is_bad()) {
    return;
  }

  base::RunLoop run_loop;
  TestObserver observer(run_loop.QuitClosure());
  SetObserverForTesting(&observer);

  ASSERT_TRUE(handler()->IsInitialized());
  ASSERT_TRUE(handler()->Play());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&AudioStreamHandler::Play),
                     base::Unretained(handler())),
      base::Seconds(1));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AudioStreamHandler::Stop, base::Unretained(handler())),
      base::Seconds(2));

  run_loop.Run();

  SetObserverForTesting(nullptr);

  ASSERT_EQ(1, observer.num_play_requests());
  ASSERT_EQ(1, observer.num_stop_requests());
}

TEST_P(AudioStreamHandlerTest, PlayWithLoop) {
  if (is_bad()) {
    return;
  }

  // Create a new handler with looping enabled.
  CreateHandler(/*loop=*/true);

  base::RunLoop quit_run_loop;
  base::RunLoop render_run_loop;
  int render_count = 0;
  TestObserver observer(
      quit_run_loop.QuitClosure(),
      /*render=*/base::BindLambdaForTesting([&render_run_loop, &render_count] {
        // Quit after 10 render callbacks, the test uses a
        // small source audio data (exhausted after the first
        // render), 10 is enough then to test the loop.
        if (++render_count >= 10) {
          render_run_loop.Quit();
        }
      }));
  SetObserverForTesting(&observer);

  ASSERT_TRUE(handler()->IsInitialized());
  ASSERT_TRUE(handler()->Play());

  render_run_loop.Run();

  handler()->Stop();
  // Wait until the playback is stopped before teardown.
  quit_run_loop.Run();

  SetObserverForTesting(nullptr);

  // The render callback is called 10 times (looping), but the play callback is
  // called only once.
  EXPECT_EQ(observer.num_play_requests(), 1);
  EXPECT_EQ(observer.num_stop_requests(), 1);
}

TEST_P(AudioStreamHandlerTest, BadDataDoesNotInitialize) {
  if (!is_bad()) {
    return;
  }
  // The class members and SetUp() will be ignored for this test. Create a
  // handler on the stack with some bad WAV or FLAC data.
  EXPECT_FALSE(handler()->IsInitialized());
  EXPECT_FALSE(handler()->Play());

  // Call Stop() to ensure that there is no crash.
  handler()->Stop();
}

INSTANTIATE_TEST_SUITE_P(Wav,
                         AudioStreamHandlerTest,
                         testing::ValuesIn(kTestParamsWav));
INSTANTIATE_TEST_SUITE_P(Flac,
                         AudioStreamHandlerTest,
                         testing::ValuesIn(kTestParamsFlac));

}  // namespace audio
