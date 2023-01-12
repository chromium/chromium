// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include <stdint.h>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/decoder_factory.h"
#include "media/base/decoder_status.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_stream_adapter.h"
#include "third_party/webrtc/api/video_codecs/video_codec.h"
#include "third_party/webrtc/api/video_codecs/vp9_profile.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace blink {

namespace {

struct TestParams {
  enum class UseHwDecoders {
    kNo = 0,
    kYes = 1,
  };
  enum class UseChromeSwDecoders {
    kNo = 0,
    kYes = 1,
  };
  UseHwDecoders use_hw_decoders = UseHwDecoders::kNo;
  UseChromeSwDecoders use_chrome_sw_decoders = UseChromeSwDecoders::kNo;
};

class MockVideoDecoder : public media::VideoDecoder {
 public:
  explicit MockVideoDecoder(bool is_platform_decoder)
      : is_platform_decoder_(is_platform_decoder),
        current_decoder_type_(media::VideoDecoderType::kTesting) {}

  media::VideoDecoderType GetDecoderType() const override {
    return current_decoder_type_;
  }
  void Initialize(const media::VideoDecoderConfig& config,
                  bool low_delay,
                  media::CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const media::WaitingCB& waiting_cb) override {
    Initialize_(config, low_delay, cdm_context, init_cb, output_cb, waiting_cb);
  }
  MOCK_METHOD6(Initialize_,
               void(const media::VideoDecoderConfig& config,
                    bool low_delay,
                    media::CdmContext* cdm_context,
                    InitCB& init_cb,
                    const OutputCB& output_cb,
                    const media::WaitingCB& waiting_cb));
  void Decode(scoped_refptr<media::DecoderBuffer> buffer,
              DecodeCB cb) override {
    Decode_(std::move(buffer), cb);
  }
  MOCK_METHOD2(Decode_,
               void(scoped_refptr<media::DecoderBuffer> buffer, DecodeCB&));
  void Reset(base::OnceClosure cb) override { Reset_(cb); }
  MOCK_METHOD1(Reset_, void(base::OnceClosure&));
  bool NeedsBitstreamConversion() const override { return false; }
  bool CanReadWithoutStalling() const override { return true; }
  int GetMaxDecodeRequests() const override { return 1; }
  // Since DecoderSelector always allows platform decoders, pretend that we are
  // a platform decoder.
  bool IsPlatformDecoder() const override { return is_platform_decoder_; }
  // We can set the type of decoder we want.
  void SetDecoderType(media::VideoDecoderType expected_decoder_type) {
    current_decoder_type_ = expected_decoder_type;
  }

 private:
  const bool is_platform_decoder_;
  media::VideoDecoderType current_decoder_type_;
};

class MockDecoderFactory : public media::DecoderFactory {
 public:
  ~MockDecoderFactory() override = default;

  MOCK_METHOD3(
      CreateAudioDecoders,
      void(scoped_refptr<base::SequencedTaskRunner> task_runner,
           media::MediaLog* media_log,
           std::vector<std::unique_ptr<media::AudioDecoder>>* audio_decoders));

  void CreateVideoDecoders(scoped_refptr<base::SequencedTaskRunner> task_runner,
                           media::GpuVideoAcceleratorFactories* gpu_factories,
                           media::MediaLog* media_log,
                           media::RequestOverlayInfoCB request_overlay_info_cb,
                           const gfx::ColorSpace& target_color_space,
                           std::vector<std::unique_ptr<media::VideoDecoder>>*
                               video_decoders) override {
    std::move(std::begin(decoders_), std::end(decoders_),
              std::back_inserter(*video_decoders));
    decoders_.clear();
  }

  // Return the first, usually only, decoder.  Only works before we've provided
  // it via CreateVideoDecoders.
  MockVideoDecoder* decoder() const {
    EXPECT_TRUE(!decoders_.empty());
    return decoders_[0].get();
  }
  MockVideoDecoder* last_decoder() const {
    EXPECT_TRUE(!decoders_.empty());
    return decoders_[decoders_.size() - 1].get();
  }
  // Return true if and only if we have decoders ready that we haven't sent to
  // DecoderSelector via CreateVideoDecoder.
  bool has_pending_decoders() const { return !decoders_.empty(); }

  // Must call this if you want us to vend more than one decoder.
  void CreatePendingDecoder(bool is_platform_decoder = true) {
    decoders_.push_back(
        std::make_unique<MockVideoDecoder>(is_platform_decoder));
  }

