// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/video_decoder_pipeline.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/cdm_context.h"
#include "media/base/media_util.h"
#include "media/base/status.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/mailbox_video_frame_converter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunClosure;
using ::testing::_;
using ::testing::TestWithParam;

namespace media {

MATCHER_P(MatchesStatusCode, status_code, "") {
  // media::Status doesn't provide an operator==(...), we add here a simple one.
  return arg.code() == status_code;
}

class MockVideoFramePool : public DmabufVideoFramePool {
 public:
  MockVideoFramePool() = default;
  ~MockVideoFramePool() override = default;

  // DmabufVideoFramePool implementation.
  MOCK_METHOD6(Initialize,
               base::Optional<GpuBufferLayout>(const Fourcc&,
                                               const gfx::Size&,
                                               const gfx::Rect&,
                                               const gfx::Size&,
                                               size_t,
                                               bool));
  MOCK_METHOD0(GetFrame, scoped_refptr<VideoFrame>());
  MOCK_METHOD0(IsExhausted, bool());
  MOCK_METHOD1(NotifyWhenFrameAvailable, void(base::OnceClosure));
};

constexpr gfx::Size kCodedSize(48, 36);

class MockDecoder : public DecoderInterface {
 public:
  MockDecoder()
      : DecoderInterface(base::ThreadTaskRunnerHandle::Get(),
                         base::WeakPtr<DecoderInterface::Client>(nullptr)) {}
  ~MockDecoder() override = default;

