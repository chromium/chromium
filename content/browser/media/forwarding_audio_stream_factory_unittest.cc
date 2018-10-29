// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/forwarding_audio_stream_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/unguessable_token.h"
#include "content/common/media/renderer_audio_input_stream_factory.mojom.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/interfaces/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Test;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::StrictMock;
using ::testing::InSequence;

namespace content {

namespace {

class MockStreamFactory : public audio::FakeStreamFactory,
                          public audio::mojom::LocalMuter {
 public:
  MockStreamFactory() : muter_binding_(this) {}
  ~MockStreamFactory() final {}

  bool IsConnected() {
    return binding_ && !binding_.handle().QuerySignalsState().peer_closed();
  }
  bool IsMuterConnected() { return muter_binding_.is_bound(); }

 private:
  void BindMuter(audio::mojom::LocalMuterAssociatedRequest request,
                 const base::UnguessableToken& group_id) final {
    muter_binding_.Bind(std::move(request));
    muter_binding_.set_connection_error_handler(base::BindOnce(
        &MockStreamFactory::MuterUnbound, base::Unretained(this)));
  }
  void MuterUnbound() { muter_binding_.Close(); }

  mojo::AssociatedBinding<audio::mojom::LocalMuter> muter_binding_;
  DISALLOW_COPY_AND_ASSIGN(MockStreamFactory);
};

class MockBroker : public AudioStreamBroker {
 public:
  explicit MockBroker(RenderFrameHost* rfh)
      : AudioStreamBroker(rfh->GetProcess()->GetID(), rfh->GetRoutingID()),
        weak_factory_(this) {}

  ~MockBroker() override {}

  MOCK_METHOD1(CreateStream, void(audio::mojom::StreamFactory* factory));

  // Can be used to verify that |this| has been destructed.
  base::WeakPtr<MockBroker> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

  DeleterCallback deleter;

 private:
  base::WeakPtrFactory<MockBroker> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(MockBroker);
};

class MockBrokerFactory : public AudioStreamBrokerFactory {
 public:
  MockBrokerFactory() {}
  ~MockBrokerFactory() final {
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
      audio::mojom::AudioProcessingConfigPtr processing_config,
      AudioStreamBroker::DeleterCallback deleter,
      mojom::RendererAudioInputStreamFactoryClientPtr renderer_factory_client)
      final {
    std::unique_ptr<MockBroker> prepared_broker =
        std::move(prepared_input_stream_brokers_.front());
    prepared_input_stream_brokers_.pop();
    CHECK_NE(nullptr, prepared_broker.get());
    EXPECT_EQ(render_process_id, prepared_broker->render_process_id());
    EXPECT_EQ(render_frame_id, prepared_broker->render_frame_id());
    prepared_broker->deleter = std::move(deleter);
    return std::move(prepared_broker);
  }

  std::unique_ptr<AudioStreamBroker> CreateAudioOutputStreamBroker(
      int render_process_id,
      int render_frame_id,
      int stream_id,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      const base::Optional<base::UnguessableToken>& processing_id,
      AudioStreamBroker::DeleterCallback deleter,
      media::mojom::AudioOutputStreamProviderClientPtr client) final {
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
      mojom::RendererAudioInputStreamFactoryClientPtr renderer_factory_client)
      final {
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
  DISALLOW_COPY_AND_ASSIGN(MockBrokerFactory);
};

class MockLoopbackSink : public AudioStreamBroker::LoopbackSink {
 public:
  MockLoopbackSink() {}
  ~MockLoopbackSink() override {}

  MOCK_METHOD0(OnSourceGone, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLoopbackSink);
};

class ForwardingAudioStreamFactoryTest : public RenderViewHostTestHarness {
 public:
  ForwardingAudioStreamFactoryTest()
      : connector_(service_manager::Connector::Create(&connector_request_)),
        broker_factory_(std::make_unique<MockBrokerFactory>()) {
    service_manager::Connector::TestApi connector_test_api(connector_.get());
    connector_test_api.OverrideBinderForTesting(
        service_manager::Identity(audio::mojom::kServiceName),
        audio::mojom::StreamFactory::Name_,
        base::BindRepeating(&ForwardingAudioStreamFactoryTest::BindFactory,
                            base::Unretained(this)));
  }

