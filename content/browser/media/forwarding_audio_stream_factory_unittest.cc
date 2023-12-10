// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/forwarding_audio_stream_factory.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/unguessable_token.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "media/mojo/mojom/audio_processing.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/media/renderer_audio_input_stream_factory.mojom.h"

using ::testing::InSequence;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::StrictMock;
using ::testing::Test;

namespace content {

namespace {

using AudioProcessingConfigPtr = media::mojom::AudioProcessingConfigPtr;

class MockStreamFactory final : public audio::FakeStreamFactory,
                                public media::mojom::LocalMuter {
 public:
  MockStreamFactory() = default;

  MockStreamFactory(const MockStreamFactory&) = delete;
  MockStreamFactory& operator=(const MockStreamFactory&) = delete;

  ~MockStreamFactory() override = default;

  bool IsConnected() const { return receiver_.is_bound(); }
  bool IsMuterConnected() const { return muter_receiver_.is_bound(); }

 private:
  void BindMuter(
      mojo::PendingAssociatedReceiver<media::mojom::LocalMuter> receiver,
      const base::UnguessableToken& group_id) override {
    muter_receiver_.Bind(std::move(receiver));
    muter_receiver_.set_disconnect_handler(base::BindOnce(
        &MockStreamFactory::MuterDisconnected, base::Unretained(this)));
  }
  void MuterDisconnected() { muter_receiver_.reset(); }

  mojo::AssociatedReceiver<media::mojom::LocalMuter> muter_receiver_{this};
};

class MockBroker : public AudioStreamBroker {
 public:
  explicit MockBroker(RenderFrameHost* rfh)
      : AudioStreamBroker(rfh->GetProcess()->GetID(), rfh->GetRoutingID()) {}

  MockBroker(const MockBroker&) = delete;
  MockBroker& operator=(const MockBroker&) = delete;

  ~MockBroker() override {}

  MOCK_METHOD1(CreateStream, void(media::mojom::AudioStreamFactory* factory));

  // Can be used to verify that |this| has been destructed.
  base::WeakPtr<MockBroker> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

  DeleterCallback deleter;
  media::mojom::AudioProcessingConfigPtr config_ptr;

 private:
  base::WeakPtrFactory<MockBroker> weak_factory_{this};
};

class MockBrokerFactory final : public AudioStreamBrokerFactory {
 public:
  MockBrokerFactory() = default;

  MockBrokerFactory(const MockBrokerFactory&) = delete;
  MockBrokerFactory& operator=(const MockBrokerFactory&) = delete;

  ~MockBrokerFactory() override {
    EXPECT_TRUE(prepared_input_stream_brokers_.empty())
        << "Input broker creation was expected but didn't happen";
    EXPECT_TRUE(prepared_output_stream_brokers_.empty())
        << "Output broker creation was expected but didn't happen";
  }

  void ExpectInputStreamBrokerCreation(std::unique_ptr<MockBroker> broker) {
    prepared_input_stream_brokers_.push(std::move(broker));
  }

  void ExpectOutputStreamBrokerCreation(std::unique_ptr<MockBroker> broker) {
    prepared_output_stream_brokers_.push(std::move(broker));
  }

  void ExpectLoopbackStreamBrokerCreation(std::unique_ptr<MockBroker> broker) {
    prepared_loopback_stream_brokers_.push(std::move(broker));
  }

  std::unique_ptr<AudioStreamBroker> CreateAudioInputStreamBroker(
      int render_process_id,
      int render_frame_id,
      const std::string& device_id,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      media::UserInputMonitorBase* user_input_monitor,
      bool enable_agc,
      media::mojom::AudioProcessingConfigPtr processing_config,
      AudioStreamBroker::DeleterCallback deleter,
      mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
          renderer_factory_client) override {
    std::unique_ptr<MockBroker> prepared_broker =
        std::move(prepared_input_stream_brokers_.front());
    prepared_input_stream_brokers_.pop();
    CHECK_NE(nullptr, prepared_broker.get());
    EXPECT_EQ(render_process_id, prepared_broker->render_process_id());
    EXPECT_EQ(render_frame_id, prepared_broker->render_frame_id());
    prepared_broker->deleter = std::move(deleter);
    prepared_broker->config_ptr = std::move(processing_config);
    return std::move(prepared_broker);
  }

