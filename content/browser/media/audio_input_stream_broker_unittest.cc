// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_input_stream_broker.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/sync_socket.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "content/public/test/browser_task_environment.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "media/mojo/mojom/audio_processing.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

using ::testing::InSequence;
using ::testing::Mock;
using ::testing::StrictMock;
using ::testing::Test;

namespace content {

namespace {

const int kRenderProcessId = 123;
const int kRenderFrameId = 234;
const uint32_t kShMemCount = 10;
const bool kEnableAgc = false;
const char kDeviceId[] = "testdeviceid";
const bool kInitiallyMuted = false;

media::AudioParameters TestParams() {
  return media::AudioParameters::UnavailableDeviceParams();
}

using MockDeleterCallback = StrictMock<
    base::MockCallback<base::OnceCallback<void(AudioStreamBroker*)>>>;

class MockRendererAudioInputStreamFactoryClient
    : public blink::mojom::RendererAudioInputStreamFactoryClient {
 public:
  MockRendererAudioInputStreamFactoryClient() = default;

  MockRendererAudioInputStreamFactoryClient(
      const MockRendererAudioInputStreamFactoryClient&) = delete;
  MockRendererAudioInputStreamFactoryClient& operator=(
      const MockRendererAudioInputStreamFactoryClient&) = delete;

  ~MockRendererAudioInputStreamFactoryClient() override = default;

  mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
  MakeRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD0(OnStreamCreated, void());

  void StreamCreated(
      mojo::PendingRemote<media::mojom::AudioInputStream> input_stream,
      mojo::PendingReceiver<media::mojom::AudioInputStreamClient>
          client_receiver,
      media::mojom::ReadOnlyAudioDataPipePtr data_pipe,
      bool initially_muted,
      const std::optional<base::UnguessableToken>& stream_id) override {
    EXPECT_TRUE(stream_id.has_value());
    input_stream_.Bind(std::move(input_stream));
    client_receiver_ = std::move(client_receiver);
    OnStreamCreated();
  }

  void CloseReceiver() { receiver_.reset(); }

  void setDisconnectHandler(
      base::OnceCallback<void(uint32_t, const std::string&)> callback) {
    receiver_.set_disconnect_with_reason_handler(std::move(callback));
  }

 private:
  mojo::Receiver<blink::mojom::RendererAudioInputStreamFactoryClient> receiver_{
      this};
  mojo::Remote<media::mojom::AudioInputStream> input_stream_;
  mojo::PendingReceiver<media::mojom::AudioInputStreamClient> client_receiver_;
};

class MockStreamFactory final : public audio::FakeStreamFactory {
 public:
  // State of an expected stream creation. |device_id| and |params| are set
  // ahead of time and verified during request. The other fields are filled in
  // when the request is received.
  struct StreamRequestData {
    StreamRequestData(const std::string& device_id,
                      const media::AudioParameters& params)
        : device_id(device_id), params(params) {}

    bool requested = false;
    mojo::PendingReceiver<media::mojom::AudioInputStream> stream_receiver;
    mojo::Remote<media::mojom::AudioInputStreamClient> client;
    mojo::Remote<media::mojom::AudioInputStreamObserver> observer;
    mojo::Remote<media::mojom::AudioLog> log;
    const std::string device_id;
    const media::AudioParameters params;
    uint32_t shared_memory_count;
    bool enable_agc;
    base::ReadOnlySharedMemoryRegion key_press_count_buffer;
    media::mojom::AudioProcessingConfigPtr processing_config;
    CreateInputStreamCallback created_callback;
  };

  explicit MockStreamFactory(StreamRequestData* ex)
      : stream_request_data_(ex) {}

  MockStreamFactory(const MockStreamFactory&) = delete;
  MockStreamFactory& operator=(const MockStreamFactory&) = delete;

  ~MockStreamFactory() override = default;

 private:
  void CreateInputStream(
      mojo::PendingReceiver<media::mojom::AudioInputStream> stream_receiver,
      mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
      mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      bool enable_agc,
      base::ReadOnlySharedMemoryRegion key_press_count_buffer,
      media::mojom::AudioProcessingConfigPtr processing_config,
      CreateInputStreamCallback created_callback) override {
    // No way to cleanly exit the test here in case of failure, so use CHECK.
    CHECK(stream_request_data_);
    EXPECT_EQ(stream_request_data_->device_id, device_id);
    EXPECT_TRUE(stream_request_data_->params.Equals(params));
    stream_request_data_->requested = true;
    stream_request_data_->stream_receiver = std::move(stream_receiver);
    stream_request_data_->client.Bind(std::move(client));
    stream_request_data_->observer.Bind(std::move(observer));
    stream_request_data_->log.Bind(std::move(log));
    stream_request_data_->shared_memory_count = shared_memory_count;
    stream_request_data_->enable_agc = enable_agc;
    stream_request_data_->key_press_count_buffer =
        std::move(key_press_count_buffer);
    stream_request_data_->processing_config = std::move(processing_config);
    stream_request_data_->created_callback = std::move(created_callback);
  }

