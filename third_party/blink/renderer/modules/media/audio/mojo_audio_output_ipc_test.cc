// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media/audio/mojo_audio_output_ipc.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::Mock;
using testing::StrictMock;

namespace blink {

namespace {

const size_t kMemoryLength = 4321;
const char kDeviceId[] = "device_id";
const char kReturnedDeviceId[] = "returned_device_id";
const double kNewVolume = 0.271828;

media::AudioParameters Params() {
  return media::AudioParameters::UnavailableDeviceParams();
}

MojoAudioOutputIPC::FactoryAccessorCB NullAccessor() {
  return WTF::BindRepeating(
      []() -> blink::mojom::blink::RendererAudioOutputStreamFactory* {
        return nullptr;
      });
}

// TODO(https://crbug.com/787252): Convert the test away from using std::string.
class TestStreamProvider
    : public media::mojom::blink::AudioOutputStreamProvider {
 public:
  explicit TestStreamProvider(media::mojom::blink::AudioOutputStream* stream)
      : stream_(stream) {}

  ~TestStreamProvider() override {
    // If we expected a stream to be acquired, make sure it is so.
    if (stream_)
      EXPECT_TRUE(receiver_);
  }

  void Acquire(
      const media::AudioParameters& params,
      mojo::PendingRemote<media::mojom::blink::AudioOutputStreamProviderClient>
          pending_provider_client) override {
    EXPECT_EQ(receiver_, std::nullopt);
    EXPECT_NE(stream_, nullptr);
    provider_client_.reset();
    provider_client_.Bind(std::move(pending_provider_client));
    mojo::PendingRemote<media::mojom::blink::AudioOutputStream>
        stream_pending_remote;
    receiver_.emplace(stream_,
                      stream_pending_remote.InitWithNewPipeAndPassReceiver());
    base::CancelableSyncSocket foreign_socket;
    EXPECT_TRUE(
        base::CancelableSyncSocket::CreatePair(&socket_, &foreign_socket));
    provider_client_->Created(
        std::move(stream_pending_remote),
        {std::in_place, base::UnsafeSharedMemoryRegion::Create(kMemoryLength),
         mojo::PlatformHandle(foreign_socket.Take())});
  }

  void SignalErrorToProviderClient() {
    provider_client_.ResetWithReason(
        static_cast<uint32_t>(media::mojom::blink::AudioOutputStreamObserver::
                                  DisconnectReason::kPlatformError),
        std::string());
  }

 private:
  raw_ptr<media::mojom::blink::AudioOutputStream> stream_;
  mojo::Remote<media::mojom::blink::AudioOutputStreamProviderClient>
      provider_client_;
  std::optional<mojo::Receiver<media::mojom::blink::AudioOutputStream>>
      receiver_;
  base::CancelableSyncSocket socket_;
};

class TestRemoteFactory
    : public blink::mojom::blink::RendererAudioOutputStreamFactory {
 public:
  TestRemoteFactory()
      : expect_request_(false),
        receiver_(this, this_remote_.BindNewPipeAndPassReceiver()) {}

  ~TestRemoteFactory() override {}

  void RequestDeviceAuthorization(
      mojo::PendingReceiver<media::mojom::blink::AudioOutputStreamProvider>
          stream_provider_receiver,
      const std::optional<base::UnguessableToken>& session_id,
      const String& device_id,
      RequestDeviceAuthorizationCallback callback) override {
    EXPECT_EQ(session_id, expected_session_id_);
    EXPECT_EQ(device_id.Utf8(), expected_device_id_);
    EXPECT_TRUE(expect_request_);
    if (provider_) {
      std::move(callback).Run(
          static_cast<media::mojom::blink::OutputDeviceStatus>(
              media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK),
          Params(), String(kReturnedDeviceId));
      provider_receiver_.emplace(provider_.get(),
                                 std::move(stream_provider_receiver));
    } else {
      std::move(callback).Run(
          static_cast<media::mojom::blink::OutputDeviceStatus>(
              media::OutputDeviceStatus::
                  OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED),
          Params(), String(""));
    }
    expect_request_ = false;
  }

  void PrepareProviderForAuthorization(
      const base::UnguessableToken& session_id,
      const std::string& device_id,
      std::unique_ptr<TestStreamProvider> provider) {
    EXPECT_FALSE(expect_request_);
    expect_request_ = true;
    expected_session_id_ = session_id.is_empty()
                               ? std::optional<base::UnguessableToken>()
                               : session_id;
    expected_device_id_ = device_id;
    provider_receiver_.reset();
    std::swap(provider_, provider);
  }

  void RefuseNextRequest(const base::UnguessableToken& session_id,
                         const std::string& device_id) {
    EXPECT_FALSE(expect_request_);
    expect_request_ = true;
    expected_session_id_ = session_id;
    expected_device_id_ = device_id;
  }

  void SignalErrorToProviderClient() {
    provider_->SignalErrorToProviderClient();
  }

  void Disconnect() {
    receiver_.reset();
    this_remote_.reset();
    receiver_.Bind(this_remote_.BindNewPipeAndPassReceiver());
    provider_receiver_.reset();
    provider_.reset();
    expect_request_ = false;
  }

  MojoAudioOutputIPC::FactoryAccessorCB GetAccessor() {
    return WTF::BindRepeating(&TestRemoteFactory::get, WTF::Unretained(this));
  }

 private:
  blink::mojom::blink::RendererAudioOutputStreamFactory* get() {
    return this_remote_.get();
  }

  bool expect_request_;
  std::optional<base::UnguessableToken> expected_session_id_;
  std::string expected_device_id_;

  mojo::Remote<blink::mojom::blink::RendererAudioOutputStreamFactory>
      this_remote_;
  mojo::Receiver<blink::mojom::blink::RendererAudioOutputStreamFactory>
      receiver_{this};
  std::unique_ptr<TestStreamProvider> provider_;
  std::optional<mojo::Receiver<media::mojom::blink::AudioOutputStreamProvider>>
      provider_receiver_;
};

class MockStream : public media::mojom::blink::AudioOutputStream {
 public:
  MOCK_METHOD0(Play, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD0(Flush, void());
  MOCK_METHOD1(SetVolume, void(double));
};

class MockDelegate : public media::AudioOutputIPCDelegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  void OnStreamCreated(base::UnsafeSharedMemoryRegion mem_handle,
                       base::SyncSocket::ScopedHandle socket_handle,
                       bool playing_automatically) override {
    GotOnStreamCreated();
  }

