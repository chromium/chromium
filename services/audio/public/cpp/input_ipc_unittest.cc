// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/input_ipc.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/audio/public/cpp/device_factory.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "services/audio/public/mojom/audio_processing.mojom.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrictMock;

namespace audio {

namespace {

const char kDeviceId[] = "1234";
const size_t kShMemSize = 456;
const double kNewVolume = 0.271828;

class MockStream : public media::mojom::AudioInputStream {
 public:
  MOCK_METHOD0(Record, void());
  MOCK_METHOD1(SetVolume, void(double));
};

class TestStreamFactory : public audio::FakeStreamFactory {
 public:
  TestStreamFactory() : stream_(), stream_receiver_(&stream_) {}
  ~TestStreamFactory() override = default;
  void CreateInputStream(
      mojo::PendingReceiver<media::mojom::AudioInputStream> stream_receiver,
      mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
      mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      bool enable_agc,
      mojo::ScopedSharedBufferHandle key_press_count_buffer,
      mojom::AudioProcessingConfigPtr processing_config,
      CreateInputStreamCallback created_callback) {
    if (should_fail_) {
      std::move(created_callback).Run(nullptr, initially_muted_, base::nullopt);
      return;
    }

    if (client_)
      client_.reset();
    // Keep the passed client alive to avoid binding errors.
    client_.Bind(std::move(client));

    if (stream_receiver_.is_bound())
      stream_receiver_.reset();
    stream_receiver_.Bind(std::move(stream_receiver));

    base::SyncSocket socket1, socket2;
    base::SyncSocket::CreatePair(&socket1, &socket2);
    auto h = mojo::SharedBufferHandle::Create(kShMemSize);
    std::move(created_callback)
        .Run({base::in_place,
              base::ReadOnlySharedMemoryRegion::Create(kShMemSize).region,
              mojo::WrapPlatformFile(socket1.Release())},
             initially_muted_, base::UnguessableToken::Create());
  }

  MOCK_METHOD2(AssociateInputAndOutputForAec,
               void(const base::UnguessableToken& input_stream_id,
                    const std::string& output_device_id));

  mojo::PendingRemote<audio::mojom::StreamFactory> MakeRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  StrictMock<MockStream> stream_;
  mojo::Remote<media::mojom::AudioInputStreamClient> client_;
  mojo::Receiver<media::mojom::AudioInputStream> stream_receiver_;
  bool initially_muted_ = true;
  bool should_fail_ = false;
};

class MockDelegate : public media::AudioInputIPCDelegate {
 public:
  MockDelegate() {}
  ~MockDelegate() override {}

  void OnStreamCreated(base::ReadOnlySharedMemoryRegion mem_handle,
                       base::SyncSocket::Handle socket_handle,
                       bool initially_muted) override {
    base::SyncSocket socket(socket_handle);  // Releases the socket descriptor.
    GotOnStreamCreated(initially_muted);
  }

  MOCK_METHOD1(GotOnStreamCreated, void(bool initially_muted));
  MOCK_METHOD0(OnError, void());
  MOCK_METHOD1(OnMuted, void(bool));
  MOCK_METHOD0(OnIPCClosed, void());
};

class InputIPCTest : public ::testing::Test {
 public:
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<audio::InputIPC> ipc;
  const media::AudioParameters audioParameters =
      media::AudioParameters(media::AudioParameters::AUDIO_PCM_LINEAR,
                             media::CHANNEL_LAYOUT_STEREO,
                             16000,
                             1600);

 protected:
  InputIPCTest()
      : task_environment(
            base::test::TaskEnvironment::MainThreadType::DEFAULT,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}
  std::unique_ptr<StrictMock<TestStreamFactory>> factory_;

