// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/filters/decrypting_video_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::base::test::RunCallback;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

const uint8_t kFakeKeyId[] = {0x4b, 0x65, 0x79, 0x20, 0x49, 0x44};
const uint8_t kFakeIv[DecryptConfig::kDecryptionKeySize] = {0};
const int kDecodingDelay = 3;

// Create a fake non-empty encrypted buffer.
static scoped_refptr<DecoderBuffer> CreateFakeEncryptedBuffer() {
  const int buffer_size = 16;  // Need a non-empty buffer;
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(buffer_size));
  buffer->set_decrypt_config(DecryptConfig::CreateCencConfig(
      std::string(reinterpret_cast<const char*>(kFakeKeyId),
                  base::size(kFakeKeyId)),
      std::string(reinterpret_cast<const char*>(kFakeIv), base::size(kFakeIv)),
      {}));
  return buffer;
}

class DecryptingVideoDecoderTest : public testing::Test {
 public:
  DecryptingVideoDecoderTest()
      : decoder_(new DecryptingVideoDecoder(
            task_environment_.GetMainThreadTaskRunner(),
            &media_log_)),
        cdm_context_(new StrictMock<MockCdmContext>()),
        decryptor_(new StrictMock<MockDecryptor>()),
        num_decrypt_and_decode_calls_(0),
        num_frames_in_decryptor_(0),
        encrypted_buffer_(CreateFakeEncryptedBuffer()),
        decoded_video_frame_(
            VideoFrame::CreateBlackFrame(TestVideoConfig::NormalCodedSize())),
        null_video_frame_(scoped_refptr<VideoFrame>()) {}

  ~DecryptingVideoDecoderTest() override { Destroy(); }

  enum CdmType { CDM_WITHOUT_DECRYPTOR, CDM_WITH_DECRYPTOR };

  void SetCdmType(CdmType cdm_type) {
    const bool has_decryptor = cdm_type == CDM_WITH_DECRYPTOR;
    EXPECT_CALL(*cdm_context_, GetDecryptor())
        .WillRepeatedly(Return(has_decryptor ? decryptor_.get() : nullptr));
  }

