// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/sounds/audio_stream_handler.h"

#include <memory>
#include <string>
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

using ::testing::TestParamInfo;
using ::testing::ValuesIn;

constexpr std::string_view kTestBadWavAudioData =
    "RIFF1234WAVEjunkjunkjunkjunk";

std::string ReadTestMediaFile(std::string_view file_name) {
  const base::FilePath file_path = media::GetTestDataFilePath(file_name);
  std::string data;
  CHECK(base::ReadFileToString(file_path, &data));
  return data;
}

struct TestParams {
  std::string audio_data;
  media::AudioCodec codec = media::AudioCodec::kUnknown;
  std::string test_suffix;
};

std::vector<TestParams> GetTestParams() {
  return {
      {.audio_data = std::string(kTestAudioData, kTestAudioDataSize),
       .codec = media::AudioCodec::kPCM,
       .test_suffix = "Wav"},
      {.audio_data = ReadTestMediaFile("bear.flac"),
       .codec = media::AudioCodec::kFLAC,
       .test_suffix = "Flac"},
  };
}

class AudioStreamHandlerTest : public testing::Test {
 public:
  ~AudioStreamHandlerTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(AudioStreamHandlerTest, BadDataDoesNotInitialize) {
  AudioStreamHandler handler(/*stream_factory_binder=*/base::DoNothing(),
                             kTestBadWavAudioData, media::AudioCodec::kPCM);

  // The handler should not be initialized with bad data, and `Play` should
  // return `false`.
  EXPECT_FALSE(handler.IsInitialized());
  EXPECT_FALSE(handler.Play());

  // Call `Stop` to ensure that there is no crash.
  handler.Stop();
}

class AudioStreamHandlerTestWithParams
    : public AudioStreamHandlerTest,
      public testing::WithParamInterface<TestParams> {
 protected:
  std::unique_ptr<AudioStreamHandler> CreateHandler(bool loop = false) {
    return std::make_unique<AudioStreamHandler>(
        /*stream_factory_binder=*/base::DoNothing(), GetParam().audio_data,
        GetParam().codec, loop);
  }
};

TEST_P(AudioStreamHandlerTestWithParams, Play) {
  std::unique_ptr<AudioStreamHandler> handler = CreateHandler();
  ASSERT_NE(handler, nullptr);

  base::RunLoop run_loop;
  TestObserver observer(run_loop.QuitClosure());
  AudioStreamHandler::SetObserverForTesting(&observer);

  ASSERT_TRUE(handler->IsInitialized());
  ASSERT_TRUE(handler->Play());

  run_loop.Run();

  AudioStreamHandler::SetObserverForTesting(nullptr);

  EXPECT_EQ(observer.num_play_requests(), 1);
  EXPECT_EQ(observer.num_stop_requests(), 1);
}

TEST_P(AudioStreamHandlerTestWithParams, ConsecutivePlayRequests) {
  std::unique_ptr<AudioStreamHandler> handler = CreateHandler();
  ASSERT_NE(handler, nullptr);

  base::RunLoop run_loop;
  TestObserver observer(run_loop.QuitClosure());
  AudioStreamHandler::SetObserverForTesting(&observer);

  ASSERT_TRUE(handler->IsInitialized());
  ASSERT_TRUE(handler->Play());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&AudioStreamHandler::Play),
                     base::Unretained(handler.get())),
      base::Seconds(1));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AudioStreamHandler::Stop,
                     base::Unretained(handler.get())),
      base::Seconds(2));

  run_loop.Run();

  AudioStreamHandler::SetObserverForTesting(nullptr);

  EXPECT_EQ(observer.num_play_requests(), 1);
  EXPECT_EQ(observer.num_stop_requests(), 1);
}

TEST_P(AudioStreamHandlerTestWithParams, PlayWithLoop) {
  std::unique_ptr<AudioStreamHandler> handler = CreateHandler(/*loop=*/true);
  ASSERT_NE(handler, nullptr);

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
  AudioStreamHandler::SetObserverForTesting(&observer);

  ASSERT_TRUE(handler->IsInitialized());
  ASSERT_TRUE(handler->Play());

  render_run_loop.Run();

  handler->Stop();
  // Wait until the playback is stopped before teardown.
  quit_run_loop.Run();

  AudioStreamHandler::SetObserverForTesting(nullptr);

  // The render callback is called 10 times (looping), but the play callback
  // is called only once.
  EXPECT_EQ(observer.num_play_requests(), 1);
  EXPECT_EQ(observer.num_stop_requests(), 1);
}

INSTANTIATE_TEST_SUITE_P(,
                         AudioStreamHandlerTestWithParams,
                         ValuesIn(GetTestParams()),
                         [](const TestParamInfo<TestParams>& info) {
                           return info.param.test_suffix;
                         });

}  // namespace
}  // namespace audio