  ~ForwardingAudioStreamFactoryTest() override {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    RenderFrameHostTester::For(main_rfh())->InitializeRenderFrameIfNeeded();
    other_rfh_ =
        RenderFrameHostTester::For(main_rfh())->AppendChild("other_rfh");
  }

  void BindFactory(mojo::ScopedMessagePipeHandle factory_request) {
    stream_factory_.binding_.Bind(
        audio::mojom::StreamFactoryRequest(std::move(factory_request)));
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
  service_manager::mojom::ConnectorRequest connector_request_;
  std::unique_ptr<service_manager::Connector> connector_;
  RenderFrameHost* other_rfh_;
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
  mojom::RendererAudioInputStreamFactoryClientPtr client;
  base::WeakPtr<MockBroker> broker = ExpectInputBrokerConstruction(main_rfh());

  ForwardingAudioStreamFactory factory(
      web_contents(), nullptr /*user_input_monitor*/, std::move(connector_),
      std::move(broker_factory_));

  EXPECT_CALL(*broker, CreateStream(NotNull()));
  mojo::MakeRequest(&client);
  factory.core()->CreateInputStream(main_rfh()->GetProcess()->GetID(),
                                    main_rfh()->GetRoutingID(), kInputDeviceId,
                                    kParams, kSharedMemoryCount, kEnableAgc,
                                    nullptr, std::move(client));
}

TEST_F(ForwardingAudioStreamFactoryTest,
       CreateLoopbackStream_CreatesLoopbackStream) {
  std::unique_ptr<WebContents> source_contents = CreateTestWebContents();
  mojom::RendererAudioInputStreamFactoryClientPtr client;
  base::WeakPtr<MockBroker> broker =
      ExpectLoopbackBrokerConstruction(main_rfh());

  std::unique_ptr<service_manager::Connector> other_connector =
      connector_->Clone();

  ForwardingAudioStreamFactory factory(
      web_contents(), nullptr /*user_input_monitor*/, std::move(connector_),
      std::move(broker_factory_));

  ForwardingAudioStreamFactory source_factory(
      source_contents.get(), nullptr /*user_input_monitor*/,
      std::move(other_connector), std::make_unique<MockBrokerFactory>());

  EXPECT_CALL(*broker, CreateStream(NotNull()));
  mojo::MakeRequest(&client);
  factory.core()->CreateLoopbackStream(
      main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
      source_factory.core(), kParams, kSharedMemoryCount, kMuteSource,
      std::move(client));
}

TEST_F(ForwardingAudioStreamFactoryTest,
       CreateOutputStream_CreatesOutputStream) {
  media::mojom::AudioOutputStreamProviderClientPtr client;
  base::WeakPtr<MockBroker> broker = ExpectOutputBrokerConstruction(main_rfh());

  ForwardingAudioStreamFactory factory(
      web_contents(), nullptr /*user_input_monitor*/, std::move(connector_),
      std::move(broker_factory_));

  EXPECT_CALL(*broker, CreateStream(NotNull()));
  mojo::MakeRequest(&client);
  factory.core()->CreateOutputStream(
      main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
      kOutputDeviceId, kParams, base::nullopt, std::move(client));
}

TEST_F(ForwardingAudioStreamFactoryTest,
       InputBrokerDeleterCalled_DestroysInputStream) {
  mojom::RendererAudioInputStreamFactoryClientPtr client;
  base::WeakPtr<MockBroker> main_rfh_broker =
      ExpectInputBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> other_rfh_broker =
      ExpectInputBrokerConstruction(other_rfh());

  ForwardingAudioStreamFactory factory(
      web_contents(), nullptr /*user_input_monitor*/, std::move(connector_),
      std::move(broker_factory_));

  {
    EXPECT_CALL(*main_rfh_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&client);
    factory.core()->CreateInputStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        kInputDeviceId, kParams, kSharedMemoryCount, kEnableAgc, nullptr,
        std::move(client));
    testing::Mock::VerifyAndClear(&*main_rfh_broker);
  }
  {
    EXPECT_CALL(*other_rfh_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&client);
    factory.core()->CreateInputStream(
        other_rfh()->GetProcess()->GetID(), other_rfh()->GetRoutingID(),
        kInputDeviceId, kParams, kSharedMemoryCount, kEnableAgc, nullptr,
        std::move(client));
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
  mojom::RendererAudioInputStreamFactoryClientPtr client;
  base::WeakPtr<MockBroker> main_rfh_broker =
      ExpectLoopbackBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> other_rfh_broker =
      ExpectLoopbackBrokerConstruction(other_rfh());

  std::unique_ptr<service_manager::Connector> other_connector =
      connector_->Clone();

  ForwardingAudioStreamFactory factory(
      web_contents(), nullptr /*user_input_monitor*/, std::move(connector_),
      std::move(broker_factory_));

  ForwardingAudioStreamFactory source_factory(
      source_contents.get(), nullptr /*user_input_monitor*/,
      std::move(other_connector), std::make_unique<MockBrokerFactory>());

  {
    EXPECT_CALL(*main_rfh_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&client);
    factory.core()->CreateLoopbackStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        source_factory.core(), kParams, kSharedMemoryCount, kMuteSource,
        std::move(client));
    testing::Mock::VerifyAndClear(&*main_rfh_broker);
  }
  {
    EXPECT_CALL(*other_rfh_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&client);
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
  media::mojom::AudioOutputStreamProviderClientPtr client;
  base::WeakPtr<MockBroker> main_rfh_broker =
      ExpectOutputBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> other_rfh_broker =
      ExpectOutputBrokerConstruction(other_rfh());

  ForwardingAudioStreamFactory factory(
      web_contents(), nullptr /*user_input_monitor*/, std::move(connector_),
      std::move(broker_factory_));

  {
    EXPECT_CALL(*main_rfh_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&client);
    factory.core()->CreateOutputStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        kOutputDeviceId, kParams, base::nullopt, std::move(client));
    testing::Mock::VerifyAndClear(&*main_rfh_broker);
  }
  {
    EXPECT_CALL(*other_rfh_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&client);
    factory.core()->CreateOutputStream(
        other_rfh()->GetProcess()->GetID(), other_rfh()->GetRoutingID(),
        kOutputDeviceId, kParams, base::nullopt, std::move(client));
    testing::Mock::VerifyAndClear(&*other_rfh_broker);
  }