  std::unique_ptr<AudioStreamBroker> CreateAudioOutputStreamBroker(
      int render_process_id,
      int render_frame_id,
      int stream_id,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      AudioStreamBroker::DeleterCallback deleter,
      mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client)
      override {
    std::unique_ptr<MockBroker> prepared_broker =
        std::move(prepared_output_stream_brokers_.front());
    prepared_output_stream_brokers_.pop();
    CHECK_NE(nullptr, prepared_broker.get());
    EXPECT_EQ(render_process_id, prepared_broker->render_process_id());
    EXPECT_EQ(render_frame_id, prepared_broker->render_frame_id());
    prepared_broker->deleter = std::move(deleter);
    return std::move(prepared_broker);
  }

  std::unique_ptr<AudioStreamBroker> CreateAudioLoopbackStreamBroker(
      int render_process_id,
      int render_frame_id,
      AudioStreamBroker::LoopbackSource* source,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      bool mute_source,
      AudioStreamBroker::DeleterCallback deleter,
      mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
          renderer_factory_client) override {
    std::unique_ptr<MockBroker> prepared_broker =
        std::move(prepared_loopback_stream_brokers_.front());
    prepared_loopback_stream_brokers_.pop();
    CHECK_NE(nullptr, prepared_broker.get());
    EXPECT_EQ(render_process_id, prepared_broker->render_process_id());
    EXPECT_EQ(render_frame_id, prepared_broker->render_frame_id());
    prepared_broker->deleter = std::move(deleter);
    return std::move(prepared_broker);
  }

 private:
  base::queue<std::unique_ptr<MockBroker>> prepared_loopback_stream_brokers_;
  base::queue<std::unique_ptr<MockBroker>> prepared_input_stream_brokers_;
  base::queue<std::unique_ptr<MockBroker>> prepared_output_stream_brokers_;
};

class MockLoopbackSink : public AudioStreamBroker::LoopbackSink {
 public:
  MockLoopbackSink() {}

  MockLoopbackSink(const MockLoopbackSink&) = delete;
  MockLoopbackSink& operator=(const MockLoopbackSink&) = delete;

  ~MockLoopbackSink() override {}

  MOCK_METHOD0(OnSourceGone, void());
};

class ForwardingAudioStreamFactoryTest : public RenderViewHostTestHarness {
 public:
  ForwardingAudioStreamFactoryTest()
      : broker_factory_(std::make_unique<MockBrokerFactory>()) {
    ForwardingAudioStreamFactory::OverrideAudioStreamFactoryBinderForTesting(
        base::BindRepeating(&ForwardingAudioStreamFactoryTest::BindFactory,
                            base::Unretained(this)));
  }

  ~ForwardingAudioStreamFactoryTest() override {
    ForwardingAudioStreamFactory::OverrideAudioStreamFactoryBinderForTesting(
        base::NullCallback());
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    RenderFrameHostTester::For(main_rfh())->InitializeRenderFrameIfNeeded();
    other_rfh_ =
        RenderFrameHostTester::For(main_rfh())->AppendChild("other_rfh");
  }

  void TearDown() override {
    other_rfh_ = nullptr;
    RenderViewHostTestHarness::TearDown();
  }
  void BindFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
    stream_factory_.receiver_.Bind(std::move(receiver));
    stream_factory_.receiver_.set_disconnect_handler(
        base::BindOnce(&audio::FakeStreamFactory::ResetReceiver,
                       base::Unretained(&stream_factory_)));
  }

  base::WeakPtr<MockBroker> ExpectLoopbackBrokerConstruction(
      RenderFrameHost* rfh) {
    auto broker = std::make_unique<StrictMock<MockBroker>>(rfh);
    auto weak_broker = broker->GetWeakPtr();
    broker_factory_->ExpectLoopbackStreamBrokerCreation(std::move(broker));
    return weak_broker;
  }

  base::WeakPtr<MockBroker> ExpectInputBrokerConstruction(
      RenderFrameHost* rfh) {
    auto broker = std::make_unique<StrictMock<MockBroker>>(rfh);
    auto weak_broker = broker->GetWeakPtr();
    broker_factory_->ExpectInputStreamBrokerCreation(std::move(broker));
    return weak_broker;
  }

