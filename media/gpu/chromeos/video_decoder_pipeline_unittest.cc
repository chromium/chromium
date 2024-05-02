// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/video_decoder_pipeline.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/base/cdm_context.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_media_log.h"
#include "media/base/status.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libdrm/src/include/drm/drm_fourcc.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
// gn check does not account for BUILDFLAG(), so including this header will
// make gn check fail for builds other than ash-chrome. See gn help nogncheck
// for more information.
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_context.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using base::test::RunClosure;
using ::testing::_;
using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::TestWithParam;

namespace media {

using PixelLayoutCandidate = ImageProcessor::PixelLayoutCandidate;

MATCHER_P(MatchesStatusCode, status_code, "") {
  // media::Status doesn't provide an operator==(...), we add here a simple one.
  return arg.code() == status_code;
}

MATCHER_P(MatchesDecoderBuffer, buffer, "") {
  DCHECK(arg);
  return arg->MatchesForTesting(*buffer);
}

class MockVideoFramePool : public DmabufVideoFramePool {
 public:
  MockVideoFramePool() = default;
  ~MockVideoFramePool() override = default;

  // DmabufVideoFramePool implementation.
  MOCK_METHOD7(Initialize,
               CroStatus::Or<GpuBufferLayout>(const Fourcc&,
                                              const gfx::Size&,
                                              const gfx::Rect&,
                                              const gfx::Size&,
                                              size_t,
                                              bool,
                                              bool));
  MOCK_METHOD0(GetFrame, scoped_refptr<FrameResource>());
  MOCK_CONST_METHOD0(GetFrameStorageType, VideoFrame::StorageType());
  MOCK_METHOD0(IsExhausted, bool());
  MOCK_METHOD1(NotifyWhenFrameAvailable, void(base::OnceClosure));
  MOCK_METHOD0(ReleaseAllFrames, void());
  MOCK_METHOD0(GetGpuBufferLayout, std::optional<GpuBufferLayout>());

  bool IsFakeVideoFramePool() override { return true; }
};

class MockDecoder : public VideoDecoderMixin {
 public:
  MockDecoder()
      : VideoDecoderMixin(std::make_unique<MockMediaLog>(),
                          base::SequencedTaskRunner::GetCurrentDefault(),
                          base::WeakPtr<VideoDecoderMixin::Client>(nullptr)) {}
  ~MockDecoder() override = default;

  MOCK_METHOD6(Initialize,
               void(const VideoDecoderConfig&,
                    bool,
                    CdmContext*,
                    InitCB,
                    const PipelineOutputCB&,
                    const WaitingCB&));
  MOCK_METHOD2(Decode, void(scoped_refptr<DecoderBuffer>, DecodeCB));
  MOCK_METHOD1(Reset, void(base::OnceClosure));
  MOCK_METHOD0(ApplyResolutionChange, void());
  MOCK_METHOD0(NeedsTranscryption, bool());
  MOCK_METHOD1(AttachSecureBuffer, CroStatus(scoped_refptr<DecoderBuffer>&));
  MOCK_METHOD1(ReleaseSecureBuffer, void(uint64_t));
  MOCK_CONST_METHOD0(GetDecoderType, VideoDecoderType());
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr uint8_t kEncryptedData[] = {1, 8, 9};
constexpr uint8_t kTranscryptedData[] = {9, 2, 4};
constexpr uint64_t kFakeSecureHandle = 75;
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
// A real implementation of this class would actually hold onto a reference of
// the owner of the CdmContext to ensure it is not destructed before the
// CdmContextRef is destructed. For the tests here, we don't need to bother with
// that because the CdmContext is a class member declared before the
// VideoDecoderPipeline so the CdmContext will get destructed after what uses
// it.
class FakeCdmContextRef : public CdmContextRef {
 public:
  FakeCdmContextRef(CdmContext* cdm_context) : cdm_context_(cdm_context) {}
  ~FakeCdmContextRef() override = default;

  CdmContext* GetCdmContext() override { return cdm_context_; }