  std::move(other_rfh_broker->deleter).Run(&*other_rfh_broker);
  EXPECT_FALSE(other_rfh_broker)
      << "Output broker should be destructed when deleter is called.";
  EXPECT_TRUE(main_rfh_broker);
}

TEST_F(ForwardingAudioStreamFactoryTest, DestroyFrame_DestroysRelatedStreams) {
  std::unique_ptr<WebContents> source_contents = CreateTestWebContents();

  mojom::RendererAudioInputStreamFactoryClientPtr input_client;
  base::WeakPtr<MockBroker> main_rfh_input_broker =
      ExpectInputBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> other_rfh_input_broker =
      ExpectInputBrokerConstruction(other_rfh());

  base::WeakPtr<MockBroker> main_rfh_loopback_broker =
      ExpectLoopbackBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> other_rfh_loopback_broker =
      ExpectLoopbackBrokerConstruction(other_rfh());

  media::mojom::AudioOutputStreamProviderClientPtr output_client;
  base::WeakPtr<MockBroker> main_rfh_output_broker =
      ExpectOutputBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> other_rfh_output_broker =
      ExpectOutputBrokerConstruction(other_rfh());

  std::unique_ptr<service_manager::Connector> other_connector =
      connector_->Clone();

  ForwardingAudioStreamFactory factory(
      web_contents(), nullptr /*user_input_monitor*/, std::move(connector_),
      std::move(broker_factory_));

  ForwardingAudioStreamFactory source_factory(
      source_contents.get(), nullptr /*user_input_monitor*/,
      std::move(other_connector), std::make_unique<MockBrokerFactory>());

  {
    EXPECT_CALL(*main_rfh_input_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&input_client);
    factory.core()->CreateInputStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        kInputDeviceId, kParams, kSharedMemoryCount, kEnableAgc, nullptr,
        std::move(input_client));
    testing::Mock::VerifyAndClear(&*main_rfh_input_broker);
  }
  {
    EXPECT_CALL(*other_rfh_input_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&input_client);
    factory.core()->CreateInputStream(
        other_rfh()->GetProcess()->GetID(), other_rfh()->GetRoutingID(),
        kInputDeviceId, kParams, kSharedMemoryCount, kEnableAgc, nullptr,
        std::move(input_client));
    testing::Mock::VerifyAndClear(&*other_rfh_input_broker);
  }