  base::WeakPtr<MockBroker> ExpectOutputBrokerConstruction(
      RenderFrameHost* rfh) {
    auto broker = std::make_unique<StrictMock<MockBroker>>(rfh);
    auto weak_broker = broker->GetWeakPtr();
    broker_factory_->ExpectOutputStreamBrokerCreation(std::move(broker));
    return weak_broker;
  }

  RenderFrameHost* other_rfh() { return other_rfh_; }

  static const char kInputDeviceId[];
  static const char kOutputDeviceId[];
  static const media::AudioParameters kParams;
  static const uint32_t kSharedMemoryCount;
  static const bool kEnableAgc;
  MockStreamFactory stream_factory_;
  raw_ptr<RenderFrameHost> other_rfh_ = nullptr;
  std::unique_ptr<MockBrokerFactory> broker_factory_;
};

const char ForwardingAudioStreamFactoryTest::kInputDeviceId[] =
    "test input device id";
const char ForwardingAudioStreamFactoryTest::kOutputDeviceId[] =
    "test output device id";
const media::AudioParameters ForwardingAudioStreamFactoryTest::kParams =
    media::AudioParameters::UnavailableDeviceParams();
const uint32_t ForwardingAudioStreamFactoryTest::kSharedMemoryCount = 10;
const bool ForwardingAudioStreamFactoryTest::kEnableAgc = false;
const bool kMuteSource = true;

}  // namespace

TEST_F(ForwardingAudioStreamFactoryTest, CreateInputStream_CreatesInputStream) {
  mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
      client;
  base::WeakPtr<MockBroker> broker = ExpectInputBrokerConstruction(main_rfh());

  ForwardingAudioStreamFactory factory(web_contents(),
                                       nullptr /*user_input_monitor*/,
                                       std::move(broker_factory_));

  auto config_ptr = media::mojom::AudioProcessingConfig::New(
      mojo::NullReceiver(), media::AudioProcessingSettings());

  EXPECT_CALL(*broker, CreateStream(NotNull()));
  std::ignore = client.InitWithNewPipeAndPassReceiver();
  factory.core()->CreateInputStream(main_rfh()->GetProcess()->GetID(),
                                    main_rfh()->GetRoutingID(), kInputDeviceId,
                                    kParams, kSharedMemoryCount, kEnableAgc,
                                    std::move(config_ptr), std::move(client));

  EXPECT_TRUE(broker->config_ptr);
}

TEST_F(ForwardingAudioStreamFactoryTest,
       CreateLoopbackStream_CreatesLoopbackStream) {
  std::unique_ptr<WebContents> source_contents = CreateTestWebContents();
  mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
      client;
  base::WeakPtr<MockBroker> broker =
      ExpectLoopbackBrokerConstruction(main_rfh());

  ForwardingAudioStreamFactory factory(web_contents(),
                                       nullptr /*user_input_monitor*/,
                                       std::move(broker_factory_));

  ForwardingAudioStreamFactory source_factory(
      source_contents.get(), nullptr /*user_input_monitor*/,
      std::make_unique<MockBrokerFactory>());

  EXPECT_CALL(*broker, CreateStream(NotNull()));
  std::ignore = client.InitWithNewPipeAndPassReceiver();
  factory.core()->CreateLoopbackStream(
      main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
      source_factory.core(), kParams, kSharedMemoryCount, kMuteSource,
      std::move(client));
}

TEST_F(ForwardingAudioStreamFactoryTest,
       CreateOutputStream_CreatesOutputStream) {
  mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client;
  base::WeakPtr<MockBroker> broker = ExpectOutputBrokerConstruction(main_rfh());

  ForwardingAudioStreamFactory factory(web_contents(),
                                       nullptr /*user_input_monitor*/,
                                       std::move(broker_factory_));

  EXPECT_CALL(*broker, CreateStream(NotNull()));
  std::ignore = client.InitWithNewPipeAndPassReceiver();
  factory.core()->CreateOutputStream(
      main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
      kOutputDeviceId, kParams, std::move(client));
}

