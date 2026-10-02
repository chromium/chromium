// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/output_device.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/audio/audio_output_device.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_renderer_sink.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/audio/sync_reader.h"
#include "services/audio/test/mock_stream_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::NotNull;
using testing::StrictMock;
using testing::WithArg;

namespace audio {

namespace {

constexpr float kAudioData = 0.618;
constexpr base::TimeDelta kDelay = base::Microseconds(123);
constexpr char kDeviceId[] = "testdeviceid";
constexpr int kFrames = 789;
constexpr char kNonDefaultDeviceId[] = "valid-nondefault-device-id";
constexpr base::TimeDelta kAuthTimeout = base::Milliseconds(10000);
// `SyncReader`'s default timeout is a fraction of the buffer duration (~17ms,
// or ~9ms on Mac and ChromeOS), which slow bots regularly exceed. `Read()`
// returns as soon as data arrives, so this doesn't slow down passing runs.
constexpr base::TimeDelta kReaderTimeout = base::Milliseconds(250);

class MockRenderCallback : public media::AudioRendererSink::RenderCallback {
 public:
  MockRenderCallback() = default;

  MockRenderCallback(const MockRenderCallback&) = delete;
  MockRenderCallback& operator=(const MockRenderCallback&) = delete;

  ~MockRenderCallback() override = default;

  MOCK_METHOD4(Render,
               int(base::TimeDelta delay,
                   base::TimeTicks timestamp,
                   const media::AudioGlitchInfo& glitch_info,
                   media::AudioBus* dest));
  void OnRenderError() override {}
};

class MockAudioOutputIPC : public media::AudioOutputIPC {
 public:
  MockAudioOutputIPC() = default;
  ~MockAudioOutputIPC() override = default;

  MOCK_METHOD3(RequestDeviceAuthorization,
               void(media::AudioOutputIPCDelegate* delegate,
                    const base::UnguessableToken& session_id,
                    const std::string& device_id));
  MOCK_METHOD2(CreateStream,
               void(media::AudioOutputIPCDelegate* delegate,
                    const media::AudioParameters& params));
  MOCK_METHOD0(PlayStream, void());
  MOCK_METHOD0(PauseStream, void());
  MOCK_METHOD0(FlushStream, void());
  MOCK_METHOD0(CloseStream, void());
  MOCK_METHOD1(SetVolume, void(double volume));
};

struct DataFlowTestEnvironment {
  explicit DataFlowTestEnvironment(const media::AudioParameters& params) {
    const uint32_t memory_size = ComputeAudioOutputBufferSize(params);
    auto shared_memory_region =
        base::UnsafeSharedMemoryRegion::Create(memory_size);
    auto shared_memory_mapping = shared_memory_region.Map();
    CHECK(shared_memory_region.IsValid());
    CHECK(shared_memory_mapping.IsValid());
    reader = std::make_unique<audio::SyncReader>(
        /*log callback*/ base::DoNothing(), params, &client_socket);
    CHECK(reader->IsValid());
    time_stamp = base::TimeTicks::Now();

    reader->set_max_wait_timeout_for_test(kReaderTimeout);
  }