 private:
  raw_ptr<CdmContext> cdm_context_;
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class MockImageProcessor : public ImageProcessor {
 public:
  explicit MockImageProcessor(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner)
      : ImageProcessor(nullptr,
                       client_task_runner,
                       /*backend_task_runner=*/
                       base::ThreadPool::CreateSequencedTaskRunner({})) {}

  MOCK_CONST_METHOD0(input_config, const ImageProcessorBackend::PortConfig&());
  MOCK_CONST_METHOD0(output_config, const ImageProcessorBackend::PortConfig&());
};

struct DecoderPipelineTestParams {
  // GTest params need to be copyable; hence we need here a RepeatingCallback
  // version of VideoDecoderPipeline::CreateDecoderFunctionCB.
  using RepeatingCreateDecoderFunctionCB = base::RepeatingCallback<
      VideoDecoderPipeline::CreateDecoderFunctionCB::RunType>;
  RepeatingCreateDecoderFunctionCB create_decoder_function_cb;
  DecoderStatus::Codes status_code;
};

constexpr gfx::Size kMinSupportedResolution(64, 64);
constexpr gfx::Size kMaxSupportedResolution(2048, 1088);
constexpr gfx::Size kCodedSize(128, 128);

static_assert(kMinSupportedResolution.width() <= kCodedSize.width() &&
                  kMinSupportedResolution.height() <= kCodedSize.height() &&
                  kCodedSize.width() <= kMaxSupportedResolution.width() &&
                  kCodedSize.height() <= kMaxSupportedResolution.height(),
              "kCodedSize must be within the supported resolutions.");

class VideoDecoderPipelineTest
    : public testing::TestWithParam<DecoderPipelineTestParams> {
 public:
  VideoDecoderPipelineTest()
      : config_(VideoCodec::kVP8,
                VP8PROFILE_ANY,
                VideoDecoderConfig::AlphaMode::kIsOpaque,
                VideoColorSpace(),
                kNoTransformation,
                kCodedSize,
                gfx::Rect(kCodedSize),
                kCodedSize,
                EmptyExtraData(),
                EncryptionScheme::kUnencrypted) {
    auto pool = std::make_unique<MockVideoFramePool>();
    pool_ = pool.get();
    decoder_ = base::WrapUnique(new VideoDecoderPipeline(
        gpu::GpuDriverBugWorkarounds(),
        base::SingleThreadTaskRunner::GetCurrentDefault(), std::move(pool),
        /*frame_converter=*/nullptr,
        VideoDecoderPipeline::DefaultPreferredRenderableFourccs(),
        std::make_unique<MockMediaLog>(),
        // This callback needs to be configured in the individual tests.
        base::BindOnce(&VideoDecoderPipelineTest::CreateNullMockDecoder),
        /*uses_oop_video_decoder=*/false,
        /*in_video_decoder_process=*/true));

    SetSupportedVideoDecoderConfigs({
        SupportedVideoDecoderConfig(
            /*profile_min,=*/VP8PROFILE_ANY,
            /*profile_max=*/VP9PROFILE_PROFILE0, kMinSupportedResolution,
            kMaxSupportedResolution,
            /*allow_encrypted=*/true,
            /*require_encrypted=*/false),
    });
  }
  ~VideoDecoderPipelineTest() override = default;

  void TearDown() override {
    pool_ = nullptr;
    VideoDecoderPipeline::DestroyAsync(std::move(decoder_));
    task_environment_.RunUntilIdle();
  }
  MOCK_METHOD1(OnInit, void(DecoderStatus));
  MOCK_METHOD1(OnOutput, void(scoped_refptr<VideoFrame>));
  MOCK_METHOD0(OnResetDone, void());
  MOCK_METHOD1(OnDecodeDone, void(DecoderStatus));
  MOCK_METHOD1(OnWaiting, void(WaitingReason));

  void SetCreateDecoderFunctionCB(VideoDecoderPipeline::CreateDecoderFunctionCB
                                      function) NO_THREAD_SAFETY_ANALYSIS {
    decoder_->create_decoder_function_cb_ = std::move(function);
  }

  void SetCreateImageProcessorCBForTesting(
      VideoDecoderPipeline::CreateImageProcessorCBForTesting function)
      NO_THREAD_SAFETY_ANALYSIS {
    decoder_->create_image_processor_cb_for_testing_ = std::move(function);
  }

  // Constructs |decoder_| with a given |create_decoder_function_cb| and
  // verifying |status_code| is received back in OnInit().
  void InitializeDecoder(
      VideoDecoderPipeline::CreateDecoderFunctionCB create_decoder_function_cb,
      DecoderStatus::Codes status_code,
      CdmContext* cdm_context = nullptr) {
    SetCreateDecoderFunctionCB(std::move(create_decoder_function_cb));

    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnInit(MatchesStatusCode(status_code)))
        .WillOnce(RunClosure(run_loop.QuitClosure()));

    decoder_->Initialize(
        config_, false /* low_delay */, cdm_context,
        base::BindOnce(&VideoDecoderPipelineTest::OnInit,
                       base::Unretained(this)),
        base::BindRepeating(&VideoDecoderPipelineTest::OnOutput,
                            base::Unretained(this)),
        base::BindRepeating(&VideoDecoderPipelineTest::OnWaiting,
                            base::Unretained(this)));
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(this);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void InitializeForTranscrypt(bool vp9 = false) {
    decoder_->allow_encrypted_content_for_testing_ = true;
    if (vp9) {
      config_ = VideoDecoderConfig(
          VideoCodec::kVP9, VP9PROFILE_PROFILE0,
          VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
          kNoTransformation, kCodedSize, gfx::Rect(kCodedSize), kCodedSize,
          EmptyExtraData(), EncryptionScheme::kCenc);
    }
    EXPECT_CALL(cdm_context_, GetChromeOsCdmContext())
        .WillRepeatedly(Return(&chromeos_cdm_context_));
    EXPECT_CALL(cdm_context_, RegisterEventCB(_))
        .WillOnce([this](CdmContext::EventCB event_cb) {
          return event_callbacks_.Register(std::move(event_cb));
        });
    EXPECT_CALL(cdm_context_, GetDecryptor())
        .WillRepeatedly(Return(&decryptor_));
    EXPECT_CALL(chromeos_cdm_context_, GetCdmContextRef())
        .WillOnce(
            Return(ByMove(std::make_unique<FakeCdmContextRef>(&cdm_context_))));
    InitializeDecoder(
        base::BindOnce(
            &VideoDecoderPipelineTest::CreateGoodMockTranscryptDecoder),
        DecoderStatus::Codes::kOk, &cdm_context_);
    testing::Mock::VerifyAndClearExpectations(&chromeos_cdm_context_);
    testing::Mock::VerifyAndClearExpectations(&cdm_context_);
    // GetDecryptor() will be called again, so set that expectation.
    EXPECT_CALL(cdm_context_, GetDecryptor())
        .WillRepeatedly(Return(&decryptor_));
    encrypted_buffer_ = DecoderBuffer::CopyFrom(kEncryptedData);
    transcrypted_buffer_ = DecoderBuffer::CopyFrom(kTranscryptedData);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  static std::unique_ptr<VideoDecoderMixin> CreateNullMockDecoder(
      std::unique_ptr<MediaLog> /* media_log */,
      scoped_refptr<base::SequencedTaskRunner> /* decoder_task_runner */,
      base::WeakPtr<VideoDecoderMixin::Client> /* client */) {
    return nullptr;
  }

  // Creates a MockDecoder with an EXPECT_CALL on Initialize that returns ok.
  static std::unique_ptr<VideoDecoderMixin> CreateGoodMockDecoder(
      std::unique_ptr<MediaLog> /* media_log */,
      scoped_refptr<base::SequencedTaskRunner> /* decoder_task_runner */,
      base::WeakPtr<VideoDecoderMixin::Client> /* client */) {
    std::unique_ptr<MockDecoder> decoder(new MockDecoder());
    EXPECT_CALL(*decoder, Initialize(_, _, _, _, _, _))
        .WillOnce(::testing::WithArgs<3>([](VideoDecoder::InitCB init_cb) {
          std::move(init_cb).Run(DecoderStatus::Codes::kOk);
        }));
    EXPECT_CALL(*decoder, NeedsTranscryption()).WillRepeatedly(Return(false));
    return std::move(decoder);
  }

  // Creates a MockDecoder with an EXPECT_CALL on Initialize that returns ok and
  // also indicates that it requires transcryption.
  static std::unique_ptr<VideoDecoderMixin> CreateGoodMockTranscryptDecoder(
      std::unique_ptr<MediaLog> /* media_log */,
      scoped_refptr<base::SequencedTaskRunner> /* decoder_task_runner */,
      base::WeakPtr<VideoDecoderMixin::Client> /* client */) {
    std::unique_ptr<MockDecoder> decoder(new MockDecoder());
    EXPECT_CALL(*decoder, Initialize(_, _, _, _, _, _))
        .WillOnce(::testing::WithArgs<3>([](VideoDecoder::InitCB init_cb) {
          std::move(init_cb).Run(DecoderStatus::Codes::kOk);
        }));
    EXPECT_CALL(*decoder, NeedsTranscryption()).WillRepeatedly(Return(true));
    return std::move(decoder);
  }

  // Creates a MockDecoder with an EXPECT_CALL on Initialize that returns error.
  static std::unique_ptr<VideoDecoderMixin> CreateBadMockDecoder(
      std::unique_ptr<MediaLog> /* media_log */,
      scoped_refptr<base::SequencedTaskRunner> /* decoder_task_runner */,
      base::WeakPtr<VideoDecoderMixin::Client> /* client */) {
    std::unique_ptr<MockDecoder> decoder(new MockDecoder());
    EXPECT_CALL(*decoder, Initialize(_, _, _, _, _, _))
        .WillOnce(::testing::WithArgs<3>([](VideoDecoder::InitCB init_cb) {
          std::move(init_cb).Run(DecoderStatus::Codes::kFailed);
        }));
    EXPECT_CALL(*decoder, NeedsTranscryption()).WillRepeatedly(Return(false));
    return std::move(decoder);
  }

  VideoDecoderMixin* GetUnderlyingDecoder() NO_THREAD_SAFETY_ANALYSIS {
    return decoder_->decoder_.get();
  }

  void SetSupportedVideoDecoderConfigs(
      const SupportedVideoDecoderConfigs& configs) {
    decoder_->supported_configs_for_testing_ = configs;
  }

  void DetachDecoderSequenceChecker() NO_THREAD_SAFETY_ANALYSIS {
    // |decoder_| will be destroyed on its |decoder_task_runner| via
    // DestroyAsync(). This will trip its |decoder_sequence_checker_| if it has
    // been pegged to the test task runner, e.g. in PickDecoderOutputFormat().
    // Since in that case we don't care about threading, just detach it.
    DETACH_FROM_SEQUENCE(decoder_->decoder_sequence_checker_);

    if (decoder_->image_processor_) {
      // |decoder_->image_processor_->sequence_checker_| is pegged to the test
      // task runner because |decoder_->image_processor_| is created when we
      // call PickDecoderOutputFormat() from test code.
      // |decoder_->image_processor_| is then destroyed on the decoder task
      // runner because of VideoDecoderPipeline::DestroyAsync(). Thus the need
      // for this detachment.
      DETACH_FROM_SEQUENCE(decoder_->image_processor_->sequence_checker_);
    }
  }

  void InvokeWaitingCB(WaitingReason reason) {
    decoder_->decoder_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VideoDecoderPipeline::OnDecoderWaiting,
                                  base::Unretained(decoder_.get()), reason));
  }

