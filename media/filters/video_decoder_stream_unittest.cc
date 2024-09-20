// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/base/fake_demuxer_stream.h"
#include "media/base/media_switches.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_helpers.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/decoder_stream.h"
#include "media/filters/fake_video_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "media/filters/decrypting_video_decoder.h"
#endif

#include <iostream>

using ::base::test::RunCallback;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Assign;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

namespace {
const int kNumConfigs = 4;
const int kNumBuffersInOneConfig = 5;
constexpr base::TimeDelta kPrepareDelay = base::Milliseconds(5);

static int GetDecoderId(int i) {
  return i;
}

}  // namespace

struct VideoDecoderStreamTestParams {
  VideoDecoderStreamTestParams(bool is_encrypted,
                               bool has_decryptor,
                               bool has_prepare,
                               int decoding_delay,
                               int parallel_decoding)
      : is_encrypted(is_encrypted),
        has_decryptor(has_decryptor),
        has_prepare(has_prepare),
        decoding_delay(decoding_delay),
        parallel_decoding(parallel_decoding) {}

  bool is_encrypted;
  bool has_decryptor;
  bool has_prepare;
  int decoding_delay;
  int parallel_decoding;
};

class VideoDecoderStreamTest
    : public testing::Test,
      public testing::WithParamInterface<VideoDecoderStreamTestParams> {
 public:
  VideoDecoderStreamTest()
      : is_initialized_(false),
        num_decoded_frames_(0),
        pending_initialize_(false),
        pending_read_(false),
        pending_reset_(false),
        pending_stop_(false),
        num_decoded_bytes_unreported_(0),
        has_no_key_(false) {
    video_decoder_stream_ = std::make_unique<VideoDecoderStream>(
        std::make_unique<VideoDecoderStream::StreamTraits>(&media_log_),
        task_environment_.GetMainThreadTaskRunner(),
        base::BindRepeating(&VideoDecoderStreamTest::CreateVideoDecodersForTest,
                            base::Unretained(this)),
        &media_log_);
    video_decoder_stream_->set_decoder_change_observer(base::BindRepeating(
        &VideoDecoderStreamTest::OnDecoderChanged, base::Unretained(this)));
    if (GetParam().has_prepare) {
      video_decoder_stream_->SetPrepareCB(base::BindRepeating(
          &VideoDecoderStreamTest::PrepareFrame, base::Unretained(this)));
    }

    if (GetParam().is_encrypted && GetParam().has_decryptor) {
      decryptor_ = std::make_unique<NiceMock<MockDecryptor>>();

      // Decryptor can only decrypt (not decrypt-and-decode) so that
      // DecryptingDemuxerStream will be used.
      EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
          .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<1>(false));
      EXPECT_CALL(*decryptor_, Decrypt(_, _, _))
          .WillRepeatedly(Invoke(this, &VideoDecoderStreamTest::Decrypt));
    }

    if (GetParam().is_encrypted) {
      cdm_context_ = std::make_unique<StrictMock<MockCdmContext>>();

      EXPECT_CALL(*cdm_context_, RegisterEventCB(_)).Times(AnyNumber());
      EXPECT_CALL(*cdm_context_, GetDecryptor())
          .WillRepeatedly(Return(decryptor_.get()));
    }

    // Covering most MediaLog messages for now.
    // TODO(wolenetz/xhwang): Fix tests to have better MediaLog checking.
    EXPECT_MEDIA_LOG(HasSubstr("video")).Times(AnyNumber());
    EXPECT_MEDIA_LOG(HasSubstr("Video")).Times(AnyNumber());
    EXPECT_MEDIA_LOG(HasSubstr("audio")).Times(AnyNumber());
    EXPECT_MEDIA_LOG(HasSubstr("Audio")).Times(AnyNumber());
    EXPECT_MEDIA_LOG(HasSubstr("decryptor")).Times(AnyNumber());
    EXPECT_MEDIA_LOG(HasSubstr("clear to encrypted buffers"))
        .Times(AnyNumber());
  }

  VideoDecoderStreamTest(const VideoDecoderStreamTest&) = delete;
  VideoDecoderStreamTest& operator=(const VideoDecoderStreamTest&) = delete;

  ~VideoDecoderStreamTest() {
    // Check that the pipeline statistics callback was fired correctly.
    EXPECT_EQ(num_decoded_bytes_unreported_, 0);

    is_initialized_ = false;
    decoders_.clear();
    video_decoder_stream_.reset();
    base::RunLoop().RunUntilIdle();

    DCHECK(!pending_initialize_);
    DCHECK(!pending_read_);
    DCHECK(!pending_reset_);
    DCHECK(!pending_stop_);
  }

  void CreateDemuxerStream(gfx::Size start_size, gfx::Vector2dF size_delta) {
    DCHECK(!demuxer_stream_);
    demuxer_stream_ = std::make_unique<FakeDemuxerStream>(
        kNumConfigs, kNumBuffersInOneConfig, GetParam().is_encrypted,
        start_size, size_delta);
  }

  void PrepareFrame(scoped_refptr<VideoFrame> frame,
                    VideoDecoderStream::OutputReadyCB output_ready_cb) {
    // Simulate some delay in return of the output.
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(output_ready_cb), std::move(frame)));
  }

  void PrepareFrameWithDelay(
      scoped_refptr<VideoFrame> frame,
      VideoDecoderStream::OutputReadyCB output_ready_cb) {
    task_environment_.FastForwardBy(kPrepareDelay);
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(output_ready_cb), std::move(frame)));
  }

  void OnBytesDecoded(int count) { num_decoded_bytes_unreported_ += count; }

  // Callback to create a list of decoders for the DecoderSelector to select
  // from. Decoder selection happens
  // - on the initial selection in Initialize(),
  // - on decoder reinitialization failure, which can be simulated by calling
  //   decoder_->SimulateFailureToInit(), and
  // - on decode error of the first buffer, which can be simulated by calling
  //   decoder_->SimulateError() before reading the first frame.
  std::vector<std::unique_ptr<VideoDecoder>> CreateVideoDecodersForTest() {
    // Previously decoders could have been destroyed on decoder reselection.
    decoders_.clear();

    // Provide 3 decoders to test fallback cases.
    // TODO(xhwang): We should test the case where only certain decoder
    // supports encrypted streams. Currently this is hard to test because we use
    // parameterized tests which need to pass in all combinations.
    std::vector<std::unique_ptr<VideoDecoder>> decoders;

#if !BUILDFLAG(IS_ANDROID)
    // Note this is _not_ inserted into |decoders_| below, so we don't need to
    // adjust the indices used below to compensate.
    decoders.push_back(std::make_unique<DecryptingVideoDecoder>(
        task_environment_.GetMainThreadTaskRunner(), &media_log_));
#endif

    for (int i = 0; i < 3; ++i) {
      auto decoder = std::make_unique<FakeVideoDecoder>(
          GetDecoderId(i), GetParam().decoding_delay,
          GetParam().parallel_decoding,
          base::BindRepeating(&VideoDecoderStreamTest::OnBytesDecoded,
                              base::Unretained(this)));

      if (GetParam().is_encrypted && !GetParam().has_decryptor)
        decoder->EnableEncryptedConfigSupport();

      // Keep a reference so we can change the behavior of each decoder.
      decoders_.push_back(decoder->GetWeakPtr());

      decoders.push_back(std::move(decoder));
    }

    for (const auto i : decoder_indices_to_fail_init_)
      decoders_[i]->SimulateFailureToInit();

    for (const auto i : decoder_indices_to_hold_init_)
      decoders_[i]->HoldNextInit();

    for (const auto i : decoder_indices_to_hold_decode_)
      decoders_[i]->HoldDecode();

    for (const auto i : platform_decoder_indices_)
      decoders_[i]->SetIsPlatformDecoder(true);

    return decoders;
  }

  void ClearDecoderInitExpectations() {
    decoder_indices_to_fail_init_.clear();
    decoder_indices_to_hold_init_.clear();
    decoder_indices_to_hold_decode_.clear();
    platform_decoder_indices_.clear();
  }

  // On next decoder selection, fail initialization on decoders specified by
  // |decoder_indices|.
  void FailDecoderInitOnSelection(std::vector<int> decoder_indices) {
    decoder_indices_to_fail_init_ = std::move(decoder_indices);
    for (int i : decoder_indices_to_fail_init_) {
      if (!decoders_.empty() && decoders_[i] && decoders_[i].get() != decoder_)
        decoders_[i]->SimulateFailureToInit();
    }
  }

  // On next decoder selection, hold initialization on decoders specified by
  // |decoder_indices|.
  void HoldDecoderInitOnSelection(std::vector<int> decoder_indices) {
    decoder_indices_to_hold_init_ = std::move(decoder_indices);
    for (int i : decoder_indices_to_hold_init_) {
      if (!decoders_.empty() && decoders_[i] && decoders_[i].get() != decoder_)
        decoders_[i]->HoldNextInit();
    }
  }

  // After next decoder selection, hold decode on decoders specified by
  // |decoder_indices|. This is needed because after decoder selection decode
  // may be resumed immediately and it'll be too late to hold decode then.
  void HoldDecodeAfterSelection(std::vector<int> decoder_indices) {
    decoder_indices_to_hold_decode_ = std::move(decoder_indices);
    for (int i : decoder_indices_to_hold_decode_) {
      if (!decoders_.empty() && decoders_[i] && decoders_[i].get() != decoder_)
        decoders_[i]->HoldDecode();
    }
  }

  void EnablePlatformDecoders(std::vector<int> decoder_indices) {
    platform_decoder_indices_ = std::move(decoder_indices);
    for (int i : platform_decoder_indices_) {
      if (!decoders_.empty() && decoders_[i] && decoders_[i].get() != decoder_)
        decoders_[i]->SetIsPlatformDecoder(true);
    }
  }

  // Updates the |decoder_| currently being used by VideoDecoderStream.
  void OnDecoderChanged(VideoDecoder* decoder) {
    if (!decoder) {
      decoder_ = nullptr;
      return;
    }

    // Ensure there's a media log created whenever selecting a decoder.
    EXPECT_MEDIA_LOG(HasSubstr("for video decoding, config"));
    decoder_ = static_cast<FakeVideoDecoder*>(decoder);
    ASSERT_TRUE(decoder_->GetDecoderId() == GetDecoderId(0) ||
                decoder_->GetDecoderId() == GetDecoderId(1) ||
                decoder_->GetDecoderId() == GetDecoderId(2));
  }

  MOCK_METHOD1(OnWaiting, void(WaitingReason));

  void OnStatistics(const PipelineStatistics& statistics) {
    num_decoded_bytes_unreported_ -= statistics.video_bytes_decoded;
  }

  void OnInitialized(bool success) {
    DCHECK(!pending_read_);
    DCHECK(!pending_reset_);
    DCHECK(pending_initialize_);
    pending_initialize_ = false;

    is_initialized_ = success;
    if (!success)
      decoders_.clear();
  }

  void Initialize() {
    if (!demuxer_stream_) {
      demuxer_stream_ = std::make_unique<FakeDemuxerStream>(
          kNumConfigs, kNumBuffersInOneConfig, GetParam().is_encrypted);
    }

    pending_initialize_ = true;
    video_decoder_stream_->Initialize(
        demuxer_stream_.get(),
        base::BindOnce(&VideoDecoderStreamTest::OnInitialized,
                       base::Unretained(this)),
        cdm_context_.get(),
        base::BindRepeating(&VideoDecoderStreamTest::OnStatistics,
                            base::Unretained(this)),
        base::BindRepeating(&VideoDecoderStreamTest::OnWaiting,
                            base::Unretained(this)));

    EXPECT_MEDIA_LOG(HasSubstr("video")).Times(AnyNumber());
    EXPECT_MEDIA_LOG(HasSubstr("Video")).Times(AnyNumber());
    EXPECT_MEDIA_LOG(HasSubstr("audio")).Times(AnyNumber());
    EXPECT_MEDIA_LOG(HasSubstr("Audio")).Times(AnyNumber());
    base::RunLoop().RunUntilIdle();
  }

  // Fake Decrypt() function used by DecryptingDemuxerStream. It does nothing
  // but removes the DecryptConfig to make the buffer unencrypted.
  void Decrypt(Decryptor::StreamType stream_type,
               scoped_refptr<DecoderBuffer> encrypted,
               Decryptor::DecryptCB decrypt_cb) {
    DCHECK(encrypted->decrypt_config());
    if (has_no_key_) {
      std::move(decrypt_cb).Run(Decryptor::kNoKey, nullptr);
      return;
    }

    DCHECK_EQ(stream_type, Decryptor::kVideo);
    scoped_refptr<DecoderBuffer> decrypted =
        DecoderBuffer::CopyFrom(*encrypted);
    if (encrypted->is_key_frame())
      decrypted->set_is_key_frame(true);
    decrypted->set_timestamp(encrypted->timestamp());
    decrypted->set_duration(encrypted->duration());
    std::move(decrypt_cb).Run(Decryptor::kSuccess, decrypted);
  }

  // Callback for VideoDecoderStream::Read().
  void FrameReady(VideoDecoderStream::ReadResult result) {
    DCHECK(pending_read_);
    last_read_status_code_ = result.code();
    scoped_refptr<VideoFrame> frame =
        last_read_status_code_ == DecoderStatus::Codes::kOk
            ? std::move(result).value()
            : nullptr;
    frame_read_ = frame;
    if (frame && !frame->metadata().end_of_stream) {
      EXPECT_EQ(*frame->metadata().frame_duration, demuxer_stream_->duration());

      num_decoded_frames_++;
    }
    pending_read_ = false;
  }

  void OnReset() {
    DCHECK(!pending_read_);
    DCHECK(pending_reset_);
    pending_reset_ = false;
  }

  void ReadOneFrame() {
    frame_read_ = nullptr;
    pending_read_ = true;
    video_decoder_stream_->Read(base::BindOnce(
        &VideoDecoderStreamTest::FrameReady, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void ReadUntilPending() {
    do {
      ReadOneFrame();
    } while (!pending_read_);
  }

  void ReadAllFrames(int expected_decoded_frames) {
    // Reading all frames reinitializes the demuxer.
    do {
      ReadOneFrame();
    } while (frame_read_.get() && !frame_read_->metadata().end_of_stream);

    DCHECK_EQ(expected_decoded_frames, num_decoded_frames_);
  }

  void ReadAllFrames() {
    // No frames should have been dropped.
    ReadAllFrames(kNumConfigs * kNumBuffersInOneConfig);
  }

  enum PendingState {
    NOT_PENDING,
    DEMUXER_READ_NORMAL,
    DEMUXER_READ_CONFIG_CHANGE,
    DECRYPTOR_NO_KEY,
    DECODER_REINIT,
    DECODER_DECODE,
    DECODER_RESET
  };

  void EnterPendingState(PendingState state) {
    DCHECK_NE(state, NOT_PENDING);
    switch (state) {
      case DEMUXER_READ_NORMAL:
        demuxer_stream_->HoldNextRead();
        ReadUntilPending();
        break;

      case DEMUXER_READ_CONFIG_CHANGE:
        demuxer_stream_->HoldNextConfigChangeRead();
        ReadUntilPending();
        break;

      case DECRYPTOR_NO_KEY:
        if (GetParam().is_encrypted && GetParam().has_decryptor) {
          EXPECT_MEDIA_LOG(HasSubstr("no key for key ID"));
          EXPECT_CALL(*this, OnWaiting(WaitingReason::kNoDecryptionKey));
          has_no_key_ = true;
        }
        ReadOneFrame();
        break;

      case DECODER_REINIT:
        decoder_->HoldNextInit();
        ReadUntilPending();
        break;

      case DECODER_DECODE:
        decoder_->HoldDecode();
        ReadUntilPending();
        break;

      case DECODER_RESET:
        decoder_->HoldNextReset();
        pending_reset_ = true;
        video_decoder_stream_->Reset(base::BindOnce(
            &VideoDecoderStreamTest::OnReset, base::Unretained(this)));
        base::RunLoop().RunUntilIdle();
        break;

      case NOT_PENDING:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  void SatisfyPendingCallback(PendingState state) {
    DCHECK_NE(state, NOT_PENDING);
    switch (state) {
      case DEMUXER_READ_CONFIG_CHANGE:
        EXPECT_MEDIA_LOG(HasSubstr("decoder config changed"))
            .Times(testing::AtLeast(1));
        [[fallthrough]];
      case DEMUXER_READ_NORMAL:
        demuxer_stream_->SatisfyRead();
        break;

      // This is only interesting to test during VideoDecoderStream destruction.
      // There's no need to satisfy a callback.
      case DECRYPTOR_NO_KEY:
        NOTREACHED_IN_MIGRATION();
        break;

      case DECODER_REINIT:
        decoder_->SatisfyInit();
        break;

      case DECODER_DECODE:
        decoder_->SatisfyDecode();
        break;

      case DECODER_RESET:
        decoder_->SatisfyReset();
        break;

      case NOT_PENDING:
        NOTREACHED_IN_MIGRATION();
        break;
    }

    base::RunLoop().RunUntilIdle();
  }

  void Read() {
    EnterPendingState(DECODER_DECODE);
    SatisfyPendingCallback(DECODER_DECODE);
  }

  void Reset() {
    EnterPendingState(DECODER_RESET);
    SatisfyPendingCallback(DECODER_RESET);
  }

  void ReadUntilDecoderReinitialized() {
    EnterPendingState(DECODER_REINIT);
    SatisfyPendingCallback(DECODER_REINIT);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList enabled_features_{
      kResolutionBasedDecoderPriority};

  StrictMock<MockMediaLog> media_log_;
  std::unique_ptr<VideoDecoderStream> video_decoder_stream_;
  std::unique_ptr<FakeDemuxerStream> demuxer_stream_;
  std::unique_ptr<StrictMock<MockCdmContext>> cdm_context_;

  // Use NiceMock since we don't care about most of calls on the decryptor.
  std::unique_ptr<NiceMock<MockDecryptor>> decryptor_;

  // References to the list of decoders to be select from by DecoderSelector.
  // Three decoders are needed to test that decoder fallback can occur more than
  // once on a config change. They are owned by |video_decoder_stream_|.
  std::vector<base::WeakPtr<FakeVideoDecoder>> decoders_;

  std::vector<int> decoder_indices_to_fail_init_;
  std::vector<int> decoder_indices_to_hold_init_;
  std::vector<int> decoder_indices_to_hold_decode_;
  std::vector<int> platform_decoder_indices_;

  // The current decoder used by |video_decoder_stream_|.
  raw_ptr<FakeVideoDecoder, AcrossTasksDanglingUntriaged> decoder_ = nullptr;

  bool is_initialized_;
  int num_decoded_frames_;
  bool pending_initialize_;
  bool pending_read_;
  bool pending_reset_;
  bool pending_stop_;
  int num_decoded_bytes_unreported_;
  scoped_refptr<VideoFrame> frame_read_;
  DecoderStatus::Codes last_read_status_code_;

  // Decryptor has no key to decrypt a frame.
  bool has_no_key_;
};

INSTANTIATE_TEST_SUITE_P(
    Clear,
    VideoDecoderStreamTest,
    ::testing::Values(VideoDecoderStreamTestParams(false, false, false, 0, 1),
                      VideoDecoderStreamTestParams(false, false, false, 3, 1),
                      VideoDecoderStreamTestParams(false, false, false, 7, 1),
                      VideoDecoderStreamTestParams(false, false, true, 0, 1),
                      VideoDecoderStreamTestParams(false, false, true, 3, 1)));

INSTANTIATE_TEST_SUITE_P(
    EncryptedWithDecryptor,
    VideoDecoderStreamTest,
    ::testing::Values(VideoDecoderStreamTestParams(true, true, false, 7, 1),
                      VideoDecoderStreamTestParams(true, true, true, 7, 1)));

INSTANTIATE_TEST_SUITE_P(
    EncryptedWithoutDecryptor,
    VideoDecoderStreamTest,
    ::testing::Values(VideoDecoderStreamTestParams(true, false, false, 7, 1),
                      VideoDecoderStreamTestParams(true, false, true, 7, 1)));

INSTANTIATE_TEST_SUITE_P(
    Clear_Parallel,
    VideoDecoderStreamTest,
    ::testing::Values(VideoDecoderStreamTestParams(false, false, false, 0, 3),
                      VideoDecoderStreamTestParams(false, false, false, 2, 3),
                      VideoDecoderStreamTestParams(false, false, true, 0, 3),
                      VideoDecoderStreamTestParams(false, false, true, 2, 3)));

TEST_P(VideoDecoderStreamTest, CanReadWithoutStallingAtAnyTime) {
  ASSERT_FALSE(video_decoder_stream_->CanReadWithoutStalling());
}

TEST_P(VideoDecoderStreamTest, Initialization) {
  Initialize();
  EXPECT_TRUE(is_initialized_);
}

TEST_P(VideoDecoderStreamTest, AllDecoderInitializationFails) {
  FailDecoderInitOnSelection({0, 1, 2});
  Initialize();
  EXPECT_FALSE(is_initialized_);
}

TEST_P(VideoDecoderStreamTest, PartialDecoderInitializationFails) {
  FailDecoderInitOnSelection({0, 1});
  Initialize();
  EXPECT_TRUE(is_initialized_);
}

TEST_P(VideoDecoderStreamTest, ReadOneFrame) {
  Initialize();
  Read();
}

TEST_P(VideoDecoderStreamTest, ReadAllFrames) {
  Initialize();
  ReadAllFrames();
}

TEST_P(VideoDecoderStreamTest, Read_AfterReset) {
  Initialize();
  Reset();
  Read();
  Reset();
  Read();
}

// Tests that the decoder stream will switch from a software decoder to a
// hardware decoder if the config size increases
TEST_P(VideoDecoderStreamTest, ConfigChangeSwToHw) {
  if (base::FeatureList::IsEnabled(kVideoDecodeBatching) &&
      GetParam().parallel_decoding != 1) {
    // Fake demuxer allows reading over different configs when batch decoding is
    // enabled, so we need to skip this test.
    return;
  }
  EnablePlatformDecoders({1});

  // Create a demuxer stream with a config that increases in size
  auto const size_delta =
      TestVideoConfig::LargeCodedSize() - TestVideoConfig::NormalCodedSize();
  auto const width_delta = size_delta.width() / (kNumConfigs - 1);
  auto const height_delta = size_delta.height() / (kNumConfigs - 1);
  CreateDemuxerStream(TestVideoConfig::NormalCodedSize(),
                      gfx::Vector2dF(width_delta, height_delta));
  auto base_config = demuxer_stream_->video_decoder_config();
  Initialize();

  // Initially we should be using a software decoder
  EXPECT_TRUE(decoder_);
  EXPECT_FALSE(decoder_->IsPlatformDecoder());

  ReadAllFrames();

  // We should end up on a hardware decoder
  EXPECT_TRUE(decoder_->IsPlatformDecoder());

  // Test goes through 3 size changes from the initial kHDSize, each
  // step increases by [width_delta, height_delta].
  auto expected_config = base_config;
  auto expected_size =
      expected_config.coded_size() +
      gfx::ScaleToCeiledSize(gfx::Size(width_delta, height_delta),
                             kNumConfigs - 1);
  expected_config.set_coded_size(expected_size);
  expected_config.set_visible_rect(gfx::Rect(expected_size));
  expected_config.set_natural_size(expected_size);
  ASSERT_FALSE(decoder_->eos_next_configs().empty());
  if (!decoder_->eos_next_configs().back().is_encrypted() &&
      expected_config.is_encrypted()) {
    expected_config.SetIsEncrypted(false);  // May be stripped by demuxer.
  }
  EXPECT_TRUE(decoder_->eos_next_configs().back().Matches(expected_config));
}

// Tests that the decoder stream will stay on a hardware decoder when the config
// size decreases.
TEST_P(VideoDecoderStreamTest, ConfigChangeHwToSw) {
  if (base::FeatureList::IsEnabled(kVideoDecodeBatching) &&
      GetParam().parallel_decoding != 1) {
    // Fake demuxer allows reading over different configs when batch decoding is
    // enabled, so we need to skip this test.
    return;
  }
  EnablePlatformDecoders({1});

  // Create a demuxer stream with a config that progressively decreases in size
  auto const size_delta =
      TestVideoConfig::LargeCodedSize() - TestVideoConfig::NormalCodedSize();
  auto const width_delta = size_delta.width() / (kNumConfigs - 1);
  auto const height_delta = size_delta.height() / (kNumConfigs - 1);
  CreateDemuxerStream(TestVideoConfig::LargeCodedSize(),
                      gfx::Vector2dF(-width_delta, -height_delta));
  Initialize();

  // We should initially be using a hardware decoder
  EXPECT_TRUE(decoder_);
  EXPECT_TRUE(decoder_->IsPlatformDecoder());
  ReadAllFrames();

  // We should remain on a hardware decoder.
  EXPECT_TRUE(decoder_->IsPlatformDecoder());
}

TEST_P(VideoDecoderStreamTest, Read_ProperMetadata) {
  // For testing simplicity, omit parallel decode tests with a delay in frames.
  if (GetParam().parallel_decoding > 1 && GetParam().decoding_delay > 0)
    return;

  if (GetParam().has_prepare) {
    // Override the basic PrepareFrame() for a version that moves the MockTime
    // by kPrepareDelay. This simulates real work done (e.g. YUV conversion).
    video_decoder_stream_->SetPrepareCB(
        base::BindRepeating(&VideoDecoderStreamTest::PrepareFrameWithDelay,
                            base::Unretained(this)));
  }

  constexpr base::TimeDelta kDecodeDelay = base::Milliseconds(10);

  Initialize();

  // Simulate time elapsed by the decoder.
  EnterPendingState(DECODER_DECODE);
  task_environment_.FastForwardBy(kDecodeDelay);

  SatisfyPendingCallback(DECODER_DECODE);

  EXPECT_TRUE(frame_read_);

  const VideoFrameMetadata& metadata = frame_read_->metadata();

  // Verify the decoding metadata is accurate.
  EXPECT_EQ(*metadata.decode_end_time - *metadata.decode_begin_time,
            kDecodeDelay);

  // Verify the processing metadata is accurate.
  const base::TimeDelta expected_processing_time =
      GetParam().has_prepare ? (kDecodeDelay + kPrepareDelay) : kDecodeDelay;

  EXPECT_EQ(*metadata.processing_time, expected_processing_time);
}

TEST_P(VideoDecoderStreamTest, Read_BlockedDemuxer) {
  Initialize();
  demuxer_stream_->HoldNextRead();
  ReadOneFrame();
  EXPECT_TRUE(pending_read_);

  int demuxed_buffers = 0;

  // Pass frames from the demuxer to the VideoDecoderStream until the first read
  // request is satisfied.
  while (pending_read_) {
    ++demuxed_buffers;
    demuxer_stream_->SatisfyReadAndHoldNext();
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_EQ(std::min(GetParam().decoding_delay + 1, kNumBuffersInOneConfig + 1),
            demuxed_buffers);

  // At this point the stream is waiting on read from the demuxer, but there is
  // no pending read from the stream. The stream should be blocked if we try
  // reading from it again.
  ReadUntilPending();

  demuxer_stream_->SatisfyRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(pending_read_);
}

TEST_P(VideoDecoderStreamTest, Read_BlockedDemuxerAndDecoder) {
  // Test applies only when the decoder allows multiple parallel requests.
  if (GetParam().parallel_decoding == 1)
    return;

  Initialize();
  demuxer_stream_->HoldNextRead();
  decoder_->HoldDecode();
  ReadOneFrame();
  EXPECT_TRUE(pending_read_);

  int demuxed_buffers = 0;

  // Pass frames from the demuxer to the VideoDecoderStream until the first read
  // request is satisfied, while always keeping one decode request pending.
  while (pending_read_) {
    ++demuxed_buffers;
    demuxer_stream_->SatisfyReadAndHoldNext();
    base::RunLoop().RunUntilIdle();

    // Always keep one decode request pending.
    if (demuxed_buffers > 1) {
      decoder_->SatisfySingleDecode();
      base::RunLoop().RunUntilIdle();
    }
  }

  ReadUntilPending();
  EXPECT_TRUE(pending_read_);

  // Unblocking one decode request should unblock read even when demuxer is
  // still blocked.
  decoder_->SatisfySingleDecode();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(pending_read_);

  // Stream should still be blocked on the demuxer after unblocking the decoder.
  decoder_->SatisfyDecode();
  ReadUntilPending();
  EXPECT_TRUE(pending_read_);

  // Verify that the stream has returned all frames that have been demuxed,
  // accounting for the decoder delay.
  EXPECT_EQ(demuxed_buffers - GetParam().decoding_delay, num_decoded_frames_);

  // Unblocking the demuxer will unblock the stream.
  demuxer_stream_->SatisfyRead();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(pending_read_);
}

TEST_P(VideoDecoderStreamTest, BatchDecodingWithPlatformDecoder) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kVideoDecodeBatching);
  int parallel_decodings = GetParam().parallel_decoding;

  Initialize();
  decoder_->SetIsPlatformDecoder(true);

  // Block the decoder so that we can check the DecoderBuffer number got
  // from a single Read() call.
  decoder_->HoldDecode();
  ReadOneFrame();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(parallel_decodings, demuxer_stream_->num_buffers_returned());

  demuxer_stream_->HoldNextRead();
  decoder_->SatisfyDecode();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(decoder_->total_decoded_frames(), parallel_decodings);
}

TEST_P(VideoDecoderStreamTest, NoBatchDecodingWithNonPlatformDecoder) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kVideoDecodeBatching);

  Initialize();
  // Set the decoder as not platform decoder, so that it prevents single
  // demuxer read to return multiple DecoderBuffers.
  decoder_->SetIsPlatformDecoder(false);

  // Block the demuxer so that we can manually unblock the first demuxer
  // read to check the DecoderBuffer number got from a single Read() call.
  demuxer_stream_->HoldNextRead();
  decoder_->HoldDecode();
  ReadOneFrame();
  demuxer_stream_->SatisfyReadAndHoldNext();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, demuxer_stream_->num_buffers_returned());

  decoder_->SatisfyDecode();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(decoder_->total_decoded_frames(), 1);
}

TEST_P(VideoDecoderStreamTest, Read_DuringEndOfStreamDecode) {
  // Test applies only when the decoder allows multiple parallel requests, and
  // they are not satisfied in a single batch.
  if (GetParam().parallel_decoding == 1 || GetParam().decoding_delay != 0)
    return;

  Initialize();
  decoder_->HoldDecode();

  // Read all of the frames up to end of stream. Since parallel decoding is
  // enabled, the end of stream buffer will be sent to the decoder immediately,
  // but we don't satisfy it yet.
  for (int configuration = 0; configuration < kNumConfigs; configuration++) {
    for (int frame = 0; frame < kNumBuffersInOneConfig; frame++) {
      ReadOneFrame();
      while (pending_read_) {
        decoder_->SatisfySingleDecode();
        base::RunLoop().RunUntilIdle();
      }
    }
  }

  // Read() again. The callback must be delayed until the decode completes.
  ReadOneFrame();
  ASSERT_TRUE(pending_read_);

  // Satisfy decoding of the end of stream buffer. The read should complete.
  decoder_->SatisfySingleDecode();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(pending_read_);
  EXPECT_EQ(last_read_status_code_, DecoderStatus::Codes::kOk);

  // The read output should indicate end of stream.
  ASSERT_TRUE(frame_read_.get());
  EXPECT_TRUE(frame_read_->metadata().end_of_stream);
}

TEST_P(VideoDecoderStreamTest, Read_DemuxerStreamReadError) {
  Initialize();
  EnterPendingState(DEMUXER_READ_NORMAL);

  InSequence s;

  if (GetParam().is_encrypted && GetParam().has_decryptor) {
    EXPECT_MEDIA_LOG(
        HasSubstr("DecryptingDemuxerStream: demuxer stream read error"));
  }
  EXPECT_MEDIA_LOG(HasSubstr("video demuxer stream read error"));

  demuxer_stream_->Error();
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(pending_read_);
  EXPECT_NE(last_read_status_code_, DecoderStatus::Codes::kOk);
  EXPECT_NE(last_read_status_code_, DecoderStatus::Codes::kAborted);
}

// No Reset() before initialization is successfully completed.
TEST_P(VideoDecoderStreamTest, Reset_AfterInitialization) {
  Initialize();
  Reset();
  Read();
}

TEST_P(VideoDecoderStreamTest, Reset_DuringReinitialization) {
  Initialize();

  EnterPendingState(DECODER_REINIT);
  // VideoDecoder::Reset() is not called when we reset during reinitialization.
  pending_reset_ = true;
  video_decoder_stream_->Reset(
      base::BindOnce(&VideoDecoderStreamTest::OnReset, base::Unretained(this)));
  SatisfyPendingCallback(DECODER_REINIT);
  Read();
}

TEST_P(VideoDecoderStreamTest, Reset_AfterReinitialization) {
  Initialize();
  EnterPendingState(DECODER_REINIT);
  SatisfyPendingCallback(DECODER_REINIT);
  Reset();
  Read();
}

TEST_P(VideoDecoderStreamTest, Reset_DuringDemuxerRead_Normal) {
  Initialize();
  EnterPendingState(DEMUXER_READ_NORMAL);
  EnterPendingState(DECODER_RESET);
  SatisfyPendingCallback(DEMUXER_READ_NORMAL);
  SatisfyPendingCallback(DECODER_RESET);
  Read();
}

TEST_P(VideoDecoderStreamTest, Reset_DuringDemuxerRead_ConfigChange) {
  Initialize();
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);
  EnterPendingState(DECODER_RESET);
  SatisfyPendingCallback(DEMUXER_READ_CONFIG_CHANGE);
  SatisfyPendingCallback(DECODER_RESET);
  Read();
}

