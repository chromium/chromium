// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_loopback_stream_broker.h"

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

using ::testing::_;
using ::testing::Test;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::StrictMock;
using ::testing::InSequence;

namespace content {

namespace {

const int kRenderProcessId = 123;
const int kRenderFrameId = 234;
const uint32_t kShMemCount = 10;

media::AudioParameters TestParams() {
  return media::AudioParameters::UnavailableDeviceParams();
}

class MockSource : public AudioStreamBroker::LoopbackSource {
 public:
  MockSource() : group_id_(base::UnguessableToken::Create()) {}
  ~MockSource() override {}

  // AudioStreamBrokerFactory::LoopbackSource mocking.
  const base::UnguessableToken& GetGroupID() override { return group_id_; }
  MOCK_METHOD1(AddLoopbackSink, void(AudioStreamBroker::LoopbackSink*));
  MOCK_METHOD1(RemoveLoopbackSink, void(AudioStreamBroker::LoopbackSink*));

 private:
  base::UnguessableToken group_id_;
  DISALLOW_COPY_AND_ASSIGN(MockSource);
};

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
    // Loopback streams have no stream ids.
    EXPECT_FALSE(stream_id.has_value());
    input_stream_ = std::move(input_stream);
    client_request_ = std::move(client_request);
    OnStreamCreated();
  }

  void CloseBinding() { binding_.Close(); }

 private:
  mojo::Binding<mojom::RendererAudioInputStreamFactoryClient> binding_;
  media::mojom::AudioInputStreamPtr input_stream_;
  media::mojom::AudioInputStreamClientRequest client_request_;
};

class MockStreamFactory : public audio::FakeStreamFactory {
 public:
  MockStreamFactory() {}
  ~MockStreamFactory() final {}

  // State of an expected stream creation. |device_id| and |params| are set
  // ahead of time and verified during request. The other fields are filled in
  // when the request is received.
  struct StreamRequestData {
    StreamRequestData(const base::UnguessableToken& group_id,
                      const media::AudioParameters& params)
        : params(params), group_id(group_id) {}

    bool requested = false;
    media::mojom::AudioInputStreamRequest stream_request;
    media::mojom::AudioInputStreamClientPtr client;
    media::mojom::AudioInputStreamObserverPtr observer;
    const media::AudioParameters params;
    uint32_t shared_memory_count;
    base::UnguessableToken group_id;
    mojo::ScopedSharedBufferHandle key_press_count_buffer;
    CreateLoopbackStreamCallback created_callback;
    audio::mojom::LocalMuterAssociatedRequest muter_request;
  };

  void ExpectStreamCreation(StreamRequestData* ex) {
    stream_request_data_ = ex;
  }

  MOCK_METHOD1(IsMuting, void(const base::UnguessableToken&));

 private:
  void CreateLoopbackStream(
      media::mojom::AudioInputStreamRequest stream_request,
      media::mojom::AudioInputStreamClientPtr client,
      media::mojom::AudioInputStreamObserverPtr observer,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      const base::UnguessableToken& group_id,
      CreateLoopbackStreamCallback created_callback) final {
    // No way to cleanly exit the test here in case of failure, so use CHECK.
    CHECK(stream_request_data_);
    EXPECT_EQ(stream_request_data_->group_id, group_id);
    EXPECT_TRUE(stream_request_data_->params.Equals(params));
    stream_request_data_->requested = true;
    stream_request_data_->stream_request = std::move(stream_request);
    stream_request_data_->client = std::move(client);
    stream_request_data_->observer = std::move(observer);
    stream_request_data_->shared_memory_count = shared_memory_count;
    stream_request_data_->created_callback = std::move(created_callback);
  }

  void BindMuter(audio::mojom::LocalMuterAssociatedRequest request,
                 const base::UnguessableToken& group_id) final {
    stream_request_data_->muter_request = std::move(request);
    IsMuting(group_id);
  }

  StreamRequestData* stream_request_data_;

  DISALLOW_COPY_AND_ASSIGN(MockStreamFactory);
};

const bool kMuteSource = true;

struct TestEnvironment {
  TestEnvironment(const base::UnguessableToken& group_id, bool mute_source) {
    // Muting should not start until CreateStream() is called.
    EXPECT_CALL(stream_factory, IsMuting(_)).Times(0);
    EXPECT_CALL(source, AddLoopbackSink(_));
    broker = std::make_unique<AudioLoopbackStreamBroker>(
        kRenderProcessId, kRenderFrameId, &source, TestParams(), kShMemCount,
        mute_source, deleter.Get(), renderer_factory_client.MakePtr());
  }

  void RunUntilIdle() { thread_bundle.RunUntilIdle(); }

  TestBrowserThreadBundle thread_bundle;
  MockDeleterCallback deleter;
  MockSource source;
  StrictMock<MockRendererAudioInputStreamFactoryClient> renderer_factory_client;
  std::unique_ptr<AudioLoopbackStreamBroker> broker;
  MockStreamFactory stream_factory;
  audio::mojom::StreamFactoryPtr factory_ptr = stream_factory.MakePtr();
};

}  // namespace

