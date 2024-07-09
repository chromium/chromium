// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_decoder.h"

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/base/media_util.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_helpers.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/vaapi/vaapi_video_decoder_delegate.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunClosure;
using ::testing::StrictMock;

namespace media {

constexpr gfx::Size kMinSupportedResolution(64, 64);
constexpr gfx::Size kMaxSupportedResolution(2048, 1088);
constexpr gfx::Size kCodedSize(128, 128);

MATCHER_P(MatchesStatusCode, status_code, "") {
  return arg.code() == status_code;
}

class MockVaapiWrapper : public VaapiWrapper {
 public:
  explicit MockVaapiWrapper(CodecMode mode)
      : VaapiWrapper(VADisplayStateHandle(), mode) {}

 private:
  ~MockVaapiWrapper() override = default;
};

class MockVideoDecoderMixinClient : public VideoDecoderMixin::Client {
 public:
  MOCK_METHOD(DmabufVideoFramePool*, GetVideoFramePool, (), (const, override));
  MOCK_METHOD(void, PrepareChangeResolution, (), (override));
  MOCK_METHOD(void, NotifyEstimatedMaxDecodeRequests, (int), (override));
  MOCK_METHOD(CroStatus::Or<ImageProcessor::PixelLayoutCandidate>,
              PickDecoderOutputFormat,
              (const std::vector<ImageProcessor::PixelLayoutCandidate>&,
               const gfx::Rect&,
               const gfx::Size&,
               std::optional<gfx::Size>,
               size_t,
               bool,
               bool,
               std::optional<DmabufVideoFramePool::CreateFrameCB>),
              (override));
  MOCK_METHOD(void, InitCallback, (DecoderStatus), ());

  base::WeakPtrFactory<MockVideoDecoderMixinClient> weak_ptr_factory_{this};
};

class MockAcceleratedVideoDecoder : public AcceleratedVideoDecoder {
 public:
  MockAcceleratedVideoDecoder() = default;
  ~MockAcceleratedVideoDecoder() override = default;

  MOCK_METHOD(void, SetStream, (int32_t, const DecoderBuffer&), (override));
  MOCK_METHOD(bool, Flush, (), (override));
  MOCK_METHOD(void, Reset, (), (override));
  MOCK_METHOD(DecodeResult, Decode, (), (override));
  MOCK_METHOD(gfx::Size, GetPicSize, (), (const, override));
  MOCK_METHOD(gfx::Rect, GetVisibleRect, (), (const, override));
  MOCK_METHOD(VideoCodecProfile, GetProfile, (), (const, override));
  MOCK_METHOD(uint8_t, GetBitDepth, (), (const, override));
  MOCK_METHOD(VideoChromaSampling, GetChromaSampling, (), (const, override));
  MOCK_METHOD(VideoColorSpace, GetVideoColorSpace, (), (const, override));
  MOCK_METHOD(std::optional<gfx::HDRMetadata>,
              GetHDRMetadata,
              (),
              (const, override));
  MOCK_METHOD(size_t, GetRequiredNumOfPictures, (), (const, override));
  MOCK_METHOD(size_t, GetNumReferenceFrames, (), (const, override));
};

class VaapiVideoDecoderTest : public ::testing::Test {
 public:
  VaapiVideoDecoderTest() = default;
  ~VaapiVideoDecoderTest() override = default;

  void SetUp() override {
    mock_vaapi_wrapper_ =
        base::MakeRefCounted<MockVaapiWrapper>(VaapiWrapper::kDecode);
    mock_vaapi_wrapper_->sequence_checker_.DetachFromSequence();
    ResetDecoder();
  }

  void ResetDecoder() {
    auto mock_accelerated_video_decoder =
        std::make_unique<StrictMock<MockAcceleratedVideoDecoder>>();
    decoder_ = VaapiVideoDecoder::Create(
        std::make_unique<media::NullMediaLog>(),
        base::SequencedTaskRunner::GetCurrentDefault(),
        client_.weak_ptr_factory_.GetWeakPtr());
    DCHECK_CALLED_ON_VALID_SEQUENCE(vaapi_decoder()->sequence_checker_);
    vaapi_decoder()->vaapi_wrapper_ = mock_vaapi_wrapper_;
    vaapi_decoder()->decoder_ = std::move(mock_accelerated_video_decoder);
  }

  void InitializeVaapiVideoDecoder(
      const SupportedVideoDecoderConfigs& supported_configs,
      const VideoDecoderConfig& config,
      const DecoderStatus::Codes& status_code) {
    base::RunLoop run_loop;
    vaapi_decoder()->supported_vaapi_configs_for_testing_ = supported_configs;
    EXPECT_CALL(client_, InitCallback(MatchesStatusCode(status_code)))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    vaapi_decoder()->Initialize(
        config, /*low_delay=*/false, /*cdm_context=*/nullptr,
        base::BindOnce(&MockVideoDecoderMixinClient::InitCallback,
                       client_.weak_ptr_factory_.GetWeakPtr()),
        /*output_cb=*/base::DoNothing(),
        /*waiting_cb*/ base::DoNothing());
    run_loop.Run();
  }

  VaapiVideoDecoder* vaapi_decoder() {
    return reinterpret_cast<VaapiVideoDecoder*>(decoder_.get());
  }

  std::unique_ptr<VideoDecoderMixin> decoder_;
  scoped_refptr<MockVaapiWrapper> mock_vaapi_wrapper_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockVideoDecoderMixinClient client_;
};

TEST_F(VaapiVideoDecoderTest, Initialize) {
  SupportedVideoDecoderConfigs supported_configs;
  supported_configs.emplace_back(/*profile_min=*/VP8PROFILE_ANY,
                                 /*profile_max=*/VP8PROFILE_ANY,
                                 kMinSupportedResolution,
                                 kMaxSupportedResolution,
                                 /*allow_encrypted=*/true,
                                 /*require_encrypted=*/false);
  const VideoDecoderConfig config(
      VideoCodec::kVP8, VP8PROFILE_ANY,
      VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
      kNoTransformation, kCodedSize, gfx::Rect(kCodedSize), kCodedSize,
      EmptyExtraData(), EncryptionScheme::kUnencrypted);
  InitializeVaapiVideoDecoder(supported_configs, config,
                              DecoderStatus::Codes::kOk);
}

}  // namespace media