TEST_P(VideoDecoderStreamTest, Reset_DuringNormalDecoderDecode) {
  Initialize();
  EnterPendingState(DECODER_DECODE);
  EnterPendingState(DECODER_RESET);
  SatisfyPendingCallback(DECODER_DECODE);
  SatisfyPendingCallback(DECODER_RESET);
  Read();
}

TEST_P(VideoDecoderStreamTest, Reset_AfterNormalRead) {
  Initialize();
  Read();
  Reset();
  Read();
}

TEST_P(VideoDecoderStreamTest, Reset_AfterDemuxerRead_ConfigChange) {
  Initialize();
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);
  SatisfyPendingCallback(DEMUXER_READ_CONFIG_CHANGE);
  Reset();
  Read();
}

TEST_P(VideoDecoderStreamTest, Reset_AfterEndOfStream) {
  Initialize();
  ReadAllFrames();
  Reset();
  num_decoded_frames_ = 0;
  demuxer_stream_->SeekToStart();
  ReadAllFrames();
}

TEST_P(VideoDecoderStreamTest, Reset_DuringNoKeyRead) {
  Initialize();
  EnterPendingState(DECRYPTOR_NO_KEY);
  Reset();
}

// In the following Destroy_* tests, |video_decoder_stream_| is destroyed in
// VideoDecoderStreamTest destructor.