  MOCK_METHOD0(OnError, void());
  MOCK_METHOD3(OnDeviceAuthorized,
               void(media::OutputDeviceStatus device_status,
                    const media::AudioParameters& output_params,
                    const std::string& matched_device_id));
  MOCK_METHOD0(GotOnStreamCreated, void());
  MOCK_METHOD0(OnIPCClosed, void());
};

}  // namespace

TEST(MojoAudioOutputIPC, AuthorizeWithoutFactory_CallsAuthorizedWithError) {
  test::TaskEnvironment task_environment;
  const base::UnguessableToken session_id = base::UnguessableToken::Create();
  StrictMock<MockDelegate> delegate;

  std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          NullAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());

  ipc->RequestDeviceAuthorization(&delegate, session_id, kDeviceId);

  // Don't call OnDeviceAuthorized synchronously, should wait until we run the
  // RunLoop.
  EXPECT_CALL(delegate,
              OnDeviceAuthorized(media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL, _,
                                 std::string()));
  base::RunLoop().RunUntilIdle();
  ipc->CloseStream();
}

TEST(MojoAudioOutputIPC,
     CreateWithoutAuthorizationWithoutFactory_CallsAuthorizedWithError) {
  test::TaskEnvironment task_environment;
  StrictMock<MockDelegate> delegate;

  std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          NullAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());

  ipc->CreateStream(&delegate, Params());

  // No call to OnDeviceAuthorized since authotization wasn't explicitly
  // requested.
  base::RunLoop().RunUntilIdle();
  ipc->CloseStream();
}

