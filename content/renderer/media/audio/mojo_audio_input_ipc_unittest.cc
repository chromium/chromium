// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio/mojo_audio_input_ipc.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::Mock;
using testing::StrictMock;

namespace content {

namespace {

const size_t kMemoryLength = 4321;
const size_t kTotalSegments = 1;
const double kNewVolume = 0.271828;
const char kOutputDeviceId[] = "2345";

media::AudioParameters Params() {
  return media::AudioParameters::UnavailableDeviceParams();
}

media::AudioSourceParameters SourceParams() {
  return media::AudioSourceParameters(
      base::UnguessableToken::Deserialize(1234, 5678));
}

class MockStream : public media::mojom::AudioInputStream {
 public:
  MOCK_METHOD0(Record, void());
  MOCK_METHOD1(SetVolume, void(double));
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

class FakeStreamCreator {
 public:
  FakeStreamCreator(media::mojom::AudioInputStream* stream,
                    bool initially_muted)
      : stream_(stream),
        receiver_(stream_),
        initially_muted_(initially_muted) {}

  void Create(const media::AudioSourceParameters& source_params,
              mojo::PendingRemote<mojom::RendererAudioInputStreamFactoryClient>
                  factory_client,
              mojo::PendingReceiver<audio::mojom::AudioProcessorControls>
                  controls_receiver,
              const media::AudioParameters& params,
              bool automatic_gain_control,
              uint32_t total_segments) {
    EXPECT_FALSE(receiver_.is_bound());
    EXPECT_NE(stream_, nullptr);
    EXPECT_EQ(source_params.session_id, SourceParams().session_id);
    factory_client_.reset();
    factory_client_.Bind(std::move(factory_client));
    base::CancelableSyncSocket foreign_socket;
    EXPECT_TRUE(
        base::CancelableSyncSocket::CreatePair(&socket_, &foreign_socket));
    factory_client_->StreamCreated(
        receiver_.BindNewPipeAndPassRemote(),
        stream_client_.BindNewPipeAndPassReceiver(),
        {base::in_place,
         base::ReadOnlySharedMemoryRegion::Create(kMemoryLength).region,
         mojo::WrapPlatformFile(foreign_socket.Release())},
        initially_muted_, base::UnguessableToken::Create());
  }

  MojoAudioInputIPC::StreamCreatorCB GetCallback() {
    return base::BindRepeating(&FakeStreamCreator::Create,
                               base::Unretained(this));
  }

  void Rearm() {
    stream_client_.reset();
    receiver_.reset();
    socket_.Close();
  }

  void SignalError() {
    ASSERT_TRUE(stream_client_);
    stream_client_->OnError();
  }

 private:
  media::mojom::AudioInputStream* stream_;
  mojo::Remote<media::mojom::AudioInputStreamClient> stream_client_;
  mojo::Remote<mojom::RendererAudioInputStreamFactoryClient> factory_client_;
  mojo::Receiver<media::mojom::AudioInputStream> receiver_;
  bool initially_muted_;
  base::CancelableSyncSocket socket_;
};

void AssociateOutputForAec(const base::UnguessableToken& stream_id,
                           const std::string& output_device_id) {
  EXPECT_FALSE(stream_id.is_empty());
  EXPECT_EQ(output_device_id, kOutputDeviceId);
}

}  // namespace

TEST(MojoAudioInputIPC, OnStreamCreated_Propagates) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, false);

  const std::unique_ptr<media::AudioInputIPC> ipc =
      std::make_unique<MojoAudioInputIPC>(
          SourceParams(), creator.GetCallback(),
          base::BindRepeating(&AssociateOutputForAec));

  EXPECT_CALL(delegate, GotOnStreamCreated(false));

