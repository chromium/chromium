// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/audio_track_mojo_encoder.h"

#include <memory>

#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "media/base/audio_encoder.h"
#include "media/base/encoder_status.h"
#include "media/base/test_helpers.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/audio_encoder.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/modules/mediarecorder/audio_track_recorder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

std::unique_ptr<media::AudioBus> GenerateInput() {
  return media::AudioBus::Create(/*channels=*/2, /*frames=*/1024);
}

class TestAudioEncoder final : public media::mojom::AudioEncoder {
 public:
  void FinishInitialization() {
    CHECK(init_cb_);
    std::move(init_cb_).Run(media::EncoderStatus::Codes::kOk);
  }

  void FinishInitializationWithFailed() {
    CHECK(init_cb_);
    std::move(init_cb_).Run(
        media::EncoderStatus::Codes::kEncoderInitializeTwice);
  }

  // media::mojom::AudioEncoder:
  void Initialize(
      mojo::PendingAssociatedRemote<media::mojom::AudioEncoderClient> client,
      const media::AudioEncoderConfig& /*config*/,
      InitializeCallback callback) override {
    client_.Bind(std::move(client));
    init_cb_ = std::move(callback);
  }
  void Encode(media::mojom::AudioBufferPtr /*buffer*/,
              EncodeCallback callback) override {
    constexpr size_t kDataSize = 38;
    auto data = base::HeapArray<uint8_t>::Uninit(kDataSize);
    const std::vector<uint8_t> description;
    client_->OnEncodedBufferReady(
        media::EncodedAudioBuffer(media::TestAudioParameters::Normal(),
                                  std::move(data), base::TimeTicks::Now()),
        description);
    std::move(callback).Run(media::EncoderStatus::Codes::kOk);
  }
  void Flush(FlushCallback /*callback*/) override {}

 private:
  mojo::AssociatedRemote<media::mojom::AudioEncoderClient> client_;
  InitializeCallback init_cb_;
};

class TestInterfaceFactory final : public media::mojom::InterfaceFactory {
 public:
  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(mojo::PendingReceiver<media::mojom::InterfaceFactory>(
        std::move(handle)));
  }

  TestAudioEncoder& audio_encoder() { return audio_encoder_; }

  // media::mojom::InterfaceFactory:
  void CreateAudioEncoder(
      mojo::PendingReceiver<media::mojom::AudioEncoder> receiver) override {
    CHECK(!audio_encoder_receiver_.is_bound())
        << "Expecting at most one encoder instance";
    audio_encoder_receiver_.Bind(std::move(receiver));
  }
  void CreateVideoDecoder(
      mojo::PendingReceiver<media::mojom::VideoDecoder> receiver,
      mojo::PendingRemote<media::stable::mojom::StableVideoDecoder>
          dst_video_decoder) override {
    NOTREACHED_IN_MIGRATION();
  }
#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  void CreateStableVideoDecoder(
      mojo::PendingReceiver<media::stable::mojom::StableVideoDecoder>
          video_decoder) override {
    NOTREACHED_IN_MIGRATION();
  }
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  void CreateAudioDecoder(
      mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) override {
    NOTREACHED_IN_MIGRATION();
  }
  void CreateDefaultRenderer(
      const std::string& audio_device_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) override {
    NOTREACHED_IN_MIGRATION();
  }
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void CreateCastRenderer(
      const base::UnguessableToken& overlay_plane_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) override {
    NOTREACHED_IN_MIGRATION();
  }
#endif
#if BUILDFLAG(IS_ANDROID)
  void CreateMediaPlayerRenderer(
      mojo::PendingRemote<media::mojom::MediaPlayerRendererClientExtension>
          client_extension_remote,
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaPlayerRendererExtension>
          renderer_extension_receiver) override {
    NOTREACHED_IN_MIGRATION();
  }
  void CreateFlingingRenderer(
      const std::string& presentation_id,
      mojo::PendingRemote<media::mojom::FlingingRendererClientExtension>
          client_extension,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) override {
    NOTREACHED_IN_MIGRATION();
  }
#endif  // BUILDFLAG(IS_ANDROID)
  void CreateCdm(const media::CdmConfig& cdm_config,
                 CreateCdmCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }
#if BUILDFLAG(IS_WIN)
  void CreateMediaFoundationRenderer(
      mojo::PendingRemote<media::mojom::MediaLog> media_log_remote,
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaFoundationRendererExtension>
          renderer_extension_receiver,
      mojo::PendingRemote<
          ::media::mojom::MediaFoundationRendererClientExtension>
          client_extension_remote) override {
    NOTREACHED_IN_MIGRATION();
  }
#endif  // BUILDFLAG(IS_WIN)

 private:
  TestAudioEncoder audio_encoder_;
  mojo::Receiver<media::mojom::AudioEncoder> audio_encoder_receiver_{
      &audio_encoder_};
  mojo::Receiver<media::mojom::InterfaceFactory> receiver_{this};
};

}  // namespace

class AudioTrackMojoEncoderTest : public testing::Test {
 public:
  AudioTrackMojoEncoderTest() {
    CHECK(Platform::Current()->GetBrowserInterfaceBroker()->SetBinderForTesting(
        media::mojom::InterfaceFactory::Name_,
        WTF::BindRepeating(&TestInterfaceFactory::BindRequest,
                           base::Unretained(&interface_factory_))));

    audio_track_encoder_.OnSetFormat(media::TestAudioParameters::Normal());
    // Progress until TestAudioEncoder receives the Initialize() call.
    base::RunLoop().RunUntilIdle();
  }

