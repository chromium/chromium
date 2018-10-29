// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/output_device.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "media/audio/audio_sync_reader.h"
#include "media/base/audio_renderer_sink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::Mock;
using testing::NotNull;
using testing::StrictMock;
using testing::WithArg;

namespace audio {

namespace {

constexpr float kAudioData = 0.618;
constexpr base::TimeDelta kDelay = base::TimeDelta::FromMicroseconds(123);
constexpr char kDeviceId[] = "testdeviceid";
constexpr int kFramesSkipped = 456;
constexpr int kFrames = 789;

class MockRenderCallback : public media::AudioRendererSink::RenderCallback {
 public:
  MockRenderCallback() = default;
  ~MockRenderCallback() override = default;

  MOCK_METHOD4(Render,
               int(base::TimeDelta delay,
                   base::TimeTicks timestamp,
                   int prior_frames_skipped,
                   media::AudioBus* dest));
  void OnRenderError() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRenderCallback);
};

class MockStream : public media::mojom::AudioOutputStream {
 public:
  MockStream() = default;
  ~MockStream() override = default;

  MOCK_METHOD0(Play, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD1(SetVolume, void(double));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockStream);
};

class FakeOutputStreamFactory : public audio::FakeStreamFactory {
 public:
  FakeOutputStreamFactory() : stream_(), stream_binding_(&stream_) {}
  ~FakeOutputStreamFactory() final {}

  void CreateOutputStream(
      media::mojom::AudioOutputStreamRequest stream_request,
      media::mojom::AudioOutputStreamObserverAssociatedPtrInfo observer_info,
      media::mojom::AudioLogPtr log,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      const base::Optional<base::UnguessableToken>& processing_id,
      CreateOutputStreamCallback created_callback) final {
    EXPECT_FALSE(observer_info);
    EXPECT_FALSE(log);
    created_callback_ = std::move(created_callback);

    if (stream_binding_.is_bound())
      stream_binding_.Unbind();
    stream_binding_.Bind(std::move(stream_request));
  }

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    binding_.Bind(audio::mojom::StreamFactoryRequest(std::move(handle)));
  }

  StrictMock<MockStream> stream_;
  CreateOutputStreamCallback created_callback_;

 private:
  mojo::Binding<media::mojom::AudioOutputStream> stream_binding_;
  DISALLOW_COPY_AND_ASSIGN(FakeOutputStreamFactory);
};

struct DataFlowTestEnvironment {
  explicit DataFlowTestEnvironment(const media::AudioParameters& params) {
    const uint32_t memory_size = ComputeAudioOutputBufferSize(params);
    auto shared_memory_region =
        base::UnsafeSharedMemoryRegion::Create(memory_size);
    auto shared_memory_mapping = shared_memory_region.Map();
    CHECK(shared_memory_region.IsValid());
    CHECK(shared_memory_mapping.IsValid());
    auto service_socket = std::make_unique<base::CancelableSyncSocket>();
    CHECK(base::CancelableSyncSocket::CreatePair(service_socket.get(),
                                                 &client_socket));
    reader = std::make_unique<media::AudioSyncReader>(
        /*log callback*/ base::DoNothing(), params,
        std::move(shared_memory_region), std::move(shared_memory_mapping),
        std::move(service_socket));
    time_stamp = base::TimeTicks::Now();

#if defined(OS_FUCHSIA)
    // TODO(https://crbug.com/838367): Fuchsia bots use nested virtualization,
    // which can result in unusually long scheduling delays, so allow a longer
    // timeout.
    reader->set_max_wait_timeout_for_test(
        base::TimeDelta::FromMilliseconds(250));
#endif
  }

  base::CancelableSyncSocket client_socket;
  StrictMock<MockRenderCallback> render_callback;
  std::unique_ptr<media::AudioSyncReader> reader;
  base::TimeTicks time_stamp;
};

}  // namespace