TEST_P(VideoDecoderStreamTest, Destroy_BeforeInitialization) {}

TEST_P(VideoDecoderStreamTest, Destroy_DuringInitialization) {
  HoldDecoderInitOnSelection({0});
  Initialize();
}

TEST_P(VideoDecoderStreamTest, Destroy_AfterInitialization) {
  Initialize();
}

TEST_P(VideoDecoderStreamTest, Destroy_DuringReinitialization) {
  Initialize();
  EnterPendingState(DECODER_REINIT);
}

TEST_P(VideoDecoderStreamTest, Destroy_AfterReinitialization) {
  Initialize();
  EnterPendingState(DECODER_REINIT);
  SatisfyPendingCallback(DECODER_REINIT);
}

TEST_P(VideoDecoderStreamTest, Destroy_DuringDemuxerRead_Normal) {
  Initialize();
  EnterPendingState(DEMUXER_READ_NORMAL);
}

TEST_P(VideoDecoderStreamTest, Destroy_DuringDemuxerRead_ConfigChange) {
  Initialize();
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);
}

TEST_P(VideoDecoderStreamTest, Destroy_DuringNormalDecoderDecode) {
  Initialize();
  EnterPendingState(DECODER_DECODE);
}

TEST_P(VideoDecoderStreamTest, Destroy_AfterNormalRead) {
  Initialize();
  Read();
}