  void SetUp() override {
    factory_ = std::make_unique<StrictMock<TestStreamFactory>>();
    ipc = std::make_unique<InputIPC>(factory_->MakeRemote(), kDeviceId,
                                     mojo::NullRemote());
  }
};

}  // namespace

TEST_F(InputIPCTest, CreateStreamPropagates) {
  StrictMock<MockDelegate> delegate;
  EXPECT_CALL(delegate, GotOnStreamCreated(_));
  ipc->CreateStream(&delegate, audioParameters, false, 0);
  task_environment.RunUntilIdle();
}

TEST_F(InputIPCTest, StreamCreatedAfterCloseIsIgnored) {
  StrictMock<MockDelegate> delegate;
  ipc->CreateStream(&delegate, audioParameters, false, 0);
  ipc->CloseStream();
  task_environment.RunUntilIdle();
}

TEST_F(InputIPCTest, CreateStreamPropagatesInitiallyMuted) {
  StrictMock<MockDelegate> delegate;

  factory_->initially_muted_ = true;
  EXPECT_CALL(delegate, GotOnStreamCreated(true));
  ipc->CreateStream(&delegate, audioParameters, false, 0);
  task_environment.RunUntilIdle();
  ipc->CloseStream();
  task_environment.RunUntilIdle();

  factory_->initially_muted_ = false;
  EXPECT_CALL(delegate, GotOnStreamCreated(false));
  ipc->CreateStream(&delegate, audioParameters, false, 0);
  task_environment.RunUntilIdle();
  ipc->CloseStream();
  task_environment.RunUntilIdle();
}

TEST_F(InputIPCTest, MutedStateChangesPropagates) {
  StrictMock<MockDelegate> delegate;

  EXPECT_CALL(delegate, GotOnStreamCreated(_));
  ipc->CreateStream(&delegate, audioParameters, false, 0);
  task_environment.RunUntilIdle();

  EXPECT_CALL(delegate, OnMuted(true));
  factory_->client_->OnMutedStateChanged(true);
  task_environment.RunUntilIdle();

  EXPECT_CALL(delegate, OnMuted(false));
  factory_->client_->OnMutedStateChanged(false);
  task_environment.RunUntilIdle();
}

TEST_F(InputIPCTest, Record_Records) {
  StrictMock<MockDelegate> delegate;
  EXPECT_CALL(delegate, GotOnStreamCreated(_));
  ipc->CreateStream(&delegate, audioParameters, false, 0);
  task_environment.RunUntilIdle();

  EXPECT_CALL(factory_->stream_, Record());
  ipc->RecordStream();
  task_environment.RunUntilIdle();
}

TEST_F(InputIPCTest, IsReusable) {
  for (int i = 0; i < 5; i++) {
    StrictMock<MockDelegate> delegate;
    EXPECT_CALL(delegate, GotOnStreamCreated(_));
    ipc->CreateStream(&delegate, audioParameters, false, 0);
    task_environment.RunUntilIdle();

    ipc->CloseStream();
    task_environment.RunUntilIdle();

    testing::Mock::VerifyAndClearExpectations(&delegate);
  }
}

TEST_F(InputIPCTest, SetVolume_SetsVolume) {
  StrictMock<MockDelegate> delegate;
  EXPECT_CALL(delegate, GotOnStreamCreated(_));
  ipc->CreateStream(&delegate, audioParameters, false, 0);
  task_environment.RunUntilIdle();

  EXPECT_CALL(factory_->stream_, SetVolume(kNewVolume));
  ipc->SetVolume(kNewVolume);
  task_environment.RunUntilIdle();
}

TEST_F(InputIPCTest, SetOutputDeviceForAec_AssociatesInputAndOutputForAec) {
  const std::string kOutputDeviceId = "2345";
  StrictMock<MockDelegate> delegate;
  EXPECT_CALL(delegate, GotOnStreamCreated(_));
  ipc->CreateStream(&delegate, audioParameters, false, 0);
  task_environment.RunUntilIdle();

  EXPECT_CALL(*factory_, AssociateInputAndOutputForAec(_, kOutputDeviceId));
  ipc->SetOutputDeviceForAec(kOutputDeviceId);
  task_environment.RunUntilIdle();
}

TEST_F(InputIPCTest, FailedStreamCreationNullCallback) {
  StrictMock<MockDelegate> delegate;
  EXPECT_CALL(delegate, OnError()).Times(2);
  factory_->should_fail_ = true;
  ipc->CreateStream(&delegate, audioParameters, false, 0);
  task_environment.RunUntilIdle();
}

TEST_F(InputIPCTest, FailedStreamCreationDestuctedFactory) {
  StrictMock<MockDelegate> delegate;
  EXPECT_CALL(delegate, OnError());
  factory_ = nullptr;
  ipc->CreateStream(&delegate, audioParameters, false, 0);
  task_environment.RunUntilIdle();
}

}  // namespace audio
