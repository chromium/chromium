// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_output_stream_broker.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/sync_socket.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/mock_preferred_audio_output_device_manager.h"
#include "content/browser/renderer_host/media/preferred_audio_output_device_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/fake_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::Test;

namespace content {

namespace {

const int kRenderProcessId = 123;
const int kRenderFrameId = 234;
const GlobalRenderFrameHostToken kMainFrameHostToken{1,
                                                     blink::LocalFrameToken()};
const int kStreamId = 345;
const size_t kShMemSize = 456;
const char kDeviceId[] = "testdeviceid";

media::AudioParameters TestParams() {
  return media::AudioParameters::UnavailableDeviceParams();
}

using MockDeleterCallback = StrictMock<
    base::MockCallback<base::OnceCallback<void(AudioStreamBroker*)>>>;

class MockAudioOutputStreamProviderClient
    : public media::mojom::AudioOutputStreamProviderClient {
 public:
  MockAudioOutputStreamProviderClient() = default;

  MockAudioOutputStreamProviderClient(
      const MockAudioOutputStreamProviderClient&) = delete;
  MockAudioOutputStreamProviderClient& operator=(
      const MockAudioOutputStreamProviderClient&) = delete;

  ~MockAudioOutputStreamProviderClient() override {}

  void Created(mojo::PendingRemote<media::mojom::AudioOutputStream>,
               media::mojom::ReadWriteAudioDataPipePtr) override {
    OnCreated();
  }

  MOCK_METHOD0(OnCreated, void());

  MOCK_METHOD2(ConnectionError, void(uint32_t, const std::string&));

  mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient>
  MakePendingRemote() {
    mojo::PendingRemote<media::mojom::AudioOutputStreamProviderClient>
        pending_remote;
    receiver_.Bind(pending_remote.InitWithNewPipeAndPassReceiver());
    receiver_.set_disconnect_with_reason_handler(
        base::BindOnce(&MockAudioOutputStreamProviderClient::ConnectionError,
                       base::Unretained(this)));
    return pending_remote;
  }

  void CloseReceiver() { receiver_.reset(); }

 private:
  mojo::Receiver<media::mojom::AudioOutputStreamProviderClient> receiver_{this};
};

class MockDeviceSwitchInterface : public media::mojom::DeviceSwitchInterface {
 public:
  MockDeviceSwitchInterface() = default;

  MockDeviceSwitchInterface(const MockDeviceSwitchInterface&) = delete;
  MockDeviceSwitchInterface& operator=(const MockDeviceSwitchInterface&) =
      delete;

  ~MockDeviceSwitchInterface() override = default;

  MOCK_METHOD(void,
              SwitchAudioOutputDeviceId,
              (const std::string&),
              (override));
};

class MockStreamFactory final : public audio::FakeStreamFactory {
 public:
  MockStreamFactory() = default;

  MockStreamFactory(const MockStreamFactory&) = delete;
  MockStreamFactory& operator=(const MockStreamFactory&) = delete;

  ~MockStreamFactory() override = default;

  // State of an expected stream creation. |output_device_id|, |params|,
  // and |groups_id| are set ahead of time and verified during request.
  // The other fields are filled in when the request is received.
  struct StreamRequestData {
    StreamRequestData(const std::string& output_device_id,
                      const media::AudioParameters& params,
                      const base::UnguessableToken& group_id)
        : output_device_id(output_device_id),
          params(params),
          group_id(group_id) {}

    bool requested = false;
    mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver;
    mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
        observer_remote;
    mojo::Remote<media::mojom::AudioLog> log;
    const std::string output_device_id;
    const media::AudioParameters params;
    const base::UnguessableToken group_id;
    CreateOutputStreamCallback created_callback;
  };

  void ExpectStreamCreation(StreamRequestData* ex) {
    stream_request_data_ = ex;
  }

 private:
  void CreateOutputStream(
      mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver,
      mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
          observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      CreateOutputStreamCallback created_callback) final {
    // No way to cleanly exit the test here in case of failure, so use CHECK.
    CHECK(stream_request_data_);
    EXPECT_EQ(stream_request_data_->output_device_id, output_device_id);
    EXPECT_TRUE(stream_request_data_->params.Equals(params));
    EXPECT_EQ(stream_request_data_->group_id, group_id);
    stream_request_data_->requested = true;
    stream_request_data_->stream_receiver = std::move(stream_receiver);
    stream_request_data_->observer_remote = std::move(observer);
    stream_request_data_->log.Bind(std ::move(log));
    stream_request_data_->created_callback = std::move(created_callback);
  }