  {
    EXPECT_CALL(*main_rfh_loopback_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&input_client);
    factory.core()->CreateLoopbackStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        source_factory.core(), kParams, kSharedMemoryCount, kMuteSource,
        std::move(input_client));
    testing::Mock::VerifyAndClear(&*main_rfh_loopback_broker);
  }
  {
    EXPECT_CALL(*other_rfh_loopback_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&input_client);
    factory.core()->CreateLoopbackStream(
        other_rfh()->GetProcess()->GetID(), other_rfh()->GetRoutingID(),
        source_factory.core(), kParams, kSharedMemoryCount, kMuteSource,
        std::move(input_client));
    testing::Mock::VerifyAndClear(&*other_rfh_loopback_broker);
  }

  {
    EXPECT_CALL(*main_rfh_output_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&output_client);
    factory.core()->CreateOutputStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        kOutputDeviceId, kParams, base::nullopt, std::move(output_client));
    testing::Mock::VerifyAndClear(&*main_rfh_output_broker);
  }
  {
    EXPECT_CALL(*other_rfh_output_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&output_client);
    factory.core()->CreateOutputStream(
        other_rfh()->GetProcess()->GetID(), other_rfh()->GetRoutingID(),
        kOutputDeviceId, kParams, base::nullopt, std::move(output_client));
    testing::Mock::VerifyAndClear(&*other_rfh_output_broker);
  }

  factory.FrameDeleted(other_rfh());
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
  mojom::RendererAudioInputStreamFactoryClientPtr input_client;
  base::WeakPtr<MockBroker> input_broker =
      ExpectInputBrokerConstruction(main_rfh());

  media::mojom::AudioOutputStreamProviderClientPtr output_client;
  base::WeakPtr<MockBroker> output_broker =
      ExpectOutputBrokerConstruction(main_rfh());

  ForwardingAudioStreamFactory factory(
      web_contents(), nullptr /*user_input_monitor*/, std::move(connector_),
      std::move(broker_factory_));

  EXPECT_CALL(*input_broker, CreateStream(NotNull()));
  mojo::MakeRequest(&input_client);
  factory.core()->CreateInputStream(main_rfh()->GetProcess()->GetID(),
                                    main_rfh()->GetRoutingID(), kInputDeviceId,
                                    kParams, kSharedMemoryCount, kEnableAgc,
                                    nullptr, std::move(input_client));

  EXPECT_CALL(*output_broker, CreateStream(NotNull()));
  mojo::MakeRequest(&output_client);
  factory.core()->CreateOutputStream(
      main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
      kOutputDeviceId, kParams, base::nullopt, std::move(output_client));

  DeleteContents();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(input_broker) << "Input broker should be destructed when owning "
                                "WebContents is destructed.";
  EXPECT_FALSE(output_broker)
      << "Output broker should be destructed when owning "
         "WebContents is destructed.";
}