  bool DecoderHasImageProcessor() NO_THREAD_SAFETY_ANALYSIS {
    return !!decoder_->image_processor_;
  }

  scoped_refptr<base::SequencedTaskRunner> GetDecoderTaskRunner()
      NO_THREAD_SAFETY_ANALYSIS {
    return decoder_->decoder_task_runner_;
  }

  base::test::TaskEnvironment task_environment_;
  VideoDecoderConfig config_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  MockCdmContext cdm_context_;  // Keep this before |decoder_|.
  MockChromeOsCdmContext chromeos_cdm_context_;
  StrictMock<MockDecryptor> decryptor_;
  scoped_refptr<DecoderBuffer> encrypted_buffer_;
  scoped_refptr<DecoderBuffer> transcrypted_buffer_;
  media::CallbackRegistry<CdmContext::EventCB::RunType> event_callbacks_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<VideoDecoderPipeline> decoder_;
  raw_ptr<MockVideoFramePool> pool_;
};

// Verifies the status code for several typical CreateDecoderFunctionCB cases.
TEST_P(VideoDecoderPipelineTest, Initialize) {
  InitializeDecoder(base::BindOnce(GetParam().create_decoder_function_cb),
                    GetParam().status_code);

  EXPECT_EQ(GetParam().status_code == DecoderStatus::Codes::kOk,
            !!GetUnderlyingDecoder());
}

const struct DecoderPipelineTestParams kDecoderPipelineTestParams[] = {
    // A CreateDecoderFunctionCB that fails to Create() (i.e. returns a
    // null Decoder)
    {base::BindRepeating(&VideoDecoderPipelineTest::CreateNullMockDecoder),
     DecoderStatus::Codes::kFailedToCreateDecoder},

    // A CreateDecoderFunctionCB that works fine, i.e. Create()s and
    // Initialize()s correctly.
    {base::BindRepeating(&VideoDecoderPipelineTest::CreateGoodMockDecoder),
     DecoderStatus::Codes::kOk},

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // A CreateDecoderFunctionCB for transcryption, where Create() is ok, and
    // the decoder will Initialize OK, but then the pipeline will not create the
    // transcryptor due to a missing CdmContext. This will succeed if called
    // through InitializeForTranscrypt where a CdmContext is set.
    {base::BindRepeating(
         &VideoDecoderPipelineTest::CreateGoodMockTranscryptDecoder),
     DecoderStatus::Codes::kUnsupportedEncryptionMode},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    // A CreateDecoderFunctionCB that Create()s ok but fails to Initialize()
    // correctly.
    {base::BindRepeating(&VideoDecoderPipelineTest::CreateBadMockDecoder),
     DecoderStatus::Codes::kFailed},
};

INSTANTIATE_TEST_SUITE_P(All,
                         VideoDecoderPipelineTest,
                         testing::ValuesIn(kDecoderPipelineTestParams));

// Verifies that trying to Initialize() with a non-supported config fails.
TEST_F(VideoDecoderPipelineTest, InitializeFailsDueToNotSupportedConfig) {
  // Configure the supported configs to something that we know is not supported,
  // e.g. making the smallest supported resolution larger than the |config_|
  // we'll be requesting.
  SetSupportedVideoDecoderConfigs({SupportedVideoDecoderConfig(
      /*profile_min=*/config_.profile(),
      /*profile_max=*/config_.profile(),
      /*coded_size_min=*/config_.coded_size() + gfx::Size(1, 1),
      kMaxSupportedResolution,
      /*allow_encrypted=*/true,
      /*require_encrypted=*/false)});

  InitializeDecoder(
      base::BindOnce(&VideoDecoderPipelineTest::CreateGoodMockDecoder),
      DecoderStatus::Codes::kUnsupportedConfig);
}

// Verifies the Reset sequence.
TEST_F(VideoDecoderPipelineTest, Reset) {
  InitializeDecoder(
      base::BindOnce(&VideoDecoderPipelineTest::CreateGoodMockDecoder),
      DecoderStatus::Codes::kOk);

  // When we call Reset(), we expect GetUnderlyingDecoder()'s Reset() method to
  // be called, and when that method Run()s its argument closure, then
  // OnResetDone() is expected to be called.

  base::RunLoop run_loop;
  EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()), Reset(_))
      .WillOnce(::testing::WithArgs<0>(
          [](base::OnceClosure closure) { std::move(closure).Run(); }));