TEST_P(VideoDecoderStreamTest, Destroy_AfterConfigChangeRead) {
  Initialize();
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);
  SatisfyPendingCallback(DEMUXER_READ_CONFIG_CHANGE);
}

TEST_P(VideoDecoderStreamTest, Destroy_DuringDecoderReinitialization) {
  Initialize();
  EnterPendingState(DECODER_REINIT);
}

TEST_P(VideoDecoderStreamTest, Destroy_DuringNoKeyRead) {
  Initialize();
  EnterPendingState(DECRYPTOR_NO_KEY);
}

TEST_P(VideoDecoderStreamTest, Destroy_DuringReset) {
  Initialize();
  EnterPendingState(DECODER_RESET);
}

TEST_P(VideoDecoderStreamTest, Destroy_AfterReset) {
  Initialize();
  Reset();
}

TEST_P(VideoDecoderStreamTest, Destroy_DuringRead_DuringReset) {
  Initialize();
  EnterPendingState(DECODER_DECODE);
  EnterPendingState(DECODER_RESET);
}

TEST_P(VideoDecoderStreamTest, Destroy_AfterRead_DuringReset) {
  Initialize();
  EnterPendingState(DECODER_DECODE);
  EnterPendingState(DECODER_RESET);
  SatisfyPendingCallback(DECODER_DECODE);
}