class AudioServiceOutputDeviceTest : public testing::Test {
 public:
  AudioServiceOutputDeviceTest()
      : task_env_(base::test::ScopedTaskEnvironment::MainThreadType::DEFAULT,
                  base::test::ScopedTaskEnvironment::ExecutionMode::QUEUED) {
    service_manager::mojom::ConnectorRequest connector_request;
    connector_ = service_manager::Connector::Create(&connector_request);
    stream_factory_ = std::make_unique<FakeOutputStreamFactory>();
    service_manager::Connector::TestApi connector_test_api(connector_.get());
    connector_test_api.OverrideBinderForTesting(
        service_manager::Identity(audio::mojom::kServiceName),
        audio::mojom::StreamFactory::Name_,
        base::BindRepeating(&AudioServiceOutputDeviceTest::BindStreamFactory,
                            base::Unretained(this)));
  }

  ~AudioServiceOutputDeviceTest() override {
    if (!stream_factory_->created_callback_)
      return;
    std::move(stream_factory_->created_callback_).Run(nullptr);
    task_env_.RunUntilIdle();
  }

  base::test::ScopedTaskEnvironment task_env_;
  std::unique_ptr<service_manager::Connector> connector_;
  std::unique_ptr<FakeOutputStreamFactory> stream_factory_;

 private:
  void BindStreamFactory(mojo::ScopedMessagePipeHandle factory_request) {
    stream_factory_->binding_.Bind(
        audio::mojom::StreamFactoryRequest(std::move(factory_request)));
  }
  DISALLOW_COPY_AND_ASSIGN(AudioServiceOutputDeviceTest);
};

TEST_F(AudioServiceOutputDeviceTest, CreatePlayPause) {
  auto params(media::AudioParameters::UnavailableDeviceParams());
  OutputDevice output_device(std::move(connector_), params, nullptr, kDeviceId);

  constexpr double volume = 0.42;
  EXPECT_CALL(stream_factory_->stream_, SetVolume(volume));
  EXPECT_CALL(stream_factory_->stream_, Play());
  EXPECT_CALL(stream_factory_->stream_, Pause());

  output_device.SetVolume(volume);
  output_device.Play();
  output_device.Pause();
  task_env_.RunUntilIdle();
}

// Flaky on Linux Chromium OS ASan LSan (https://crbug.com/889845)
#if defined(OS_CHROMEOS) && defined(ADDRESS_SANITIZER)
#define MAYBE_VerifyDataFlow DISABLED_VerifyDataFlow
#else
#define MAYBE_VerifyDataFlow VerifyDataFlow
#endif
TEST_F(AudioServiceOutputDeviceTest, MAYBE_VerifyDataFlow) {
  auto params(media::AudioParameters::UnavailableDeviceParams());
  params.set_frames_per_buffer(kFrames);
  ASSERT_EQ(2, params.channels());
  DataFlowTestEnvironment env(params);
  OutputDevice output_device(std::move(connector_), params,
                             &env.render_callback, kDeviceId);
  EXPECT_CALL(stream_factory_->stream_, Play());
  output_device.Play();
  task_env_.RunUntilIdle();

  std::move(stream_factory_->created_callback_)
      .Run({base::in_place, env.reader->TakeSharedMemoryRegion(),
            mojo::WrapPlatformFile(env.client_socket.Release())});
  task_env_.RunUntilIdle();

  // At this point, the callback thread should be running. Send some data over
  // and verify that it's propagated to |env.render_callback|. Do it a few
  // times.
  auto test_bus = media::AudioBus::Create(params);
  for (int i = 0; i < 10; ++i) {
    test_bus->Zero();
    EXPECT_CALL(env.render_callback,
                Render(kDelay, env.time_stamp, kFramesSkipped, NotNull()))
        .WillOnce(WithArg<3>(Invoke([](media::AudioBus* client_bus) -> int {
          // Place some test data in the bus so that we can check that it was
          // copied to the audio service side.
          std::fill_n(client_bus->channel(0), client_bus->frames(), kAudioData);
          std::fill_n(client_bus->channel(1), client_bus->frames(), kAudioData);
          return client_bus->frames();
        })));
    env.reader->RequestMoreData(kDelay, env.time_stamp, kFramesSkipped);
    env.reader->Read(test_bus.get());

    Mock::VerifyAndClear(&env.render_callback);
    for (int i = 0; i < kFrames; ++i) {
      EXPECT_EQ(kAudioData, test_bus->channel(0)[i]);
      EXPECT_EQ(kAudioData, test_bus->channel(1)[i]);
    }
  }
}

}  // namespace audio