  EXPECT_CALL(*this, OnResetDone())
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  decoder_->Reset(base::BindOnce(&VideoDecoderPipelineTest::OnResetDone,
                                 base::Unretained(this)));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(VideoDecoderPipelineTest, TranscryptThenEos) {
  InitializeForTranscrypt();

  // First send in a DecoderBuffer.
  {
    InSequence sequence;
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                AttachSecureBuffer(encrypted_buffer_))
        .WillOnce(Return(CroStatus::Codes::kOk));
    EXPECT_CALL(decryptor_, Decrypt(Decryptor::kVideo, encrypted_buffer_, _))
        .WillOnce([this](Decryptor::StreamType stream_type,
                         scoped_refptr<DecoderBuffer> encrypted,
                         Decryptor::DecryptCB decrypt_cb) {
          std::move(decrypt_cb).Run(Decryptor::kSuccess, transcrypted_buffer_);
        });
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                Decode(transcrypted_buffer_, _))
        .WillOnce([](scoped_refptr<DecoderBuffer> transcrypted,
                     VideoDecoderMixin::DecodeCB decode_cb) {
          std::move(decode_cb).Run(DecoderStatus::Codes::kOk);
        });
    EXPECT_CALL(*this,
                OnDecodeDone(MatchesStatusCode(DecoderStatus::Codes::kOk)));
  }
  decoder_->Decode(encrypted_buffer_,
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  task_environment_.RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&decryptor_);
  testing::Mock::VerifyAndClearExpectations(
      reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()));
  testing::Mock::VerifyAndClearExpectations(this);

  // Now send in the EOS, this should not invoke Decrypt.
  scoped_refptr<DecoderBuffer> eos_buffer = DecoderBuffer::CreateEOSBuffer();
  {
    InSequence sequence;
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                Decode(eos_buffer, _))
        .WillOnce([](scoped_refptr<DecoderBuffer> transcrypted,
                     VideoDecoderMixin::DecodeCB decode_cb) {
          std::move(decode_cb).Run(DecoderStatus::Codes::kOk);
        });
    EXPECT_CALL(*this,
                OnDecodeDone(MatchesStatusCode(DecoderStatus::Codes::kOk)));
  }
  decoder_->Decode(eos_buffer,
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  task_environment_.RunUntilIdle();
}

TEST_F(VideoDecoderPipelineTest, TranscryptReset) {
  InitializeForTranscrypt();
  scoped_refptr<DecoderBuffer> encrypted_buffer2 =
      DecoderBuffer::CopyFrom(base::span(kEncryptedData).subspan(1));
  // Send in a buffer, but don't invoke the Decrypt callback so it stays as
  // pending. Then send in 2 more buffers so they are in the queue.
  {
    InSequence sequence;
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                AttachSecureBuffer(encrypted_buffer_))
        .WillOnce(Return(CroStatus::Codes::kOk));
    EXPECT_CALL(decryptor_, Decrypt(Decryptor::kVideo, encrypted_buffer_, _))
        .Times(1);
  }
  decoder_->Decode(encrypted_buffer_,
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  decoder_->Decode(encrypted_buffer2,
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  decoder_->Decode(encrypted_buffer2,
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(
      reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()));
  testing::Mock::VerifyAndClearExpectations(&decryptor_);

  // Now when we reset, we should see 3 decode callbacks occur as well as the
  // reset callback.
  {
    InSequence sequence;
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                Reset(_))
        .WillOnce([](base::OnceClosure closure) { std::move(closure).Run(); });
    EXPECT_CALL(*this,
                OnDecodeDone(MatchesStatusCode(DecoderStatus::Codes::kAborted)))
        .Times(3);
    EXPECT_CALL(*this, OnResetDone()).Times(1);
  }
  decoder_->Reset(base::BindOnce(&VideoDecoderPipelineTest::OnResetDone,
                                 base::Unretained(this)));
  task_environment_.RunUntilIdle();
}

// Verifies that any decode calls from
// VideoDecoderPipeline::OnBufferTranscrypted() received while the underlying
// VideoDecoderMixin is performing a reset operation are aborted.
TEST_F(VideoDecoderPipelineTest, TranscryptDecodeDuringReset) {
  InitializeForTranscrypt();

  // First send in a buffer, which will go to the decryptor and hold on to that
  // callback.
  Decryptor::DecryptCB saved_decrypt_cb;
  {
    InSequence sequence;

    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                AttachSecureBuffer(encrypted_buffer_))
        .WillOnce(Return(CroStatus::Codes::kOk));
    EXPECT_CALL(decryptor_, Decrypt(Decryptor::kVideo, encrypted_buffer_, _))
        .WillOnce([&saved_decrypt_cb](Decryptor::StreamType stream_type,
                                      scoped_refptr<DecoderBuffer> encrypted,
                                      Decryptor::DecryptCB decrypt_cb) {
          saved_decrypt_cb =
              base::BindPostTaskToCurrentDefault(std::move(decrypt_cb));
        });
  }

  // Reset the underlying decoder but don't invoke the reset callback yet. Save
  // it for later.
  base::OnceClosure saved_reset_cb;
  EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()), Reset(_))
      .WillOnce([&saved_reset_cb](base::OnceClosure closure) {
        saved_reset_cb = base::BindPostTaskToCurrentDefault(std::move(closure));
      });

  decoder_->Decode(encrypted_buffer_,
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  decoder_->Reset(base::BindOnce(&VideoDecoderPipelineTest::OnResetDone,
                                 base::Unretained(this)));
  task_environment_.RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(
      reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()));
  testing::Mock::VerifyAndClearExpectations(&decryptor_);

  ASSERT_TRUE(saved_decrypt_cb);
  ASSERT_TRUE(saved_reset_cb);

  EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
              Decode(_, _))
      .Times(0);

  EXPECT_CALL(*this,
              OnDecodeDone(MatchesStatusCode(DecoderStatus::Codes::kAborted)))
      .Times(1);

  std::move(saved_decrypt_cb).Run(Decryptor::kSuccess, transcrypted_buffer_);
  task_environment_.RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnResetDone()).Times(1);

  std::move(saved_reset_cb).Run();
  task_environment_.RunUntilIdle();
}

