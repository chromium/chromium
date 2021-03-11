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
  VideoDecoderPipeline::CreateDecoderFunctionCB create_decoder_function_cb;
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
            // This callback needs to be configured in the individual tests.
            base::BindRepeating(
                &VideoDecoderPipelineTest::CreateNullMockDecoder))) {}
  ~VideoDecoderPipelineTest() override = default;

  void TearDown() override {
    VideoDecoderPipeline::DestroyAsync(std::move(decoder_));
    task_environment_.RunUntilIdle();
  }
  MOCK_METHOD1(OnInit, void(Status));
  MOCK_METHOD1(OnOutput, void(scoped_refptr<VideoFrame>));
  MOCK_METHOD0(OnResetDone, void());

  void SetCreateDecoderFunctionCB(
      VideoDecoderPipeline::CreateDecoderFunctionCB function) {
    decoder_->create_decoder_function_cb_ = std::move(function);
  }

  // Constructs |decoder_| with a given |create_decoder_function_cb| and
  // verifying |status_code| is received back in OnInit().
  void InitializeDecoder(
      VideoDecoderPipeline::CreateDecoderFunctionCB create_decoder_function_cb,
      StatusCode status_code) {
    SetCreateDecoderFunctionCB(create_decoder_function_cb);

    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnInit(MatchesStatusCode(status_code)))
        .WillOnce(RunClosure(run_loop.QuitClosure()));

    decoder_->Initialize(
        config_, false /* low_delay */, nullptr /* cdm_context */,
        base::BindOnce(&VideoDecoderPipelineTest::OnInit,
                       base::Unretained(this)),
        base::BindRepeating(&VideoDecoderPipelineTest::OnOutput,
                            base::Unretained(this)),
        base::DoNothing());
    run_loop.Run();
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
          std::move(init_cb).Run(StatusCode::kDecoderInitializationFailed);
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

// Verifies the status code for several typical CreateDecoderFunctionCB cases.
TEST_P(VideoDecoderPipelineTest, Initialize) {
  InitializeDecoder(GetParam().create_decoder_function_cb,
                    GetParam().status_code);

  EXPECT_EQ(GetParam().status_code == StatusCode::kOk,
            !!GetUnderlyingDecoder());
}

const struct DecoderPipelineTestParams kDecoderPipelineTestParams[] = {
    // A CreateDecoderFunctionCB that fails to Create() (i.e. returns a
    // null Decoder)
    {base::BindRepeating(&VideoDecoderPipelineTest::CreateNullMockDecoder),
     StatusCode::kDecoderFailedCreation},

    // A CreateDecoderFunctionCB that works fine, i.e. Create()s and
    // Initialize()s correctly.
    {base::BindRepeating(&VideoDecoderPipelineTest::CreateGoodMockDecoder),
     StatusCode::kOk},

    // A CreateDecoderFunctionCB that Create()s ok but fails to Initialize()
    // correctly.
    {base::BindRepeating(&VideoDecoderPipelineTest::CreateBadMockDecoder),
     StatusCode::kDecoderInitializationFailed},
};

INSTANTIATE_TEST_SUITE_P(All,
                         VideoDecoderPipelineTest,
                         testing::ValuesIn(kDecoderPipelineTestParams));

// Verifies the Reset sequence.
TEST_F(VideoDecoderPipelineTest, Reset) {
  InitializeDecoder(
      base::BindRepeating(&VideoDecoderPipelineTest::CreateGoodMockDecoder),
      StatusCode::kOk);

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
}  // namespace media
