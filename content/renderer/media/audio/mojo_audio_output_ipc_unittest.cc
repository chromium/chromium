// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio/mojo_audio_output_ipc.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_task_environment.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::Mock;
using testing::StrictMock;

namespace content {

namespace {

const int kSessionId = 1234;
const size_t kMemoryLength = 4321;
const char kDeviceId[] = "device_id";
const char kReturnedDeviceId[] = "returned_device_id";
const double kNewVolume = 0.271828;

media::AudioParameters Params() {
  return media::AudioParameters::UnavailableDeviceParams();
}

MojoAudioOutputIPC::FactoryAccessorCB NullAccessor() {
  return base::BindRepeating(
      []() -> mojom::RendererAudioOutputStreamFactory* { return nullptr; });
}

class TestStreamProvider : public media::mojom::AudioOutputStreamProvider {
 public:
  explicit TestStreamProvider(media::mojom::AudioOutputStream* stream)
      : stream_(stream) {}

  ~TestStreamProvider() override {
    // If we expected a stream to be acquired, make sure it is so.
    if (stream_)
      EXPECT_TRUE(binding_);
  }

  void Acquire(
      const media::AudioParameters& params,
      media::mojom::AudioOutputStreamProviderClientPtr provider_client,
      const base::Optional<base::UnguessableToken>& processing_id) override {
    EXPECT_EQ(binding_, base::nullopt);
    EXPECT_NE(stream_, nullptr);
    std::swap(provider_client, provider_client_);
    media::mojom::AudioOutputStreamPtr stream_ptr;
    binding_.emplace(stream_, mojo::MakeRequest(&stream_ptr));
    base::CancelableSyncSocket foreign_socket;
    EXPECT_TRUE(
        base::CancelableSyncSocket::CreatePair(&socket_, &foreign_socket));
    provider_client_->Created(
        std::move(stream_ptr),
        {base::in_place, base::UnsafeSharedMemoryRegion::Create(kMemoryLength),
         mojo::WrapPlatformFile(foreign_socket.Release())});
  }

  void SignalErrorToProviderClient() {
    provider_client_.ResetWithReason(
        static_cast<uint32_t>(media::mojom::AudioOutputStreamObserver::
                                  DisconnectReason::kPlatformError),
        std::string());
  }

 private:
  media::mojom::AudioOutputStream* stream_;
  media::mojom::AudioOutputStreamProviderClientPtr provider_client_;
  base::Optional<mojo::Binding<media::mojom::AudioOutputStream>> binding_;
  base::CancelableSyncSocket socket_;
};

class TestRemoteFactory : public mojom::RendererAudioOutputStreamFactory {
 public:
  TestRemoteFactory()
      : expect_request_(false),
        binding_(this, mojo::MakeRequest(&this_proxy_)) {}

  ~TestRemoteFactory() override {}

  void RequestDeviceAuthorization(
      media::mojom::AudioOutputStreamProviderRequest stream_provider_request,
      int32_t session_id,
      const std::string& device_id,
      RequestDeviceAuthorizationCallback callback) override {
    EXPECT_EQ(session_id, expected_session_id_);
    EXPECT_EQ(device_id, expected_device_id_);
    EXPECT_TRUE(expect_request_);
    if (provider_) {
      std::move(callback).Run(
          media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK, Params(),
          std::string(kReturnedDeviceId));
      provider_binding_.emplace(provider_.get(),
                                std::move(stream_provider_request));
    } else {
      std::move(callback).Run(
          media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED,
          Params(), std::string(""));
    }
    expect_request_ = false;
  }

  void PrepareProviderForAuthorization(
      int32_t session_id,
      const std::string& device_id,
      std::unique_ptr<TestStreamProvider> provider) {
    EXPECT_FALSE(expect_request_);
    expect_request_ = true;
    expected_session_id_ = session_id;
    expected_device_id_ = device_id;
    provider_binding_.reset();
    std::swap(provider_, provider);
  }