// Verifies that if we get notified about a new decrypt key while we are
// performing a transcrypt that fails w/out a key, we immediately retry again.
TEST_F(VideoDecoderPipelineTest, TranscryptKeyAddedDuringTranscrypt) {
  InitializeForTranscrypt();
  // First send in a buffer, which will go to the decryptor and hold on to that
  // callback.
  Decryptor::DecryptCB saved_decrypt_cb;
  {
    InSequence sequence;
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                AttachSecureBuffer(encrypted_buffer_))
        .WillOnce(Return(CroStatus::Codes::kOk));
    EXPECT_CALL(decryptor_, Decrypt(Decryptor::kVideo, encrypted_buffer_, _))
        .WillOnce([&saved_decrypt_cb](Decryptor::StreamType stream_type,
                                      scoped_refptr<DecoderBuffer> encrypted,
                                      Decryptor::DecryptCB decrypt_cb) {
          saved_decrypt_cb =
              base::BindPostTaskToCurrentDefault(std::move(decrypt_cb));
        });
  }
  decoder_->Decode(encrypted_buffer_,
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(
      reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()));
  testing::Mock::VerifyAndClearExpectations(&decryptor_);

  // Now we invoke the CDM callback to indicate there is a new key available.
  event_callbacks_.Notify(CdmContext::Event::kHasAdditionalUsableKey);
  task_environment_.RunUntilIdle();

  // Now we have the decryptor callback return with kNoKey which should then
  // cause another call into the decryptor which we will have succeed and then
  // that should go through decoding. This should not invoke the waiting CB.
  {
    InSequence sequence;
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                AttachSecureBuffer(encrypted_buffer_))
        .WillOnce(Return(CroStatus::Codes::kOk));
    EXPECT_CALL(decryptor_, Decrypt(Decryptor::kVideo, encrypted_buffer_, _))
        .WillOnce([this](Decryptor::StreamType stream_type,
                         scoped_refptr<DecoderBuffer> encrypted,
                         Decryptor::DecryptCB decrypt_cb) {
          std::move(decrypt_cb).Run(Decryptor::kSuccess, transcrypted_buffer_);
        });
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                Decode(transcrypted_buffer_, _))
        .WillOnce([](scoped_refptr<DecoderBuffer> transcrypted,
                     VideoDecoderMixin::DecodeCB decode_cb) {
          std::move(decode_cb).Run(DecoderStatus::Codes::kOk);
        });
    EXPECT_CALL(*this,
                OnDecodeDone(MatchesStatusCode(DecoderStatus::Codes::kOk)));
  }
  EXPECT_CALL(*this, OnWaiting(_)).Times(0);
  std::move(saved_decrypt_cb).Run(Decryptor::kNoKey, nullptr);
  task_environment_.RunUntilIdle();
}

// Verifies that if we have a condition where we need to retry a pending
// transcrypt task that it doesn't try to reacquire a secure buffer if it
// already has one.
TEST_F(VideoDecoderPipelineTest, RetryDoesntReattachSecureBuffer) {
  InitializeForTranscrypt();
  // First send in a buffer, which will go to the decryptor and hold on to that
  // callback.
  Decryptor::DecryptCB saved_decrypt_cb;
  {
    InSequence sequence;
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                AttachSecureBuffer(encrypted_buffer_))
        .WillOnce([](scoped_refptr<DecoderBuffer>& buffer) {
          buffer->WritableSideData().secure_handle = kFakeSecureHandle;
          return CroStatus::Codes::kOk;
        });
    EXPECT_CALL(decryptor_, Decrypt(Decryptor::kVideo, encrypted_buffer_, _))
        .WillOnce([&saved_decrypt_cb](Decryptor::StreamType stream_type,
                                      scoped_refptr<DecoderBuffer> encrypted,
                                      Decryptor::DecryptCB decrypt_cb) {
          saved_decrypt_cb =
              base::BindPostTaskToCurrentDefault(std::move(decrypt_cb));
        });
  }
  decoder_->Decode(encrypted_buffer_,
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(
      reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()));
  testing::Mock::VerifyAndClearExpectations(&decryptor_);

  // Now we invoke the CDM callback to indicate there is a new key available.
  event_callbacks_.Notify(CdmContext::Event::kHasAdditionalUsableKey);
  task_environment_.RunUntilIdle();

  // Now we have the decryptor callback return with kNoKey which should then
  // cause another call into the decryptor which we will have succeed and then
  // that should go through decoding. This should not invoke the call to attach
  // a secure buffer, but after it's done decoding it should invoke the call to
  // release the secure buffer.
  {
    InSequence sequence;
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                AttachSecureBuffer(_))
        .Times(0);
    EXPECT_CALL(decryptor_, Decrypt(Decryptor::kVideo, encrypted_buffer_, _))
        .WillOnce([this](Decryptor::StreamType stream_type,
                         scoped_refptr<DecoderBuffer> encrypted,
                         Decryptor::DecryptCB decrypt_cb) {
          std::move(decrypt_cb).Run(Decryptor::kSuccess, transcrypted_buffer_);
        });
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                Decode(transcrypted_buffer_, _))
        .WillOnce([](scoped_refptr<DecoderBuffer> transcrypted,
                     VideoDecoderMixin::DecodeCB decode_cb) {
          std::move(decode_cb).Run(DecoderStatus::Codes::kOk);
        });
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                ReleaseSecureBuffer(kFakeSecureHandle))
        .Times(1);
    EXPECT_CALL(*this,
                OnDecodeDone(MatchesStatusCode(DecoderStatus::Codes::kOk)));
  }
  EXPECT_CALL(*this, OnWaiting(_)).Times(0);
  std::move(saved_decrypt_cb).Run(Decryptor::kNoKey, nullptr);
  task_environment_.RunUntilIdle();
}

