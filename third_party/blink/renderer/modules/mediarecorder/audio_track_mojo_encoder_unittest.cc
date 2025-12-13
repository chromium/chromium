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
#include "media/base/audio_bus.h"
#include "media/base/audio_encoder.h"
#include "media/base/audio_timestamp_helper.h"
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

using ::testing::ElementsAre;

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
  void Encode(media::mojom::AudioBufferPtr buffer,
              EncodeCallback callback) override {
    constexpr size_t kDataSize = 38;
    auto data = base::HeapArray<uint8_t>::Uninit(kDataSize);
    const std::vector<uint8_t> description;

    auto capture_timestamp = base::TimeTicks() + buffer->timestamp;
    if (!timestamp_helper_) {
      timestamp_helper_ =
          std::make_unique<media::AudioTimestampHelper>(buffer->sample_rate);
      timestamp_helper_->SetBaseTimestamp(capture_timestamp -
                                          base::TimeTicks());
    }
    client_->OnEncodedBufferReady(
        media::EncodedAudioBuffer(
            media::TestAudioParameters::Normal(), std::move(data),
            base::TimeTicks() + timestamp_helper_->GetTimestamp()),
        description);
    std::move(callback).Run(media::EncoderStatus::Codes::kOk);
    timestamp_helper_->AddFrames(buffer->frame_count);
  }
  void Flush(FlushCallback /*callback*/) override {}

 private:
  mojo::AssociatedRemote<media::mojom::AudioEncoderClient> client_;
  InitializeCallback init_cb_;
  std::unique_ptr<media::AudioTimestampHelper> timestamp_helper_;
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
      mojo::PendingRemote<media::mojom::VideoDecoder> dst_video_decoder)
      override {
    NOTREACHED();
  }
#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  void CreateVideoDecoderWithTracker(
      mojo::PendingReceiver<media::mojom::VideoDecoder> receiver,
      mojo::PendingRemote<media::mojom::VideoDecoderTracker> tracker) override {
    NOTREACHED();
  }
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  void CreateAudioDecoder(
      mojo::PendingReceiver<media::mojom::AudioDecoder> receiver) override {
    NOTREACHED();
  }
  void CreateDefaultRenderer(
      const std::string& audio_device_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) override {
    NOTREACHED();
  }
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void CreateCastRenderer(
      const base::UnguessableToken& overlay_plane_id,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) override {
    NOTREACHED();
  }
#endif
#if BUILDFLAG(IS_ANDROID)
  void CreateFlingingRenderer(
      const std::string& presentation_id,
      mojo::PendingRemote<media::mojom::FlingingRendererClientExtension>
          client_extension,
      mojo::PendingReceiver<media::mojom::Renderer> receiver) override {
    NOTREACHED();
  }
#endif  // BUILDFLAG(IS_ANDROID)
  void CreateCdm(const media::CdmConfig& cdm_config,
                 CreateCdmCallback callback) override {
    NOTREACHED();
  }
#if BUILDFLAG(IS_WIN)
  void CreateMediaFoundationRenderer(
      mojo::PendingRemote<media::mojom::MediaLog> media_log_remote,
      mojo::PendingReceiver<media::mojom::Renderer> receiver,
      mojo::PendingReceiver<media::mojom::MediaFoundationRendererExtension>
          renderer_extension_receiver) override {
    NOTREACHED();
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
        BindRepeating(&TestInterfaceFactory::BindRequest,
                      Unretained(&interface_factory_))));

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
  const std::vector<base::TimeTicks>& capture_times() const {
    return capture_times_;
  }
  media::EncoderStatus::Codes error_code() const { return error_code_; }

 private:
  test::TaskEnvironment task_environment_;
  TestInterfaceFactory interface_factory_;
  int output_count_ = 0;
  media::EncoderStatus::Codes error_code_ = media::EncoderStatus::Codes::kOk;
  std::vector<base::TimeTicks> capture_times_;
  AudioTrackMojoEncoder audio_track_encoder_{
      scheduler::GetSequencedTaskRunnerForTesting(), media::AudioCodec::kAAC,
      /*on_encoded_audio_cb=*/
      CrossThreadBindRepeating(base::BindLambdaForTesting(
          [this](const media::AudioParameters& /*params*/,
                 scoped_refptr<media::DecoderBuffer> /*encoded_data*/,
                 std::optional<
                     media::AudioEncoder::CodecDescription> /*codec_desc*/,
                 base::TimeTicks capture_time) {
            ++output_count_;
            capture_times_.push_back(capture_time);
          })),
      /*on_encoded_audio_error_cb=*/
      CrossThreadBindOnce(
          base::BindLambdaForTesting([this](media::EncoderStatus status) {
            ASSERT_EQ(error_code_, media::EncoderStatus::Codes::kOk);
            ASSERT_FALSE(status.is_ok());
            error_code_ = status.code();
          }))};
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

TEST_F(AudioTrackMojoEncoderTest, TimeInPauseIsRespected) {
  audio_encoder().FinishInitialization();
  auto params = media::TestAudioParameters::Normal();
  media::AudioTimestampHelper helper(params.sample_rate());
  size_t input_frames = params.frames_per_buffer() / params.channels();
  helper.SetBaseTimestamp(base::Seconds(1));
  auto timestamp_frame_0 = base::TimeTicks() + helper.GetTimestamp();
  audio_track_encoder().EncodeAudio(GenerateInput(), timestamp_frame_0);
  helper.AddFrames(input_frames);

  // Ensure encoder has seen all data as set_paused acts directly on
  // audio_track_encoder and EncodeAudio posts tasks.
  base::RunLoop().RunUntilIdle();
  audio_track_encoder().set_paused(true);

  // Frames while paused should not be forwarded.
  audio_track_encoder().EncodeAudio(GenerateInput(),
                                    base::TimeTicks() + helper.GetTimestamp());
  helper.AddFrames(input_frames);
  audio_track_encoder().EncodeAudio(GenerateInput(),
                                    base::TimeTicks() + helper.GetTimestamp());
  helper.AddFrames(input_frames);

  // Ensure encoder has seen all data as set_paused acts directly on
  // audio_track_encoder and EncodeAudio posts tasks.
  base::RunLoop().RunUntilIdle();
  audio_track_encoder().set_paused(false);
  auto timestamp_frame_1 = base::TimeTicks() + helper.GetTimestamp();
  audio_track_encoder().EncodeAudio(GenerateInput(), timestamp_frame_1);
  helper.AddFrames(input_frames);
  auto timestamp_frame_2 = base::TimeTicks() + helper.GetTimestamp();
  audio_track_encoder().EncodeAudio(GenerateInput(), timestamp_frame_2);
  helper.AddFrames(input_frames);

  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(capture_times(), ElementsAre(timestamp_frame_0, timestamp_frame_1,
                                           timestamp_frame_2));
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