  void CreateSwitchableOutputStream(
      mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver,
      mojo::PendingReceiver<media::mojom::DeviceSwitchInterface>
          device_switch_receiver,
      mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
          observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      CreateOutputStreamCallback created_callback) final {
    CreateOutputStream(std::move(stream_receiver), std::move(observer),
                       std::move(log), output_device_id, params, group_id,
                       std::move(created_callback));
  }

  raw_ptr<StreamRequestData> stream_request_data_;
};

// This struct collects test state we need without doing anything fancy.
struct TestEnvironment {
  explicit TestEnvironment(const std::string device_id = kDeviceId)
      : group(base::UnguessableToken::Create()),
        broker(std::make_unique<AudioOutputStreamBroker>(
            kRenderProcessId,
            kRenderFrameId,
            kMainFrameHostToken,
            kStreamId,
            device_id,
            TestParams(),
            group,
            deleter.Get(),
            provider_client.MakePendingRemote())) {
    audio_manager = std::make_unique<media::FakeAudioManager>(
        std::make_unique<media::TestAudioThread>(), &log_factory);
    audio_system =
        std::make_unique<media::AudioSystemImpl>(audio_manager.get());
    media_stream_manager =
        std::make_unique<MediaStreamManager>(audio_system.get());
  }

  ~TestEnvironment() { audio_manager->Shutdown(); }

  void RunUntilIdle() { env.RunUntilIdle(); }

  void BindAndSetDeviceSwitchInterface() {
    // Inject mock into AudioOutputStreamBroker
    mojo::PendingRemote<media::mojom::DeviceSwitchInterface> pending_remote;
    receiver =
        std::make_unique<mojo::Receiver<media::mojom::DeviceSwitchInterface>>(
            &device_switch_interface,
            pending_remote.InitWithNewPipeAndPassReceiver());
    mojo::Remote<media::mojom::DeviceSwitchInterface> remote(
        std::move(pending_remote));
    broker->SetDeviceSwichInterfaceForTesting(std::move(remote));
  }

  // MediaInternals RenderProcessHost observation setup asserts being run on the
  // UI thread.
  media::FakeAudioLogFactory log_factory;
  std::unique_ptr<media::FakeAudioManager> audio_manager;
  std::unique_ptr<media::AudioSystemImpl> audio_system;
  std::unique_ptr<MediaStreamManager> media_stream_manager;