// Verifies that if we don't have the key during transcrypt, the WaitingCB is
// invoked and then it retries again when we notify it of the new key.
TEST_F(VideoDecoderPipelineTest, TranscryptNoKeyWaitRetry) {
  InitializeForTranscrypt();
  // First send in a buffer, which will go to the decryptor and indicate there
  // is no key. This should also invoke the WaitingCB.
  {
    InSequence sequence;
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                AttachSecureBuffer(encrypted_buffer_))
        .WillOnce(Return(CroStatus::Codes::kOk));
    EXPECT_CALL(decryptor_, Decrypt(Decryptor::kVideo, encrypted_buffer_, _))
        .WillOnce([](Decryptor::StreamType stream_type,
                     scoped_refptr<DecoderBuffer> encrypted,
                     Decryptor::DecryptCB decrypt_cb) {
          std::move(decrypt_cb).Run(Decryptor::kNoKey, nullptr);
        });
    EXPECT_CALL(*this, OnWaiting(WaitingReason::kNoDecryptionKey)).Times(1);
  }
  decoder_->Decode(encrypted_buffer_,
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(
      reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()));
  testing::Mock::VerifyAndClearExpectations(&decryptor_);
  testing::Mock::VerifyAndClearExpectations(this);

  // Now we invoke the CDM callback to indicate there is a new key available.
  // This should invoke the decryptor again which we will have succeed and
  // complete the decode operation.
  {
    InSequence sequence;
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                AttachSecureBuffer(encrypted_buffer_))
        .WillOnce(Return(CroStatus::Codes::kOk));
    EXPECT_CALL(decryptor_, Decrypt(Decryptor::kVideo, encrypted_buffer_, _))
        .WillOnce([this](Decryptor::StreamType stream_type,
                         scoped_refptr<DecoderBuffer> encrypted,
                         Decryptor::DecryptCB decrypt_cb) {
          std::move(decrypt_cb).Run(Decryptor::kSuccess, transcrypted_buffer_);
        });
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                Decode(transcrypted_buffer_, _))
        .WillOnce([](scoped_refptr<DecoderBuffer> transcrypted,
                     VideoDecoderMixin::DecodeCB decode_cb) {
          std::move(decode_cb).Run(DecoderStatus::Codes::kOk);
        });
    EXPECT_CALL(*this,
                OnDecodeDone(MatchesStatusCode(DecoderStatus::Codes::kOk)));
  }
  event_callbacks_.Notify(CdmContext::Event::kHasAdditionalUsableKey);
  task_environment_.RunUntilIdle();
}

TEST_F(VideoDecoderPipelineTest, TranscryptError) {
  InitializeForTranscrypt();
  {
    InSequence sequence;
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                AttachSecureBuffer(encrypted_buffer_))
        .WillOnce(Return(CroStatus::Codes::kOk));
    EXPECT_CALL(decryptor_, Decrypt(Decryptor::kVideo, encrypted_buffer_, _))
        .WillOnce([](Decryptor::StreamType stream_type,
                     scoped_refptr<DecoderBuffer> encrypted,
                     Decryptor::DecryptCB decrypt_cb) {
          std::move(decrypt_cb).Run(Decryptor::kError, nullptr);
        });
    EXPECT_CALL(*this,
                OnDecodeDone(MatchesStatusCode(DecoderStatus::Codes::kFailed)));
  }
  decoder_->Decode(encrypted_buffer_,
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  task_environment_.RunUntilIdle();
}

TEST_F(VideoDecoderPipelineTest, SecureBufferFailure) {
  InitializeForTranscrypt();
  {
    InSequence sequence;
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                AttachSecureBuffer(encrypted_buffer_))
        .WillOnce(Return(CroStatus::Codes::kUnableToAllocateSecureBuffer));
    EXPECT_CALL(*this,
                OnDecodeDone(MatchesStatusCode(DecoderStatus::Codes::kFailed)));
  }
  decoder_->Decode(encrypted_buffer_,
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  task_environment_.RunUntilIdle();
}