TEST_P(VideoDecoderStreamTest, Destroy_AfterRead_AfterReset) {
  Initialize();
  Read();
  Reset();
}

// The following tests cover the fallback logic after reinitialization error or
// decode error of the first buffer after initialization.

TEST_P(VideoDecoderStreamTest, FallbackDecoder_DecodeError) {
  Initialize();
  decoder_->SimulateError();
  ReadOneFrame();

  // |video_decoder_stream_| should have fallen back to a new decoder.
  ASSERT_EQ(GetDecoderId(1), decoder_->GetDecoderId());

  ASSERT_FALSE(pending_read_);
  ASSERT_EQ(last_read_status_code_, DecoderStatus::Codes::kOk);

  // Check that we fell back to Decoder2.
  ASSERT_GT(decoder_->total_bytes_decoded(), 0);

  // Verify no frame was dropped.
  ReadAllFrames();
}

TEST_P(VideoDecoderStreamTest,
       FallbackDecoder_EndOfStreamReachedBeforeFallback) {
  // Only consider cases where there is a decoder delay. For test simplicity,
  // omit the parallel case.
  if (GetParam().decoding_delay == 0 || GetParam().parallel_decoding > 1)
    return;

  Initialize();
  decoder_->HoldDecode();
  ReadOneFrame();

  // One buffer should have already pulled from the demuxer stream. Set the next
  // one to be an EOS.
  demuxer_stream_->SeekToEndOfStream();

  decoder_->SatisfySingleDecode();
  base::RunLoop().RunUntilIdle();

  // |video_decoder_stream_| should not have emitted a frame.
  EXPECT_TRUE(pending_read_);

  // Pending buffers should contain a regular buffer and an EOS buffer.
  EXPECT_EQ(video_decoder_stream_->get_pending_buffers_size_for_testing(), 2);

  decoder_->SimulateError();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(GetDecoderId(1), decoder_->GetDecoderId());

  // A frame should have been emitted.
  EXPECT_FALSE(pending_read_);
  EXPECT_EQ(last_read_status_code_, DecoderStatus::Codes::kOk);
  EXPECT_FALSE(frame_read_->metadata().end_of_stream);
  EXPECT_GT(decoder_->total_bytes_decoded(), 0);

  ReadOneFrame();

  EXPECT_FALSE(pending_read_);
  EXPECT_EQ(0, video_decoder_stream_->get_fallback_buffers_size_for_testing());
  EXPECT_TRUE(frame_read_->metadata().end_of_stream);
}

