// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media/audio/mojo_audio_input_ipc.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom-blink.h"
#include "media/mojo/mojom/audio_processing.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::Mock;
using testing::StrictMock;

namespace blink {

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
      base::UnguessableToken::CreateForTesting(1234, 5678));
}

media::AudioSourceParameters SourceParamsWithProcessing() {
  media::AudioSourceParameters params(
      base::UnguessableToken::CreateForTesting(1234, 5678));
  params.processing = media::AudioProcessingSettings();
  return params;
}

class MockStream : public media::mojom::blink::AudioInputStream {
 public:
  MOCK_METHOD0(Record, void());
  MOCK_METHOD1(SetVolume, void(double));
};

class MockAudioProcessorControls
    : public media::mojom::blink::AudioProcessorControls {
 public:
  void GetStats(GetStatsCallback cb) override {
    GetStatsCalled();
    std::move(cb).Run(media::AudioProcessingStats());
  }
  MOCK_METHOD0(GetStatsCalled, void());
  MOCK_METHOD1(SetPreferredNumCaptureChannels, void(int32_t));
};

class MockDelegate : public media::AudioInputIPCDelegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  void OnStreamCreated(base::ReadOnlySharedMemoryRegion mem_handle,
                       base::SyncSocket::ScopedHandle socket_handle,
                       bool initially_muted) override {
    GotOnStreamCreated(initially_muted);
  }

  MOCK_METHOD1(GotOnStreamCreated, void(bool initially_muted));
  MOCK_METHOD1(OnError, void(media::AudioCapturerSource::ErrorCode));
  MOCK_METHOD1(OnMuted, void(bool));
  MOCK_METHOD0(OnIPCClosed, void());
};

class FakeStreamCreator {
 public:
  FakeStreamCreator(media::mojom::blink::AudioInputStream* stream,
                    media::mojom::blink::AudioProcessorControls* controls,
                    bool initially_muted,
                    bool expect_processing_config = false)
      : receiver_(stream),
        controls_receiver_(controls),
        initially_muted_(initially_muted),
        expect_processing_config_(expect_processing_config) {}

  void Create(
      const media::AudioSourceParameters& source_params,
      mojo::PendingRemote<mojom::blink::RendererAudioInputStreamFactoryClient>
          factory_client,
      mojo::PendingReceiver<media::mojom::blink::AudioProcessorControls>
          pending_controls_receiver,
      const media::AudioParameters& params,
      bool automatic_gain_control,
      uint32_t total_segments) {
    EXPECT_FALSE(receiver_.is_bound());
    EXPECT_EQ(source_params.session_id, SourceParams().session_id);
    factory_client_.reset();
    factory_client_.Bind(std::move(factory_client));
    base::CancelableSyncSocket foreign_socket;
    EXPECT_TRUE(
        base::CancelableSyncSocket::CreatePair(&socket_, &foreign_socket));

    EXPECT_EQ(!!pending_controls_receiver, expect_processing_config_);
    if (pending_controls_receiver)
      controls_receiver_.Bind(std::move(pending_controls_receiver));

    factory_client_->StreamCreated(
        receiver_.BindNewPipeAndPassRemote(),
        stream_client_.BindNewPipeAndPassReceiver(),
        {std::in_place,
         base::ReadOnlySharedMemoryRegion::Create(kMemoryLength).region,
         mojo::PlatformHandle(foreign_socket.Take())},
        initially_muted_, base::UnguessableToken::Create());
  }

  MojoAudioInputIPC::StreamCreatorCB GetCallback() {
    return base::BindRepeating(&FakeStreamCreator::Create,
                               base::Unretained(this));
  }

  void Rearm() {
    stream_client_.reset();
    receiver_.reset();
    controls_receiver_.reset();
    socket_.Close();
  }

  void SignalError() {
    ASSERT_TRUE(stream_client_);
    stream_client_->OnError(media::mojom::InputStreamErrorCode::kUnknown);
  }

 private:
  mojo::Remote<media::mojom::blink::AudioInputStreamClient> stream_client_;
  mojo::Remote<mojom::blink::RendererAudioInputStreamFactoryClient>
      factory_client_;
  mojo::Receiver<media::mojom::blink::AudioInputStream> receiver_;
  mojo::Receiver<media::mojom::blink::AudioProcessorControls>
      controls_receiver_;
  bool initially_muted_;
  bool expect_processing_config_;
  base::CancelableSyncSocket socket_;
};