  base::WeakPtr<DecoderFactory> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::vector<std::unique_ptr<MockVideoDecoder>> decoders_;

  base::WeakPtrFactory<MockDecoderFactory> weak_factory_{this};
};

// Wraps a callback as a webrtc::DecodedImageCallback.
class DecodedImageCallback : public webrtc::DecodedImageCallback {
 public:
  explicit DecodedImageCallback(
      base::RepeatingCallback<void(const webrtc::VideoFrame&)> callback)
      : callback_(callback) {}
  DecodedImageCallback(const DecodedImageCallback&) = delete;
  DecodedImageCallback& operator=(const DecodedImageCallback&) = delete;

  int32_t Decoded(webrtc::VideoFrame& decodedImage) override {
    callback_.Run(decodedImage);
    // TODO(sandersd): Does the return value matter? RTCVideoDecoder
    // ignores it.
    return 0;
  }

 private:
  base::RepeatingCallback<void(const webrtc::VideoFrame&)> callback_;
};

}  // namespace

class RTCVideoDecoderStreamAdapterTest
    : public ::testing::TestWithParam<TestParams> {
 public:
  RTCVideoDecoderStreamAdapterTest()
      : task_environment_(
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
        media_thread_task_runner_(
            base::ThreadPool::CreateSequencedTaskRunner({})),
        use_hw_decoders_(GetParam().use_hw_decoders ==
                         TestParams::UseHwDecoders::kYes),
        decoded_image_callback_(decoded_cb_.Get()),
        sdp_format_(webrtc::SdpVideoFormat(
            webrtc::CodecTypeToPayloadString(webrtc::kVideoCodecVP9))),
        spatial_index_(0) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
#if BUILDFLAG(IS_WIN)
    enabled_features.push_back(::media::kD3D11Vp9kSVCHWDecoding);
#endif
    if (GetParam().use_chrome_sw_decoders ==
        TestParams::UseChromeSwDecoders::kYes) {
      enabled_features.push_back(::media::kExposeSwDecodersToWebRTC);
    } else {
      disabled_features.push_back(::media::kExposeSwDecodersToWebRTC);
    }

    decoder_factory_ = std::make_unique<MockDecoderFactory>();
    // Create one hw decoder.
    decoder_factory_->CreatePendingDecoder(true);
    // Unless specifically overridden by a test, the gpu claims to support any
    // decoder config.
    SetGpuFactorySupport(true);
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  RTCVideoDecoderStreamAdapterTest(const RTCVideoDecoderStreamAdapterTest&) =
      delete;
  RTCVideoDecoderStreamAdapterTest& operator=(
      const RTCVideoDecoderStreamAdapterTest&) = delete;

  ~RTCVideoDecoderStreamAdapterTest() override {
    if (!adapter_)
      return;

    media_thread_task_runner_->DeleteSoon(FROM_HERE, std::move(adapter_));
    task_environment_.RunUntilIdle();
  }

 protected:
  bool BasicSetup() {
    SetupDecoders();
    if (!CreateDecoderStream())
      return false;
    if (!InitDecode())
      return false;
    if (RegisterDecodeCompleteCallback() != WEBRTC_VIDEO_CODEC_OK)
      return false;
    task_environment_.RunUntilIdle();
    return true;
  }

  bool BasicTeardown() {
    // Flush the media thread, to finish any in-flight decodes.  Otherwise, they
    // will be cancelled by the call to Release().
    task_environment_.RunUntilIdle();

    if (Release() != WEBRTC_VIDEO_CODEC_OK)
      return false;
    return true;
  }

  void SetUpMockDecoder(MockVideoDecoder* decoder, bool init_cb_result) {
    EXPECT_CALL(*decoder, Initialize_(_, _, _, _, _, _))
        .WillOnce(
            DoAll(SaveArg<0>(&vda_config_), SaveArg<4>(&output_cb_),
                  base::test::RunOnceCallback<3>(
                      init_cb_result ? media::DecoderStatus::Codes::kOk
                                     : media::DecoderStatus::Codes::kFailed)));
  }

  // Set up our decoder factory to provide a decoder that will succeed or fail
  // init based on `init_cb_result`.
  void SetupDecoders(bool init_cb_result = true) {
    auto* decoder = decoder_factory_->decoder();
    SetUpMockDecoder(decoder, init_cb_result);
  }

  bool CreateDecoderStream() {
    adapter_ = RTCVideoDecoderStreamAdapter::Create(
        use_hw_decoders_ ? &gpu_factories_ : nullptr,
        decoder_factory_->GetWeakPtr(), media_thread_task_runner_,
        gfx::ColorSpace{}, sdp_format_);
    return !!adapter_;
  }

  // Start initialization on `adapter_`.
  bool InitDecode() {
    webrtc::VideoDecoder::Settings settings;
    settings.set_codec_type(webrtc::kVideoCodecVP9);
    // Important but subtle note: this resolution must be under the cutoff for
    // choosing sw decoders in DecoderStream.  Some of the tests require it.
    settings.set_max_render_resolution(webrtc::RenderResolution(128, 128));
    return adapter_->Configure(settings);
  }

  int32_t RegisterDecodeCompleteCallback() {
    return adapter_->RegisterDecodeCompleteCallback(&decoded_image_callback_);
  }

  int32_t Decode(uint32_t timestamp,
                 bool missing_frames = false,
                 bool is_keyframe = true) {
    webrtc::EncodedImage input_image;
    static const uint8_t data[1] = {0};
    input_image.SetSpatialIndex(spatial_index_);
    input_image.SetEncodedData(
        webrtc::EncodedImageBuffer::Create(data, sizeof(data)));
    input_image._frameType = is_keyframe
                                 ? webrtc::VideoFrameType::kVideoFrameKey
                                 : webrtc::VideoFrameType::kVideoFrameDelta;
    input_image.SetTimestamp(timestamp);
    return adapter_->Decode(input_image, missing_frames, 0);
  }

  void FinishDecode(uint32_t timestamp) {
    media_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &RTCVideoDecoderStreamAdapterTest::FinishDecodeOnMediaThread,
            base::Unretained(this), timestamp));
  }

  void FinishDecodeOnMediaThread(uint32_t timestamp) {
    DCHECK(media_thread_task_runner_->RunsTasksInCurrentSequence());
    gpu::MailboxHolder mailbox_holders[media::VideoFrame::kMaxPlanes];
    mailbox_holders[0].mailbox = gpu::Mailbox::GenerateForSharedImage();
    scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::WrapNativeTextures(
            media::PIXEL_FORMAT_ARGB, mailbox_holders,
            media::VideoFrame::ReleaseMailboxCB(), gfx::Size(640, 360),
            gfx::Rect(640, 360), gfx::Size(640, 360),
            base::Microseconds(timestamp));
    output_cb_.Run(std::move(frame));
  }

  int32_t Release() { return adapter_->Release(); }

  // Notify `gpu_factories_` if it is supposed to claim to support all decoder
  // config or none of them.
  void SetGpuFactorySupport(bool supported) {
    // Use EXPECT_CALL rather than ON_CALL so it doesn't warn.
    EXPECT_CALL(gpu_factories_, IsDecoderConfigSupported(_))
        .Times(AtLeast(0))
        .WillRepeatedly(Return(
            supported
                ? media::GpuVideoAcceleratorFactories::Supported::kTrue
                : media::GpuVideoAcceleratorFactories::Supported::kFalse));
  }

  bool GetUseHwDecoders() const { return use_hw_decoders_; }

  // Override our use of hw decoders.  Generally you should not use this -- it
  // will be set by the test parameters.  However, we allow it for the very few
  // tests that actually care.
  void OverrideHwDecoders(bool use_hw_decoders) {
    use_hw_decoders_ = use_hw_decoders;
  }

  // We can set the spatial index we want, the default value is 0.
  void SetSpatialIndex(int spatial_index) { spatial_index_ = spatial_index; }

  webrtc::EncodedImage GetEncodedImageWithColorSpace(uint32_t timestamp) {
    webrtc::EncodedImage input_image;
    static const uint8_t data[1] = {0};
    input_image.SetEncodedData(
        webrtc::EncodedImageBuffer::Create(data, sizeof(data)));
    input_image._frameType = webrtc::VideoFrameType::kVideoFrameKey;
    input_image.SetTimestamp(timestamp);
    webrtc::ColorSpace webrtc_color_space;
    webrtc_color_space.set_primaries_from_uint8(1);
    webrtc_color_space.set_transfer_from_uint8(1);
    webrtc_color_space.set_matrix_from_uint8(1);
    webrtc_color_space.set_range_from_uint8(1);
    input_image.SetColorSpace(webrtc_color_space);
    return input_image;
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> media_thread_task_runner_;

  bool use_hw_decoders_;
  StrictMock<media::MockGpuVideoAcceleratorFactories> gpu_factories_{
      nullptr /* SharedImageInterface* */};
  std::unique_ptr<MockDecoderFactory> decoder_factory_;
  StrictMock<base::MockCallback<
      base::RepeatingCallback<void(const webrtc::VideoFrame&)>>>
      decoded_cb_;
  DecodedImageCallback decoded_image_callback_;
  std::unique_ptr<RTCVideoDecoderStreamAdapter> adapter_;
  media::VideoDecoder::OutputCB output_cb_;

  const gfx::ColorSpace rendering_colorspace_{gfx::ColorSpace::CreateSRGB()};

 private:
  webrtc::SdpVideoFormat sdp_format_;

  media::VideoDecoderConfig vda_config_;

  base::test::ScopedFeatureList feature_list_;

  int spatial_index_;
};