TEST_F(ForwardingAudioStreamFactoryTest,
       InputBrokerDeleterCalled_DestroysInputStream) {
  base::WeakPtr<MockBroker> main_rfh_broker =
      ExpectInputBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> other_rfh_broker =
      ExpectInputBrokerConstruction(other_rfh());

  ForwardingAudioStreamFactory factory(web_contents(),
                                       nullptr /*user_input_monitor*/,
                                       std::move(broker_factory_));

  {
    EXPECT_CALL(*main_rfh_broker, CreateStream(NotNull()));
    mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
        client;
    std::ignore = client.InitWithNewPipeAndPassReceiver();
    factory.core()->CreateInputStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        kInputDeviceId, kParams, kSharedMemoryCount, kEnableAgc,
        AudioProcessingConfigPtr(), std::move(client));
    testing::Mock::VerifyAndClear(&*main_rfh_broker);
  }
  {
    EXPECT_CALL(*other_rfh_broker, CreateStream(NotNull()));
    mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
        client;
    std::ignore = client.InitWithNewPipeAndPassReceiver();
    factory.core()->CreateInputStream(
        other_rfh()->GetProcess()->GetID(), other_rfh()->GetRoutingID(),
        kInputDeviceId, kParams, kSharedMemoryCount, kEnableAgc,
        AudioProcessingConfigPtr(), std::move(client));
    testing::Mock::VerifyAndClear(&*other_rfh_broker);
  }

  std::move(other_rfh_broker->deleter).Run(&*other_rfh_broker);
  EXPECT_FALSE(other_rfh_broker)
      << "Input broker should be destructed when deleter is called.";
  EXPECT_TRUE(main_rfh_broker);
}

TEST_F(ForwardingAudioStreamFactoryTest,
       LoopbackBrokerDeleterCalled_DestroysInputStream) {
  std::unique_ptr<WebContents> source_contents = CreateTestWebContents();
  base::WeakPtr<MockBroker> main_rfh_broker =
      ExpectLoopbackBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> other_rfh_broker =
      ExpectLoopbackBrokerConstruction(other_rfh());

  ForwardingAudioStreamFactory factory(web_contents(),
                                       nullptr /*user_input_monitor*/,
                                       std::move(broker_factory_));

  ForwardingAudioStreamFactory source_factory(
      source_contents.get(), nullptr /*user_input_monitor*/,
      std::make_unique<MockBrokerFactory>());

  {
    EXPECT_CALL(*main_rfh_broker, CreateStream(NotNull()));
    mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
        client;
    std::ignore = client.InitWithNewPipeAndPassReceiver();
    factory.core()->CreateLoopbackStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        source_factory.core(), kParams, kSharedMemoryCount, kMuteSource,
        std::move(client));
    testing::Mock::VerifyAndClear(&*main_rfh_broker);
  }
  {
    EXPECT_CALL(*other_rfh_broker, CreateStream(NotNull()));
    mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
        client;
    std::ignore = client.InitWithNewPipeAndPassReceiver();
    factory.core()->CreateLoopbackStream(
        other_rfh()->GetProcess()->GetID(), other_rfh()->GetRoutingID(),
        source_factory.core(), kParams, kSharedMemoryCount, kMuteSource,
        std::move(client));
    testing::Mock::VerifyAndClear(&*other_rfh_broker);
  }

  std::move(other_rfh_broker->deleter).Run(&*other_rfh_broker);
  EXPECT_FALSE(other_rfh_broker)
      << "Loopback broker should be destructed when deleter is called.";
  EXPECT_TRUE(main_rfh_broker);
}