  void RefuseNextRequest(int32_t session_id, const std::string& device_id) {
    EXPECT_FALSE(expect_request_);
    expect_request_ = true;
    expected_session_id_ = session_id;
    expected_device_id_ = device_id;
  }

  void SignalErrorToProviderClient() {
    provider_->SignalErrorToProviderClient();
  }

  void Disconnect() {
    binding_.Close();
    this_proxy_.reset();
    binding_.Bind(mojo::MakeRequest(&this_proxy_));
    provider_binding_.reset();
    provider_.reset();
    expect_request_ = false;
  }

  MojoAudioOutputIPC::FactoryAccessorCB GetAccessor() {
    return base::BindRepeating(&TestRemoteFactory::get, base::Unretained(this));
  }

 private:
  mojom::RendererAudioOutputStreamFactory* get() { return this_proxy_.get(); }

  bool expect_request_;
  int32_t expected_session_id_;
  std::string expected_device_id_;

  mojom::RendererAudioOutputStreamFactoryPtr this_proxy_;
  mojo::Binding<mojom::RendererAudioOutputStreamFactory> binding_;
  std::unique_ptr<TestStreamProvider> provider_;
  base::Optional<mojo::Binding<media::mojom::AudioOutputStreamProvider>>
      provider_binding_;
};

class MockStream : public media::mojom::AudioOutputStream {
 public:
  MOCK_METHOD0(Play, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD1(SetVolume, void(double));
};

class MockDelegate : public media::AudioOutputIPCDelegate {
 public:
  MockDelegate() {}
  ~MockDelegate() override {}

  void OnStreamCreated(base::UnsafeSharedMemoryRegion mem_handle,
                       base::SyncSocket::Handle socket_handle,
                       bool playing_automatically) override {
    base::SyncSocket socket(socket_handle);  // Releases the socket descriptor.
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
  base::test::ScopedTaskEnvironment task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);
  StrictMock<MockDelegate> delegate;

  std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          NullAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());

  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId);

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
  base::test::ScopedTaskEnvironment task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);
  StrictMock<MockDelegate> delegate;

  std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          NullAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());

  ipc->CreateStream(&delegate, Params(), base::nullopt);

  // No call to OnDeviceAuthorized since authotization wasn't explicitly
  // requested.
  base::RunLoop().RunUntilIdle();
  ipc->CloseStream();
}