TEST(AudioLoopbackStreamBrokerTest, StoresProcessAndFrameId) {
  InSequence seq;
  TestBrowserThreadBundle thread_bundle;
  MockDeleterCallback deleter;
  StrictMock<MockRendererAudioInputStreamFactoryClient> renderer_factory_client;
  MockSource source;

  EXPECT_CALL(source, AddLoopbackSink(_));

  AudioLoopbackStreamBroker broker(
      kRenderProcessId, kRenderFrameId, &source, TestParams(), kShMemCount,
      !kMuteSource, deleter.Get(), renderer_factory_client.MakePtr());

  EXPECT_EQ(kRenderProcessId, broker.render_process_id());
  EXPECT_EQ(kRenderFrameId, broker.render_frame_id());

  EXPECT_CALL(source, RemoveLoopbackSink(&broker));
}

TEST(AudioLoopbackStreamBrokerTest, StreamCreationSuccess_Propagates) {
  TestEnvironment env(base::UnguessableToken::Create(), !kMuteSource);
  MockStreamFactory::StreamRequestData stream_request_data(
      env.source.GetGroupID(), TestParams());
  env.stream_factory.ExpectStreamCreation(&stream_request_data);

  EXPECT_CALL(env.stream_factory, IsMuting(_)).Times(0);

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
            mojo::WrapPlatformFile(socket1.Release())});

  EXPECT_CALL(env.renderer_factory_client, OnStreamCreated());

  env.RunUntilIdle();

  Mock::VerifyAndClear(&env.renderer_factory_client);
  env.broker.reset();
}

TEST(AudioLoopbackStreamBrokerTest, MutedStreamCreation_Mutes) {
  TestEnvironment env(base::UnguessableToken::Create(), kMuteSource);
  MockStreamFactory::StreamRequestData stream_request_data(
      env.source.GetGroupID(), TestParams());
  env.stream_factory.ExpectStreamCreation(&stream_request_data);

  EXPECT_CALL(env.stream_factory, IsMuting(env.source.GetGroupID()));

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
            mojo::WrapPlatformFile(socket1.Release())});

  EXPECT_CALL(env.renderer_factory_client, OnStreamCreated());

  env.RunUntilIdle();

  Mock::VerifyAndClear(&env.renderer_factory_client);
  env.broker.reset();
}

TEST(AudioLoopbackStreamBrokerTest, SourceGone_CallsDeleter) {
  TestEnvironment env(base::UnguessableToken::Create(), kMuteSource);
  MockStreamFactory::StreamRequestData stream_request_data(
      env.source.GetGroupID(), TestParams());
  env.stream_factory.ExpectStreamCreation(&stream_request_data);

  EXPECT_CALL(env.stream_factory, IsMuting(env.source.GetGroupID()));

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
            mojo::WrapPlatformFile(socket1.Release())});

  EXPECT_CALL(env.renderer_factory_client, OnStreamCreated());

  env.RunUntilIdle();

  Mock::VerifyAndClear(&env.renderer_factory_client);

  EXPECT_CALL(env.deleter, Run(env.broker.get()));

  // Accessing source is not allowed after OnSourceGone.
  EXPECT_CALL(env.source, RemoveLoopbackSink(_)).Times(0);

  env.broker->OnSourceGone();

  env.RunUntilIdle();
}

TEST(AudioLoopbackStreamBrokerTest, StreamCreationFailure_CallsDeleter) {
  TestEnvironment env(base::UnguessableToken::Create(), !kMuteSource);
  MockStreamFactory::StreamRequestData stream_request_data(
      env.source.GetGroupID(), TestParams());
  env.stream_factory.ExpectStreamCreation(&stream_request_data);

  EXPECT_CALL(env.stream_factory, IsMuting(_)).Times(0);

  env.broker->CreateStream(env.factory_ptr.get());
  env.RunUntilIdle();

  EXPECT_TRUE(stream_request_data.requested);
  EXPECT_CALL(env.deleter, Run(env.broker.release()))
      .WillOnce(testing::DeleteArg<0>());

  std::move(stream_request_data.created_callback).Run(nullptr);

  env.RunUntilIdle();
}

TEST(AudioLoopbackStreamBrokerTest,
     RendererFactoryClientDisconnect_CallsDeleter) {
  TestEnvironment env(base::UnguessableToken::Create(), !kMuteSource);
  MockStreamFactory::StreamRequestData stream_request_data(
      env.source.GetGroupID(), TestParams());
  env.stream_factory.ExpectStreamCreation(&stream_request_data);

  EXPECT_CALL(env.stream_factory, IsMuting(_)).Times(0);

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

TEST(AudioLoopbackStreamBrokerTest, ObserverDisconnect_CallsDeleter) {
  TestEnvironment env(base::UnguessableToken::Create(), !kMuteSource);
  MockStreamFactory::StreamRequestData stream_request_data(
      env.source.GetGroupID(), TestParams());
  env.stream_factory.ExpectStreamCreation(&stream_request_data);

  EXPECT_CALL(env.stream_factory, IsMuting(_)).Times(0);

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

TEST(AudioLoopbackStreamBrokerTest,
     FactoryDisconnectDuringConstruction_CallsDeleter) {
  TestEnvironment env(base::UnguessableToken::Create(), !kMuteSource);
  env.broker->CreateStream(env.factory_ptr.get());
  env.stream_factory.CloseBinding();

  EXPECT_CALL(env.deleter, Run(env.broker.release()))
      .WillOnce(testing::DeleteArg<0>());

  env.RunUntilIdle();
}

}  // namespace content