TEST_F(ForwardingAudioStreamFactoryTest,
       OutputBrokerDeleterCalled_DestroysOutputStream) {
  base::WeakPtr<MockBroker> main_rfh_broker =
      ExpectOutputBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> other_rfh_broker =
      ExpectOutputBrokerConstruction(other_rfh());

  ForwardingAudioStreamFactory factory(web_contents(),
                                       nullptr /*user_input_monitor*/,
                                       std::move(broker_factory_));

  {
    EXPECT_CALL(*main_rfh_broker, CreateStream(NotNull()));
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client;
    std::ignore = client.InitWithNewPipeAndPassReceiver();
    factory.core()->CreateOutputStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        kOutputDeviceId, kParams, std::move(client));
    testing::Mock::VerifyAndClear(&*main_rfh_broker);
  }
  {
    EXPECT_CALL(*other_rfh_broker, CreateStream(NotNull()));
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client;
    std::ignore = client.InitWithNewPipeAndPassReceiver();
    factory.core()->CreateOutputStream(
        other_rfh()->GetProcess()->GetID(), other_rfh()->GetRoutingID(),
        kOutputDeviceId, kParams, std::move(client));
    testing::Mock::VerifyAndClear(&*other_rfh_broker);
  }

  std::move(other_rfh_broker->deleter).Run(&*other_rfh_broker);
  EXPECT_FALSE(other_rfh_broker)
      << "Output broker should be destructed when deleter is called.";
  EXPECT_TRUE(main_rfh_broker);
}

TEST_F(ForwardingAudioStreamFactoryTest, DestroyFrame_DestroysRelatedStreams) {
  std::unique_ptr<WebContents> source_contents = CreateTestWebContents();

  base::WeakPtr<MockBroker> main_rfh_input_broker =
      ExpectInputBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> other_rfh_input_broker =
      ExpectInputBrokerConstruction(other_rfh());

  base::WeakPtr<MockBroker> main_rfh_loopback_broker =
      ExpectLoopbackBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> other_rfh_loopback_broker =
      ExpectLoopbackBrokerConstruction(other_rfh());

  base::WeakPtr<MockBroker> main_rfh_output_broker =
      ExpectOutputBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> other_rfh_output_broker =
      ExpectOutputBrokerConstruction(other_rfh());

  ForwardingAudioStreamFactory factory(web_contents(),
                                       nullptr /*user_input_monitor*/,
                                       std::move(broker_factory_));

  ForwardingAudioStreamFactory source_factory(
      source_contents.get(), nullptr /*user_input_monitor*/,
      std::make_unique<MockBrokerFactory>());

  {
    EXPECT_CALL(*main_rfh_input_broker, CreateStream(NotNull()));
    mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
        input_client;
    std::ignore = input_client.InitWithNewPipeAndPassReceiver();
    factory.core()->CreateInputStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        kInputDeviceId, kParams, kSharedMemoryCount, kEnableAgc,
        AudioProcessingConfigPtr(), std::move(input_client));
    testing::Mock::VerifyAndClear(&*main_rfh_input_broker);
  }
  {
    EXPECT_CALL(*other_rfh_input_broker, CreateStream(NotNull()));
    mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
        input_client;
    std::ignore = input_client.InitWithNewPipeAndPassReceiver();
    factory.core()->CreateInputStream(
        other_rfh()->GetProcess()->GetID(), other_rfh()->GetRoutingID(),
        kInputDeviceId, kParams, kSharedMemoryCount, kEnableAgc,
        AudioProcessingConfigPtr(), std::move(input_client));
    testing::Mock::VerifyAndClear(&*other_rfh_input_broker);
  }

  {
    EXPECT_CALL(*main_rfh_loopback_broker, CreateStream(NotNull()));
    mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
        input_client;
    std::ignore = input_client.InitWithNewPipeAndPassReceiver();
    factory.core()->CreateLoopbackStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        source_factory.core(), kParams, kSharedMemoryCount, kMuteSource,
        std::move(input_client));
    testing::Mock::VerifyAndClear(&*main_rfh_loopback_broker);
  }
  {
    EXPECT_CALL(*other_rfh_loopback_broker, CreateStream(NotNull()));
    mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
        input_client;
    std::ignore = input_client.InitWithNewPipeAndPassReceiver();
    factory.core()->CreateLoopbackStream(
        other_rfh()->GetProcess()->GetID(), other_rfh()->GetRoutingID(),
        source_factory.core(), kParams, kSharedMemoryCount, kMuteSource,
        std::move(input_client));
    testing::Mock::VerifyAndClear(&*other_rfh_loopback_broker);
  }

  {
    EXPECT_CALL(*main_rfh_output_broker, CreateStream(NotNull()));
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient>
        output_client;
    std::ignore = output_client.InitWithNewPipeAndPassReceiver();
    factory.core()->CreateOutputStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        kOutputDeviceId, kParams, std::move(output_client));
    testing::Mock::VerifyAndClear(&*main_rfh_output_broker);
  }
  {
    EXPECT_CALL(*other_rfh_output_broker, CreateStream(NotNull()));
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient>
        output_client;
    std::ignore = output_client.InitWithNewPipeAndPassReceiver();
    factory.core()->CreateOutputStream(
        other_rfh()->GetProcess()->GetID(), other_rfh()->GetRoutingID(),
        kOutputDeviceId, kParams, std::move(output_client));
    testing::Mock::VerifyAndClear(&*other_rfh_output_broker);
  }

  factory.RenderFrameDeleted(other_rfh());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(other_rfh_input_broker)
      << "Input broker should be destructed when owning frame is destructed.";
  EXPECT_TRUE(main_rfh_input_broker);
  EXPECT_FALSE(other_rfh_loopback_broker) << "Loopback broker should be "
                                             "destructed when owning frame is "
                                             "destructed.";
  EXPECT_TRUE(main_rfh_loopback_broker);
  EXPECT_FALSE(other_rfh_output_broker)
      << "Output broker should be destructed when owning frame is destructed.";
  EXPECT_TRUE(main_rfh_output_broker);
}

