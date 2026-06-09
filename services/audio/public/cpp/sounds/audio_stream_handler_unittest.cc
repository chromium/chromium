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
#include "base/logging.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"
#include "ui/base/resource/resource_bundle.h"

namespace audio {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::TestParamInfo;
using ::testing::ValuesIn;

constexpr std::string_view kTestBadWavAudioData =
    "RIFF1234WAVEjunkjunkjunkjunk";

constexpr int kTestResourceId = 1;

std::string ReadTestMediaFile(std::string_view file_name) {
  const base::FilePath file_path = media::GetTestDataFilePath(file_name);
  std::string data;
  CHECK(base::ReadFileToString(file_path, &data));
  return data;
}

struct TestParams {
  base::RepeatingCallback<std::string()> data_factory;
  media::AudioCodec codec = media::AudioCodec::kUnknown;
  std::string test_suffix;
};

std::vector<TestParams> GetTestParams() {
  return {
      {.data_factory = base::BindRepeating(&ReadTestMediaFile, "bear_pcm.wav"),
       .codec = media::AudioCodec::kPCM,
       .test_suffix = "Wav"},
      {.data_factory = base::BindRepeating(&ReadTestMediaFile, "bear.flac"),
       .codec = media::AudioCodec::kFLAC,
       .test_suffix = "Flac"},
  };
}

class AudioStreamHandlerTest : public testing::Test {
 public:
  ~AudioStreamHandlerTest() override = default;

 protected:
  ui::MockResourceBundleDelegate& mock_resource_delegate() {
    return mock_resource_delegate_;
  }

 private:
  NiceMock<ui::MockResourceBundleDelegate> mock_resource_delegate_;
  ui::ResourceBundle resource_bundle_{&mock_resource_delegate_};
  ui::ResourceBundle::SharedInstanceSwapperForTesting resource_bundle_swapper_{
      &resource_bundle_};

  base::test::TaskEnvironment task_environment_;
};

TEST_F(AudioStreamHandlerTest, BadDataDoesNotInitialize) {
  EXPECT_CALL(mock_resource_delegate(),
              GetRawDataResource(kTestResourceId, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(kTestBadWavAudioData), Return(true)));

  AudioStreamHandler handler(/*stream_factory_binder=*/base::DoNothing(),
                             kTestResourceId, media::AudioCodec::kPCM);

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
    const std::string audio_data = GetParam().data_factory.Run();
    EXPECT_CALL(mock_resource_delegate(),
                GetRawDataResource(kTestResourceId, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(audio_data), Return(true)));
    return std::make_unique<AudioStreamHandler>(
        /*stream_factory_binder=*/base::DoNothing(), kTestResourceId,
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

TEST_P(AudioStreamHandlerTestWithParams, PauseAndResume) {
  // Calculate the total (baseline) number of frames for a given audio source.
  int baseline_frames = 0;
  {
    std::unique_ptr<AudioStreamHandler> handler = CreateHandler();
    base::RunLoop run_loop;
    TestObserver observer(run_loop.QuitClosure());
    AudioStreamHandler::SetObserverForTesting(&observer);
    ASSERT_TRUE(handler->Play());
    run_loop.Run();
    baseline_frames = observer.total_frames_rendered();
    AudioStreamHandler::SetObserverForTesting(nullptr);
  }

  std::unique_ptr<AudioStreamHandler> handler = CreateHandler();
  ASSERT_NE(handler, nullptr);

  base::RunLoop run_loop;
  bool pause_requested = false;
  TestObserver observer(
      run_loop.QuitClosure(),
      /*render=*/base::BindLambdaForTesting([&]() {
        if (pause_requested) {
          return;
        }
        // Request `Pause` on the first `Render` request.
        pause_requested = true;
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(base::IgnoreResult(&AudioStreamHandler::Pause),
                           base::Unretained(handler.get())));
      }));
  AudioStreamHandler::SetObserverForTesting(&observer);

  ASSERT_TRUE(handler->IsInitialized());
  ASSERT_TRUE(handler->Play());

  run_loop.Run();

  EXPECT_EQ(observer.num_pause_requests(), 1);
  EXPECT_EQ(observer.num_stop_requests(), 0);

  const int frames_before_pause = observer.total_frames_rendered();

  // Resume play.
  base::RunLoop resume_run_loop;
  observer.set_quit_closure(resume_run_loop.QuitClosure());

  ASSERT_TRUE(handler->Play());
  resume_run_loop.Run();

  EXPECT_EQ(observer.num_stop_requests(), 1);

  const int total_frames = observer.total_frames_rendered();
  EXPECT_GT(total_frames, 0);

  EXPECT_GT(frames_before_pause, 0);
  EXPECT_GT(total_frames, frames_before_pause);
  EXPECT_EQ(total_frames, baseline_frames);

  AudioStreamHandler::SetObserverForTesting(nullptr);
}

INSTANTIATE_TEST_SUITE_P(,
                         AudioStreamHandlerTestWithParams,
                         ValuesIn(GetTestParams()),
                         [](const TestParamInfo<TestParams>& info) {
                           return info.param.test_suffix;
                         });

}  // namespace
}  // namespace audio