void AssociateOutputForAec(const base::UnguessableToken& stream_id,
                           const std::string& output_device_id) {
  EXPECT_FALSE(stream_id.is_empty());
  EXPECT_EQ(output_device_id, kOutputDeviceId);
}

}  // namespace

TEST(MojoAudioInputIPC, OnStreamCreated_Propagates) {
  test::TaskEnvironment task_environment;
  StrictMock<MockStream> stream;
  StrictMock<MockAudioProcessorControls> controls;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, &controls, false);

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

TEST(MojoAudioInputIPC, OnStreamCreated_Propagates_WithProcessingConfig) {
  test::TaskEnvironment task_environment;
  StrictMock<MockStream> stream;
  StrictMock<MockAudioProcessorControls> controls;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, &controls, false,
                            /*expect_processing_config*/ true);

  const std::unique_ptr<media::AudioInputIPC> ipc =
      std::make_unique<MojoAudioInputIPC>(
          SourceParamsWithProcessing(), creator.GetCallback(),
          base::BindRepeating(&AssociateOutputForAec));

  EXPECT_CALL(delegate, GotOnStreamCreated(false));

  ipc->CreateStream(&delegate, Params(), false, kTotalSegments);
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioInputIPC, FactoryDisconnected_SendsError) {
  test::TaskEnvironment task_environment;
  StrictMock<MockDelegate> delegate;

  const std::unique_ptr<media::AudioInputIPC> ipc = std::make_unique<
      MojoAudioInputIPC>(
      SourceParams(),
      base::BindRepeating(
          [](const media::AudioSourceParameters&,
             mojo::PendingRemote<
                 mojom::blink::RendererAudioInputStreamFactoryClient>
                 factory_client,
             mojo::PendingReceiver<media::mojom::blink::AudioProcessorControls>
                 controls_receiver,
             const media::AudioParameters& params, bool automatic_gain_control,
             uint32_t total_segments) {}),
      base::BindRepeating(&AssociateOutputForAec));

  EXPECT_CALL(delegate,
              OnError(media::AudioCapturerSource::ErrorCode::kUnknown));

  ipc->CreateStream(&delegate, Params(), false, kTotalSegments);
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioInputIPC, OnStreamCreated_PropagatesInitiallyMuted) {
  test::TaskEnvironment task_environment;
  StrictMock<MockStream> stream;
  StrictMock<MockAudioProcessorControls> controls;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, &controls, true);

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
  test::TaskEnvironment task_environment;
  StrictMock<MockStream> stream;
  StrictMock<MockAudioProcessorControls> controls;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, &controls, false);

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
  test::TaskEnvironment task_environment;
  StrictMock<MockStream> stream;
  StrictMock<MockAudioProcessorControls> controls;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, &controls, false);

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

    EXPECT_CALL(delegate,
                OnError(media::AudioCapturerSource::ErrorCode::kUnknown));
    creator.SignalError();
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(&delegate);

    ipc->CloseStream();
    base::RunLoop().RunUntilIdle();
  }
}

TEST(MojoAudioInputIPC, Record_Records) {
  test::TaskEnvironment task_environment;
  StrictMock<MockStream> stream;
  StrictMock<MockAudioProcessorControls> controls;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, &controls, false);

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
  test::TaskEnvironment task_environment;
  StrictMock<MockStream> stream;
  StrictMock<MockAudioProcessorControls> controls;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, &controls, false);

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
  test::TaskEnvironment task_environment;
  StrictMock<MockStream> stream;
  StrictMock<MockAudioProcessorControls> controls;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, &controls, false);

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