TEST_F(ForwardingAudioStreamFactoryTest, DestroyWebContents_DestroysStreams) {
  mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
      input_client;
  base::WeakPtr<MockBroker> input_broker =
      ExpectInputBrokerConstruction(main_rfh());

  mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient>
      output_client;
  base::WeakPtr<MockBroker> output_broker =
      ExpectOutputBrokerConstruction(main_rfh());

  ForwardingAudioStreamFactory factory(web_contents(),
                                       nullptr /*user_input_monitor*/,
                                       std::move(broker_factory_));

  EXPECT_CALL(*input_broker, CreateStream(NotNull()));
  std::ignore = input_client.InitWithNewPipeAndPassReceiver();
  factory.core()->CreateInputStream(
      main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
      kInputDeviceId, kParams, kSharedMemoryCount, kEnableAgc,
      AudioProcessingConfigPtr(), std::move(input_client));

  EXPECT_CALL(*output_broker, CreateStream(NotNull()));
  std::ignore = output_client.InitWithNewPipeAndPassReceiver();
  factory.core()->CreateOutputStream(
      main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
      kOutputDeviceId, kParams, std::move(output_client));

  // We're about to reset the |TestWebContents|. As such we need to remove the
  // reference to |other_rfh_| beforehand, otherwise it will become dangling.
  other_rfh_ = nullptr;

  DeleteContents();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(input_broker) << "Input broker should be destructed when owning "
                                "WebContents is destructed.";
  EXPECT_FALSE(output_broker)
      << "Output broker should be destructed when owning "
         "WebContents is destructed.";
}