  const raw_ptr<StreamRequestData> stream_request_data_;
};

struct TestEnvironment {
  explicit TestEnvironment(MockStreamFactory::StreamRequestData* ex)
      : broker(std::make_unique<AudioInputStreamBroker>(
            kRenderProcessId,
            kRenderFrameId,
            kDeviceId,
            TestParams(),
            kShMemCount,
            nullptr /*user_input_monitor*/,
            kEnableAgc,
            media::mojom::AudioProcessingConfig::New(
                remote_controls_.BindNewPipeAndPassReceiver(),
                media::AudioProcessingSettings()),
            deleter.Get(),
            renderer_factory_client.MakeRemote())),
        stream_factory(ex) {}

  void RunUntilIdle() { task_environment.RunUntilIdle(); }

  BrowserTaskEnvironment task_environment;
  MockDeleterCallback deleter;
  StrictMock<MockRendererAudioInputStreamFactoryClient> renderer_factory_client;
  mojo::Remote<media::mojom::AudioProcessorControls> remote_controls_;
  std::unique_ptr<AudioInputStreamBroker> broker;
  MockStreamFactory stream_factory;
  mojo::Remote<media::mojom::AudioStreamFactory> factory_ptr{
      stream_factory.MakeRemote()};
};

void ExpectRendererFactoryClientDisconnection(
    TestEnvironment& env,
    media::mojom::InputStreamErrorCode expected_error) {
  env.renderer_factory_client.setDisconnectHandler(base::BindLambdaForTesting(
      [expected_error](uint32_t reason, const std::string& description) {
        EXPECT_EQ(static_cast<uint32_t>(expected_error), reason);
        EXPECT_EQ("", description);
      }));
}

}  // namespace

TEST(AudioInputStreamBrokerTest, StoresProcessAndFrameId) {
  BrowserTaskEnvironment task_environment;
  MockDeleterCallback deleter;
  StrictMock<MockRendererAudioInputStreamFactoryClient> renderer_factory_client;

  AudioInputStreamBroker broker(
      kRenderProcessId, kRenderFrameId, kDeviceId, TestParams(), kShMemCount,
      nullptr /*user_input_monitor*/, kEnableAgc, nullptr /*processing_config*/,
      deleter.Get(), renderer_factory_client.MakeRemote());

  EXPECT_EQ(kRenderProcessId, broker.render_process_id());
  EXPECT_EQ(kRenderFrameId, broker.render_frame_id());
}

TEST(AudioInputStreamBrokerTest, StreamCreationSuccess_Propagates) {
  MockStreamFactory::StreamRequestData stream_request_data(kDeviceId,
                                                           TestParams());
  TestEnvironment env(&stream_request_data);
  env.broker->CreateStream(env.factory_ptr.get());
  env.RunUntilIdle();

  EXPECT_TRUE(stream_request_data.requested);
  EXPECT_TRUE(stream_request_data.processing_config);
  EXPECT_TRUE(stream_request_data.processing_config->controls_receiver);

  // Set up test IPC primitives.
  const size_t shmem_size = 456;
  base::SyncSocket socket1, socket2;
  base::SyncSocket::CreatePair(&socket1, &socket2);
  std::move(stream_request_data.created_callback)
      .Run({std::in_place,
            base::ReadOnlySharedMemoryRegion::Create(shmem_size).region,
            mojo::PlatformHandle(socket1.Take())},
           kInitiallyMuted, base::UnguessableToken::Create());

  EXPECT_CALL(env.renderer_factory_client, OnStreamCreated());

  env.RunUntilIdle();

  Mock::VerifyAndClear(&env.renderer_factory_client);

  env.broker.reset();
}

TEST(AudioInputStreamBrokerTest, StreamCreationFailure_CallsDeleter) {
  MockStreamFactory::StreamRequestData stream_request_data(kDeviceId,
                                                           TestParams());
  TestEnvironment env(&stream_request_data);
  env.broker->CreateStream(env.factory_ptr.get());
  env.RunUntilIdle();

  EXPECT_TRUE(stream_request_data.requested);
  EXPECT_CALL(env.deleter, Run(env.broker.release()))
      .WillOnce(testing::DeleteArg<0>());

  std::move(stream_request_data.created_callback)
      .Run(nullptr, kInitiallyMuted, std::nullopt);

  env.RunUntilIdle();
}

TEST(AudioInputStreamBrokerTest, RendererFactoryClientDisconnect_CallsDeleter) {
  InSequence seq;
  MockStreamFactory::StreamRequestData stream_request_data(kDeviceId,
                                                           TestParams());
  TestEnvironment env(&stream_request_data);
  env.broker->CreateStream(env.factory_ptr.get());
  env.RunUntilIdle();
  EXPECT_TRUE(stream_request_data.requested);

  EXPECT_CALL(env.deleter, Run(env.broker.release()))
      .WillOnce(testing::DeleteArg<0>());
  env.renderer_factory_client.CloseReceiver();
  env.RunUntilIdle();
  Mock::VerifyAndClear(&env.deleter);

  env.stream_factory.ResetReceiver();
  env.RunUntilIdle();
}

TEST(AudioInputStreamBrokerTest, ObserverDisconnect_CallsDeleter) {
  InSequence seq;
  MockStreamFactory::StreamRequestData stream_request_data(kDeviceId,
                                                           TestParams());
  TestEnvironment env(&stream_request_data);
  env.broker->CreateStream(env.factory_ptr.get());
  env.RunUntilIdle();
  EXPECT_TRUE(stream_request_data.requested);

  EXPECT_CALL(env.deleter, Run(env.broker.release()))
      .WillOnce(testing::DeleteArg<0>());
  stream_request_data.observer.reset();
  env.RunUntilIdle();
  Mock::VerifyAndClear(&env.deleter);

  env.stream_factory.ResetReceiver();
  env.RunUntilIdle();
}

TEST(AudioInputStreamBrokerTest, ObserverDisconnect_PropagateError) {
  InSequence seq;
  MockStreamFactory::StreamRequestData stream_request_data(kDeviceId,
                                                           TestParams());
  TestEnvironment env(&stream_request_data);
  env.broker->CreateStream(env.factory_ptr.get());
  env.RunUntilIdle();
  EXPECT_TRUE(stream_request_data.requested);

  ExpectRendererFactoryClientDisconnection(
      env, media::mojom::InputStreamErrorCode::kUnknown);

  EXPECT_CALL(env.deleter, Run(env.broker.release()))
      .WillOnce(testing::DeleteArg<0>());
  stream_request_data.observer.ResetWithReason(
      static_cast<uint32_t>(
          media::mojom::AudioInputStreamObserver::DisconnectReason::kDefault),
      std::string());
  env.RunUntilIdle();
  Mock::VerifyAndClear(&env.deleter);

  env.stream_factory.ResetReceiver();
  env.RunUntilIdle();
}

TEST(AudioInputStreamBrokerTest, ObserverDisconnect_PropagatePermissionsError) {
  InSequence seq;
  MockStreamFactory::StreamRequestData stream_request_data(kDeviceId,
                                                           TestParams());
  TestEnvironment env(&stream_request_data);
  env.broker->CreateStream(env.factory_ptr.get());
  env.RunUntilIdle();
  EXPECT_TRUE(stream_request_data.requested);

  ExpectRendererFactoryClientDisconnection(
      env, media::mojom::InputStreamErrorCode::kSystemPermissions);

  EXPECT_CALL(env.deleter, Run(env.broker.release()))
      .WillOnce(testing::DeleteArg<0>());
  stream_request_data.observer.ResetWithReason(
      static_cast<uint32_t>(media::mojom::AudioInputStreamObserver::
                                DisconnectReason::kSystemPermissions),
      std::string());
  env.RunUntilIdle();
  Mock::VerifyAndClear(&env.deleter);

  env.stream_factory.ResetReceiver();
  env.RunUntilIdle();
}

TEST(AudioInputStreamBrokerTest,
     FactoryDisconnectDuringConstruction_CallsDeleter) {
  TestEnvironment env(nullptr);
  env.broker->CreateStream(env.factory_ptr.get());
  env.stream_factory.ResetReceiver();

  EXPECT_CALL(env.deleter, Run(env.broker.release()))
      .WillOnce(testing::DeleteArg<0>());

  env.RunUntilIdle();
}

}  // namespace content
