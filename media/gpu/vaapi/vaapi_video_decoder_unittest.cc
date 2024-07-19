// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_decoder.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#error This file should only be built for Ash.
#endif

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_context.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_helpers.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/vaapi/vaapi_video_decoder_delegate.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunClosure;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;
using ::testing::StrictMock;

namespace media {

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

class MockChromeOsCdmContext : public chromeos::ChromeOsCdmContext {
 public:
  MockChromeOsCdmContext() : chromeos::ChromeOsCdmContext() {}
  ~MockChromeOsCdmContext() override = default;

  MOCK_METHOD3(GetHwKeyData,
               void(const DecryptConfig*,
                    const std::vector<uint8_t>&,
                    chromeos::ChromeOsCdmContext::GetHwKeyDataCB));
  MOCK_METHOD1(GetHwConfigData,
               void(chromeos::ChromeOsCdmContext::GetHwConfigDataCB));
  MOCK_METHOD1(GetScreenResolutions,
               void(chromeos::ChromeOsCdmContext::GetScreenResolutionsCB));
  MOCK_METHOD0(GetCdmContextRef, std::unique_ptr<CdmContextRef>());
  MOCK_CONST_METHOD0(UsingArcCdm, bool());
  MOCK_CONST_METHOD0(IsRemoteCdm, bool());
  MOCK_METHOD2(AllocateSecureBuffer,
               void(uint32_t,
                    chromeos::ChromeOsCdmContext::AllocateSecureBufferCB));
  MOCK_METHOD4(ParseEncryptedSliceHeader,
               void(uint64_t,
                    uint32_t,
                    const std::vector<uint8_t>&,
                    ParseEncryptedSliceHeaderCB));
};

class FakeCdmContextRef : public CdmContextRef {
 public:
  FakeCdmContextRef(CdmContext* cdm_context) : cdm_context_(cdm_context) {}
  ~FakeCdmContextRef() override = default;

  CdmContext* GetCdmContext() override { return cdm_context_; }

 private:
  raw_ptr<CdmContext> cdm_context_;
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

  void InitializeVaapiVideoDecoder(const VideoDecoderConfig& config,
                                   const DecoderStatus::Codes& status_code,
                                   CdmContext* cdm_context = nullptr) {
    base::RunLoop run_loop;
    EXPECT_CALL(client_, InitCallback(MatchesStatusCode(status_code)))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    vaapi_decoder()->Initialize(
        config, /*low_delay=*/false, /*cdm_context=*/cdm_context,
        base::BindOnce(&MockVideoDecoderMixinClient::InitCallback,
                       client_.weak_ptr_factory_.GetWeakPtr()),
        /*output_cb=*/base::DoNothing(),
        /*waiting_cb*/ base::DoNothing());
    ASSERT_NE(vaapi_decoder(), nullptr);
    DCHECK_CALLED_ON_VALID_SEQUENCE(vaapi_decoder()->sequence_checker_);
    ASSERT_NE(vaapi_decoder()->vaapi_wrapper_, nullptr);
    ASSERT_NE(vaapi_decoder()->decoder_, nullptr);
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(this);
  }

  VaapiVideoDecoder* vaapi_decoder() {
    return reinterpret_cast<VaapiVideoDecoder*>(decoder_.get());
  }

  MockCdmContext cdm_context_;
  MockChromeOsCdmContext chromeos_cdm_context_;
  media::CallbackRegistry<CdmContext::EventCB::RunType> event_callbacks_;
  std::unique_ptr<VideoDecoderMixin> decoder_;
  scoped_refptr<MockVaapiWrapper> mock_vaapi_wrapper_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockVideoDecoderMixinClient client_;
};

TEST_F(VaapiVideoDecoderTest, Initialize) {
  InitializeVaapiVideoDecoder(
      VideoDecoderConfig(VideoCodec::kVP8, VP8PROFILE_ANY,
                         VideoDecoderConfig::AlphaMode::kIsOpaque,
                         VideoColorSpace(), kNoTransformation, kCodedSize,
                         gfx::Rect(kCodedSize), kCodedSize, EmptyExtraData(),
                         EncryptionScheme::kUnencrypted),
      DecoderStatus::Codes::kOk);
  EXPECT_FALSE(vaapi_decoder()->NeedsTranscryption());
}

// Verifies that Initialize() fails when trying to decode encrypted content with
// a missing CdmContext.
TEST_F(VaapiVideoDecoderTest,
       InitializeFailsDueToMissingCdmContextForEncryptedContent) {
  InitializeVaapiVideoDecoder(
      VideoDecoderConfig(VideoCodec::kVP8, VP8PROFILE_ANY,
                         VideoDecoderConfig::AlphaMode::kIsOpaque,
                         VideoColorSpace(), kNoTransformation, kCodedSize,
                         gfx::Rect(kCodedSize), kCodedSize, EmptyExtraData(),
                         EncryptionScheme::kCenc),
      DecoderStatus::Codes::kUnsupportedEncryptionMode);
}

// Verifies that Initialize() fails when trying to decode encrypted content with
// VP8 video codec as it's not supported by VA-API.
TEST_F(VaapiVideoDecoderTest, InitializeFailsDueToEncryptedContentForVP8) {
  EXPECT_CALL(cdm_context_, GetChromeOsCdmContext())
      .WillRepeatedly(Return(&chromeos_cdm_context_));
  InitializeVaapiVideoDecoder(
      VideoDecoderConfig(VideoCodec::kVP8, VP8PROFILE_ANY,
                         VideoDecoderConfig::AlphaMode::kIsOpaque,
                         VideoColorSpace(), kNoTransformation, kCodedSize,
                         gfx::Rect(kCodedSize), kCodedSize, EmptyExtraData(),
                         EncryptionScheme::kCenc),
      DecoderStatus::Codes::kUnsupportedEncryptionMode, &cdm_context_);
  testing::Mock::VerifyAndClearExpectations(&chromeos_cdm_context_);
  testing::Mock::VerifyAndClearExpectations(&cdm_context_);
}

// Verifies that Initialize() succeeds for VP9 encrypted content.
TEST_F(VaapiVideoDecoderTest, InitializeForVP9EncryptedContent) {
  EXPECT_CALL(cdm_context_, GetChromeOsCdmContext())
      .WillRepeatedly(Return(&chromeos_cdm_context_));
  EXPECT_CALL(cdm_context_, RegisterEventCB(_))
      .WillOnce([this](CdmContext::EventCB event_cb) {
        return event_callbacks_.Register(std::move(event_cb));
      });
  EXPECT_CALL(chromeos_cdm_context_, GetCdmContextRef())
      .WillOnce(
          Return(ByMove(std::make_unique<FakeCdmContextRef>(&cdm_context_))));
  EXPECT_CALL(chromeos_cdm_context_, IsRemoteCdm()).WillOnce(Return(false));
  InitializeVaapiVideoDecoder(
      VideoDecoderConfig(VideoCodec::kVP9, VP9PROFILE_PROFILE0,
                         VideoDecoderConfig::AlphaMode::kIsOpaque,
                         VideoColorSpace(), kNoTransformation, kCodedSize,
                         gfx::Rect(kCodedSize), kCodedSize, EmptyExtraData(),
                         EncryptionScheme::kCenc),
      DecoderStatus::Codes::kOk, &cdm_context_);
  EXPECT_FALSE(vaapi_decoder()->NeedsTranscryption());
  testing::Mock::VerifyAndClearExpectations(&chromeos_cdm_context_);
  testing::Mock::VerifyAndClearExpectations(&cdm_context_);
}

}  // namespace media