TEST_F(ForwardingAudioStreamFactoryTest, LastStreamDeleted_ClearsFactoryPtr) {
  base::WeakPtr<MockBroker> main_rfh_input_broker =
      ExpectInputBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> other_rfh_input_broker =
      ExpectInputBrokerConstruction(other_rfh());

  base::WeakPtr<MockBroker> main_rfh_output_broker =
      ExpectOutputBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> other_rfh_output_broker =
      ExpectOutputBrokerConstruction(other_rfh());

  ForwardingAudioStreamFactory factory(web_contents(),
                                       nullptr /*user_input_monitor*/,
                                       std::move(broker_factory_));

  {
    EXPECT_CALL(*main_rfh_input_broker, CreateStream(NotNull()));
    mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
        input_client;
    std::ignore = input_client.InitWithNewPipeAndPassReceiver();
    factory.core()->CreateInputStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        kInputDeviceId, kParams, kSharedMemoryCount, kEnableAgc,
        AudioProcessingConfigPtr(), std::move(input_client));
    testing::Mock::VerifyAndClear(&*main_rfh_input_broker);
  }
  {
    EXPECT_CALL(*other_rfh_input_broker, CreateStream(NotNull()));
    mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
        input_client;
    std::ignore = input_client.InitWithNewPipeAndPassReceiver();
    factory.core()->CreateInputStream(
        other_rfh()->GetProcess()->GetID(), other_rfh()->GetRoutingID(),
        kInputDeviceId, kParams, kSharedMemoryCount, kEnableAgc,
        AudioProcessingConfigPtr(), std::move(input_client));
    testing::Mock::VerifyAndClear(&*other_rfh_input_broker);
  }

  {
    EXPECT_CALL(*main_rfh_output_broker, CreateStream(NotNull()));
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient>
        output_client;
    std::ignore = output_client.InitWithNewPipeAndPassReceiver();
    factory.core()->CreateOutputStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        kOutputDeviceId, kParams, std::move(output_client));
    testing::Mock::VerifyAndClear(&*main_rfh_output_broker);
  }
  {
    EXPECT_CALL(*other_rfh_output_broker, CreateStream(NotNull()));
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient>
        output_client;
    std::ignore = output_client.InitWithNewPipeAndPassReceiver();
    factory.core()->CreateOutputStream(
        other_rfh()->GetProcess()->GetID(), other_rfh()->GetRoutingID(),
        kOutputDeviceId, kParams, std::move(output_client));
    testing::Mock::VerifyAndClear(&*other_rfh_output_broker);
  }

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(stream_factory_.IsConnected());
  std::move(other_rfh_input_broker->deleter).Run(&*other_rfh_input_broker);
  base::RunLoop().RunUntilIdle();
  // Connection should still be open, since there are still streams left.
  EXPECT_TRUE(stream_factory_.IsConnected());
  std::move(main_rfh_input_broker->deleter).Run(&*main_rfh_input_broker);
  base::RunLoop().RunUntilIdle();
  // Connection should still be open, since there are still streams left.
  EXPECT_TRUE(stream_factory_.IsConnected());
  std::move(other_rfh_output_broker->deleter).Run(&*other_rfh_output_broker);
  base::RunLoop().RunUntilIdle();
  // Connection should still be open, since there's still a stream left.
  EXPECT_TRUE(stream_factory_.IsConnected());
  std::move(main_rfh_output_broker->deleter).Run(&*main_rfh_output_broker);
  stream_factory_.WaitForDisconnect();

  // Now there are no streams left, connection should be broken.
  EXPECT_FALSE(stream_factory_.IsConnected());
}