TEST_P(VideoDecoderStreamTest,
       FallbackDecoder_DoesReinitializeStompPendingRead) {
  // Test only the case where there is no decoding delay and parallel decoding.
  if (GetParam().decoding_delay != 0 || GetParam().parallel_decoding <= 1)
    return;

  Initialize();
  decoder_->HoldDecode();

  // Queue one read, defer the second.
  frame_read_ = nullptr;
  pending_read_ = true;
  video_decoder_stream_->Read(base::BindOnce(
      &VideoDecoderStreamTest::FrameReady, base::Unretained(this)));
  demuxer_stream_->HoldNextRead();

  // Force an error to occur on the first decode, but ensure it isn't propagated
  // until after the next read has been started.
  decoder_->SimulateError();
  HoldDecodeAfterSelection({1});

  // Complete the fallback to the second decoder with the read still pending.
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(GetDecoderId(1), decoder_->GetDecoderId());

  // Can't check the original decoder right now, it might have been destroyed
  // already. Verify that there was nothing decoded until we kicked the decoder.
  EXPECT_EQ(decoder_->total_bytes_decoded(), 0);
  decoder_->SatisfyDecode();
  const int first_decoded_bytes = decoder_->total_bytes_decoded();
  ASSERT_GT(first_decoded_bytes, 0);

  // Satisfy the previously pending read and ensure it is decoded.
  demuxer_stream_->SatisfyRead();
  base::RunLoop().RunUntilIdle();
  ASSERT_GT(decoder_->total_bytes_decoded(), first_decoded_bytes);
}

TEST_P(VideoDecoderStreamTest, FallbackDecoder_DecodeErrorRepeated) {
  Initialize();

  // Hold other decoders to simulate errors.
  HoldDecodeAfterSelection({1, 2});

  // Simulate decode error to trigger the fallback path.
  decoder_->SimulateError();
  ReadOneFrame();
  base::RunLoop().RunUntilIdle();

  // Expect decoder 1 to be tried.
  ASSERT_EQ(GetDecoderId(1), decoder_->GetDecoderId());
  decoder_->SimulateError();
  base::RunLoop().RunUntilIdle();

  // Then decoder 2.
  ASSERT_EQ(GetDecoderId(2), decoder_->GetDecoderId());
  decoder_->SimulateError();
  base::RunLoop().RunUntilIdle();

  // No decoders left, expect failure.
  EXPECT_EQ(decoder_, nullptr);
  EXPECT_FALSE(pending_read_);
  EXPECT_NE(last_read_status_code_, DecoderStatus::Codes::kOk);
  EXPECT_NE(last_read_status_code_, DecoderStatus::Codes::kAborted);
}