TEST(MojoAudioOutputIPC, DeviceAuthorized_Propagates) {
  test::TaskEnvironment task_environment;
  const base::UnguessableToken session_id = base::UnguessableToken::Create();
  TestRemoteFactory stream_factory;
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  stream_factory.PrepareProviderForAuthorization(
      session_id, kDeviceId, std::make_unique<TestStreamProvider>(nullptr));

  ipc->RequestDeviceAuthorization(&delegate, session_id, kDeviceId);

  EXPECT_CALL(delegate, OnDeviceAuthorized(
                            media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                            _, std::string(kReturnedDeviceId)));
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, OnDeviceCreated_Propagates) {
  test::TaskEnvironment task_environment;
  const base::UnguessableToken session_id = base::UnguessableToken::Create();
  TestRemoteFactory stream_factory;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  stream_factory.PrepareProviderForAuthorization(
      session_id, kDeviceId, std::make_unique<TestStreamProvider>(&stream));

  ipc->RequestDeviceAuthorization(&delegate, session_id, kDeviceId);
  ipc->CreateStream(&delegate, Params());

  EXPECT_CALL(delegate, OnDeviceAuthorized(
                            media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                            _, std::string(kReturnedDeviceId)));
  EXPECT_CALL(delegate, GotOnStreamCreated());
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC,
     CreateWithoutAuthorization_RequestsAuthorizationFirst) {
  test::TaskEnvironment task_environment;
  TestRemoteFactory stream_factory;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;
  const std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());

  // Note: This call implicitly EXPECTs that authorization is requested,
  // and constructing the TestStreamProvider with a |&stream| EXPECTs that the
  // stream is created. This implicit request should always be for the default
  // device and no session id.
  stream_factory.PrepareProviderForAuthorization(
      base::UnguessableToken(),
      std::string(media::AudioDeviceDescription::kDefaultDeviceId),
      std::make_unique<TestStreamProvider>(&stream));

  ipc->CreateStream(&delegate, Params());

  EXPECT_CALL(delegate, GotOnStreamCreated());
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, IsReusable) {
  test::TaskEnvironment task_environment;
  const base::UnguessableToken session_id = base::UnguessableToken::Create();
  TestRemoteFactory stream_factory;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());

  for (int i = 0; i < 5; ++i) {
    stream_factory.PrepareProviderForAuthorization(
        session_id, kDeviceId, std::make_unique<TestStreamProvider>(&stream));

    ipc->RequestDeviceAuthorization(&delegate, session_id, kDeviceId);
    ipc->CreateStream(&delegate, Params());

    EXPECT_CALL(
        delegate,
        OnDeviceAuthorized(media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                           _, std::string(kReturnedDeviceId)));
    EXPECT_CALL(delegate, GotOnStreamCreated());
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&delegate);

    ipc->CloseStream();
    base::RunLoop().RunUntilIdle();
  }
}

TEST(MojoAudioOutputIPC, IsReusableAfterError) {
  test::TaskEnvironment task_environment;
  const base::UnguessableToken session_id = base::UnguessableToken::Create();
  TestRemoteFactory stream_factory;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());

  stream_factory.PrepareProviderForAuthorization(
      session_id, kDeviceId, std::make_unique<TestStreamProvider>(nullptr));
  ipc->RequestDeviceAuthorization(&delegate, session_id, kDeviceId);

  EXPECT_CALL(delegate, OnDeviceAuthorized(
                            media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                            _, std::string(kReturnedDeviceId)));
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate);

  stream_factory.Disconnect();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate);

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();

  for (int i = 0; i < 5; ++i) {
    stream_factory.PrepareProviderForAuthorization(
        session_id, kDeviceId, std::make_unique<TestStreamProvider>(&stream));

    ipc->RequestDeviceAuthorization(&delegate, session_id, kDeviceId);
    ipc->CreateStream(&delegate, Params());

    EXPECT_CALL(
        delegate,
        OnDeviceAuthorized(media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                           _, std::string(kReturnedDeviceId)));
    EXPECT_CALL(delegate, GotOnStreamCreated());
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&delegate);

    EXPECT_CALL(delegate, OnError());
    stream_factory.SignalErrorToProviderClient();
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&delegate);

    ipc->CloseStream();
    base::RunLoop().RunUntilIdle();
  }
}