TEST_F(ForwardingAudioStreamFactoryTest, LastStreamDeleted_ClearsFactoryPtr) {
  mojom::RendererAudioInputStreamFactoryClientPtr input_client;
  base::WeakPtr<MockBroker> main_rfh_input_broker =
      ExpectInputBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> other_rfh_input_broker =
      ExpectInputBrokerConstruction(other_rfh());

  media::mojom::AudioOutputStreamProviderClientPtr output_client;
  base::WeakPtr<MockBroker> main_rfh_output_broker =
      ExpectOutputBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> other_rfh_output_broker =
      ExpectOutputBrokerConstruction(other_rfh());

  ForwardingAudioStreamFactory factory(
      web_contents(), nullptr /*user_input_monitor*/, std::move(connector_),
      std::move(broker_factory_));

  {
    EXPECT_CALL(*main_rfh_input_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&input_client);
    factory.core()->CreateInputStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        kInputDeviceId, kParams, kSharedMemoryCount, kEnableAgc, nullptr,
        std::move(input_client));
    testing::Mock::VerifyAndClear(&*main_rfh_input_broker);
  }
  {
    EXPECT_CALL(*other_rfh_input_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&input_client);
    factory.core()->CreateInputStream(
        other_rfh()->GetProcess()->GetID(), other_rfh()->GetRoutingID(),
        kInputDeviceId, kParams, kSharedMemoryCount, kEnableAgc, nullptr,
        std::move(input_client));
    testing::Mock::VerifyAndClear(&*other_rfh_input_broker);
  }

  {
    EXPECT_CALL(*main_rfh_output_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&output_client);
    factory.core()->CreateOutputStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        kOutputDeviceId, kParams, base::nullopt, std::move(output_client));
    testing::Mock::VerifyAndClear(&*main_rfh_output_broker);
  }
  {
    EXPECT_CALL(*other_rfh_output_broker, CreateStream(NotNull()));
    mojo::MakeRequest(&output_client);
    factory.core()->CreateOutputStream(
        other_rfh()->GetProcess()->GetID(), other_rfh()->GetRoutingID(),
        kOutputDeviceId, kParams, base::nullopt, std::move(output_client));
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
  base::RunLoop().RunUntilIdle();
  // Now there are no streams left, connection should be broken.
  EXPECT_FALSE(stream_factory_.IsConnected());
}

TEST_F(ForwardingAudioStreamFactoryTest,
       MuteNoOutputStreams_DoesNotConnectMuter) {
  ForwardingAudioStreamFactory factory(
      web_contents(), nullptr /*user_input_monitor*/, std::move(connector_),
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
  media::mojom::AudioOutputStreamProviderClientPtr client;
  base::WeakPtr<MockBroker> broker = ExpectOutputBrokerConstruction(main_rfh());
  ForwardingAudioStreamFactory factory(
      web_contents(), nullptr /*user_input_monitor*/, std::move(connector_),
      std::move(broker_factory_));

  EXPECT_CALL(*broker, CreateStream(NotNull()));
  mojo::MakeRequest(&client);
  factory.core()->CreateOutputStream(
      main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
      kOutputDeviceId, kParams, base::nullopt, std::move(client));
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
  media::mojom::AudioOutputStreamProviderClientPtr client;
  base::WeakPtr<MockBroker> broker = ExpectOutputBrokerConstruction(main_rfh());
  ForwardingAudioStreamFactory factory(
      web_contents(), nullptr /*user_input_monitor*/, std::move(connector_),
      std::move(broker_factory_));

  EXPECT_FALSE(stream_factory_.IsConnected());
  EXPECT_FALSE(factory.IsMuted());

  factory.SetMuted(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(factory.IsMuted());
  EXPECT_FALSE(stream_factory_.IsConnected());
  EXPECT_FALSE(stream_factory_.IsMuterConnected());

  EXPECT_CALL(*broker, CreateStream(NotNull()));
  mojo::MakeRequest(&client);
  factory.core()->CreateOutputStream(
      main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
      kOutputDeviceId, kParams, base::nullopt, std::move(client));
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
  media::mojom::AudioOutputStreamProviderClientPtr client;
  base::WeakPtr<MockBroker> broker = ExpectOutputBrokerConstruction(main_rfh());
  base::WeakPtr<MockBroker> another_broker =
      ExpectOutputBrokerConstruction(main_rfh());
  ForwardingAudioStreamFactory factory(
      web_contents(), nullptr /*user_input_monitor*/, std::move(connector_),
      std::move(broker_factory_));

  {
    EXPECT_CALL(*broker, CreateStream(NotNull()));
    mojo::MakeRequest(&client);
    factory.core()->CreateOutputStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        kOutputDeviceId, kParams, base::nullopt, std::move(client));
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
    mojo::MakeRequest(&client);
    factory.core()->CreateOutputStream(
        main_rfh()->GetProcess()->GetID(), main_rfh()->GetRoutingID(),
        kOutputDeviceId, kParams, base::nullopt, std::move(client));
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
    ForwardingAudioStreamFactory factory(
        web_contents(), nullptr /*user_input_monitor*/, std::move(connector_),
        std::move(broker_factory_));

    factory.core()->AddLoopbackSink(&sink1);
    factory.core()->AddLoopbackSink(&sink2);
    factory.core()->RemoveLoopbackSink(&sink1);
  }
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