// This tests verifies that we properly fallback to a new decoder if the first
// decode after a config change fails.
TEST_P(VideoDecoderStreamTest,
       FallbackDecoder_SelectedOnMidstreamDecodeErrorAfterReinitialization) {
  // For simplicity of testing, this test applies only when there is no decoder
  // delay and parallel decoding is disabled.
  if (GetParam().decoding_delay != 0 || GetParam().parallel_decoding > 1)
    return;

  Initialize();

  // Note: Completes decoding one frame, results in Decode() being called with
  // second frame that is not completed.
  ReadOneFrame();

  // Verify that the first frame was decoded successfully.
  EXPECT_FALSE(pending_read_);
  EXPECT_GT(decoder_->total_bytes_decoded(), 0);
  EXPECT_EQ(last_read_status_code_, DecoderStatus::Codes::kOk);

  // Continue up to the point of reinitialization.
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);

  // Hold decodes to prevent a frame from being outputted upon reinitialization.
  decoder_->HoldDecode();
  SatisfyPendingCallback(DEMUXER_READ_CONFIG_CHANGE);

  // DecoderStream sends an EOS to flush the decoder during config changes.
  // Let the EOS decode be satisfied to properly complete the decoder reinit.
  decoder_->SatisfySingleDecode();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(pending_read_);

  // Fail the first decode, before a frame can be outputted.
  decoder_->SimulateError();
  base::RunLoop().RunUntilIdle();

  ReadOneFrame();

  // Verify that fallback happened.
  EXPECT_EQ(GetDecoderId(0), decoder_->GetDecoderId());
  EXPECT_FALSE(pending_read_);
  EXPECT_EQ(last_read_status_code_, DecoderStatus::Codes::kOk);
  EXPECT_GT(decoder_->total_bytes_decoded(), 0);
}

TEST_P(VideoDecoderStreamTest,
       FallbackDecoder_DecodeErrorRepeated_AfterReinitialization) {
  Initialize();

  // Simulate decode error to trigger fallback.
  decoder_->SimulateError();
  ReadOneFrame();
  base::RunLoop().RunUntilIdle();

  // Simulate reinitialize error of decoder 1.
  ASSERT_EQ(GetDecoderId(1), decoder_->GetDecoderId());
  decoder_->SimulateFailureToInit();
  HoldDecodeAfterSelection({0, 1, 2});
  ReadUntilDecoderReinitialized();

  // Decoder 0 should be selected again.
  ASSERT_EQ(GetDecoderId(0), decoder_->GetDecoderId());
  decoder_->SimulateError();
  base::RunLoop().RunUntilIdle();

  // Decoder 1.
  ASSERT_EQ(GetDecoderId(1), decoder_->GetDecoderId());
  decoder_->SimulateError();
  base::RunLoop().RunUntilIdle();

  // Decoder 2.
  ASSERT_EQ(GetDecoderId(2), decoder_->GetDecoderId());
  decoder_->SimulateError();
  base::RunLoop().RunUntilIdle();

  // No decoders left.
  EXPECT_EQ(decoder_, nullptr);
  EXPECT_FALSE(pending_read_);
  EXPECT_NE(last_read_status_code_, DecoderStatus::Codes::kOk);
  EXPECT_NE(last_read_status_code_, DecoderStatus::Codes::kAborted);
}

TEST_P(VideoDecoderStreamTest,
       FallbackDecoder_ConfigChangeClearsPendingBuffers) {
  // Test case is only interesting if the decoder can receive a config change
  // before returning its first frame.
  if (GetParam().decoding_delay < kNumBuffersInOneConfig)
    return;

  Initialize();
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);
  ASSERT_GT(video_decoder_stream_->get_pending_buffers_size_for_testing(), 0);

  SatisfyPendingCallback(DEMUXER_READ_CONFIG_CHANGE);
  ASSERT_EQ(video_decoder_stream_->get_pending_buffers_size_for_testing(), 0);
  EXPECT_FALSE(pending_read_);

  ReadAllFrames();
}

TEST_P(VideoDecoderStreamTest,
       FallbackDecoder_ErrorDuringConfigChangeFlushing) {
  // Test case is only interesting if the decoder can receive a config change
  // before returning its first frame.
  if (GetParam().decoding_delay < kNumBuffersInOneConfig)
    return;

  Initialize();
  EnterPendingState(DEMUXER_READ_CONFIG_CHANGE);
  EXPECT_GT(video_decoder_stream_->get_pending_buffers_size_for_testing(), 0);

  decoder_->HoldDecode();
  SatisfyPendingCallback(DEMUXER_READ_CONFIG_CHANGE);

  // The flush request should have been sent and held.
  EXPECT_EQ(video_decoder_stream_->get_pending_buffers_size_for_testing(), 0);
  EXPECT_TRUE(pending_read_);

  // Triggering an error here will cause the frames in selected decoder to be
  // lost. There are no pending buffers to give to |decoders_[1]| due to
  // http://crbug.com/603713
  decoder_->SimulateError();
  base::RunLoop().RunUntilIdle();

  // We want to make sure the fallback decoder can decode the rest of the frames
  // in the demuxer stream.
  ReadAllFrames(kNumBuffersInOneConfig * (kNumConfigs - 1));
}

TEST_P(VideoDecoderStreamTest,
       FallbackDecoder_PendingBuffersIsFilledAndCleared) {
  // Test applies only when there is a decoder delay, and the decoder will not
  // receive a config change before outputting its first frame. Parallel
  // decoding is also disabled in this test case, for readability and simplicity
  // of the unit test.
  if (GetParam().decoding_delay == 0 ||
      GetParam().decoding_delay > kNumBuffersInOneConfig ||
      GetParam().parallel_decoding > 1) {
    return;
  }

  Initialize();

  // Block on demuxer read and decoder decode so we can step through.
  demuxer_stream_->HoldNextRead();
  decoder_->HoldDecode();
  ReadOneFrame();

  int demuxer_reads_satisfied = 0;
  // Send back and requests buffers until the next one would fill the decoder
  // delay.
  while (demuxer_reads_satisfied < GetParam().decoding_delay - 1) {
    // Send a buffer back.
    demuxer_stream_->SatisfyReadAndHoldNext();
    base::RunLoop().RunUntilIdle();
    ++demuxer_reads_satisfied;

    // Decode one buffer.
    decoder_->SatisfySingleDecode();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(pending_read_);
    EXPECT_EQ(demuxer_reads_satisfied,
              video_decoder_stream_->get_pending_buffers_size_for_testing());
    // No fallback buffers should be queued up yet.
    EXPECT_EQ(0,
              video_decoder_stream_->get_fallback_buffers_size_for_testing());
  }

  // Hold the init before triggering the error, to verify internal state.
  demuxer_stream_->SatisfyReadAndHoldNext();
  ++demuxer_reads_satisfied;

  decoder_->SimulateError();

  HoldDecoderInitOnSelection({1});
  HoldDecodeAfterSelection({1});

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(pending_read_);
  EXPECT_EQ(demuxer_reads_satisfied,
            video_decoder_stream_->get_pending_buffers_size_for_testing());

  decoders_[1]->SatisfyInit();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(GetDecoderId(1), decoder_->GetDecoderId());

  // Make sure the pending buffers have been transferred to fallback buffers.
  // One call to Decode() during the initialization process, so we expect one
  // buffer to already have been consumed from the fallback buffers.
  // Pending buffers should never go down (unless we encounter a config change)
  EXPECT_EQ(demuxer_reads_satisfied - 1,
            video_decoder_stream_->get_fallback_buffers_size_for_testing());
  EXPECT_EQ(demuxer_reads_satisfied,
            video_decoder_stream_->get_pending_buffers_size_for_testing());

  decoder_->SatisfyDecode();
  base::RunLoop().RunUntilIdle();

  // Make sure all buffers consumed by |decoders_| have come from the fallback.
  // Pending buffers should not have been cleared yet.
  EXPECT_EQ(0, video_decoder_stream_->get_fallback_buffers_size_for_testing());
  EXPECT_EQ(demuxer_reads_satisfied,
            video_decoder_stream_->get_pending_buffers_size_for_testing());
  EXPECT_TRUE(pending_read_);

  // Give the decoder one more buffer, enough to release a frame.
  demuxer_stream_->SatisfyReadAndHoldNext();
  base::RunLoop().RunUntilIdle();

  // New buffers should not have been added after the frame was released.
  EXPECT_EQ(video_decoder_stream_->get_pending_buffers_size_for_testing(), 0);
  EXPECT_FALSE(pending_read_);

  demuxer_stream_->SatisfyRead();

  // Confirm no frames were dropped.
  ReadAllFrames();
}