TEST_P(RTCVideoDecoderStreamAdapterTest, Create_UnknownFormat) {
  auto adapter = RTCVideoDecoderStreamAdapter::Create(
      use_hw_decoders_ ? &gpu_factories_ : nullptr,
      decoder_factory_->GetWeakPtr(), media_thread_task_runner_,
      gfx::ColorSpace{},
      webrtc::SdpVideoFormat(
          webrtc::CodecTypeToPayloadString(webrtc::kVideoCodecGeneric)));
  ASSERT_FALSE(adapter);
}

TEST_P(RTCVideoDecoderStreamAdapterTest, FailInitIfNoHwNorSwDecoders) {
  // If the adapter is configured to use neither hw nor sw decoders, then Create
  // should fail immediately.
  const bool allow_sw = GetParam().use_chrome_sw_decoders ==
                        TestParams::UseChromeSwDecoders::kYes;
  if (!allow_sw) {
    OverrideHwDecoders(false);
    EXPECT_FALSE(CreateDecoderStream());
  }  // else pass trivially
}

TEST_P(RTCVideoDecoderStreamAdapterTest, FailInit_DecodeFails) {
  // If decoder initialization fails after InitDecode runs, then the first
  // Decode should fail.  If chrome sw decoders are allowed, then it should
  // request a keyframe.  Otherwise, it should fall back to rtc sw decoders.
  SetupDecoders(false);
  EXPECT_TRUE(CreateDecoderStream());
  EXPECT_TRUE(InitDecode());
  const bool allow_sw = GetParam().use_chrome_sw_decoders ==
                        TestParams::UseChromeSwDecoders::kYes;
  task_environment_.RunUntilIdle();
  if (allow_sw) {
    // Queue up another decoder, since it should ask for one.
    EXPECT_FALSE(decoder_factory_->has_pending_decoders());
    decoder_factory_->CreatePendingDecoder();
    EXPECT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_ERROR);
    task_environment_.RunUntilIdle();
    EXPECT_FALSE(decoder_factory_->has_pending_decoders());
    // TODO(liberato): Can we verify that it switched to a sw decoder?
    // This one should succeed, since the sw decoder isn't set up to fail.
    EXPECT_TRUE(BasicTeardown());
  } else {
    EXPECT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE);
    EXPECT_FALSE(BasicTeardown());
  }
}