TEST_F(ForwardingAudioStreamFactoryTest,
       MuteNoOutputStreams_DoesNotConnectMuter) {
  ForwardingAudioStreamFactory factory(web_contents(),
                                       nullptr /*user_input_monitor*/,
                                       std::move(broker_factory_));
  EXPECT_FALSE(factory.IsMuted());

  factory.SetMuted(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(factory.IsMuted());
  EXPECT_FALSE(stream_factory_.IsConnected());
  EXPECT_FALSE(stream_factory_.IsMuterConnected());

  factory.SetMuted(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(factory.IsMuted());
  EXPECT_FALSE(stream_factory_.IsConnected());
  EXPECT_FALSE(stream_factory_.IsMuterConnected());
}

TEST_F(ForwardingAudioStreamFactoryTest, MuteWithOutputStream_ConnectsMuter) {
  mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client;
  base::WeakPtr<MockBroker> broker = ExpectOutputBrokerConstruction(main_rfh());
  ForwardingAudioStreamFactory factory(web_contents(),
                                       nullptr /*user_input_monitor*/,
                                       std::move(broker_factory_));

  EXPECT_CALL(*broker, CreateStream(NotNull()));
  std::ignore = client.InitWithNewPipeAndPassReceiver();
  factory.core()->CreateOutputStream(
      main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
      kOutputDeviceId, kParams, std::move(client));
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClear(&*broker);

  EXPECT_TRUE(stream_factory_.IsConnected());
  EXPECT_FALSE(factory.IsMuted());

  factory.SetMuted(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(factory.IsMuted());
  EXPECT_TRUE(stream_factory_.IsConnected());
  EXPECT_TRUE(stream_factory_.IsMuterConnected());

  factory.SetMuted(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(factory.IsMuted());
  EXPECT_TRUE(stream_factory_.IsConnected());
  EXPECT_FALSE(stream_factory_.IsMuterConnected());
}

TEST_F(ForwardingAudioStreamFactoryTest,
       WhenMuting_ConnectedWhenOutputStreamExists) {
  mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client;
  base::WeakPtr<MockBroker> broker = ExpectOutputBrokerConstruction(main_rfh());
  ForwardingAudioStreamFactory factory(web_contents(),
                                       nullptr /*user_input_monitor*/,
                                       std::move(broker_factory_));

  EXPECT_FALSE(stream_factory_.IsConnected());
  EXPECT_FALSE(factory.IsMuted());

  factory.SetMuted(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(factory.IsMuted());
  EXPECT_FALSE(stream_factory_.IsConnected());
  EXPECT_FALSE(stream_factory_.IsMuterConnected());

  EXPECT_CALL(*broker, CreateStream(NotNull()));
  std::ignore = client.InitWithNewPipeAndPassReceiver();
  factory.core()->CreateOutputStream(
      main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
      kOutputDeviceId, kParams, std::move(client));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(factory.IsMuted());
  EXPECT_TRUE(stream_factory_.IsConnected());
  EXPECT_TRUE(stream_factory_.IsMuterConnected());
  testing::Mock::VerifyAndClear(&*broker);

  std::move(broker->deleter).Run(&*broker);
  EXPECT_FALSE(broker);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(factory.IsMuted());
  EXPECT_FALSE(stream_factory_.IsConnected());
  EXPECT_FALSE(stream_factory_.IsMuterConnected());
}

TEST_F(ForwardingAudioStreamFactoryTest,
       WhenMuting_AddRemoveSecondStream_DoesNotChangeMuting) {
  base::WeakPtr<MockBroker> broker = ExpectOutputBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> another_broker =
      ExpectOutputBrokerConstruction(main_rfh());
  ForwardingAudioStreamFactory factory(web_contents(),
                                       nullptr /*user_input_monitor*/,
                                       std::move(broker_factory_));

  {
    EXPECT_CALL(*broker, CreateStream(NotNull()));
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client;
    std::ignore = client.InitWithNewPipeAndPassReceiver();
    factory.core()->CreateOutputStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        kOutputDeviceId, kParams, std::move(client));
    base::RunLoop().RunUntilIdle();
    testing::Mock::VerifyAndClear(&*broker);
  }
  EXPECT_TRUE(stream_factory_.IsConnected());
  EXPECT_FALSE(factory.IsMuted());

  factory.SetMuted(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(factory.IsMuted());
  EXPECT_TRUE(stream_factory_.IsConnected());
  EXPECT_TRUE(stream_factory_.IsMuterConnected());

  {
    EXPECT_CALL(*another_broker, CreateStream(NotNull()));
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient> client;
    std::ignore = client.InitWithNewPipeAndPassReceiver();
    factory.core()->CreateOutputStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        kOutputDeviceId, kParams, std::move(client));
    base::RunLoop().RunUntilIdle();
    testing::Mock::VerifyAndClear(&*another_broker);
  }

  EXPECT_TRUE(factory.IsMuted());
  EXPECT_TRUE(stream_factory_.IsConnected());
  EXPECT_TRUE(stream_factory_.IsMuterConnected());

  std::move(another_broker->deleter).Run(&*another_broker);
  EXPECT_FALSE(another_broker);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(factory.IsMuted());
  EXPECT_TRUE(stream_factory_.IsConnected());
  EXPECT_TRUE(stream_factory_.IsMuterConnected());
}

TEST_F(ForwardingAudioStreamFactoryTest,
       Destruction_CallsOnSourceGoneOnRegisteredLoopbackSinks) {
  StrictMock<MockLoopbackSink> sink1;
  StrictMock<MockLoopbackSink> sink2;

  // We remove |sink1| before |factory| is destructed, so it shouldn't be
  // called.
  EXPECT_CALL(sink2, OnSourceGone());
  {
    ForwardingAudioStreamFactory factory(web_contents(),
                                         nullptr /*user_input_monitor*/,
                                         std::move(broker_factory_));

    factory.core()->AddLoopbackSink(&sink1);
    factory.core()->AddLoopbackSink(&sink2);
    factory.core()->RemoveLoopbackSink(&sink1);
  }
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