TEST(MojoAudioInputIPC,
     Controls_NotCalled_BeforeStreamCreated_WithoutProcessing) {
  test::TaskEnvironment task_environment;
  StrictMock<MockStream> stream;
  StrictMock<MockAudioProcessorControls> controls;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, &controls, false);

  const std::unique_ptr<media::AudioInputIPC> ipc =
      std::make_unique<MojoAudioInputIPC>(
          SourceParams(), creator.GetCallback(),
          base::BindRepeating(&AssociateOutputForAec));

  // StrictMock will verify that no calls are made to |controls|.
  media::AudioProcessorControls* media_controls = ipc->GetProcessorControls();
  media_controls->SetPreferredNumCaptureChannels(1);
  media_controls->GetStats(media::AudioProcessorControls::GetStatsCB());
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioInputIPC,
     Controls_NotCalled_AfterStreamCreated_WithoutProcessing) {
  test::TaskEnvironment task_environment;
  StrictMock<MockStream> stream;
  StrictMock<MockAudioProcessorControls> controls;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, &controls, false);

  const std::unique_ptr<media::AudioInputIPC> ipc =
      std::make_unique<MojoAudioInputIPC>(
          SourceParams(), creator.GetCallback(),
          base::BindRepeating(&AssociateOutputForAec));

  media::AudioProcessorControls* media_controls = ipc->GetProcessorControls();

  EXPECT_CALL(delegate, GotOnStreamCreated(_));

  ipc->CreateStream(&delegate, Params(), false, kTotalSegments);
  base::RunLoop().RunUntilIdle();

  // StrictMock will verify that no calls are made to |controls|.
  media_controls->SetPreferredNumCaptureChannels(1);
  media_controls->GetStats(media::AudioProcessorControls::GetStatsCB());
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioInputIPC, Controls_NotCalled_BeforeStreamCreated_WithProcessing) {
  test::TaskEnvironment task_environment;
  StrictMock<MockStream> stream;
  StrictMock<MockAudioProcessorControls> controls;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, &controls, false,
                            /*expect_processing_config*/ true);

  const std::unique_ptr<media::AudioInputIPC> ipc =
      std::make_unique<MojoAudioInputIPC>(
          SourceParamsWithProcessing(), creator.GetCallback(),
          base::BindRepeating(&AssociateOutputForAec));

  // StrictMock will verify that no calls are made to |controls|.
  media::AudioProcessorControls* media_controls = ipc->GetProcessorControls();
  media_controls->SetPreferredNumCaptureChannels(1);
  media_controls->GetStats(media::AudioProcessorControls::GetStatsCB());
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioInputIPC, Controls_Called_AfterStreamCreated_WithProcessing) {
  test::TaskEnvironment task_environment;
  StrictMock<MockStream> stream;
  StrictMock<MockAudioProcessorControls> controls;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, &controls, false,
                            /*expect_processing_config*/ true);

  const std::unique_ptr<media::AudioInputIPC> ipc =
      std::make_unique<MojoAudioInputIPC>(
          SourceParamsWithProcessing(), creator.GetCallback(),
          base::BindRepeating(&AssociateOutputForAec));

  media::AudioProcessorControls* media_controls = ipc->GetProcessorControls();

  EXPECT_CALL(delegate, GotOnStreamCreated(_));
  EXPECT_CALL(controls, SetPreferredNumCaptureChannels(1));
  EXPECT_CALL(controls, GetStatsCalled());

  ipc->CreateStream(&delegate, Params(), false, kTotalSegments);
  base::RunLoop().RunUntilIdle();

  media_controls->SetPreferredNumCaptureChannels(1);
  media_controls->GetStats(
      base::BindOnce([](const media::AudioProcessingStats& stats) {}));
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();
}

TEST(MojoAudioInputIPC, Controls_NotCalled_AfterStreamClosed_WithProcessing) {
  test::TaskEnvironment task_environment;
  StrictMock<MockStream> stream;
  StrictMock<MockAudioProcessorControls> controls;
  StrictMock<MockDelegate> delegate;
  FakeStreamCreator creator(&stream, &controls, false,
                            /*expect_processing_config*/ true);

  const std::unique_ptr<media::AudioInputIPC> ipc =
      std::make_unique<MojoAudioInputIPC>(
          SourceParamsWithProcessing(), creator.GetCallback(),
          base::BindRepeating(&AssociateOutputForAec));

  media::AudioProcessorControls* media_controls = ipc->GetProcessorControls();

  EXPECT_CALL(delegate, GotOnStreamCreated(_));

  ipc->CreateStream(&delegate, Params(), false, kTotalSegments);
  base::RunLoop().RunUntilIdle();

  ipc->CloseStream();
  base::RunLoop().RunUntilIdle();

  // StrictMock will verify that no calls are made to |controls|.
  media_controls->SetPreferredNumCaptureChannels(1);
  media_controls->GetStats(media::AudioProcessorControls::GetStatsCB());
  base::RunLoop().RunUntilIdle();
}

}  // namespace blink