  base::CancelableSyncSocket client_socket;
  StrictMock<MockRenderCallback> render_callback;
  std::unique_ptr<audio::SyncReader> reader;
  base::TimeTicks time_stamp;
};

}  // namespace

class AudioServiceOutputDeviceTest : public testing::Test {
 public:
  AudioServiceOutputDeviceTest()
      : task_env_(
            base::test::TaskEnvironment::MainThreadType::DEFAULT,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {
    stream_factory_ = std::make_unique<MockStreamFactory>();
  }

  AudioServiceOutputDeviceTest(const AudioServiceOutputDeviceTest&) = delete;
  AudioServiceOutputDeviceTest& operator=(const AudioServiceOutputDeviceTest&) =
      delete;

  ~AudioServiceOutputDeviceTest() override {
    if (!stream_factory_->created_callback()) {
      return;
    }
    std::move(stream_factory_->created_callback()).Run(nullptr);
    task_env_.RunUntilIdle();
  }

  mojo::PendingRemote<media::mojom::AudioStreamFactory> MakeFactoryRemote() {
    return stream_factory_->receiver_.BindNewPipeAndPassRemote();
  }

  base::test::TaskEnvironment task_env_;
  std::unique_ptr<MockStreamFactory> stream_factory_;
};

TEST_F(AudioServiceOutputDeviceTest, CreatePlayPause) {
  auto params(media::AudioParameters::UnavailableDeviceParams());
  OutputDevice output_device(MakeFactoryRemote(), params, nullptr, kDeviceId);

  constexpr double volume = 0.42;
  EXPECT_CALL(stream_factory_->stream(), SetVolume(volume));
  EXPECT_CALL(stream_factory_->stream(), Play());
  EXPECT_CALL(stream_factory_->stream(), Pause());

  output_device.SetVolume(volume);
  output_device.Play();
  output_device.Pause();
  task_env_.RunUntilIdle();
}

TEST_F(AudioServiceOutputDeviceTest, VerifyDataFlow) {
  auto params(media::AudioParameters::UnavailableDeviceParams());
  params.set_frames_per_buffer(kFrames);
  ASSERT_EQ(2, params.channels());
  DataFlowTestEnvironment env(params);
  OutputDevice output_device(MakeFactoryRemote(), params, &env.render_callback,
                             kDeviceId);
  EXPECT_CALL(stream_factory_->stream(), Play());
  output_device.Play();
  task_env_.RunUntilIdle();

  std::move(stream_factory_->created_callback())
      .Run({std::in_place, env.reader->TakeSharedMemoryRegion(),
            mojo::PlatformHandle(env.client_socket.Take())});
  task_env_.RunUntilIdle();

  // At this point, the callback thread should be running. Send some data over
  // and verify that it's propagated to |env.render_callback|. Do it a few
  // times.
  auto test_bus = media::AudioBus::Create(params);
  for (int i = 0; i < 10; ++i) {
    test_bus->Zero();
    media::AudioGlitchInfo glitch_info{.duration = base::Milliseconds(100),
                                       .count = 123};
    EXPECT_CALL(env.render_callback,
                Render(kDelay, env.time_stamp, glitch_info, NotNull()))
        .WillOnce(WithArg<3>([](media::AudioBus* client_bus) -> int {
          // Place some test data in the bus so that we can check that it was
          // copied to the audio service side.
          std::ranges::fill(client_bus->channel(0), kAudioData);
          std::ranges::fill(client_bus->channel(1), kAudioData);
          return client_bus->frames();
        }));
    env.reader->RequestMoreData(kDelay, env.time_stamp, glitch_info);
    ASSERT_TRUE(env.reader->Read(test_bus.get(), false));

    Mock::VerifyAndClear(&env.render_callback);
    constexpr auto samples_match = [](float sample) {
      return sample == kAudioData;
    };

    EXPECT_TRUE(std::ranges::all_of(test_bus->channel(0), samples_match));
    EXPECT_TRUE(std::ranges::all_of(test_bus->channel(1), samples_match));
  }
}

#if BUILDFLAG(ENABLE_PASSTHROUGH_AUDIO_CODECS)
TEST_F(AudioServiceOutputDeviceTest, CreateBitStreamStream) {
  constexpr int kAudioParameterFrames = 4321;
  media::AudioParameters params(media::AudioParameters::AUDIO_BITSTREAM_EAC3,
                                media::ChannelLayoutConfig::Stereo(), 48000,
                                kAudioParameterFrames);

  DataFlowTestEnvironment env(params);
  auto* ipc = new MockAudioOutputIPC();  // owned by |audio_device|.
  auto audio_device = base::MakeRefCounted<media::AudioOutputDevice>(
      base::WrapUnique(ipc), task_env_.GetMainThreadTaskRunner(),
      media::AudioSinkParameters(base::UnguessableToken(), kNonDefaultDeviceId),
      kAuthTimeout);

  // Start a stream.
  audio_device->RequestDeviceAuthorization();
  audio_device->Initialize(params, &env.render_callback);
  audio_device->Start();
  EXPECT_CALL(*ipc, RequestDeviceAuthorization(audio_device.get(),
                                               base::UnguessableToken(),
                                               kNonDefaultDeviceId));
  EXPECT_CALL(*ipc, CreateStream(audio_device.get(), _));
  EXPECT_CALL(*ipc, PlayStream());
  task_env_.RunUntilIdle();
  Mock::VerifyAndClear(ipc);
  audio_device->OnDeviceAuthorized(media::OUTPUT_DEVICE_STATUS_OK, params,
                                   kNonDefaultDeviceId);
  audio_device->OnStreamCreated(env.reader->TakeSharedMemoryRegion(),
                                env.client_socket.Take(),
                                /*playing_automatically*/ false);

  task_env_.RunUntilIdle();
  // At this point, the callback thread should be running. Send some data over
  // and verify that it's propagated to |env.callback|. Do it a few times.
  auto test_bus = media::AudioBus::Create(params);
  constexpr uint8_t kAudioByteData = 127;
  constexpr size_t kBitstreamDataSize = 512;
  constexpr int kBitstreamFrames = 101;
  for (int i = 0; i < 10; ++i) {
    test_bus->Zero();
    media::AudioGlitchInfo glitch_info{.duration = base::Milliseconds(100),
                                       .count = 123};
    EXPECT_CALL(env.render_callback,
                Render(kDelay, env.time_stamp, glitch_info, NotNull()))
        .WillOnce(WithArg<3>([](media::AudioBus* renderer_bus) -> int {
          EXPECT_TRUE(renderer_bus->is_bitstream_format());
          // Place some test data in the bus so that we can check that it was
          // copied to the browser side.
          renderer_bus->SetBitstreamFrames(kBitstreamFrames);
          renderer_bus->SetBitstreamSize(kBitstreamDataSize);
          std::ranges::fill(renderer_bus->bitstream_data(), kAudioByteData);
          return renderer_bus->frames();
        }));
    env.reader->RequestMoreData(kDelay, env.time_stamp, glitch_info);
    env.reader->Read(test_bus.get(), false);

    Mock::VerifyAndClear(&env.render_callback);
    EXPECT_TRUE(test_bus->is_bitstream_format());
    EXPECT_EQ(kBitstreamFrames, test_bus->GetBitstreamFrames());
    EXPECT_EQ(kBitstreamDataSize, test_bus->bitstream_data().size());
    for (auto datum : test_bus->bitstream_data()) {
      // Note: if all of these fail, the bots will behave strangely due to the
      // large amount of text output. Assert is used to avoid this.
      ASSERT_EQ(kAudioByteData, datum);
    }
  }

  audio_device->Stop();
  EXPECT_CALL(*ipc, CloseStream());
  task_env_.RunUntilIdle();
}
#endif

TEST_F(AudioServiceOutputDeviceTest, CreateNondefaultDevice) {
  auto params = media::AudioParameters::UnavailableDeviceParams();
  params.set_frames_per_buffer(kFrames);
  ASSERT_EQ(2, params.channels());
  DataFlowTestEnvironment env(params);
  auto* ipc = new MockAudioOutputIPC();  // owned by |audio_device|.
  auto audio_device = base::MakeRefCounted<media::AudioOutputDevice>(
      base::WrapUnique(ipc), task_env_.GetMainThreadTaskRunner(),
      media::AudioSinkParameters(base::UnguessableToken(), kNonDefaultDeviceId),
      kAuthTimeout);

  audio_device->RequestDeviceAuthorization();
  audio_device->Initialize(params, &env.render_callback);
  audio_device->Start();
  EXPECT_CALL(*ipc, RequestDeviceAuthorization(audio_device.get(),
                                               base::UnguessableToken(),
                                               kNonDefaultDeviceId));
  EXPECT_CALL(*ipc, CreateStream(audio_device.get(), _));
  EXPECT_CALL(*ipc, PlayStream());
  task_env_.RunUntilIdle();
  Mock::VerifyAndClear(ipc);
  audio_device->OnDeviceAuthorized(media::OUTPUT_DEVICE_STATUS_OK, params,
                                   kNonDefaultDeviceId);
  audio_device->OnStreamCreated(env.reader->TakeSharedMemoryRegion(),
                                env.client_socket.Take(),
                                /*playing_automatically*/ false);

  audio_device->Stop();
  EXPECT_CALL(*ipc, CloseStream());
  task_env_.RunUntilIdle();
}

}  // namespace audio