#if BUILDFLAG(USE_V4L2_CODEC)
TEST_F(VideoDecoderPipelineTest, SplitVp9Superframe) {
  InitializeForTranscrypt(true);

  // This one requires specially crafted DecoderBuffer data so that the frame
  // split occurs. The superframe (which contains 2 frames) gets sent into the
  // pipeline for decoding, it then goes into the transcryptor...but then before
  // it gets sent for decrypt + decode it should get split into the 2 separate
  // frames.

  constexpr uint8_t kEncryptedSuperframe[] = {
      // Frame 0
      // Clear data
      1,
      2,
      3,
      4,
      // Encrypted Data (one block to cause IV increment)
      1,
      2,
      3,
      4,
      5,
      6,
      7,
      8,
      9,
      10,
      11,
      12,
      13,
      14,
      15,
      16,
      // Frame 1
      // Clear data
      5,
      6,
      7,
      8,
      9,
      10,
      // Encrypted Data (must be at least a block size)
      17,
      18,
      19,
      20,
      21,
      22,
      23,
      24,
      25,
      26,
      27,
      28,
      29,
      30,
      31,
      32,
      // Superframe marker (2 frames, mag 1)
      0xc1,
      // Frame sizes (1 byte each)
      0x14,
      0x16,
      // Superframe marker (2 frames, mag 1)
      0xc1,
  };
  constexpr uint8_t kEncryptedFrame0[] = {
      // Clear data
      1,
      2,
      3,
      4,
      // Encrypted Data (one block to cause IV increment)
      1,
      2,
      3,
      4,
      5,
      6,
      7,
      8,
      9,
      10,
      11,
      12,
      13,
      14,
      15,
      16,
  };
  constexpr uint8_t kEncryptedFrame1[] = {
      // Clear data
      5,
      6,
      7,
      8,
      9,
      10,
      // Encrypted Data (must be at least a block size)
      17,
      18,
      19,
      20,
      21,
      22,
      23,
      24,
      25,
      26,
      27,
      28,
      29,
      30,
      31,
      32,
  };

  scoped_refptr<DecoderBuffer> superframe_buffer =
      DecoderBuffer::CopyFrom(kEncryptedSuperframe);
  superframe_buffer->set_decrypt_config(DecryptConfig::CreateCencConfig(
      "fakekey", std::string(16, '0'),
      {SubsampleEntry(4, 16), SubsampleEntry(6, 16), SubsampleEntry(4, 0)}));

  std::string iv(16, '0');
  scoped_refptr<DecoderBuffer> frame0_buffer =
      DecoderBuffer::CopyFrom(kEncryptedFrame0);
  frame0_buffer->set_decrypt_config(
      DecryptConfig::CreateCencConfig("fakekey", iv, {SubsampleEntry(4, 16)}));

  scoped_refptr<DecoderBuffer> frame1_buffer =
      DecoderBuffer::CopyFrom(kEncryptedFrame1);
  // The IV should be incremented by one.
  iv[15]++;
  frame1_buffer->set_decrypt_config(
      DecryptConfig::CreateCencConfig("fakekey", iv, {SubsampleEntry(6, 16)}));

  {
    InSequence sequence;
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                AttachSecureBuffer(MatchesDecoderBuffer(frame0_buffer)))
        .WillOnce(Return(CroStatus::Codes::kOk));
    EXPECT_CALL(decryptor_, Decrypt(Decryptor::kVideo,
                                    MatchesDecoderBuffer(frame0_buffer), _))
        .WillOnce([&frame0_buffer](Decryptor::StreamType stream_type,
                                   scoped_refptr<DecoderBuffer> encrypted,
                                   Decryptor::DecryptCB decrypt_cb) {
          std::move(decrypt_cb).Run(Decryptor::kSuccess, frame0_buffer);
        });
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                Decode(MatchesDecoderBuffer(frame0_buffer), _))
        .WillOnce([](scoped_refptr<DecoderBuffer> transcrypted,
                     VideoDecoderMixin::DecodeCB decode_cb) {
          std::move(decode_cb).Run(DecoderStatus::Codes::kOk);
        });
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                AttachSecureBuffer(MatchesDecoderBuffer(frame1_buffer)))
        .WillOnce(Return(CroStatus::Codes::kOk));
    EXPECT_CALL(decryptor_, Decrypt(Decryptor::kVideo,
                                    MatchesDecoderBuffer(frame1_buffer), _))
        .WillOnce([&frame1_buffer](Decryptor::StreamType stream_type,
                                   scoped_refptr<DecoderBuffer> encrypted,
                                   Decryptor::DecryptCB decrypt_cb) {
          std::move(decrypt_cb).Run(Decryptor::kSuccess, frame1_buffer);
        });
    EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()),
                Decode(MatchesDecoderBuffer(frame1_buffer), _))
        .WillOnce([](scoped_refptr<DecoderBuffer> transcrypted,
                     VideoDecoderMixin::DecodeCB decode_cb) {
          std::move(decode_cb).Run(DecoderStatus::Codes::kOk);
        });
    EXPECT_CALL(*this,
                OnDecodeDone(MatchesStatusCode(DecoderStatus::Codes::kOk)));
  }
  decoder_->Decode(std::move(superframe_buffer),
                   base::BindOnce(&VideoDecoderPipelineTest::OnDecodeDone,
                                  base::Unretained(this)));
  task_environment_.RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(&decryptor_);
  testing::Mock::VerifyAndClearExpectations(
      reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()));
  testing::Mock::VerifyAndClearExpectations(this);
}
#endif  // BUILDFLAG(USE_V4L2_CODEC)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Verifies the algorithm for choosing formats in PickDecoderOutputFormat works
// as expected.
TEST_F(VideoDecoderPipelineTest, PickDecoderOutputFormat) {
  constexpr gfx::Size kSize(320, 240);
  constexpr gfx::Rect kVisibleRect(320, 240);
  constexpr size_t kNumCodecReferenceFrames = 4u;
  constexpr uint64_t kModifier = ~DRM_FORMAT_MOD_LINEAR;

  const struct {
    std::vector<PixelLayoutCandidate> input_candidates;
    PixelLayoutCandidate expected_chosen_candidate;
  } test_vectors[] = {
      // Easy cases: one candidate that is supported, should be chosen.
      {{PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier}},
       PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier}},
      {{PixelLayoutCandidate{Fourcc(Fourcc::YV12), kSize, kModifier}},
       PixelLayoutCandidate{Fourcc(Fourcc::YV12), kSize, kModifier}},
      {{PixelLayoutCandidate{Fourcc(Fourcc::P010), kSize, kModifier}},
       PixelLayoutCandidate{Fourcc(Fourcc::P010), kSize, kModifier}},
      // Two candidates, both supported: pick as per implementation.
      {{PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier},
        PixelLayoutCandidate{Fourcc(Fourcc::YV12), kSize, kModifier}},
       PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier}},
      {{PixelLayoutCandidate{Fourcc(Fourcc::YV12), kSize, kModifier},
        PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier}},
       PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier}},
      {{PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier},
        PixelLayoutCandidate{Fourcc(Fourcc::P010), kSize, kModifier}},
       PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier}},
      // Two candidates, only one supported, the supported one should be picked.
      {{PixelLayoutCandidate{Fourcc(Fourcc::YU16), kSize, kModifier},
        PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier}},
       PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier}},
      {{PixelLayoutCandidate{Fourcc(Fourcc::YU16), kSize, kModifier},
        PixelLayoutCandidate{Fourcc(Fourcc::YV12), kSize, kModifier}},
       PixelLayoutCandidate{Fourcc(Fourcc::YV12), kSize, kModifier}},
      {{PixelLayoutCandidate{Fourcc(Fourcc::YU16), kSize, kModifier},
        PixelLayoutCandidate{Fourcc(Fourcc::P010), kSize, kModifier}},
       PixelLayoutCandidate{Fourcc(Fourcc::P010), kSize, kModifier}}};

  for (const auto& test_vector : test_vectors) {
    const Fourcc& expected_fourcc =
        test_vector.expected_chosen_candidate.fourcc;
    const gfx::Size& expected_coded_size =
        test_vector.expected_chosen_candidate.size;
    std::vector<ColorPlaneLayout> planes(
        VideoFrame::NumPlanes(expected_fourcc.ToVideoPixelFormat()));
    EXPECT_CALL(
        *pool_,
        Initialize(expected_fourcc, expected_coded_size, kVisibleRect,
                   /*natural_size=*/kVisibleRect.size(),
                   /*max_num_frames=*/::testing::Gt(kNumCodecReferenceFrames),
                   /*use_protected=*/false,
                   /*use_linear_buffers=*/false))
        .WillOnce(Return(*GpuBufferLayout::Create(
            expected_fourcc, expected_coded_size, std::move(planes),
            /*modifier=*/kModifier)));
    auto status_or_chosen_candidate = decoder_->PickDecoderOutputFormat(
        test_vector.input_candidates, kVisibleRect,
        /*decoder_natural_size=*/kVisibleRect.size(),
        /*output_size=*/std::nullopt,
        /*num_codec_reference_frames=*/kNumCodecReferenceFrames,
        /*use_protected=*/false, /*need_aux_frame_pool=*/false, std::nullopt);
    ASSERT_TRUE(status_or_chosen_candidate.has_value());
    const PixelLayoutCandidate chosen_candidate =
        std::move(status_or_chosen_candidate).value();
    EXPECT_EQ(test_vector.expected_chosen_candidate, chosen_candidate)
        << " expected: "
        << test_vector.expected_chosen_candidate.fourcc.ToString()
        << ", actual: " << chosen_candidate.fourcc.ToString();
    EXPECT_FALSE(DecoderHasImageProcessor());
    testing::Mock::VerifyAndClearExpectations(pool_);
  }
  DetachDecoderSequenceChecker();
}