  ~AudioTrackMojoEncoderTest() override {
    Platform::Current()->GetBrowserInterfaceBroker()->SetBinderForTesting(
        media::mojom::InterfaceFactory::Name_, {});
  }

  TestAudioEncoder& audio_encoder() {
    return interface_factory_.audio_encoder();
  }
  AudioTrackMojoEncoder& audio_track_encoder() { return audio_track_encoder_; }
  int output_count() const { return output_count_; }
  media::EncoderStatus::Codes error_code() const { return error_code_; }

 private:
  test::TaskEnvironment task_environment_;
  TestInterfaceFactory interface_factory_;
  int output_count_ = 0;
  media::EncoderStatus::Codes error_code_ = media::EncoderStatus::Codes::kOk;

  AudioTrackMojoEncoder audio_track_encoder_{
      scheduler::GetSequencedTaskRunnerForTesting(),
      AudioTrackRecorder::CodecId::kAac,
      /*on_encoded_audio_cb=*/
      base::BindLambdaForTesting(
          [this](const media::AudioParameters& /*params*/,
                 scoped_refptr<media::DecoderBuffer> /*encoded_data*/,
                 std::optional<
                     media::AudioEncoder::CodecDescription> /*codec_desc*/,
                 base::TimeTicks /*capture_time*/) { ++output_count_; }),
      /*on_encoded_audio_error_cb=*/
      base::BindLambdaForTesting([this](media::EncoderStatus status) {
        ASSERT_EQ(error_code_, media::EncoderStatus::Codes::kOk);
        ASSERT_FALSE(status.is_ok());
        error_code_ = status.code();
      })};
};

TEST_F(AudioTrackMojoEncoderTest, InputArrivingAfterInitialization) {
  audio_encoder().FinishInitialization();
  base::RunLoop().RunUntilIdle();

  audio_track_encoder().EncodeAudio(GenerateInput(), base::TimeTicks::Now());
  audio_track_encoder().EncodeAudio(GenerateInput(), base::TimeTicks::Now());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(output_count(), 2);
}

TEST_F(AudioTrackMojoEncoderTest, InputArrivingWhileUninitialized) {
  audio_track_encoder().EncodeAudio(GenerateInput(), base::TimeTicks::Now());

  audio_encoder().FinishInitialization();
  base::RunLoop().RunUntilIdle();

  audio_track_encoder().EncodeAudio(GenerateInput(), base::TimeTicks::Now());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(output_count(), 2);
}

TEST_F(AudioTrackMojoEncoderTest, PausedAfterInitialization) {
  audio_encoder().FinishInitialization();
  base::RunLoop().RunUntilIdle();

  audio_track_encoder().set_paused(true);

  audio_track_encoder().EncodeAudio(GenerateInput(), base::TimeTicks::Now());
  audio_track_encoder().EncodeAudio(GenerateInput(), base::TimeTicks::Now());

  audio_track_encoder().set_paused(false);

  audio_track_encoder().EncodeAudio(GenerateInput(), base::TimeTicks::Now());
  audio_track_encoder().EncodeAudio(GenerateInput(), base::TimeTicks::Now());

  audio_track_encoder().set_paused(true);

  audio_track_encoder().EncodeAudio(GenerateInput(), base::TimeTicks::Now());
  audio_track_encoder().EncodeAudio(GenerateInput(), base::TimeTicks::Now());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(output_count(), 2);
}

TEST_F(AudioTrackMojoEncoderTest, PausedWhileUninitialized) {
  audio_track_encoder().set_paused(true);

  audio_track_encoder().EncodeAudio(GenerateInput(), base::TimeTicks::Now());
  audio_track_encoder().EncodeAudio(GenerateInput(), base::TimeTicks::Now());

  audio_track_encoder().set_paused(false);

  audio_track_encoder().EncodeAudio(GenerateInput(), base::TimeTicks::Now());
  audio_track_encoder().EncodeAudio(GenerateInput(), base::TimeTicks::Now());

  audio_track_encoder().set_paused(true);

  audio_track_encoder().EncodeAudio(GenerateInput(), base::TimeTicks::Now());
  audio_track_encoder().EncodeAudio(GenerateInput(), base::TimeTicks::Now());

  audio_encoder().FinishInitialization();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(output_count(), 2);
}

TEST_F(AudioTrackMojoEncoderTest, OnSetFormatError) {
  audio_encoder().FinishInitialization();
  media::AudioParameters invalid_params = media::TestAudioParameters::Normal();
  invalid_params.set_sample_rate(0);
  audio_track_encoder().OnSetFormat(invalid_params);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(error_code(),
            media::EncoderStatus::Codes::kEncoderUnsupportedConfig);
}

TEST_F(AudioTrackMojoEncoderTest, EncoderInitializationError) {
  audio_encoder().FinishInitializationWithFailed();
  base::RunLoop().RunUntilIdle();

  audio_track_encoder().EncodeAudio(GenerateInput(), base::TimeTicks::Now());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(output_count(), 0);
  EXPECT_EQ(error_code(), media::EncoderStatus::Codes::kEncoderInitializeTwice);
}

}  // namespace blink
