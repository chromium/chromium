// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_input_stream_broker.h"

#include <memory>
#include <utility>

#include "base/sync_socket.h"
#include "base/test/mock_callback.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/mojo/interfaces/audio_input_stream.mojom.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Test;
using ::testing::Mock;
using ::testing::StrictMock;
using ::testing::InSequence;

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
    : public mojom::RendererAudioInputStreamFactoryClient {
 public:
  MockRendererAudioInputStreamFactoryClient() : binding_(this) {}
  ~MockRendererAudioInputStreamFactoryClient() override {}

  mojom::RendererAudioInputStreamFactoryClientPtr MakePtr() {
    mojom::RendererAudioInputStreamFactoryClientPtr ret;
    binding_.Bind(mojo::MakeRequest(&ret));
    return ret;
  }

  MOCK_METHOD0(OnStreamCreated, void());

  void StreamCreated(
      media::mojom::AudioInputStreamPtr input_stream,
      media::mojom::AudioInputStreamClientRequest client_request,
      media::mojom::ReadOnlyAudioDataPipePtr data_pipe,
      bool initially_muted,
      const base::Optional<base::UnguessableToken>& stream_id) override {
    EXPECT_TRUE(stream_id.has_value());
    input_stream_ = std::move(input_stream);
    client_request_ = std::move(client_request);
    OnStreamCreated();
  }

  void CloseBinding() { binding_.Close(); }

 private:
  mojo::Binding<mojom::RendererAudioInputStreamFactoryClient> binding_;
  media::mojom::AudioInputStreamPtr input_stream_;
  media::mojom::AudioInputStreamClientRequest client_request_;
  DISALLOW_COPY_AND_ASSIGN(MockRendererAudioInputStreamFactoryClient);
};

class MockStreamFactory : public audio::FakeStreamFactory {
 public:
  MockStreamFactory() {}
  ~MockStreamFactory() final {}

  // State of an expected stream creation. |device_id| and |params| are set
  // ahead of time and verified during request. The other fields are filled in
  // when the request is received.
  struct StreamRequestData {
    StreamRequestData(const std::string& device_id,
                      const media::AudioParameters& params)
        : device_id(device_id), params(params) {}

    bool requested = false;
    media::mojom::AudioInputStreamRequest stream_request;
    media::mojom::AudioInputStreamClientPtr client;
    media::mojom::AudioInputStreamObserverPtr observer;
    media::mojom::AudioLogPtr log;
    const std::string device_id;
    const media::AudioParameters params;
    uint32_t shared_memory_count;
    bool enable_agc;
    mojo::ScopedSharedBufferHandle key_press_count_buffer;
    CreateInputStreamCallback created_callback;
  };

  void ExpectStreamCreation(StreamRequestData* ex) {
    stream_request_data_ = ex;
  }

 private:
  void CreateInputStream(
      media::mojom::AudioInputStreamRequest stream_request,
      media::mojom::AudioInputStreamClientPtr client,
      media::mojom::AudioInputStreamObserverPtr observer,
      media::mojom::AudioLogPtr log,
      const std::string& device_id,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      bool enable_agc,
      mojo::ScopedSharedBufferHandle key_press_count_buffer,
      audio::mojom::AudioProcessingConfigPtr processing_config,
      CreateInputStreamCallback created_callback) final {
    // No way to cleanly exit the test here in case of failure, so use CHECK.
    CHECK(stream_request_data_);
    EXPECT_EQ(stream_request_data_->device_id, device_id);
    EXPECT_TRUE(stream_request_data_->params.Equals(params));
    stream_request_data_->requested = true;
    stream_request_data_->stream_request = std::move(stream_request);
    stream_request_data_->client = std::move(client);
    stream_request_data_->observer = std::move(observer);
    stream_request_data_->log = std::move(log);
    stream_request_data_->shared_memory_count = shared_memory_count;
    stream_request_data_->enable_agc = enable_agc;
    stream_request_data_->key_press_count_buffer =
        std::move(key_press_count_buffer);
    stream_request_data_->created_callback = std::move(created_callback);
  }

  StreamRequestData* stream_request_data_;
  DISALLOW_COPY_AND_ASSIGN(MockStreamFactory);
};

struct TestEnvironment {
  TestEnvironment()
      : broker(std::make_unique<AudioInputStreamBroker>(
            kRenderProcessId,
            kRenderFrameId,
            kDeviceId,
            TestParams(),
            kShMemCount,
            nullptr /*user_input_monitor*/,
            kEnableAgc,
            nullptr,
            deleter.Get(),
            renderer_factory_client.MakePtr())) {}

  void RunUntilIdle() { thread_bundle.RunUntilIdle(); }