TEST_P(RTCVideoDecoderStreamAdapterTest, UnsupportedGpuConfigFailsImmediately) {
  // If the gpu factories don't claim to support a config, then it shouldn't
  // create the decoder.
  if (GetUseHwDecoders()) {
    SetGpuFactorySupport(false);
    EXPECT_FALSE(CreateDecoderStream());
  }  // else pass.
}

TEST_P(RTCVideoDecoderStreamAdapterTest, MissingFramesRequestsKeyframe) {
  EXPECT_TRUE(BasicSetup());
  EXPECT_EQ(Decode(0, true), WEBRTC_VIDEO_CODEC_ERROR);
}

TEST_P(RTCVideoDecoderStreamAdapterTest, DecodeOneFrame) {
  auto* decoder = decoder_factory_->decoder();
  EXPECT_TRUE(BasicSetup());
  EXPECT_CALL(*decoder, Decode_(_, _))
      .WillOnce(
          base::test::RunOnceCallback<1>(media::DecoderStatus::Codes::kOk));
  EXPECT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_OK);
  task_environment_.RunUntilIdle();
  EXPECT_CALL(decoded_cb_, Run(_));
  FinishDecode(0);
  EXPECT_TRUE(BasicTeardown());
}

TEST_P(RTCVideoDecoderStreamAdapterTest, SlowDecodingCausesReset) {
  // If we send enough(tm) decodes without returning any decoded frames, then
  // the decoder should try a flush + reset.
  auto* decoder = decoder_factory_->decoder();
  EXPECT_TRUE(BasicSetup());

  // All Decodes succeed immediately.  The backup will come from the fact that
  // we won't run the media thread while sending decode requests in.
  EXPECT_CALL(*decoder, Decode_(_, _))
      .WillRepeatedly(
          base::test::RunOnceCallback<1>(media::DecoderStatus::Codes::kOk));
  // At some point, `adapter_` should trigger a reset.
  EXPECT_CALL(*decoder, Reset_(_)).WillOnce(base::test::RunOnceCallback<0>());

  // Add decodes without calling FinishDecode.
  int limit = -1;
  for (int i = 0; i < 100 && limit < 0; i++) {
    switch (auto result = Decode(i)) {
      case WEBRTC_VIDEO_CODEC_OK:
        // Keep going -- it's still happy.
        break;
      case WEBRTC_VIDEO_CODEC_ERROR:
        // Yay -- it now believes that it's hopelessly behind, and has requested
        // a keyframe.
        limit = i;
        break;
      default:
        ASSERT_TRUE(false) << result;
    }
  }

  // We should have found a limit at some point.
  EXPECT_GT(limit, -1);

  // Let the decodes / reset complete.
  task_environment_.RunUntilIdle();

  // Everything should be reset, so one more decode should succeed.  Push a
  // frame through to make sure.
  // Should we consider pushing some non-keyframes as well, which should be
  // rejected since it reset?
  EXPECT_EQ(Decode(1000), WEBRTC_VIDEO_CODEC_OK);
  task_environment_.RunUntilIdle();
  EXPECT_CALL(decoded_cb_, Run(_));
  FinishDecode(1000);
  EXPECT_TRUE(BasicTeardown());
}