TEST(MojoAudioOutputIPC, DeviceAuthorized_Propagates) {
  base::test::ScopedTaskEnvironment task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);
  TestRemoteFactory stream_factory;
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  stream_factory.PrepareProviderForAuthorization(
      kSessionId, kDeviceId, std::make_unique<TestStreamProvider>(nullptr));

  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId);

  EXPECT_CALL(delegate, OnDeviceAuthorized(
                            media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK,
                            _, std::string(kReturnedDeviceId)));
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, OnDeviceCreated_Propagates) {
  base::test::ScopedTaskEnvironment task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);
  TestRemoteFactory stream_factory;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  stream_factory.PrepareProviderForAuthorization(
      kSessionId, kDeviceId, std::make_unique<TestStreamProvider>(&stream));

  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId);
  ipc->CreateStream(&delegate, Params(), base::nullopt);

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
  base::test::ScopedTaskEnvironment task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);
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
      0, std::string(media::AudioDeviceDescription::kDefaultDeviceId),
      std::make_unique<TestStreamProvider>(&stream));

  ipc->CreateStream(&delegate, Params(), base::nullopt);

  EXPECT_CALL(delegate, GotOnStreamCreated());
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, IsReusable) {
  base::test::ScopedTaskEnvironment task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);
  TestRemoteFactory stream_factory;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());

  for (int i = 0; i < 5; ++i) {
    stream_factory.PrepareProviderForAuthorization(
        kSessionId, kDeviceId, std::make_unique<TestStreamProvider>(&stream));

    ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId);
    ipc->CreateStream(&delegate, Params(), base::nullopt);

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
  base::test::ScopedTaskEnvironment task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);
  TestRemoteFactory stream_factory;
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());

  stream_factory.PrepareProviderForAuthorization(
      kSessionId, kDeviceId, std::make_unique<TestStreamProvider>(nullptr));
  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId);

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
        kSessionId, kDeviceId, std::make_unique<TestStreamProvider>(&stream));

    ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId);
    ipc->CreateStream(&delegate, Params(), base::nullopt);

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
  base::test::ScopedTaskEnvironment task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);
  TestRemoteFactory stream_factory;
  StrictMock<MockDelegate> delegate;

  std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  stream_factory.RefuseNextRequest(kSessionId, kDeviceId);

  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId);

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
  // The authorization IPC message might be aborted by the remote end
  // disconnecting. In this case, the MojoAudioOutputIPC object must still
  // send a notification to unblock the AudioOutputIPCDelegate.
  base::test::ScopedTaskEnvironment task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);
  TestRemoteFactory stream_factory;
  StrictMock<MockDelegate> delegate;

  std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());

  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId);

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
  // This test makes sure that the MojoAudioOutputIPC doesn't callback for
  // authorization when the factory disconnects if it already got a callback
  // for authorization.
  base::test::ScopedTaskEnvironment task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);
  TestRemoteFactory stream_factory;
  stream_factory.PrepareProviderForAuthorization(
      kSessionId, kDeviceId, std::make_unique<TestStreamProvider>(nullptr));
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());

  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId);

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
  base::test::ScopedTaskEnvironment task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);
  TestRemoteFactory stream_factory;
  StrictMock<MockDelegate> delegate;

  stream_factory.PrepareProviderForAuthorization(
      kSessionId, kDeviceId, std::make_unique<TestStreamProvider>(nullptr));

  std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());

  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId);
  EXPECT_DCHECK_DEATH(ipc.reset());
  ipc->CloseStream();
  ipc.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, CreateNoClose_DCHECKs) {
  base::test::ScopedTaskEnvironment task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);
  TestRemoteFactory stream_factory;
  StrictMock<MockDelegate> delegate;
  StrictMock<MockStream> stream;

  stream_factory.PrepareProviderForAuthorization(
      0, std::string(media::AudioDeviceDescription::kDefaultDeviceId),
      std::make_unique<TestStreamProvider>(&stream));

  std::unique_ptr<media::AudioOutputIPC> ipc =
      std::make_unique<MojoAudioOutputIPC>(
          stream_factory.GetAccessor(),
          blink::scheduler::GetSingleThreadTaskRunnerForTesting());

  ipc->CreateStream(&delegate, Params(), base::nullopt);
  EXPECT_DCHECK_DEATH(ipc.reset());
  ipc->CloseStream();
  ipc.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, Play_Plays) {
  base::test::ScopedTaskEnvironment task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);
  TestRemoteFactory stream_factory;
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
      kSessionId, kDeviceId, std::make_unique<TestStreamProvider>(&stream));

  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId);
  ipc->CreateStream(&delegate, Params(), base::nullopt);
  base::RunLoop().RunUntilIdle();
  ipc->PlayStream();
  base::RunLoop().RunUntilIdle();
  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, Pause_Pauses) {
  base::test::ScopedTaskEnvironment task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);
  TestRemoteFactory stream_factory;
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
      kSessionId, kDeviceId, std::make_unique<TestStreamProvider>(&stream));

  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId);
  ipc->CreateStream(&delegate, Params(), base::nullopt);
  base::RunLoop().RunUntilIdle();
  ipc->PauseStream();
  base::RunLoop().RunUntilIdle();
  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioOutputIPC, SetVolume_SetsVolume) {
  base::test::ScopedTaskEnvironment task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::IO);
  TestRemoteFactory stream_factory;
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
      kSessionId, kDeviceId, std::make_unique<TestStreamProvider>(&stream));

  ipc->RequestDeviceAuthorization(&delegate, kSessionId, kDeviceId);
  ipc->CreateStream(&delegate, Params(), base::nullopt);
  base::RunLoop().RunUntilIdle();
  ipc->SetVolume(kNewVolume);
  base::RunLoop().RunUntilIdle();
  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