  BrowserTaskEnvironment env;
  base::UnguessableToken group;
  MockDeleterCallback deleter;
  StrictMock<MockAudioOutputStreamProviderClient> provider_client;
  std::unique_ptr<AudioOutputStreamBroker> broker;
  MockStreamFactory stream_factory;
  MockDeviceSwitchInterface device_switch_interface;
  std::unique_ptr<mojo::Receiver<media::mojom::DeviceSwitchInterface>> receiver;
  mojo::Remote<media::mojom::AudioStreamFactory> factory_ptr{
      stream_factory.MakeRemote()};
};

}  // namespace

TEST(AudioOutputStreamBrokerTest, StoresProcessAndFrameId) {
  BrowserTaskEnvironment env;
  MockDeleterCallback deleter;
  StrictMock<MockAudioOutputStreamProviderClient> provider_client;

  AudioOutputStreamBroker broker(
      kRenderProcessId, kRenderFrameId, kMainFrameHostToken, kStreamId,
      kDeviceId, TestParams(), base::UnguessableToken::Create(), deleter.Get(),
      provider_client.MakePendingRemote());

  EXPECT_EQ(kRenderProcessId, broker.render_process_id());
  EXPECT_EQ(kRenderFrameId, broker.render_frame_id());
}

TEST(AudioOutputStreamBrokerTest, ClientDisconnect_CallsDeleter) {
  TestEnvironment env;

  EXPECT_CALL(env.deleter, Run(env.broker.release()))
      .WillOnce(testing::DeleteArg<0>());
  env.provider_client.CloseReceiver();
  env.RunUntilIdle();
}

TEST(AudioOutputStreamBrokerTest, StreamCreationSuccess_Propagates) {
  TestEnvironment env;
  MockStreamFactory::StreamRequestData stream_request_data(
      kDeviceId, TestParams(), env.group);
  env.stream_factory.ExpectStreamCreation(&stream_request_data);

  env.broker->CreateStream(env.factory_ptr.get());
  env.RunUntilIdle();

  EXPECT_TRUE(stream_request_data.requested);

  // Set up test IPC primitives.
  base::SyncSocket socket1, socket2;
  base::SyncSocket::CreatePair(&socket1, &socket2);
  std::move(stream_request_data.created_callback)
      .Run({std::in_place, base::UnsafeSharedMemoryRegion::Create(kShMemSize),
            mojo::PlatformHandle(socket1.Take())});

  EXPECT_CALL(env.provider_client, OnCreated());

  env.RunUntilIdle();

  Mock::VerifyAndClear(&env.provider_client);

  EXPECT_CALL(env.provider_client, ConnectionError(_, _));

  env.broker.reset();
}

TEST(AudioOutputStreamBrokerTest,
     StreamCreationFailure_PropagatesErrorAndCallsDeleter) {
  TestEnvironment env;
  MockStreamFactory::StreamRequestData stream_request_data(
      kDeviceId, TestParams(), env.group);
  env.stream_factory.ExpectStreamCreation(&stream_request_data);

  env.broker->CreateStream(env.factory_ptr.get());
  env.RunUntilIdle();

  EXPECT_TRUE(stream_request_data.requested);
  EXPECT_CALL(env.provider_client,
              ConnectionError(static_cast<uint32_t>(
                                  media::mojom::AudioOutputStreamObserver::
                                      DisconnectReason::kPlatformError),
                              std::string()));
  EXPECT_CALL(env.deleter, Run(env.broker.release()))
      .WillOnce(testing::DeleteArg<0>());

  std::move(stream_request_data.created_callback).Run(nullptr);

  env.RunUntilIdle();
}

TEST(AudioOutputStreamBrokerTest,
     ObserverDisconnect_PropagatesErrorAndCallsDeleter) {
  TestEnvironment env;
  MockStreamFactory::StreamRequestData stream_request_data(
      kDeviceId, TestParams(), env.group);
  env.stream_factory.ExpectStreamCreation(&stream_request_data);

  env.broker->CreateStream(env.factory_ptr.get());
  env.RunUntilIdle();

  EXPECT_TRUE(stream_request_data.requested);
  EXPECT_CALL(env.provider_client,
              ConnectionError(static_cast<uint32_t>(
                                  media::mojom::AudioOutputStreamObserver::
                                      DisconnectReason::kPlatformError),
                              std::string()));
  EXPECT_CALL(env.deleter, Run(env.broker.release()))
      .WillOnce(testing::DeleteArg<0>());

  // This results in a disconnect.
  stream_request_data.observer_remote.PassHandle();

  env.RunUntilIdle();
  env.stream_factory.ResetReceiver();
  env.RunUntilIdle();
}

TEST(AudioOutputStreamBrokerTest,
     FactoryDisconnectDuringConstruction_PropagatesErrorAndCallsDeleter) {
  TestEnvironment env;

  env.broker->CreateStream(env.factory_ptr.get());
  env.stream_factory.ResetReceiver();

  EXPECT_CALL(env.deleter, Run(env.broker.release()))
      .WillOnce(testing::DeleteArg<0>());
  EXPECT_CALL(env.provider_client,
              ConnectionError(static_cast<uint32_t>(
                                  media::mojom::AudioOutputStreamObserver::
                                      DisconnectReason::kPlatformError),
                              std::string()));

  env.RunUntilIdle();
}

TEST(AudioOutputStreamBrokerTest, SwitchableStreamCreationSuccess) {
  TestEnvironment env(media::AudioDeviceDescription::kDefaultDeviceId);
  MockStreamFactory::StreamRequestData stream_request_data(
      media::AudioDeviceDescription::kDefaultDeviceId, TestParams(), env.group);
  env.stream_factory.ExpectStreamCreation(&stream_request_data);

  auto mock_preferred_audio_output_device_manager =
      std::make_unique<MockPreferredAudioOutputDeviceManager>();
  MockPreferredAudioOutputDeviceManager* manager =
      mock_preferred_audio_output_device_manager.get();
  env.media_stream_manager->SetPreferredAudioOutputDeviceManagerForTesting(
      std::move(mock_preferred_audio_output_device_manager));

  EXPECT_CALL(*manager, AddSwitcher(_, _)).Times(1);

  env.broker->CreateStream(env.factory_ptr.get());
  env.RunUntilIdle();
  EXPECT_TRUE(env.broker->IsSwitchableStreamCreatedForTesting());

  EXPECT_TRUE(stream_request_data.requested);

  // Set up device switcher.
  env.BindAndSetDeviceSwitchInterface();
  constexpr char kRawDeviceId[] = "rawdeviceid";
  EXPECT_CALL(*manager,
              SetPreferredSinkId(kMainFrameHostToken, kRawDeviceId, _))
      .WillOnce(testing::Invoke(
          [&env](const GlobalRenderFrameHostToken&,
                 const std::string& device_id,
                 base::OnceCallback<void(media::OutputDeviceStatus)>) {
            env.broker->SwitchAudioOutputDeviceId(device_id);
          }));

  EXPECT_CALL(env.device_switch_interface,
              SwitchAudioOutputDeviceId(kRawDeviceId))
      .Times(1);
  manager->SetPreferredSinkId(kMainFrameHostToken, kRawDeviceId,
                              base::DoNothing());

  // Set up test IPC primitives.
  base::SyncSocket socket1, socket2;
  base::SyncSocket::CreatePair(&socket1, &socket2);
  std::move(stream_request_data.created_callback)
      .Run({std::in_place, base::UnsafeSharedMemoryRegion::Create(kShMemSize),
            mojo::PlatformHandle(socket1.Take())});

  EXPECT_CALL(env.provider_client, OnCreated());
  env.RunUntilIdle();
  Mock::VerifyAndClear(&env.provider_client);

  EXPECT_CALL(env.provider_client, ConnectionError(_, _));

  EXPECT_CALL(*manager, RemoveSwitcher(_, _)).Times(1);
  env.broker.reset();
}

TEST(AudioOutputStreamBrokerTest,
     NonSwitchableStreamCreationForNonPreferredManager) {
  // Do not call `AddSwitcher()` and `RemoveSwitcher` if
  // `PreferredAudioOutputDeviceManager` is not set.
  TestEnvironment env(media::AudioDeviceDescription::kDefaultDeviceId);
  MockStreamFactory::StreamRequestData stream_request_data(
      media::AudioDeviceDescription::kDefaultDeviceId, TestParams(), env.group);
  env.stream_factory.ExpectStreamCreation(&stream_request_data);

  auto mock_preferred_audio_output_device_manager =
      std::make_unique<MockPreferredAudioOutputDeviceManager>();
  MockPreferredAudioOutputDeviceManager* manager =
      mock_preferred_audio_output_device_manager.get();
  env.media_stream_manager->SetPreferredAudioOutputDeviceManagerForTesting(
      nullptr);

  EXPECT_CALL(*manager, AddSwitcher(_, _)).Times(0);

  env.broker->CreateStream(env.factory_ptr.get());
  env.RunUntilIdle();
  EXPECT_FALSE(env.broker->IsSwitchableStreamCreatedForTesting());

  EXPECT_TRUE(stream_request_data.requested);

  // Set up test IPC primitives.
  base::SyncSocket socket1, socket2;
  base::SyncSocket::CreatePair(&socket1, &socket2);
  std::move(stream_request_data.created_callback)
      .Run({std::in_place, base::UnsafeSharedMemoryRegion::Create(kShMemSize),
            mojo::PlatformHandle(socket1.Take())});

  EXPECT_CALL(env.provider_client, OnCreated());
  env.RunUntilIdle();
  Mock::VerifyAndClear(&env.provider_client);

  EXPECT_CALL(env.provider_client, ConnectionError(_, _));

  EXPECT_CALL(*manager, RemoveSwitcher(_, _)).Times(0);
  env.broker.reset();
}

TEST(AudioOutputStreamBrokerTest,
     UnSwitchableStreamCreationWhenNonDefaultDevice) {
  // Do not call `AddSwitcher()` if not default device id.
  TestEnvironment env(kDeviceId);
  MockStreamFactory::StreamRequestData stream_request_data(
      kDeviceId, TestParams(), env.group);
  env.stream_factory.ExpectStreamCreation(&stream_request_data);

  auto mock_preferred_audio_output_device_manager =
      std::make_unique<MockPreferredAudioOutputDeviceManager>();
  MockPreferredAudioOutputDeviceManager* manager =
      mock_preferred_audio_output_device_manager.get();
  env.media_stream_manager->SetPreferredAudioOutputDeviceManagerForTesting(
      std::move(mock_preferred_audio_output_device_manager));

  EXPECT_CALL(*manager, AddSwitcher(_, _)).Times(0);

  env.broker->CreateStream(env.factory_ptr.get());
  env.RunUntilIdle();
  EXPECT_FALSE(env.broker->IsSwitchableStreamCreatedForTesting());

  EXPECT_TRUE(stream_request_data.requested);

  // Set up test IPC primitives.
  base::SyncSocket socket1, socket2;
  base::SyncSocket::CreatePair(&socket1, &socket2);
  std::move(stream_request_data.created_callback)
      .Run({std::in_place, base::UnsafeSharedMemoryRegion::Create(kShMemSize),
            mojo::PlatformHandle(socket1.Take())});

  EXPECT_CALL(env.provider_client, OnCreated());
  env.RunUntilIdle();
  Mock::VerifyAndClear(&env.provider_client);

  EXPECT_CALL(env.provider_client, ConnectionError(_, _));

  EXPECT_CALL(*manager, RemoveSwitcher(_, _)).Times(1);
  env.broker.reset();
}

TEST(AudioOutputStreamBrokerTest, SwitchableStreamCreationWithPreferredSinkId) {
  // Switchable stream is created when preferred sink id.
  TestEnvironment env(media::AudioDeviceDescription::kDefaultDeviceId);
  MockStreamFactory::StreamRequestData stream_request_data(
      kDeviceId, TestParams(), env.group);
  env.stream_factory.ExpectStreamCreation(&stream_request_data);

  auto mock_preferred_audio_output_device_manager =
      std::make_unique<MockPreferredAudioOutputDeviceManager>();
  MockPreferredAudioOutputDeviceManager* manager =
      mock_preferred_audio_output_device_manager.get();
  env.media_stream_manager->SetPreferredAudioOutputDeviceManagerForTesting(
      std::move(mock_preferred_audio_output_device_manager));

  EXPECT_CALL(*manager, AddSwitcher(_, _))
      .WillOnce(testing::Invoke([](const GlobalRenderFrameHostToken&,
                                   AudioOutputDeviceSwitcher* switcher) {
        switcher->SwitchAudioOutputDeviceId(kDeviceId);
      }));

  env.broker->CreateStream(env.factory_ptr.get());
  env.RunUntilIdle();
  EXPECT_TRUE(env.broker->IsSwitchableStreamCreatedForTesting());

  EXPECT_TRUE(stream_request_data.requested);

  // Set up device switcher.
  env.BindAndSetDeviceSwitchInterface();
  EXPECT_CALL(*manager, SetPreferredSinkId(
                            kMainFrameHostToken,
                            media::AudioDeviceDescription::kDefaultDeviceId, _))
      .WillOnce(testing::Invoke(
          [&env](const GlobalRenderFrameHostToken&,
                 const std::string& device_id,
                 base::OnceCallback<void(media::OutputDeviceStatus)>) {
            env.broker->SwitchAudioOutputDeviceId(device_id);
          }));

  EXPECT_CALL(env.device_switch_interface,
              SwitchAudioOutputDeviceId(
                  media::AudioDeviceDescription::kDefaultDeviceId))
      .Times(1);
  manager->SetPreferredSinkId(kMainFrameHostToken,
                              media::AudioDeviceDescription::kDefaultDeviceId,
                              base::DoNothing());

  // Set up test IPC primitives.
  base::SyncSocket socket1, socket2;
  base::SyncSocket::CreatePair(&socket1, &socket2);
  std::move(stream_request_data.created_callback)
      .Run({std::in_place, base::UnsafeSharedMemoryRegion::Create(kShMemSize),
            mojo::PlatformHandle(socket1.Take())});

  EXPECT_CALL(env.provider_client, OnCreated());
  env.RunUntilIdle();
  Mock::VerifyAndClear(&env.provider_client);

  EXPECT_CALL(env.provider_client, ConnectionError(_, _));

  EXPECT_CALL(*manager, RemoveSwitcher(_, _)).Times(1);
  env.broker.reset();
}

}  // namespace content
