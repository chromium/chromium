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
#include "media/base/audio_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/decrypting_audio_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::base::test::RunCallback;
using ::testing::_;
using ::testing::AtMost;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

const int kSampleRate = 44100;

// Make sure the kFakeAudioFrameSize is a valid frame size for all audio decoder
// configs used in this test.
const int kFakeAudioFrameSize = 48;
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
      std::vector<SubsampleEntry>()));
  return buffer;
}

class DecryptingAudioDecoderTest : public testing::Test {
 public:
  DecryptingAudioDecoderTest()
      : decoder_(new DecryptingAudioDecoder(
            task_environment_.GetMainThreadTaskRunner(),
            &media_log_)),
        cdm_context_(new StrictMock<MockCdmContext>()),
        decryptor_(new StrictMock<MockDecryptor>()),
        num_decrypt_and_decode_calls_(0),
        num_frames_in_decryptor_(0),
        encrypted_buffer_(CreateFakeEncryptedBuffer()),
        decoded_frame_(nullptr),
        decoded_frame_list_() {}

  ~DecryptingAudioDecoderTest() override { Destroy(); }

  void InitializeAndExpectResult(const AudioDecoderConfig& config,
                                 bool success) {
    // Initialize data now that the config is known. Since the code uses
    // invalid values (that CreateEmptyBuffer() doesn't support), tweak them
    // just for CreateEmptyBuffer().
    int channels = ChannelLayoutToChannelCount(config.channel_layout());
    if (channels < 0)
      channels = 0;
    decoded_frame_ = AudioBuffer::CreateEmptyBuffer(
        config.channel_layout(), channels, kSampleRate, kFakeAudioFrameSize,
        kNoTimestamp);
    decoded_frame_list_.push_back(decoded_frame_);

    decoder_->Initialize(config, cdm_context_.get(), NewExpectedBoolCB(success),
                         base::Bind(&DecryptingAudioDecoderTest::FrameReady,
                                    base::Unretained(this)),
                         base::Bind(&DecryptingAudioDecoderTest::OnWaiting,
                                    base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  enum CdmType { CDM_WITHOUT_DECRYPTOR, CDM_WITH_DECRYPTOR };

  void SetCdmType(CdmType cdm_type) {
    const bool has_decryptor = cdm_type == CDM_WITH_DECRYPTOR;
    EXPECT_CALL(*cdm_context_, GetDecryptor())
        .WillRepeatedly(Return(has_decryptor ? decryptor_.get() : nullptr));
  }

  void Initialize() {
    SetCdmType(CDM_WITH_DECRYPTOR);
    EXPECT_CALL(*decryptor_, InitializeAudioDecoder(_, _))
        .Times(AtMost(1))
        .WillOnce(RunCallback<1>(true));
    EXPECT_CALL(*decryptor_, RegisterNewKeyCB(Decryptor::kAudio, _))
        .WillOnce(SaveArg<1>(&key_added_cb_));

    config_.Initialize(kCodecVorbis, kSampleFormatPlanarF32,
                       CHANNEL_LAYOUT_STEREO, kSampleRate, EmptyExtraData(),
                       EncryptionScheme::kCenc, base::TimeDelta(), 0);
    InitializeAndExpectResult(config_, true);
  }

  void Reinitialize() { ReinitializeConfigChange(config_); }

  void ReinitializeConfigChange(const AudioDecoderConfig& new_config) {
    EXPECT_CALL(*decryptor_, DeinitializeDecoder(Decryptor::kAudio));
    EXPECT_CALL(*decryptor_, InitializeAudioDecoder(_, _))
        .WillOnce(RunCallback<1>(true));
    EXPECT_CALL(*decryptor_, RegisterNewKeyCB(Decryptor::kAudio, _))
        .WillOnce(SaveArg<1>(&key_added_cb_));
    decoder_->Initialize(new_config, cdm_context_.get(),
                         NewExpectedBoolCB(true),
                         base::Bind(&DecryptingAudioDecoderTest::FrameReady,
                                    base::Unretained(this)),
                         base::Bind(&DecryptingAudioDecoderTest::OnWaiting,
                                    base::Unretained(this)));
  }

  // Decode |buffer| and expect DecodeDone to get called with |status|.
  void DecodeAndExpect(scoped_refptr<DecoderBuffer> buffer,
                       DecodeStatus status) {
    EXPECT_CALL(*this, DecodeDone(status));
    decoder_->Decode(buffer, base::Bind(&DecryptingAudioDecoderTest::DecodeDone,
                                        base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  // Helper function to simulate the decrypting and decoding process in the
  // |decryptor_| with a decoding delay of kDecodingDelay buffers.
  void DecryptAndDecodeAudio(scoped_refptr<DecoderBuffer> encrypted,
                             const Decryptor::AudioDecodeCB& audio_decode_cb) {
    num_decrypt_and_decode_calls_++;
    if (!encrypted->end_of_stream())
      num_frames_in_decryptor_++;

    if (num_decrypt_and_decode_calls_ <= kDecodingDelay ||
        num_frames_in_decryptor_ == 0) {
      audio_decode_cb.Run(Decryptor::kNeedMoreData, Decryptor::AudioFrames());
      return;
    }

    num_frames_in_decryptor_--;
    audio_decode_cb.Run(Decryptor::kSuccess,
                        Decryptor::AudioFrames(1, decoded_frame_));
  }

  // Sets up expectations and actions to put DecryptingAudioDecoder in an
  // active normal decoding state.
  void EnterNormalDecodingState() {
    EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
        .WillRepeatedly(
            Invoke(this, &DecryptingAudioDecoderTest::DecryptAndDecodeAudio));
    EXPECT_CALL(*this, FrameReady(decoded_frame_));
    for (int i = 0; i < kDecodingDelay + 1; ++i)
      DecodeAndExpect(encrypted_buffer_, DecodeStatus::OK);
  }

  // Sets up expectations and actions to put DecryptingAudioDecoder in an end
  // of stream state. This function must be called after
  // EnterNormalDecodingState() to work.
  void EnterEndOfStreamState() {
    // The codec in the |decryptor_| will be flushed.
    EXPECT_CALL(*this, FrameReady(decoded_frame_)).Times(kDecodingDelay);
    DecodeAndExpect(DecoderBuffer::CreateEOSBuffer(), DecodeStatus::OK);
    EXPECT_EQ(0, num_frames_in_decryptor_);
  }

  // Make the audio decode callback pending by saving and not firing it.
  void EnterPendingDecodeState() {
    EXPECT_TRUE(!pending_audio_decode_cb_);
    EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(encrypted_buffer_, _))
        .WillOnce(SaveArg<1>(&pending_audio_decode_cb_));

    decoder_->Decode(encrypted_buffer_,
                     base::Bind(&DecryptingAudioDecoderTest::DecodeDone,
                                base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    // Make sure the Decode() on the decoder triggers a DecryptAndDecode() on
    // the decryptor.
    EXPECT_FALSE(!pending_audio_decode_cb_);
  }

  void EnterWaitingForKeyState() {
    EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(encrypted_buffer_, _))
        .WillRepeatedly(
            RunCallback<1>(Decryptor::kNoKey, Decryptor::AudioFrames()));
    EXPECT_CALL(*this, OnWaiting(WaitingReason::kNoDecryptionKey));
    decoder_->Decode(encrypted_buffer_,
                     base::Bind(&DecryptingAudioDecoderTest::DecodeDone,
                                base::Unretained(this)));

    base::RunLoop().RunUntilIdle();
  }

  void AbortPendingAudioDecodeCB() {
    if (pending_audio_decode_cb_) {
      std::move(pending_audio_decode_cb_)
          .Run(Decryptor::kSuccess, Decryptor::AudioFrames());
    }
  }

  void AbortAllPendingCBs() {
    if (pending_init_cb_) {
      ASSERT_TRUE(!pending_audio_decode_cb_);
      std::move(pending_init_cb_).Run(false);
      return;
    }

    AbortPendingAudioDecodeCB();
  }

  void Reset() {
    EXPECT_CALL(*decryptor_, ResetDecoder(Decryptor::kAudio))
        .WillRepeatedly(InvokeWithoutArgs(
            this, &DecryptingAudioDecoderTest::AbortPendingAudioDecodeCB));

    decoder_->Reset(NewExpectedClosure());
    base::RunLoop().RunUntilIdle();
  }

  void Destroy() {
    EXPECT_CALL(*decryptor_, DeinitializeDecoder(Decryptor::kAudio))
        .WillRepeatedly(InvokeWithoutArgs(
            this, &DecryptingAudioDecoderTest::AbortAllPendingCBs));

    decoder_.reset();
    base::RunLoop().RunUntilIdle();
  }

  MOCK_METHOD1(FrameReady, void(scoped_refptr<AudioBuffer>));
  MOCK_METHOD1(DecodeDone, void(DecodeStatus));

  MOCK_METHOD1(OnWaiting, void(WaitingReason));

  base::test::SingleThreadTaskEnvironment task_environment_;
  NullMediaLog media_log_;
  std::unique_ptr<DecryptingAudioDecoder> decoder_;
  std::unique_ptr<StrictMock<MockCdmContext>> cdm_context_;
  std::unique_ptr<StrictMock<MockDecryptor>> decryptor_;
  AudioDecoderConfig config_;

  // Variables to help the |decryptor_| to simulate decoding delay and flushing.
  int num_decrypt_and_decode_calls_;
  int num_frames_in_decryptor_;

  Decryptor::DecoderInitCB pending_init_cb_;
  Decryptor::NewKeyCB key_added_cb_;
  Decryptor::AudioDecodeCB pending_audio_decode_cb_;

  // Constant buffer/frames, to be used/returned by |decoder_| and |decryptor_|.
  scoped_refptr<DecoderBuffer> encrypted_buffer_;
  scoped_refptr<AudioBuffer> decoded_frame_;
  Decryptor::AudioFrames decoded_frame_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DecryptingAudioDecoderTest);
};

TEST_F(DecryptingAudioDecoderTest, Initialize_Normal) {
  Initialize();
}

// Ensure decoder handles invalid audio configs without crashing.
TEST_F(DecryptingAudioDecoderTest, Initialize_InvalidAudioConfig) {
  AudioDecoderConfig config(kUnknownAudioCodec, kUnknownSampleFormat,
                            CHANNEL_LAYOUT_STEREO, 0, EmptyExtraData(),
                            EncryptionScheme::kCenc);

  InitializeAndExpectResult(config, false);
}

// Ensure decoder handles unsupported audio configs without crashing.
TEST_F(DecryptingAudioDecoderTest, Initialize_UnsupportedAudioConfig) {
  SetCdmType(CDM_WITH_DECRYPTOR);
  EXPECT_CALL(*decryptor_, InitializeAudioDecoder(_, _))
      .WillOnce(RunCallback<1>(false));

  AudioDecoderConfig config(kCodecVorbis, kSampleFormatPlanarF32,
                            CHANNEL_LAYOUT_STEREO, kSampleRate,
                            EmptyExtraData(), EncryptionScheme::kCenc);
  InitializeAndExpectResult(config, false);
}

TEST_F(DecryptingAudioDecoderTest, Initialize_CdmWithoutDecryptor) {
  SetCdmType(CDM_WITHOUT_DECRYPTOR);
  AudioDecoderConfig config(kCodecVorbis, kSampleFormatPlanarF32,
                            CHANNEL_LAYOUT_STEREO, kSampleRate,
                            EmptyExtraData(), EncryptionScheme::kCenc);
  InitializeAndExpectResult(config, false);
}

// Test normal decrypt and decode case.
TEST_F(DecryptingAudioDecoderTest, DecryptAndDecode_Normal) {
  Initialize();
  EnterNormalDecodingState();
}

// Test the case where the decryptor returns error when doing decrypt and
// decode.
TEST_F(DecryptingAudioDecoderTest, DecryptAndDecode_DecodeError) {
  Initialize();

  EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
      .WillRepeatedly(
          RunCallback<1>(Decryptor::kError, Decryptor::AudioFrames()));

  DecodeAndExpect(encrypted_buffer_, DecodeStatus::DECODE_ERROR);
}

// Test the case where the decryptor returns multiple decoded frames.
TEST_F(DecryptingAudioDecoderTest, DecryptAndDecode_MultipleFrames) {
  Initialize();

  scoped_refptr<AudioBuffer> frame_a = AudioBuffer::CreateEmptyBuffer(
      config_.channel_layout(),
      ChannelLayoutToChannelCount(config_.channel_layout()), kSampleRate,
      kFakeAudioFrameSize, kNoTimestamp);
  scoped_refptr<AudioBuffer> frame_b = AudioBuffer::CreateEmptyBuffer(
      config_.channel_layout(),
      ChannelLayoutToChannelCount(config_.channel_layout()), kSampleRate,
      kFakeAudioFrameSize, kNoTimestamp);
  decoded_frame_list_.push_back(frame_a);
  decoded_frame_list_.push_back(frame_b);

  EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
      .WillOnce(RunCallback<1>(Decryptor::kSuccess, decoded_frame_list_));

  EXPECT_CALL(*this, FrameReady(decoded_frame_));
  EXPECT_CALL(*this, FrameReady(frame_a));
  EXPECT_CALL(*this, FrameReady(frame_b));
  DecodeAndExpect(encrypted_buffer_, DecodeStatus::OK);
}

// Test the case where the decryptor receives end-of-stream buffer.
TEST_F(DecryptingAudioDecoderTest, DecryptAndDecode_EndOfStream) {
  Initialize();
  EnterNormalDecodingState();
  EnterEndOfStreamState();
}

// Test reinitializing decode with a new encrypted config.
TEST_F(DecryptingAudioDecoderTest, Reinitialize_EncryptedToEncrypted) {
  Initialize();

  EXPECT_CALL(*decryptor_, InitializeAudioDecoder(_, _))
      .Times(AtMost(1))
      .WillOnce(RunCallback<1>(true));

  // The new config is different from the initial config in bits-per-channel,
  // channel layout and samples_per_second.
  AudioDecoderConfig new_config(kCodecVorbis, kSampleFormatPlanarS16,
                                CHANNEL_LAYOUT_5_1, 88200, EmptyExtraData(),
                                EncryptionScheme::kCenc);
  EXPECT_NE(new_config.bits_per_channel(), config_.bits_per_channel());
  EXPECT_NE(new_config.channel_layout(), config_.channel_layout());
  EXPECT_NE(new_config.samples_per_second(), config_.samples_per_second());
  ASSERT_TRUE(new_config.is_encrypted());

  ReinitializeConfigChange(new_config);
  base::RunLoop().RunUntilIdle();
}

// Test reinitializing decode with a new clear config.
TEST_F(DecryptingAudioDecoderTest, Reinitialize_EncryptedToClear) {
  Initialize();

  EXPECT_CALL(*decryptor_, InitializeAudioDecoder(_, _))
      .Times(AtMost(1))
      .WillOnce(RunCallback<1>(true));

  // The new config is different from the initial config in bits-per-channel,
  // channel layout and samples_per_second.
  AudioDecoderConfig new_config(kCodecVorbis, kSampleFormatPlanarS16,
                                CHANNEL_LAYOUT_5_1, 88200, EmptyExtraData(),
                                EncryptionScheme::kUnencrypted);
  EXPECT_NE(new_config.bits_per_channel(), config_.bits_per_channel());
  EXPECT_NE(new_config.channel_layout(), config_.channel_layout());
  EXPECT_NE(new_config.samples_per_second(), config_.samples_per_second());
  ASSERT_FALSE(new_config.is_encrypted());

  ReinitializeConfigChange(new_config);
  base::RunLoop().RunUntilIdle();
}

// Test the case where the a key is added when the decryptor is in
// kWaitingForKey state.
TEST_F(DecryptingAudioDecoderTest, KeyAdded_DuringWaitingForKey) {
  Initialize();
  EnterWaitingForKeyState();

  EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
      .WillRepeatedly(RunCallback<1>(Decryptor::kSuccess, decoded_frame_list_));
  EXPECT_CALL(*this, FrameReady(decoded_frame_));
  EXPECT_CALL(*this, DecodeDone(DecodeStatus::OK));
  key_added_cb_.Run();
  base::RunLoop().RunUntilIdle();
}

// Test the case where the a key is added when the decryptor is in
// kPendingDecode state.
TEST_F(DecryptingAudioDecoderTest, KeyAdded_DruingPendingDecode) {
  Initialize();
  EnterPendingDecodeState();

  EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
      .WillRepeatedly(RunCallback<1>(Decryptor::kSuccess, decoded_frame_list_));
  EXPECT_CALL(*this, FrameReady(decoded_frame_));
  EXPECT_CALL(*this, DecodeDone(DecodeStatus::OK));
  // The audio decode callback is returned after the correct decryption key is
  // added.
  key_added_cb_.Run();
  std::move(pending_audio_decode_cb_)
      .Run(Decryptor::kNoKey, Decryptor::AudioFrames());
  base::RunLoop().RunUntilIdle();
}

// Test resetting when the decoder is in kIdle state but has not decoded any
// frame.
TEST_F(DecryptingAudioDecoderTest, Reset_DuringIdleAfterInitialization) {
  Initialize();
  Reset();
}

// Test resetting when the decoder is in kIdle state after it has decoded one
// frame.
TEST_F(DecryptingAudioDecoderTest, Reset_DuringIdleAfterDecodedOneFrame) {
  Initialize();
  EnterNormalDecodingState();
  Reset();
}

// Test resetting when the decoder is in kPendingDecode state.
TEST_F(DecryptingAudioDecoderTest, Reset_DuringPendingDecode) {
  Initialize();
  EnterPendingDecodeState();

  EXPECT_CALL(*this, DecodeDone(DecodeStatus::ABORTED));

  Reset();
}

// Test resetting when the decoder is in kWaitingForKey state.
TEST_F(DecryptingAudioDecoderTest, Reset_DuringWaitingForKey) {
  Initialize();
  EnterWaitingForKeyState();

  EXPECT_CALL(*this, DecodeDone(DecodeStatus::ABORTED));

  Reset();
}

// Test resetting when the decoder has hit end of stream and is in
// kDecodeFinished state.
TEST_F(DecryptingAudioDecoderTest, Reset_AfterDecodeFinished) {
  Initialize();
  EnterNormalDecodingState();
  EnterEndOfStreamState();
  Reset();
}

// Test resetting after the decoder has been reset.
TEST_F(DecryptingAudioDecoderTest, Reset_AfterReset) {
  Initialize();
  EnterNormalDecodingState();
  Reset();
  Reset();
}

}  // namespace media