TEST_P(RTCVideoDecoderStreamAdapterTest, ReallySlowDecodingCausesFallback) {
  // If we send really enough(tm) decodes without returning any decoded frames,
  // then the decoder should fall back to software.  It should also request
  // some keyframes along the way.
  auto* decoder = decoder_factory_->decoder();
  EXPECT_TRUE(BasicSetup());

  // All Decodes succeed immediately.  The backup will come from the fact that
  // we won't run the media thread while sending decode requests in.
  EXPECT_CALL(*decoder, Decode_(_, _))
      .WillRepeatedly(
          base::test::RunOnceCallback<1>(media::DecoderStatus::Codes::kOk));
  // At some point, `adapter_` should trigger a reset, before it falls back.  It
  // should not do so more than once, since we won't complete the reset.
  EXPECT_CALL(*decoder, Reset_(_)).WillOnce(base::test::RunOnceCallback<0>());

  // Add decodes without calling FinishDecode.
  int keyframes_requested = 0;
  int limit = -1;
  bool next_is_keyframe = true;
  for (int i = 0; i < 400 && limit < 0; i++) {
    switch (auto result = Decode(i, false, next_is_keyframe)) {
      case WEBRTC_VIDEO_CODEC_OK:
        // Keep going -- it's still happy.
        next_is_keyframe = false;
        break;
      case WEBRTC_VIDEO_CODEC_ERROR:
        next_is_keyframe = true;
        keyframes_requested++;
        break;
      case WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE:
        // Yay -- it now believes that it's hopelessly behind, and has requested
        // a rtc software fallback.
        limit = i;
        break;
      default:
        EXPECT_TRUE(false) << result;
    }
  }

  // We should have found a limit at some point.
  EXPECT_GT(limit, -1);

  // It doesn't really matter how many, as long as it requests enough that it'll
  // try more than once before giving up.
  EXPECT_GT(keyframes_requested, 2);

  // Let the decodes / reset complete.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(BasicTeardown());
}

TEST_P(RTCVideoDecoderStreamAdapterTest, WontForwardFramesAfterRelease) {
  // Deliver frames after calling Release, and verify that it doesn't (a)
  // crash or (b) deliver the frames.
  auto* decoder = decoder_factory_->decoder();
  EXPECT_TRUE(BasicSetup());
  EXPECT_CALL(*decoder, Decode_(_, _))
      .WillOnce(
          base::test::RunOnceCallback<1>(media::DecoderStatus::Codes::kOk));
  EXPECT_EQ(Decode(0), WEBRTC_VIDEO_CODEC_OK);
  task_environment_.RunUntilIdle();
  // Should not be called.
  EXPECT_CALL(decoded_cb_, Run(_)).Times(0);
  FinishDecode(0);
  // Note that the decode is queued at this point, but hasn't run yet on the
  // media thread.  Release on the decoder thread, so that it should not try to
  // forward the frame when the media thread finally runs.
  adapter_->Release();
  EXPECT_TRUE(BasicTeardown());
}