TEST_P(VideoDecoderStreamTest, FallbackDecoder_SelectedOnDecodeThenInitErrors) {
  Initialize();
  decoder_->SimulateError();
  FailDecoderInitOnSelection({1});
  ReadOneFrame();

  // Decoder 0 should be blocked, and decoder 1 fails to initialize, so
  // |video_decoder_stream_| should have fallen back to decoder 2.
  ASSERT_EQ(GetDecoderId(2), decoder_->GetDecoderId());

  ASSERT_FALSE(pending_read_);
  ASSERT_EQ(last_read_status_code_, DecoderStatus::Codes::kOk);

  // Can't check previously selected decoder(s) right now, they might have been
  // destroyed already.
  ASSERT_GT(decoder_->total_bytes_decoded(), 0);

  // Verify no frame was dropped.
  ReadAllFrames();
}

TEST_P(VideoDecoderStreamTest, FallbackDecoder_SelectedOnInitThenDecodeErrors) {
  FailDecoderInitOnSelection({0});
  Initialize();
  ASSERT_EQ(GetDecoderId(1), decoder_->GetDecoderId());
  ClearDecoderInitExpectations();

  decoder_->HoldDecode();
  ReadOneFrame();
  decoder_->SimulateError();
  base::RunLoop().RunUntilIdle();

  // |video_decoder_stream_| should have fallen back to decoder 2.
  ASSERT_EQ(GetDecoderId(2), decoder_->GetDecoderId());

  ASSERT_FALSE(pending_read_);
  ASSERT_EQ(last_read_status_code_, DecoderStatus::Codes::kOk);

  // Can't check previously selected decoder(s) right now, they might have been
  // destroyed already.
  ASSERT_GT(decoder_->total_bytes_decoded(), 0);

  // Verify no frame was dropped.
  ReadAllFrames();
}

TEST_P(VideoDecoderStreamTest,
       FallbackDecoder_NotSelectedOnMidstreamDecodeError) {
  Initialize();
  ReadOneFrame();

  // Successfully received a frame.
  EXPECT_FALSE(pending_read_);
  ASSERT_GT(decoder_->total_bytes_decoded(), 0);

  decoder_->SimulateError();

  // The error must surface from Read() as DECODE_ERROR.
  while (last_read_status_code_ == DecoderStatus::Codes::kOk) {
    ReadOneFrame();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(pending_read_);
  }

  // Verify the error was surfaced, rather than falling back to other decoders.
  ASSERT_EQ(GetDecoderId(0), decoder_->GetDecoderId());
  EXPECT_FALSE(pending_read_);
  EXPECT_NE(last_read_status_code_, DecoderStatus::Codes::kOk);
  EXPECT_NE(last_read_status_code_, DecoderStatus::Codes::kAborted);
}

TEST_P(VideoDecoderStreamTest, DecoderErrorWhenNotReading) {
  Initialize();
  decoder_->HoldDecode();
  ReadOneFrame();
  EXPECT_TRUE(pending_read_);

  // Satisfy decode requests until we get the first frame out.
  while (pending_read_) {
    decoder_->SatisfySingleDecode();
    base::RunLoop().RunUntilIdle();
  }

  // Trigger an error in the decoding.
  decoder_->SimulateError();

  // The error must surface from Read() as DECODE_ERROR.
  while (last_read_status_code_ == DecoderStatus::Codes::kOk) {
    ReadOneFrame();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(pending_read_);
  }
  EXPECT_NE(last_read_status_code_, DecoderStatus::Codes::kOk);
  EXPECT_NE(last_read_status_code_, DecoderStatus::Codes::kAborted);
}

TEST_P(VideoDecoderStreamTest, ReinitializeFailure_Once) {
  Initialize();
  decoder_->SimulateFailureToInit();
  ReadUntilDecoderReinitialized();
  // Should have fallen back to a new instance of decoder 0.
  ASSERT_EQ(GetDecoderId(0), decoder_->GetDecoderId());
  ReadAllFrames();
  ASSERT_GT(decoder_->total_bytes_decoded(), 0);
}

TEST_P(VideoDecoderStreamTest, ReinitializeFailure_Twice) {
  Initialize();

  // Trigger reinitialization error, and fallback to a new instance.
  decoder_->SimulateFailureToInit();
  ReadUntilDecoderReinitialized();
  ASSERT_EQ(GetDecoderId(0), decoder_->GetDecoderId());

  ReadOneFrame();

  // Trigger reinitialization error again. Since a frame was output, this will
  // be a new instance of decoder 0 again.
  decoder_->SimulateFailureToInit();
  ReadUntilDecoderReinitialized();
  ASSERT_EQ(GetDecoderId(0), decoder_->GetDecoderId());
  ReadAllFrames();
}

TEST_P(VideoDecoderStreamTest, ReinitializeFailure_OneUnsupportedDecoder) {
  Initialize();

  // The current decoder will fail to reinitialize.
  decoder_->SimulateFailureToInit();

  // Decoder 1 will also fail to initialize on decoder selection.
  FailDecoderInitOnSelection({0, 1});

  ReadUntilDecoderReinitialized();

  // As a result, decoder 2 will be selected.
  ASSERT_EQ(GetDecoderId(2), decoder_->GetDecoderId());

  ReadAllFrames();
}

TEST_P(VideoDecoderStreamTest, ReinitializeFailure_NoSupportedDecoder) {
  Initialize();

  // The current decoder will fail to reinitialize, triggering decoder
  // selection.
  decoder_->SimulateFailureToInit();

  // All of the decoders will fail in decoder selection.
  FailDecoderInitOnSelection({0, 1, 2});

  ReadUntilDecoderReinitialized();

  // The error will surface from Read() as DECODE_ERROR.
  while (last_read_status_code_ == DecoderStatus::Codes::kOk) {
    ReadOneFrame();
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(pending_read_);
  }
  EXPECT_NE(last_read_status_code_, DecoderStatus::Codes::kOk);
  EXPECT_NE(last_read_status_code_, DecoderStatus::Codes::kAborted);
}

TEST_P(VideoDecoderStreamTest, Destroy_DuringFallbackDecoderSelection) {
  Initialize();
  decoder_->SimulateFailureToInit();
  EnterPendingState(DECODER_REINIT);
  HoldDecoderInitOnSelection({1});
  SatisfyPendingCallback(DECODER_REINIT);
}

}  // namespace media