  // Initializes the |decoder_| and expects |success|. Note the initialization
  // can succeed or fail.
  void InitializeAndExpectResult(const VideoDecoderConfig& config,
                                 bool success) {
    decoder_->Initialize(config, false, cdm_context_.get(),
                         NewExpectedBoolCB(success),
                         base::Bind(&DecryptingVideoDecoderTest::FrameReady,
                                    base::Unretained(this)),
                         base::Bind(&DecryptingVideoDecoderTest::OnWaiting,
                                    base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  // Initialize the |decoder_| and expects it to succeed.
  void Initialize() {
    SetCdmType(CDM_WITH_DECRYPTOR);
    EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
        .WillOnce(RunCallback<1>(true));
    EXPECT_CALL(*decryptor_, RegisterNewKeyCB(Decryptor::kVideo, _))
        .WillOnce(SaveArg<1>(&key_added_cb_));

    InitializeAndExpectResult(TestVideoConfig::NormalEncrypted(), true);
  }

  // Reinitialize the |decoder_| and expects it to succeed.
  void Reinitialize(const VideoDecoderConfig& new_config) {
    EXPECT_CALL(*decryptor_, DeinitializeDecoder(Decryptor::kVideo));
    EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
        .WillOnce(RunCallback<1>(true));
    EXPECT_CALL(*decryptor_, RegisterNewKeyCB(Decryptor::kVideo, _))
        .WillOnce(SaveArg<1>(&key_added_cb_));

    InitializeAndExpectResult(new_config, true);
  }

  // Decode |buffer| and expect DecodeDone to get called with |status|.
  void DecodeAndExpect(scoped_refptr<DecoderBuffer> buffer,
                       DecodeStatus status) {
    EXPECT_CALL(*this, DecodeDone(status));
    decoder_->Decode(buffer, base::Bind(&DecryptingVideoDecoderTest::DecodeDone,
                                        base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  // Helper function to simulate the decrypting and decoding process in the
  // |decryptor_| with a decoding delay of kDecodingDelay buffers.
  void DecryptAndDecodeVideo(scoped_refptr<DecoderBuffer> encrypted,
                             const Decryptor::VideoDecodeCB& video_decode_cb) {
    num_decrypt_and_decode_calls_++;
    if (!encrypted->end_of_stream())
      num_frames_in_decryptor_++;

    if (num_decrypt_and_decode_calls_ <= kDecodingDelay ||
        num_frames_in_decryptor_ == 0) {
      video_decode_cb.Run(Decryptor::kNeedMoreData,
                          scoped_refptr<VideoFrame>());
      return;
    }

    num_frames_in_decryptor_--;
    video_decode_cb.Run(Decryptor::kSuccess, decoded_video_frame_);
  }

  // Sets up expectations and actions to put DecryptingVideoDecoder in an
  // active normal decoding state.
  void EnterNormalDecodingState() {
    EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
        .WillRepeatedly(
            Invoke(this, &DecryptingVideoDecoderTest::DecryptAndDecodeVideo));
    EXPECT_CALL(*this, FrameReady(decoded_video_frame_));
    for (int i = 0; i < kDecodingDelay + 1; ++i)
      DecodeAndExpect(encrypted_buffer_, DecodeStatus::OK);
  }

  // Sets up expectations and actions to put DecryptingVideoDecoder in an end
  // of stream state. This function must be called after
  // EnterNormalDecodingState() to work.
  void EnterEndOfStreamState() {
    // The codec in the |decryptor_| will be flushed.
    EXPECT_CALL(*this, FrameReady(decoded_video_frame_)).Times(kDecodingDelay);
    DecodeAndExpect(DecoderBuffer::CreateEOSBuffer(), DecodeStatus::OK);
    EXPECT_EQ(0, num_frames_in_decryptor_);
  }

  // Make the video decode callback pending by saving and not firing it.
  void EnterPendingDecodeState() {
    EXPECT_TRUE(!pending_video_decode_cb_);
    EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(encrypted_buffer_, _))
        .WillOnce(SaveArg<1>(&pending_video_decode_cb_));

    decoder_->Decode(encrypted_buffer_,
                     base::Bind(&DecryptingVideoDecoderTest::DecodeDone,
                                base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    // Make sure the Decode() on the decoder triggers a DecryptAndDecode() on
    // the decryptor.
    EXPECT_FALSE(!pending_video_decode_cb_);
  }

  void EnterWaitingForKeyState() {
    EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
        .WillRepeatedly(RunCallback<1>(Decryptor::kNoKey, null_video_frame_));
    EXPECT_CALL(*this, OnWaiting(WaitingReason::kNoDecryptionKey));
    decoder_->Decode(encrypted_buffer_,
                     base::Bind(&DecryptingVideoDecoderTest::DecodeDone,
                                base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void AbortPendingVideoDecodeCB() {
    if (pending_video_decode_cb_) {
      std::move(pending_video_decode_cb_)
          .Run(Decryptor::kSuccess, scoped_refptr<VideoFrame>(nullptr));
    }
  }

  void AbortAllPendingCBs() {
    if (pending_init_cb_) {
      ASSERT_TRUE(!pending_video_decode_cb_);
      std::move(pending_init_cb_).Run(false);
      return;
    }

    AbortPendingVideoDecodeCB();
  }

  void Reset() {
    EXPECT_CALL(*decryptor_, ResetDecoder(Decryptor::kVideo))
        .WillRepeatedly(InvokeWithoutArgs(
            this, &DecryptingVideoDecoderTest::AbortPendingVideoDecodeCB));

    decoder_->Reset(NewExpectedClosure());
    base::RunLoop().RunUntilIdle();
  }

  void Destroy() {
    EXPECT_CALL(*decryptor_, DeinitializeDecoder(Decryptor::kVideo))
        .WillRepeatedly(InvokeWithoutArgs(
            this, &DecryptingVideoDecoderTest::AbortAllPendingCBs));

    decoder_.reset();
    base::RunLoop().RunUntilIdle();
  }

  MOCK_METHOD1(FrameReady, void(scoped_refptr<VideoFrame>));
  MOCK_METHOD1(DecodeDone, void(DecodeStatus));

  MOCK_METHOD1(OnWaiting, void(WaitingReason));

  base::test::SingleThreadTaskEnvironment task_environment_;
  NullMediaLog media_log_;
  std::unique_ptr<DecryptingVideoDecoder> decoder_;
  std::unique_ptr<StrictMock<MockCdmContext>> cdm_context_;
  std::unique_ptr<StrictMock<MockDecryptor>> decryptor_;

  // Variables to help the |decryptor_| to simulate decoding delay and flushing.
  int num_decrypt_and_decode_calls_;
  int num_frames_in_decryptor_;

  Decryptor::DecoderInitCB pending_init_cb_;
  Decryptor::NewKeyCB key_added_cb_;
  Decryptor::VideoDecodeCB pending_video_decode_cb_;

  // Constant buffer/frames.
  scoped_refptr<DecoderBuffer> encrypted_buffer_;
  scoped_refptr<VideoFrame> decoded_video_frame_;
  scoped_refptr<VideoFrame> null_video_frame_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DecryptingVideoDecoderTest);
};

TEST_F(DecryptingVideoDecoderTest, Initialize_Normal) {
  Initialize();
}

TEST_F(DecryptingVideoDecoderTest, Initialize_CdmWithoutDecryptor) {
  SetCdmType(CDM_WITHOUT_DECRYPTOR);
  InitializeAndExpectResult(TestVideoConfig::NormalEncrypted(), false);
}

TEST_F(DecryptingVideoDecoderTest, Initialize_Failure) {
  SetCdmType(CDM_WITH_DECRYPTOR);
  EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
      .WillRepeatedly(RunCallback<1>(false));
  EXPECT_CALL(*decryptor_, RegisterNewKeyCB(Decryptor::kVideo, _))
      .WillRepeatedly(SaveArg<1>(&key_added_cb_));

  InitializeAndExpectResult(TestVideoConfig::NormalEncrypted(), false);
}

TEST_F(DecryptingVideoDecoderTest, Reinitialize_EncryptedToEncrypted) {
  Initialize();
  EnterNormalDecodingState();
  Reinitialize(TestVideoConfig::LargeEncrypted());
}

// Test reinitializing decode with a new clear config.
TEST_F(DecryptingVideoDecoderTest, Reinitialize_EncryptedToClear) {
  Initialize();
  EnterNormalDecodingState();
  Reinitialize(TestVideoConfig::Normal());
}

TEST_F(DecryptingVideoDecoderTest, Reinitialize_Failure) {
  Initialize();
  EnterNormalDecodingState();

  EXPECT_CALL(*decryptor_, DeinitializeDecoder(Decryptor::kVideo));
  EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
      .WillOnce(RunCallback<1>(false));

  // Reinitialize() expects the reinitialization to succeed. Call
  // InitializeAndExpectResult() directly to test the reinitialization failure.
  InitializeAndExpectResult(TestVideoConfig::NormalEncrypted(), false);
}

// Test normal decrypt and decode case.
TEST_F(DecryptingVideoDecoderTest, DecryptAndDecode_Normal) {
  Initialize();
  EnterNormalDecodingState();
}

// Test the case where the decryptor returns error when doing decrypt and
// decode.
TEST_F(DecryptingVideoDecoderTest, DecryptAndDecode_DecodeError) {
  Initialize();

  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
      .WillRepeatedly(RunCallback<1>(Decryptor::kError,
                                     scoped_refptr<VideoFrame>(nullptr)));

  DecodeAndExpect(encrypted_buffer_, DecodeStatus::DECODE_ERROR);

  // After a decode error occurred, all following decodes return DECODE_ERROR.
  DecodeAndExpect(encrypted_buffer_, DecodeStatus::DECODE_ERROR);
}

// Test the case where the decryptor receives end-of-stream buffer.
TEST_F(DecryptingVideoDecoderTest, DecryptAndDecode_EndOfStream) {
  Initialize();
  EnterNormalDecodingState();
  EnterEndOfStreamState();
}

// Test the case where the a key is added when the decryptor is in
// kWaitingForKey state.
TEST_F(DecryptingVideoDecoderTest, KeyAdded_DuringWaitingForKey) {
  Initialize();
  EnterWaitingForKeyState();

  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
      .WillRepeatedly(
          RunCallback<1>(Decryptor::kSuccess, decoded_video_frame_));
  EXPECT_CALL(*this, FrameReady(decoded_video_frame_));
  EXPECT_CALL(*this, DecodeDone(DecodeStatus::OK));
  key_added_cb_.Run();
  base::RunLoop().RunUntilIdle();
}

// Test the case where the a key is added when the decryptor is in
// kPendingDecode state.
TEST_F(DecryptingVideoDecoderTest, KeyAdded_DuringPendingDecode) {
  Initialize();
  EnterPendingDecodeState();

  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
      .WillRepeatedly(
          RunCallback<1>(Decryptor::kSuccess, decoded_video_frame_));
  EXPECT_CALL(*this, FrameReady(decoded_video_frame_));
  EXPECT_CALL(*this, DecodeDone(DecodeStatus::OK));
  // The video decode callback is returned after the correct decryption key is
  // added.
  key_added_cb_.Run();
  std::move(pending_video_decode_cb_).Run(Decryptor::kNoKey, null_video_frame_);
  base::RunLoop().RunUntilIdle();
}

// Test resetting when the decoder is in kIdle state but has not decoded any
// frame.
TEST_F(DecryptingVideoDecoderTest, Reset_DuringIdleAfterInitialization) {
  Initialize();
  Reset();
}

// Test resetting when the decoder is in kIdle state after it has decoded one
// frame.
TEST_F(DecryptingVideoDecoderTest, Reset_DuringIdleAfterDecodedOneFrame) {
  Initialize();
  EnterNormalDecodingState();
  Reset();
}

// Test resetting when the decoder is in kPendingDecode state.
TEST_F(DecryptingVideoDecoderTest, Reset_DuringPendingDecode) {
  Initialize();
  EnterPendingDecodeState();

  EXPECT_CALL(*this, DecodeDone(DecodeStatus::ABORTED));

  Reset();
}

// Test resetting when the decoder is in kWaitingForKey state.
TEST_F(DecryptingVideoDecoderTest, Reset_DuringWaitingForKey) {
  Initialize();
  EnterWaitingForKeyState();

  EXPECT_CALL(*this, DecodeDone(DecodeStatus::ABORTED));

  Reset();
}

// Test resetting when the decoder has hit end of stream and is in
// kDecodeFinished state.
TEST_F(DecryptingVideoDecoderTest, Reset_AfterDecodeFinished) {
  Initialize();
  EnterNormalDecodingState();
  EnterEndOfStreamState();
  Reset();
}

// Test resetting after the decoder has been reset.
TEST_F(DecryptingVideoDecoderTest, Reset_AfterReset) {
  Initialize();
  EnterNormalDecodingState();
  Reset();
  Reset();
}

// Test destruction when the decoder is in kPendingDecoderInit state.
TEST_F(DecryptingVideoDecoderTest, Destroy_DuringPendingDecoderInit) {
  SetCdmType(CDM_WITH_DECRYPTOR);
  EXPECT_CALL(*decryptor_, InitializeVideoDecoder(_, _))
      .WillOnce(SaveArg<1>(&pending_init_cb_));

  InitializeAndExpectResult(TestVideoConfig::NormalEncrypted(), false);
  EXPECT_FALSE(!pending_init_cb_);

  Destroy();
}

// Test destruction when the decoder is in kIdle state but has not decoded any
// frame.
TEST_F(DecryptingVideoDecoderTest, Destroy_DuringIdleAfterInitialization) {
  Initialize();
  Destroy();
}

// Test destruction when the decoder is in kIdle state after it has decoded one
// frame.
TEST_F(DecryptingVideoDecoderTest, Destroy_DuringIdleAfterDecodedOneFrame) {
  Initialize();
  EnterNormalDecodingState();
  Destroy();
}

// Test destruction when the decoder is in kPendingDecode state.
TEST_F(DecryptingVideoDecoderTest, Destroy_DuringPendingDecode) {
  Initialize();
  EnterPendingDecodeState();

  EXPECT_CALL(*this, DecodeDone(DecodeStatus::ABORTED));

  Destroy();
}

// Test destruction when the decoder is in kWaitingForKey state.
TEST_F(DecryptingVideoDecoderTest, Destroy_DuringWaitingForKey) {
  Initialize();
  EnterWaitingForKeyState();

  EXPECT_CALL(*this, DecodeDone(DecodeStatus::ABORTED));

  Destroy();
}

// Test destruction when the decoder has hit end of stream and is in
// kDecodeFinished state.
TEST_F(DecryptingVideoDecoderTest, Destroy_AfterDecodeFinished) {
  Initialize();
  EnterNormalDecodingState();
  EnterEndOfStreamState();
  Destroy();
}

// Test destruction when there is a pending reset on the decoder.
// Reset is pending because it cannot complete when the video decode callback
// is pending.
TEST_F(DecryptingVideoDecoderTest, Destroy_DuringPendingReset) {
  Initialize();
  EnterPendingDecodeState();

  EXPECT_CALL(*decryptor_, ResetDecoder(Decryptor::kVideo));
  EXPECT_CALL(*this, DecodeDone(DecodeStatus::ABORTED));

  decoder_->Reset(NewExpectedClosure());
  Destroy();
}

// Test destruction after the decoder has been reset.
TEST_F(DecryptingVideoDecoderTest, Destroy_AfterReset) {
  Initialize();
  EnterNormalDecodingState();
  Reset();
  Destroy();
}

// Test the case where color space in the config is set in the decoded frame.
TEST_F(DecryptingVideoDecoderTest, ColorSpace) {
  Initialize();
  EXPECT_FALSE(decoded_video_frame_->ColorSpace().IsValid());
  EnterNormalDecodingState();
  EXPECT_TRUE(decoded_video_frame_->ColorSpace().IsValid());
}

}  // namespace media