  TestBrowserThreadBundle thread_bundle;
  MockDeleterCallback deleter;
  StrictMock<MockRendererAudioInputStreamFactoryClient> renderer_factory_client;
  std::unique_ptr<AudioInputStreamBroker> broker;
  MockStreamFactory stream_factory;
  audio::mojom::StreamFactoryPtr factory_ptr = stream_factory.MakePtr();
};

}  // namespace

TEST(AudioInputStreamBrokerTest, StoresProcessAndFrameId) {
  TestBrowserThreadBundle thread_bundle;
  MockDeleterCallback deleter;
  StrictMock<MockRendererAudioInputStreamFactoryClient> renderer_factory_client;

  AudioInputStreamBroker broker(
      kRenderProcessId, kRenderFrameId, kDeviceId, TestParams(), kShMemCount,
      nullptr /*user_input_monitor*/, kEnableAgc, nullptr, deleter.Get(),
      renderer_factory_client.MakePtr());

  EXPECT_EQ(kRenderProcessId, broker.render_process_id());
  EXPECT_EQ(kRenderFrameId, broker.render_frame_id());
}

TEST(AudioInputStreamBrokerTest, StreamCreationSuccess_Propagates) {
  TestEnvironment env;
  MockStreamFactory::StreamRequestData stream_request_data(kDeviceId,
                                                           TestParams());
  env.stream_factory.ExpectStreamCreation(&stream_request_data);

  env.broker->CreateStream(env.factory_ptr.get());
  env.RunUntilIdle();

  EXPECT_TRUE(stream_request_data.requested);

  // Set up test IPC primitives.
  const size_t shmem_size = 456;
  base::SyncSocket socket1, socket2;
  base::SyncSocket::CreatePair(&socket1, &socket2);
  std::move(stream_request_data.created_callback)
      .Run({base::in_place,
            base::ReadOnlySharedMemoryRegion::Create(shmem_size).region,
            mojo::WrapPlatformFile(socket1.Release())},
           kInitiallyMuted, base::UnguessableToken::Create());

  EXPECT_CALL(env.renderer_factory_client, OnStreamCreated());

  env.RunUntilIdle();

  Mock::VerifyAndClear(&env.renderer_factory_client);

  env.broker.reset();
}

TEST(AudioInputStreamBrokerTest, StreamCreationFailure_CallsDeleter) {
  TestEnvironment env;
  MockStreamFactory::StreamRequestData stream_request_data(kDeviceId,
                                                           TestParams());
  env.stream_factory.ExpectStreamCreation(&stream_request_data);

  env.broker->CreateStream(env.factory_ptr.get());
  env.RunUntilIdle();

  EXPECT_TRUE(stream_request_data.requested);
  EXPECT_CALL(env.deleter, Run(env.broker.release()))
      .WillOnce(testing::DeleteArg<0>());

  std::move(stream_request_data.created_callback)
      .Run(nullptr, kInitiallyMuted, base::nullopt);

  env.RunUntilIdle();
}

TEST(AudioInputStreamBrokerTest, RendererFactoryClientDisconnect_CallsDeleter) {
  InSequence seq;
  TestEnvironment env;
  MockStreamFactory::StreamRequestData stream_request_data(kDeviceId,
                                                           TestParams());
  env.stream_factory.ExpectStreamCreation(&stream_request_data);

  env.broker->CreateStream(env.factory_ptr.get());
  env.RunUntilIdle();
  EXPECT_TRUE(stream_request_data.requested);

  EXPECT_CALL(env.deleter, Run(env.broker.release()))
      .WillOnce(testing::DeleteArg<0>());
  env.renderer_factory_client.CloseBinding();
  env.RunUntilIdle();
  Mock::VerifyAndClear(&env.deleter);

  env.stream_factory.CloseBinding();
  env.RunUntilIdle();
}

TEST(AudioInputStreamBrokerTest, ObserverDisconnect_CallsDeleter) {
  InSequence seq;
  TestEnvironment env;
  MockStreamFactory::StreamRequestData stream_request_data(kDeviceId,
                                                           TestParams());
  env.stream_factory.ExpectStreamCreation(&stream_request_data);

  env.broker->CreateStream(env.factory_ptr.get());
  env.RunUntilIdle();
  EXPECT_TRUE(stream_request_data.requested);

  EXPECT_CALL(env.deleter, Run(env.broker.release()))
      .WillOnce(testing::DeleteArg<0>());
  stream_request_data.observer.reset();
  env.RunUntilIdle();
  Mock::VerifyAndClear(&env.deleter);

  env.stream_factory.CloseBinding();
  env.RunUntilIdle();
}

TEST(AudioInputStreamBrokerTest,
     FactoryDisconnectDuringConstruction_CallsDeleter) {
  TestEnvironment env;

  env.broker->CreateStream(env.factory_ptr.get());
  env.stream_factory.CloseBinding();

  EXPECT_CALL(env.deleter, Run(env.broker.release()))
      .WillOnce(testing::DeleteArg<0>());

  env.RunUntilIdle();
}

}  // namespace content
