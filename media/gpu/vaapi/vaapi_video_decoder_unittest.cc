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
using testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

namespace media {
namespace {
constexpr std::string kEmptyData = "";
constexpr std::string kInvalidData = "ThisIsInvalidData";
constexpr uint8_t kEncodedData[] = {1, 2, 3};

constexpr gfx::Size kCodedSize(128, 128);
VideoDecoderConfig DefaultVideoDecoderConfig() {
  const VideoDecoderConfig config(
      media::VideoCodec::kVP8, VP8PROFILE_ANY,
      VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
      kNoTransformation, kCodedSize, gfx::Rect(kCodedSize), kCodedSize,
      EmptyExtraData(), EncryptionScheme::kUnencrypted);
  DCHECK(config.IsValidConfig());
  return config;
}
}  // namespace

MATCHER_P(MatchesStatusCode, status_code, "") {
  return arg.code() == status_code;
}

scoped_refptr<DecoderBuffer> CreateDecoderBuffer(
    const base::span<const uint8_t>& bitstream) {
  scoped_refptr<DecoderBuffer> buffer = DecoderBuffer::CopyFrom(bitstream);
  EXPECT_NE(buffer.get(), nullptr);
  return buffer;
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
    mock_accelerated_video_decoder_ = mock_accelerated_video_decoder.get();
    decoder_ = VaapiVideoDecoder::Create(
        std::make_unique<media::NullMediaLog>(),
        base::SequencedTaskRunner::GetCurrentDefault(),
        client_.weak_ptr_factory_.GetWeakPtr());
    DCHECK_CALLED_ON_VALID_SEQUENCE(vaapi_decoder()->sequence_checker_);
    vaapi_decoder()->vaapi_wrapper_ = mock_vaapi_wrapper_;
    vaapi_decoder()->decoder_ = std::move(mock_accelerated_video_decoder);
  }

  void InitializeVaapiVideoDecoder(
      const DecoderStatus::Codes& status_code,
      const VideoDecoderConfig& config = DefaultVideoDecoderConfig(),
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

  void Decode(scoped_refptr<DecoderBuffer> buffer,
              AcceleratedVideoDecoder::DecodeResult mock_decoder_result,
              DecoderStatus::Codes vaapi_decoder_status) {
    ASSERT_TRUE(mock_accelerated_video_decoder_);
    {
      InSequence sequence;
      EXPECT_CALL(*mock_accelerated_video_decoder_, SetStream(_, _));
      EXPECT_CALL(*mock_accelerated_video_decoder_, Decode())
          .WillOnce(Return(mock_decoder_result));
      EXPECT_CALL(*this,
                  OnDecodeCompleted(MatchesStatusCode(vaapi_decoder_status)));
    }
    vaapi_decoder()->Decode(
        std::move(buffer),
        base::BindOnce(&VaapiVideoDecoderTest::OnDecodeCompleted,
                       base::Unretained(this)));
    task_environment_.RunUntilIdle();
  }

  VaapiVideoDecoder* vaapi_decoder() {
    return reinterpret_cast<VaapiVideoDecoder*>(decoder_.get());
  }

  MOCK_METHOD(void, OnDecodeCompleted, (DecoderStatus), ());
  MOCK_METHOD(void, OnResetDone, (), ());

  MockCdmContext cdm_context_;
  MockChromeOsCdmContext chromeos_cdm_context_;
  media::CallbackRegistry<CdmContext::EventCB::RunType> event_callbacks_;
  std::unique_ptr<VideoDecoderMixin> decoder_;
  scoped_refptr<MockVaapiWrapper> mock_vaapi_wrapper_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockVideoDecoderMixinClient client_;
  raw_ptr<StrictMock<MockAcceleratedVideoDecoder>>
      mock_accelerated_video_decoder_ = nullptr;
};

TEST_F(VaapiVideoDecoderTest, Initialize) {
  InitializeVaapiVideoDecoder(DecoderStatus::Codes::kOk);
  EXPECT_FALSE(vaapi_decoder()->NeedsTranscryption());
}

// Verifies that Initialize() fails when trying to decode encrypted content with
// a missing CdmContext.
TEST_F(VaapiVideoDecoderTest,
       InitializeFailsDueToMissingCdmContextForEncryptedContent) {
  InitializeVaapiVideoDecoder(
      DecoderStatus::Codes::kUnsupportedEncryptionMode,
      VideoDecoderConfig(VideoCodec::kVP8, VP8PROFILE_ANY,
                         VideoDecoderConfig::AlphaMode::kIsOpaque,
                         VideoColorSpace(), kNoTransformation, kCodedSize,
                         gfx::Rect(kCodedSize), kCodedSize, EmptyExtraData(),
                         EncryptionScheme::kCenc));
}

// Verifies that Initialize() fails when trying to decode encrypted content with
// VP8 video codec as it's not supported by VA-API.
TEST_F(VaapiVideoDecoderTest, InitializeFailsDueToEncryptedContentForVP8) {
  EXPECT_CALL(cdm_context_, GetChromeOsCdmContext())
      .WillRepeatedly(Return(&chromeos_cdm_context_));
  InitializeVaapiVideoDecoder(
      DecoderStatus::Codes::kUnsupportedEncryptionMode,
      VideoDecoderConfig(VideoCodec::kVP8, VP8PROFILE_ANY,
                         VideoDecoderConfig::AlphaMode::kIsOpaque,
                         VideoColorSpace(), kNoTransformation, kCodedSize,
                         gfx::Rect(kCodedSize), kCodedSize, EmptyExtraData(),
                         EncryptionScheme::kCenc),
      &cdm_context_);
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
      DecoderStatus::Codes::kOk,
      VideoDecoderConfig(VideoCodec::kVP9, VP9PROFILE_PROFILE0,
                         VideoDecoderConfig::AlphaMode::kIsOpaque,
                         VideoColorSpace(), kNoTransformation, kCodedSize,
                         gfx::Rect(kCodedSize), kCodedSize, EmptyExtraData(),
                         EncryptionScheme::kCenc),
      &cdm_context_);
  EXPECT_FALSE(vaapi_decoder()->NeedsTranscryption());
  testing::Mock::VerifyAndClearExpectations(&chromeos_cdm_context_);
  testing::Mock::VerifyAndClearExpectations(&cdm_context_);
}