  ipc->CreateStream(&delegate, Params(), false, kTotalSegments);
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioInputIPC, FactoryDisconnected_SendsError) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioInputIPC> ipc = std::make_unique<
      MojoAudioInputIPC>(
      SourceParams(),
      base::BindRepeating(
          [](const media::AudioSourceParameters&,
             mojo::PendingRemote<mojom::RendererAudioInputStreamFactoryClient>
                 factory_client,
             mojo::PendingReceiver<audio::mojom::AudioProcessorControls>
                 controls_receiver,
             const media::AudioParameters& params, bool automatic_gain_control,
             uint32_t total_segments) {}),
      base::BindRepeating(&AssociateOutputForAec));

  EXPECT_CALL(delegate, OnError());

  ipc->CreateStream(&delegate, Params(), false, kTotalSegments);
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioInputIPC, OnStreamCreated_PropagatesInitiallyMuted) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, true);

  const std::unique_ptr<media::AudioInputIPC> ipc =
      std::make_unique<MojoAudioInputIPC>(
          SourceParams(), creator.GetCallback(),
          base::BindRepeating(&AssociateOutputForAec));

  EXPECT_CALL(delegate, GotOnStreamCreated(true));

  ipc->CreateStream(&delegate, Params(), false, kTotalSegments);
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioInputIPC, IsReusable) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, false);

  const std::unique_ptr<media::AudioInputIPC> ipc =
      std::make_unique<MojoAudioInputIPC>(
          SourceParams(), creator.GetCallback(),
          base::BindRepeating(&AssociateOutputForAec));

  for (int i = 0; i < 5; ++i) {
    creator.Rearm();

    EXPECT_CALL(delegate, GotOnStreamCreated(_));

    ipc->CreateStream(&delegate, Params(), false, kTotalSegments);
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&delegate);

    ipc->CloseStream();
    base::RunLoop().RunUntilIdle();
  }
}

TEST(MojoAudioInputIPC, IsReusableAfterError) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, false);

  const std::unique_ptr<media::AudioInputIPC> ipc =
      std::make_unique<MojoAudioInputIPC>(
          SourceParams(), creator.GetCallback(),
          base::BindRepeating(&AssociateOutputForAec));

  for (int i = 0; i < 5; ++i) {
    creator.Rearm();

    EXPECT_CALL(delegate, GotOnStreamCreated(_));

    ipc->CreateStream(&delegate, Params(), false, kTotalSegments);
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&delegate);

    EXPECT_CALL(delegate, OnError());
    creator.SignalError();
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&delegate);

    ipc->CloseStream();
    base::RunLoop().RunUntilIdle();
  }
}

TEST(MojoAudioInputIPC, Record_Records) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, false);

  const std::unique_ptr<media::AudioInputIPC> ipc =
      std::make_unique<MojoAudioInputIPC>(
          SourceParams(), creator.GetCallback(),
          base::BindRepeating(&AssociateOutputForAec));

  EXPECT_CALL(delegate, GotOnStreamCreated(_));
  EXPECT_CALL(stream, Record());

  ipc->CreateStream(&delegate, Params(), false, kTotalSegments);
  base::RunLoop().RunUntilIdle();
  ipc->RecordStream();
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioInputIPC, SetVolume_SetsVolume) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, false);

  const std::unique_ptr<media::AudioInputIPC> ipc =
      std::make_unique<MojoAudioInputIPC>(
          SourceParams(), creator.GetCallback(),
          base::BindRepeating(&AssociateOutputForAec));

  EXPECT_CALL(delegate, GotOnStreamCreated(_));
  EXPECT_CALL(stream, SetVolume(kNewVolume));

  ipc->CreateStream(&delegate, Params(), false, kTotalSegments);
  base::RunLoop().RunUntilIdle();
  ipc->SetVolume(kNewVolume);
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioInputIPC, SetOutputDeviceForAec_AssociatesInputAndOutputForAec) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  StrictMock<MockStream> stream;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, false);

  const std::unique_ptr<media::AudioInputIPC> ipc =
      std::make_unique<MojoAudioInputIPC>(
          SourceParams(), creator.GetCallback(),
          base::BindRepeating(&AssociateOutputForAec));

  EXPECT_CALL(delegate, GotOnStreamCreated(_));

  ipc->CreateStream(&delegate, Params(), false, kTotalSegments);
  base::RunLoop().RunUntilIdle();
  ipc->SetOutputDeviceForAec(kOutputDeviceId);
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