// These tests only work on non-linux and non-lacros vaapi systems, since on
// linux and lacros there is no support for different modifiers.
#if BUILDFLAG(USE_VAAPI) && !BUILDFLAG(IS_LINUX) && \
    !BUILDFLAG(IS_CHROMEOS_LACROS)

// Verifies the algorithm for choosing formats in PickDecoderOutputFormat works
// as expected when the pool returns linear buffers. It should allocate an image
// processor in those cases.
TEST_F(VideoDecoderPipelineTest, PickDecoderOutputFormatLinearModifier) {
  constexpr gfx::Size kSize(320, 240);
  constexpr gfx::Rect kVisibleRect(320, 240);
  constexpr size_t kNumCodecReferenceFrames = 4u;
  const Fourcc kFourcc(Fourcc::NV12);

  auto image_processor =
      std::make_unique<MockImageProcessor>(GetDecoderTaskRunner());
  ImageProcessorBackend::PortConfig port_config(
      Fourcc(Fourcc::NV12), gfx::Size(320, 240), {}, gfx::Rect(320, 240), {});
  EXPECT_CALL(*image_processor, input_config())
      .WillRepeatedly(testing::ReturnRef(port_config));
  EXPECT_CALL(*image_processor, output_config())
      .WillRepeatedly(testing::ReturnRef(port_config));

  base::MockCallback<VideoDecoderPipeline::CreateImageProcessorCBForTesting>
      image_processor_cb;
  EXPECT_CALL(image_processor_cb, Run(_, _, kSize, _))
      .WillOnce(Return(testing::ByMove(std::move(image_processor))));
  SetCreateImageProcessorCBForTesting(image_processor_cb.Get());

  // Modifier should be the linear format.
  GpuBufferLayout gpu_buffer_layout = *GpuBufferLayout::Create(
      kFourcc, kSize,
      std::vector<ColorPlaneLayout>(
          VideoFrame::NumPlanes(kFourcc.ToVideoPixelFormat())),
      /*modifier=*/DRM_FORMAT_MOD_LINEAR);
  EXPECT_CALL(*pool_, Initialize(_, _, _, _, _, _, _))
      .WillRepeatedly(Return(gpu_buffer_layout));

  PixelLayoutCandidate candidate{Fourcc(Fourcc::NV12), kSize,
                                 /*modifier=*/~DRM_FORMAT_MOD_LINEAR};
  auto status_or_chosen_candidate = decoder_->PickDecoderOutputFormat(
      {candidate}, kVisibleRect,
      /*decoder_natural_size=*/kVisibleRect.size(),
      /*output_size=*/std::nullopt,
      /*num_codec_reference_frames=*/kNumCodecReferenceFrames,
      /*use_protected=*/false, /*need_aux_frame_pool=*/false, std::nullopt);

  EXPECT_TRUE(status_or_chosen_candidate.has_value());
  // Main concern is that the image processor was set.
  EXPECT_TRUE(DecoderHasImageProcessor());
  DetachDecoderSequenceChecker();
}

// Verifies the algorithm for choosing formats in PickDecoderOutputFormat works
// as expected when the frame pool returns buffers that have an unsupported
// modifier.
TEST_F(VideoDecoderPipelineTest, PickDecoderOutputFormatUnsupportedModifier) {
  constexpr gfx::Size kSize(320, 240);
  constexpr gfx::Rect kVisibleRect(320, 240);
  constexpr size_t kNumCodecReferenceFrames = 4u;
  const Fourcc kFourcc(Fourcc::NV12);

  // Modifier is *not* the linear format.
  GpuBufferLayout gpu_buffer_layout = *GpuBufferLayout::Create(
      kFourcc, kSize,
      std::vector<ColorPlaneLayout>(
          VideoFrame::NumPlanes(kFourcc.ToVideoPixelFormat())),
      /*modifier=*/~DRM_FORMAT_MOD_LINEAR);
  EXPECT_CALL(*pool_, Initialize(_, _, _, _, _, _, _))
      .WillRepeatedly(Return(gpu_buffer_layout));

  // Make sure the modifier mismatches the |gpu_buffer_layout|'s
  constexpr uint64_t modifier = ~DRM_FORMAT_MOD_LINEAR + 1;
  PixelLayoutCandidate candidate{Fourcc(Fourcc::NV12), kSize, modifier};
  auto status_or_chosen_candidate = decoder_->PickDecoderOutputFormat(
      {candidate}, kVisibleRect,
      /*decoder_natural_size=*/kVisibleRect.size(),
      /*output_size=*/std::nullopt,
      /*num_codec_reference_frames=*/kNumCodecReferenceFrames,
      /*use_protected=*/false, /*need_aux_frame_pool=*/false, std::nullopt);

  EXPECT_FALSE(status_or_chosen_candidate.has_value());
  EXPECT_FALSE(DecoderHasImageProcessor());
  DetachDecoderSequenceChecker();
}

#endif  // BUILDFLAG(USE_VAAPI) && !BUILDFLAG(IS_LINUX) &&
        // !BUILDFLAG(IS_CHROMEOS_LACROS)

// Verifies that ReleaseAllFrames is called on the frame pool when we receive
// the kDecoderStateLost event through the waiting callback. This can occur
// during protected content playback on Intel.
TEST_F(VideoDecoderPipelineTest, RebuildFramePoolsOnStateLost) {
  InitializeDecoder(
      base::BindOnce(&VideoDecoderPipelineTest::CreateGoodMockDecoder),
      DecoderStatus::Codes::kOk);

  // Simulate the waiting callback from the decoder for kDecoderStateLost.
  EXPECT_CALL(*this, OnWaiting(media::WaitingReason::kDecoderStateLost));
  InvokeWaitingCB(media::WaitingReason::kDecoderStateLost);
  task_environment_.RunUntilIdle();

  // Invoke Reset() as a client would do, and we then expect that to invoke the
  // method to rebuild the frame pool.
  EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()), Reset(_))
      .WillOnce(::testing::WithArgs<0>(
          [](base::OnceClosure closure) { std::move(closure).Run(); }));
  EXPECT_CALL(*this, OnResetDone());
  EXPECT_CALL(*pool_, ReleaseAllFrames());

  decoder_->Reset(base::BindOnce(&VideoDecoderPipelineTest::OnResetDone,
                                 base::Unretained(this)));
  task_environment_.RunUntilIdle();
}
}  // namespace media