TEST_P(RTCVideoDecoderStreamAdapterTest, LowResSelectsCorrectDecoder) {
  // Low-res streams should select a sw decoder if those are enabled.
  const bool allow_sw = GetParam().use_chrome_sw_decoders ==
                        TestParams::UseChromeSwDecoders::kYes;

  // There should already be a platform decoder to choose from, else the test is
  // not set up correctly.
  ASSERT_TRUE(decoder_factory_->decoder()->IsPlatformDecoder());
  // Assume that this is the correct decoder.
  auto* correct_decoder = decoder_factory_->decoder();

  // Create a sw decoder, even if they are not allowed to be selected.
  decoder_factory_->CreatePendingDecoder(false);

  if (allow_sw) {
    // The hw decoder is the wrong one, so expect no calls to initialize it.
    EXPECT_CALL(*decoder_factory_->decoder(), Initialize_(_, _, _, _, _, _))
        .Times(0);
    // The sw one is the correct one
    correct_decoder = decoder_factory_->last_decoder();
  } else {
    // The software decoder is the wrong one.
    EXPECT_CALL(*decoder_factory_->last_decoder(),
                Initialize_(_, _, _, _, _, _))
        .Times(0);
  }

  // Set up the correct decoder to succeed init.
  SetUpMockDecoder(correct_decoder, true);

  EXPECT_TRUE(CreateDecoderStream());
  EXPECT_TRUE(InitDecode());
  EXPECT_EQ(RegisterDecodeCompleteCallback(), WEBRTC_VIDEO_CODEC_OK);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(*correct_decoder, Decode_(_, _))
      .WillOnce(
          base::test::RunOnceCallback<1>(media::DecoderStatus::Codes::kOk));
  Decode(0);
  task_environment_.RunUntilIdle();
}

#if BUILDFLAG(IS_WIN)
TEST_P(RTCVideoDecoderStreamAdapterTest, UseD3D11ToDecodeVP9kSVCStream) {
  auto* decoder = decoder_factory_->decoder();
  EXPECT_TRUE(decoder->IsPlatformDecoder());
  SetSpatialIndex(2);
  decoder->SetDecoderType(media::VideoDecoderType::kD3D11);
  EXPECT_TRUE(BasicSetup());
  EXPECT_CALL(*decoder, Decode_(_, _))
      .WillOnce(
          base::test::RunOnceCallback<1>(media::DecoderStatus::Codes::kOk));
  EXPECT_EQ(Decode(0, false), WEBRTC_VIDEO_CODEC_OK);
  task_environment_.RunUntilIdle();
  EXPECT_CALL(decoded_cb_, Run(_));
  FinishDecode(0);
  EXPECT_TRUE(BasicTeardown());
}
#endif

// On ChromeOS, only based on x86(use VaapiDecoder) architecture has the ability
// to decode VP9 kSVC Stream. Other cases should fallback to sw decoder.
#if !(defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS))
TEST_P(RTCVideoDecoderStreamAdapterTest,
       FallbackToSoftwareWhenDecodeVP9kSVCStream) {
  auto* decoder = decoder_factory_->decoder();
  EXPECT_TRUE(decoder->IsPlatformDecoder());
  SetSpatialIndex(2);
  EXPECT_TRUE(BasicSetup());
  // kTesting will represent hw decoders for other use cases mentioned above.
  EXPECT_CALL(*decoder, Decode_(_, _)).Times(0);
  EXPECT_EQ(Decode(0, false), WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(BasicTeardown());
}
#endif

INSTANTIATE_TEST_SUITE_P(
    UseHwDecoding,
    RTCVideoDecoderStreamAdapterTest,
    ::testing::Values(TestParams{TestParams::UseHwDecoders::kNo,
                                 TestParams::UseChromeSwDecoders::kYes},
                      TestParams{TestParams::UseHwDecoders::kYes,
                                 TestParams::UseChromeSwDecoders::kNo},
                      TestParams{TestParams::UseHwDecoders::kYes,
                                 TestParams::UseChromeSwDecoders::kYes}));

}  // namespace blink