TEST(MojoAudioOutputIPC, DeviceNotAuthorized_Propagates) {
  test::TaskEnvironment task_environment;
  const base::UnguessableToken session_id = base::UnguessableToken::Create();
  TestRemoteFactory stream_factory;
  StrictMock<MockDelegate> delegate;

  std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  stream_factory.RefuseNextRequest(session_id, kDeviceId);

  ipc->RequestDeviceAuthorization(&delegate, session_id, kDeviceId);

  EXPECT_CALL(
      delegate,
      OnDeviceAuthorized(
          media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED,
          _, std::string()))
      .WillOnce(Invoke([&](media::OutputDeviceStatus,
                           const media::AudioParameters&, const std::string&) {
        ipc->CloseStream();
        ipc.reset();
      }));
  EXPECT_CALL(delegate, OnError()).Times(AtLeast(0));
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC,
     FactoryDisconnectedBeforeAuthorizationReply_CallsAuthorizedAnyways) {
  test::TaskEnvironment task_environment_;
  // The authorization IPC message might be aborted by the remote end
  // disconnecting. In this case, the MojoAudioOutputIPC object must still
  // send a notification to unblock the AudioOutputIPCDelegate.
  const base::UnguessableToken session_id = base::UnguessableToken::Create();
  TestRemoteFactory stream_factory;
  StrictMock<MockDelegate> delegate;

  std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());

  ipc->RequestDeviceAuthorization(&delegate, session_id, kDeviceId);

  EXPECT_CALL(
      delegate,
      OnDeviceAuthorized(
          media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL, _,
          std::string()))
      .WillOnce(Invoke([&](media::OutputDeviceStatus,
                           const media::AudioParameters&, const std::string&) {
        ipc->CloseStream();
        ipc.reset();
      }));
  stream_factory.Disconnect();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC,
     FactoryDisconnectedAfterAuthorizationReply_CallsAuthorizedOnlyOnce) {
  test::TaskEnvironment task_environment_;
  // This test makes sure that the MojoAudioOutputIPC doesn't callback for
  // authorization when the factory disconnects if it already got a callback
  // for authorization.
  const base::UnguessableToken session_id = base::UnguessableToken::Create();
  TestRemoteFactory stream_factory;
  stream_factory.PrepareProviderForAuthorization(
      session_id, kDeviceId, std::make_unique<TestStreamProvider>(nullptr));
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());

  ipc->RequestDeviceAuthorization(&delegate, session_id, kDeviceId);

  EXPECT_CALL(delegate, OnDeviceAuthorized(
                            media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                            _, std::string(kReturnedDeviceId)));
  base::RunLoop().RunUntilIdle();

  stream_factory.Disconnect();
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, AuthorizeNoClose_DCHECKs) {
  test::TaskEnvironment task_environment;
  TestRemoteFactory stream_factory;
  const base::UnguessableToken session_id = base::UnguessableToken::Create();
  StrictMock<MockDelegate> delegate;

  stream_factory.PrepareProviderForAuthorization(
      session_id, kDeviceId, std::make_unique<TestStreamProvider>(nullptr));

  std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());

  ipc->RequestDeviceAuthorization(&delegate, session_id, kDeviceId);
  EXPECT_DCHECK_DEATH(ipc.reset());
  ipc->CloseStream();
  ipc.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, CreateNoClose_DCHECKs) {
  test::TaskEnvironment task_environment;
  TestRemoteFactory stream_factory;
  StrictMock<MockDelegate> delegate;
  StrictMock<MockStream> stream;

  stream_factory.PrepareProviderForAuthorization(
      base::UnguessableToken(),
      std::string(media::AudioDeviceDescription::kDefaultDeviceId),
      std::make_unique<TestStreamProvider>(&stream));

  std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());

  ipc->CreateStream(&delegate, Params());
  EXPECT_DCHECK_DEATH(ipc.reset());
  ipc->CloseStream();
  ipc.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, Play_Plays) {
  test::TaskEnvironment task_environment;
  TestRemoteFactory stream_factory;
  const base::UnguessableToken session_id = base::UnguessableToken::Create();
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;

  EXPECT_CALL(delegate, OnDeviceAuthorized(
                            media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                            _, std::string(kReturnedDeviceId)));
  EXPECT_CALL(delegate, GotOnStreamCreated());
  EXPECT_CALL(stream, Play());

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  stream_factory.PrepareProviderForAuthorization(
      session_id, kDeviceId, std::make_unique<TestStreamProvider>(&stream));

  ipc->RequestDeviceAuthorization(&delegate, session_id, kDeviceId);
  ipc->CreateStream(&delegate, Params());
  base::RunLoop().RunUntilIdle();
  ipc->PlayStream();
  base::RunLoop().RunUntilIdle();
  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, Pause_Pauses) {
  test::TaskEnvironment task_environment;
  TestRemoteFactory stream_factory;
  const base::UnguessableToken session_id = base::UnguessableToken::Create();
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;

  EXPECT_CALL(delegate, OnDeviceAuthorized(
                            media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                            _, std::string(kReturnedDeviceId)));
  EXPECT_CALL(delegate, GotOnStreamCreated());
  EXPECT_CALL(stream, Pause());

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  stream_factory.PrepareProviderForAuthorization(
      session_id, kDeviceId, std::make_unique<TestStreamProvider>(&stream));

  ipc->RequestDeviceAuthorization(&delegate, session_id, kDeviceId);
  ipc->CreateStream(&delegate, Params());
  base::RunLoop().RunUntilIdle();
  ipc->PauseStream();
  base::RunLoop().RunUntilIdle();
  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, SetVolume_SetsVolume) {
  test::TaskEnvironment task_environment;
  TestRemoteFactory stream_factory;
  const base::UnguessableToken session_id = base::UnguessableToken::Create();
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;

  EXPECT_CALL(delegate, OnDeviceAuthorized(
                            media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                            _, std::string(kReturnedDeviceId)));
  EXPECT_CALL(delegate, GotOnStreamCreated());
  EXPECT_CALL(stream, SetVolume(kNewVolume));

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  stream_factory.PrepareProviderForAuthorization(
      session_id, kDeviceId, std::make_unique<TestStreamProvider>(&stream));

  ipc->RequestDeviceAuthorization(&delegate, session_id, kDeviceId);
  ipc->CreateStream(&delegate, Params());
  base::RunLoop().RunUntilIdle();
  ipc->SetVolume(kNewVolume);
  base::RunLoop().RunUntilIdle();
  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

}  // namespace blink