  MOCK_METHOD5(Initialize,
               void(const VideoDecoderConfig&,
                    CdmContext*,
                    InitCB,
                    const OutputCB&,
                    const WaitingCB&));
  MOCK_METHOD2(Decode, void(scoped_refptr<DecoderBuffer>, DecodeCB));
  MOCK_METHOD1(Reset, void(base::OnceClosure));
  MOCK_METHOD0(ApplyResolutionChange, void());
};

struct DecoderPipelineTestParams {
  VideoDecoderPipeline::CreateDecoderFunctions create_decoder_functions;
  StatusCode status_code;
};

class VideoDecoderPipelineTest
    : public testing::TestWithParam<DecoderPipelineTestParams> {
 public:
  VideoDecoderPipelineTest()
      : config_(kCodecVP8,
                VP8PROFILE_ANY,
                VideoDecoderConfig::AlphaMode::kIsOpaque,
                VideoColorSpace(),
                kNoTransformation,
                kCodedSize,
                gfx::Rect(kCodedSize),
                kCodedSize,
                EmptyExtraData(),
                EncryptionScheme::kUnencrypted),
        pool_(new MockVideoFramePool),
        converter_(new VideoFrameConverter),
        decoder_(new VideoDecoderPipeline(
            base::ThreadTaskRunnerHandle::Get(),
            std::move(pool_),
            std::move(converter_),
            base::BindRepeating([]() {
              // This callback needs to be configured in the individual tests.
              return VideoDecoderPipeline::CreateDecoderFunctions();
            }))) {}
  ~VideoDecoderPipelineTest() override = default;

  void TearDown() override {
    VideoDecoderPipeline::DestroyAsync(std::move(decoder_));
    task_environment_.RunUntilIdle();
  }
  MOCK_METHOD1(OnInit, void(Status));
  MOCK_METHOD1(OnOutput, void(scoped_refptr<VideoFrame>));

  void SetCreateDecoderFunctions(
      VideoDecoderPipeline::CreateDecoderFunctions functions) {
    decoder_->remaining_create_decoder_functions_ = functions;
  }

  void InitializeDecoder() {
    decoder_->Initialize(
        config_, false /* low_delay */, nullptr /* cdm_context */,
        base::BindOnce(&VideoDecoderPipelineTest::OnInit,
                       base::Unretained(this)),
        base::BindRepeating(&VideoDecoderPipelineTest::OnOutput,
                            base::Unretained(this)),
        base::DoNothing());
  }

  static std::unique_ptr<DecoderInterface> CreateNullMockDecoder(
      scoped_refptr<base::SequencedTaskRunner> /* decoder_task_runner */,
      base::WeakPtr<DecoderInterface::Client> /* client */) {
    return nullptr;
  }

  // Creates a MockDecoder with an EXPECT_CALL on Initialize that returns ok.
  static std::unique_ptr<DecoderInterface> CreateGoodMockDecoder(
      scoped_refptr<base::SequencedTaskRunner> /* decoder_task_runner */,
      base::WeakPtr<DecoderInterface::Client> /* client */) {
    std::unique_ptr<MockDecoder> decoder(new MockDecoder());
    EXPECT_CALL(*decoder, Initialize(_, _, _, _, _))
        .WillOnce(::testing::WithArgs<2>([](VideoDecoder::InitCB init_cb) {
          std::move(init_cb).Run(OkStatus());
        }));
    return std::move(decoder);
  }

  // Creates a MockDecoder with an EXPECT_CALL on Initialize that returns error.
  static std::unique_ptr<DecoderInterface> CreateBadMockDecoder(
      scoped_refptr<base::SequencedTaskRunner> /* decoder_task_runner */,
      base::WeakPtr<DecoderInterface::Client> /* client */) {
    std::unique_ptr<MockDecoder> decoder(new MockDecoder());
    EXPECT_CALL(*decoder, Initialize(_, _, _, _, _))
        .WillOnce(::testing::WithArgs<2>([](VideoDecoder::InitCB init_cb) {
          std::move(init_cb).Run(StatusCode::kDecoderFailedInitialization);
        }));
    return std::move(decoder);
  }

  DecoderInterface* GetUnderlyingDecoder() { return decoder_->decoder_.get(); }

  base::test::TaskEnvironment task_environment_;
  const VideoDecoderConfig config_;
  DecoderInterface* underlying_decoder_ptr_ = nullptr;

  std::unique_ptr<MockVideoFramePool> pool_;
  std::unique_ptr<VideoFrameConverter> converter_;
  std::unique_ptr<VideoDecoderPipeline> decoder_;
};

// Verifies the status code for several typical CreateDecoderFunctions cases.
TEST_P(VideoDecoderPipelineTest, Initialize) {
  SetCreateDecoderFunctions(GetParam().create_decoder_functions);

  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnInit(MatchesStatusCode(GetParam().status_code)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  InitializeDecoder();
  run_loop.Run();

  EXPECT_EQ(GetParam().status_code == StatusCode::kOk,
            !!GetUnderlyingDecoder());
}

const struct DecoderPipelineTestParams kDecoderPipelineTestParams[] = {
    // An empty set of CreateDecoderFunctions.
    {{}, StatusCode::kChromeOSVideoDecoderNoDecoders},

    // Just one CreateDecoderFunctions that fails to Create() (i.e. returns a
    // null Decoder)
    {{&VideoDecoderPipelineTest::CreateNullMockDecoder},
     StatusCode::kDecoderFailedCreation},

    // Just one CreateDecoderFunctions that works fine, i.e. Create()s and
    // Initialize()s correctly.
    {{&VideoDecoderPipelineTest::CreateGoodMockDecoder}, StatusCode::kOk},

    // One CreateDecoderFunctions that Create()s ok but fails to Initialize()
    // correctly
    {{&VideoDecoderPipelineTest::CreateBadMockDecoder},
     StatusCode::kDecoderFailedInitialization},

    // Two CreateDecoderFunctions, one that fails to Create() (i.e. returns a
    // null Decoder), and one that works. The first error StatusCode is lost
    // because VideoDecoderPipeline::OnInitializeDone() throws it away.
    {{&VideoDecoderPipelineTest::CreateNullMockDecoder,
      &VideoDecoderPipelineTest::CreateGoodMockDecoder},
     StatusCode::kOk},

    // Two CreateDecoderFunctions, one that Create()s ok but fails  to
    // Initialize(), and one that works. The first error StatusCode is lost
    // because VideoDecoderPipeline::OnInitializeDone() throws it away.
    {{&VideoDecoderPipelineTest::CreateBadMockDecoder,
      &VideoDecoderPipelineTest::CreateGoodMockDecoder},
     StatusCode::kOk},

    // Two CreateDecoderFunctions, one that fails to Create() (i.e. returns a
    // null Decoder), and one that fails to Initialize(). The first error
    // StatusCode is the only one we can check here: a Status object is created
    // with a "primary" StatusCode, archiving subsequent ones in a private
    // member.
    {{&VideoDecoderPipelineTest::CreateNullMockDecoder,
      &VideoDecoderPipelineTest::CreateBadMockDecoder},
     StatusCode::kDecoderFailedCreation},
    // Previous one in reverse order.
    {{&VideoDecoderPipelineTest::CreateBadMockDecoder,
      &VideoDecoderPipelineTest::CreateNullMockDecoder},
     StatusCode::kDecoderFailedInitialization},

    {{&VideoDecoderPipelineTest::CreateBadMockDecoder,
      &VideoDecoderPipelineTest::CreateBadMockDecoder,
      &VideoDecoderPipelineTest::CreateGoodMockDecoder},
     StatusCode::kOk},
};

INSTANTIATE_TEST_SUITE_P(All,
                         VideoDecoderPipelineTest,
                         testing::ValuesIn(kDecoderPipelineTestParams));

}  // namespace media