TEST_F(VaapiVideoDecoderTest, DecodeSucceeds) {
  InitializeVaapiVideoDecoder(DecoderStatus::Codes::kOk);
  EXPECT_FALSE(vaapi_decoder()->NeedsTranscryption());
  auto buffer = CreateDecoderBuffer(base::as_byte_span(kEncodedData));
  Decode(buffer, AcceleratedVideoDecoder::DecodeResult::kRanOutOfStreamData,
         DecoderStatus::Codes::kOk);
  testing::Mock::VerifyAndClearExpectations(mock_accelerated_video_decoder_);
}

TEST_F(VaapiVideoDecoderTest, DecodeFails) {
  InitializeVaapiVideoDecoder(DecoderStatus::Codes::kOk);
  EXPECT_FALSE(vaapi_decoder()->NeedsTranscryption());
  auto buffer = CreateDecoderBuffer(base::as_byte_span(kInvalidData));
  Decode(buffer, AcceleratedVideoDecoder::DecodeResult::kDecodeError,
         DecoderStatus::Codes::kFailed);
  testing::Mock::VerifyAndClearExpectations(mock_accelerated_video_decoder_);
}

// Verifies that kConfigChange event can be triggered correctly.
TEST_F(VaapiVideoDecoderTest, ConfigChange) {
  InitializeVaapiVideoDecoder(DecoderStatus::Codes::kOk);
  EXPECT_FALSE(vaapi_decoder()->NeedsTranscryption());
  auto buffer = CreateDecoderBuffer(base::as_byte_span(kEmptyData));
  Decode(buffer, AcceleratedVideoDecoder::DecodeResult::kRanOutOfStreamData,
         DecoderStatus::Codes::kOk);
  buffer = CreateDecoderBuffer(kEncodedData);
  EXPECT_CALL(client_, PrepareChangeResolution());
  Decode(buffer, AcceleratedVideoDecoder::DecodeResult::kConfigChange,
         DecoderStatus::Codes::kAborted);
  testing::Mock::VerifyAndClearExpectations(mock_accelerated_video_decoder_);
}

// Verifies the Reset sequence.
TEST_F(VaapiVideoDecoderTest, Reset) {
  InitializeVaapiVideoDecoder(DecoderStatus::Codes::kOk);
  EXPECT_FALSE(vaapi_decoder()->NeedsTranscryption());
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_accelerated_video_decoder_, Reset());
  EXPECT_CALL(*this, OnResetDone())
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  vaapi_decoder()->Reset(base::BindOnce(&VaapiVideoDecoderTest::OnResetDone,
                                        base::Unretained(this)));
  task_environment_.RunUntilIdle();
}

}  // namespace media
