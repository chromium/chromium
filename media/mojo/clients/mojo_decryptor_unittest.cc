// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/test_message_loop.h"
#include "media/base/decryptor.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/mojo/clients/mojo_decryptor.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"
#include "media/mojo/mojom/decryptor.mojom.h"
#include "media/mojo/services/mojo_decryptor_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::InSequence;
using testing::IsNull;
using testing::NotNull;
using testing::SaveArg;
using testing::StrictMock;
using testing::_;

namespace media {

class MojoDecryptorTest : public ::testing::Test {
 public:
  MojoDecryptorTest() = default;
  ~MojoDecryptorTest() override = default;

  void SetWriterCapacity(uint32_t capacity) { writer_capacity_ = capacity; }

  void Initialize() {
    decryptor_.reset(new StrictMock<MockDecryptor>());

    mojo_decryptor_service_.reset(
        new MojoDecryptorService(decryptor_.get(), nullptr));

    receiver_ = std::make_unique<mojo::Receiver<mojom::Decryptor>>(
        mojo_decryptor_service_.get());
    mojo_decryptor_ = std::make_unique<MojoDecryptor>(
        receiver_->BindNewPipeAndPassRemote(), writer_capacity_);
    receiver_->set_disconnect_handler(base::BindOnce(
        &MojoDecryptorTest::OnConnectionClosed, base::Unretained(this)));
  }

  void DestroyClient() {
    EXPECT_CALL(*this, OnConnectionClosed());
    mojo_decryptor_.reset();
  }

  void DestroyService() {
    // MojoDecryptor has no way to notify callers that the connection is closed.
    // TODO(jrummell): Determine if notification is needed.
    receiver_.reset();
    mojo_decryptor_service_.reset();
  }

  void ReturnSharedBufferVideoFrame(
      scoped_refptr<DecoderBuffer> encrypted,
      const Decryptor::VideoDecodeCB& video_decode_cb) {
    // We don't care about the encrypted data, just create a simple VideoFrame.
    scoped_refptr<VideoFrame> frame(
        MojoSharedBufferVideoFrame::CreateDefaultI420ForTesting(
            gfx::Size(100, 100), base::TimeDelta::FromSeconds(100)));
    frame->AddDestructionObserver(base::Bind(
        &MojoDecryptorTest::OnFrameDestroyed, base::Unretained(this)));

    // Currently freeing buffers only works for MojoSharedMemory, so make
    // sure |frame| is of that type.
    EXPECT_EQ(VideoFrame::STORAGE_MOJO_SHARED_BUFFER, frame->storage_type());
    video_decode_cb.Run(Decryptor::kSuccess, std::move(frame));
  }

  void ReturnAudioFrames(scoped_refptr<DecoderBuffer> encrypted,
                         const Decryptor::AudioDecodeCB& audio_decode_cb) {
    const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_4_0;
    const int kSampleRate = 48000;
    const base::TimeDelta start_time = base::TimeDelta::FromSecondsD(1000.0);
    auto audio_buffer = MakeAudioBuffer<float>(
        kSampleFormatPlanarF32, kChannelLayout,
        ChannelLayoutToChannelCount(kChannelLayout), kSampleRate, 0.0f, 1.0f,
        kSampleRate / 10, start_time);
    Decryptor::AudioFrames audio_frames = {audio_buffer};
    audio_decode_cb.Run(Decryptor::kSuccess, audio_frames);
  }

  void ReturnEOSVideoFrame(scoped_refptr<DecoderBuffer> encrypted,
                           const Decryptor::VideoDecodeCB& video_decode_cb) {
    // Simply create and return an End-Of-Stream VideoFrame.
    video_decode_cb.Run(Decryptor::kSuccess, VideoFrame::CreateEOSFrame());
  }

  MOCK_METHOD2(AudioDecoded,
               void(Decryptor::Status status,
                    const Decryptor::AudioFrames& frames));
  MOCK_METHOD2(VideoDecoded,
               void(Decryptor::Status status, scoped_refptr<VideoFrame> frame));
  MOCK_METHOD0(OnConnectionClosed, void());
  MOCK_METHOD0(OnFrameDestroyed, void());

 protected:
  // Fixture members.
  base::TestMessageLoop message_loop_;

  uint32_t writer_capacity_ = 0;

  // The MojoDecryptor that we are testing.
  std::unique_ptr<MojoDecryptor> mojo_decryptor_;

  // The matching MojoDecryptorService for |mojo_decryptor_|.
  std::unique_ptr<MojoDecryptorService> mojo_decryptor_service_;
  std::unique_ptr<mojo::Receiver<mojom::Decryptor>> receiver_;

  // The actual Decryptor object used by |mojo_decryptor_service_|.
  std::unique_ptr<StrictMock<MockDecryptor>> decryptor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoDecryptorTest);
};

// DecryptAndDecodeAudio() and ResetDecoder(kAudio) immediately.
TEST_F(MojoDecryptorTest, Reset_DuringDecryptAndDecode_Audio) {
  Initialize();

  {
    // Make sure calls are made in order.
    InSequence seq;
    EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
        .WillOnce(Invoke(this, &MojoDecryptorTest::ReturnAudioFrames));
    EXPECT_CALL(*decryptor_, ResetDecoder(Decryptor::kAudio));
    // The returned status could be success or aborted.
    EXPECT_CALL(*this, AudioDecoded(_, _));
  }

  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(100));
  mojo_decryptor_->DecryptAndDecodeAudio(
      std::move(buffer),
      base::Bind(&MojoDecryptorTest::AudioDecoded, base::Unretained(this)));
  mojo_decryptor_->ResetDecoder(Decryptor::kAudio);
  base::RunLoop().RunUntilIdle();
}

// DecryptAndDecodeAudio() and ResetDecoder(kAudio) immediately.
TEST_F(MojoDecryptorTest, Reset_DuringDecryptAndDecode_Audio_ChunkedWrite) {
  SetWriterCapacity(20);
  Initialize();

  {
    // Make sure calls are made in order.
    InSequence seq;
    EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
        .WillOnce(Invoke(this, &MojoDecryptorTest::ReturnAudioFrames));
    EXPECT_CALL(*decryptor_, ResetDecoder(Decryptor::kAudio));
    // The returned status could be success or aborted.
    EXPECT_CALL(*this, AudioDecoded(_, _));
  }

  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(100));
  mojo_decryptor_->DecryptAndDecodeAudio(
      std::move(buffer),
      base::Bind(&MojoDecryptorTest::AudioDecoded, base::Unretained(this)));
  mojo_decryptor_->ResetDecoder(Decryptor::kAudio);
  base::RunLoop().RunUntilIdle();
}

// DecryptAndDecodeVideo() and ResetDecoder(kVideo) immediately.
TEST_F(MojoDecryptorTest, Reset_DuringDecryptAndDecode_Video) {
  Initialize();

  {
    // Make sure calls are made in order.
    InSequence seq;
    EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
        .WillOnce(
            Invoke(this, &MojoDecryptorTest::ReturnSharedBufferVideoFrame));
    EXPECT_CALL(*decryptor_, ResetDecoder(Decryptor::kVideo));
    // The returned status could be success or aborted.
    EXPECT_CALL(*this, VideoDecoded(_, _));
    EXPECT_CALL(*this, OnFrameDestroyed());
  }

  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(100));
  mojo_decryptor_->DecryptAndDecodeVideo(
      std::move(buffer),
      base::Bind(&MojoDecryptorTest::VideoDecoded, base::Unretained(this)));
  mojo_decryptor_->ResetDecoder(Decryptor::kVideo);
  base::RunLoop().RunUntilIdle();
}

// DecryptAndDecodeVideo() and ResetDecoder(kVideo) immediately.
TEST_F(MojoDecryptorTest, Reset_DuringDecryptAndDecode_Video_ChunkedWrite) {
  SetWriterCapacity(20);
  Initialize();

  {
    // Make sure calls are made in order.
    InSequence seq;
    EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
        .WillOnce(
            Invoke(this, &MojoDecryptorTest::ReturnSharedBufferVideoFrame));
    EXPECT_CALL(*decryptor_, ResetDecoder(Decryptor::kVideo));
    // The returned status could be success or aborted.
    EXPECT_CALL(*this, VideoDecoded(_, _));
    EXPECT_CALL(*this, OnFrameDestroyed());
  }

  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(100));
  mojo_decryptor_->DecryptAndDecodeVideo(
      std::move(buffer),
      base::Bind(&MojoDecryptorTest::VideoDecoded, base::Unretained(this)));
  mojo_decryptor_->ResetDecoder(Decryptor::kVideo);
  base::RunLoop().RunUntilIdle();
}

// DecryptAndDecodeAudio(), DecryptAndDecodeVideo(), ResetDecoder(kAudio) and
// ResetDecoder(kVideo).
TEST_F(MojoDecryptorTest, Reset_DuringDecryptAndDecode_AudioAndVideo) {
  // Only test chunked write as it's the most complex and error prone case.
  SetWriterCapacity(20);
  Initialize();

  {
    // Make sure calls are made in order.
    InSequence seq;
    EXPECT_CALL(*decryptor_, DecryptAndDecodeAudio(_, _))
        .WillOnce(Invoke(this, &MojoDecryptorTest::ReturnAudioFrames));
    EXPECT_CALL(*decryptor_, ResetDecoder(Decryptor::kAudio));
  }

  {
    // Make sure calls are made in order.
    InSequence seq;
    EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
        .WillOnce(
            Invoke(this, &MojoDecryptorTest::ReturnSharedBufferVideoFrame));
    EXPECT_CALL(*decryptor_, ResetDecoder(Decryptor::kVideo));
  }

  // The returned status could be success or aborted, and we don't care about
  // the order.
  EXPECT_CALL(*this, AudioDecoded(_, _));
  EXPECT_CALL(*this, VideoDecoded(_, _));
  EXPECT_CALL(*this, OnFrameDestroyed());

  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(100));

  mojo_decryptor_->DecryptAndDecodeAudio(
      buffer,
      base::Bind(&MojoDecryptorTest::AudioDecoded, base::Unretained(this)));
  mojo_decryptor_->DecryptAndDecodeVideo(
      std::move(buffer),
      base::Bind(&MojoDecryptorTest::VideoDecoded, base::Unretained(this)));
  mojo_decryptor_->ResetDecoder(Decryptor::kAudio);
  mojo_decryptor_->ResetDecoder(Decryptor::kVideo);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoDecryptorTest, VideoDecodeFreesBuffer) {
  Initialize();

  // Call DecryptAndDecodeVideo(). Once the callback VideoDecoded() completes,
  // the frame will be destroyed, and the buffer will be released.
  {
    InSequence seq;
    EXPECT_CALL(*this, VideoDecoded(Decryptor::Status::kSuccess, NotNull()));
    EXPECT_CALL(*this, OnFrameDestroyed());
  }
  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
      .WillOnce(Invoke(this, &MojoDecryptorTest::ReturnSharedBufferVideoFrame));

  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(100));
  mojo_decryptor_->DecryptAndDecodeVideo(
      std::move(buffer),
      base::Bind(&MojoDecryptorTest::VideoDecoded, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoDecryptorTest, VideoDecodeFreesMultipleBuffers) {
  Initialize();

  // Call DecryptAndDecodeVideo() multiple times.
  const int TIMES = 5;
  EXPECT_CALL(*this, VideoDecoded(Decryptor::Status::kSuccess, NotNull()))
      .Times(TIMES);
  EXPECT_CALL(*this, OnFrameDestroyed()).Times(TIMES);
  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
      .WillRepeatedly(
          Invoke(this, &MojoDecryptorTest::ReturnSharedBufferVideoFrame));

  for (int i = 0; i < TIMES; ++i) {
    scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(100));
    mojo_decryptor_->DecryptAndDecodeVideo(
        std::move(buffer),
        base::Bind(&MojoDecryptorTest::VideoDecoded, base::Unretained(this)));
  }
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoDecryptorTest, VideoDecodeHoldThenFreeBuffers) {
  Initialize();

  // Call DecryptAndDecodeVideo() twice. Hang on to the buffers returned,
  // and free them later.
  scoped_refptr<VideoFrame> saved_frame1;
  scoped_refptr<VideoFrame> saved_frame2;

  EXPECT_CALL(*this, VideoDecoded(Decryptor::Status::kSuccess, NotNull()))
      .WillOnce(SaveArg<1>(&saved_frame1))
      .WillOnce(SaveArg<1>(&saved_frame2));
  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
      .WillRepeatedly(
          Invoke(this, &MojoDecryptorTest::ReturnSharedBufferVideoFrame));

  for (int i = 0; i < 2; ++i) {
    scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(100));
    mojo_decryptor_->DecryptAndDecodeVideo(
        std::move(buffer),
        base::Bind(&MojoDecryptorTest::VideoDecoded, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  // Free the first frame.
  EXPECT_CALL(*this, OnFrameDestroyed());
  saved_frame1 = nullptr;
  base::RunLoop().RunUntilIdle();

  // Repeat for the second frame.
  EXPECT_CALL(*this, OnFrameDestroyed());
  saved_frame2 = nullptr;
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoDecryptorTest, EOSBuffer) {
  Initialize();

  // Call DecryptAndDecodeVideo(), but return an EOS frame (which has no frame
  // data, so no memory needs to be freed).
  EXPECT_CALL(*this, VideoDecoded(Decryptor::Status::kSuccess, NotNull()));
  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _))
      .WillOnce(Invoke(this, &MojoDecryptorTest::ReturnEOSVideoFrame));

  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(100));
  mojo_decryptor_->DecryptAndDecodeVideo(
      std::move(buffer),
      base::Bind(&MojoDecryptorTest::VideoDecoded, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoDecryptorTest, DestroyClient) {
  Initialize();
  DestroyClient();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MojoDecryptorTest, DestroyService) {
  Initialize();
  DestroyService();
  base::RunLoop().RunUntilIdle();

  // Attempts to use the client should fail.
  EXPECT_CALL(*this, VideoDecoded(Decryptor::Status::kError, IsNull()));
  EXPECT_CALL(*decryptor_, DecryptAndDecodeVideo(_, _)).Times(0);

  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(100));
  mojo_decryptor_->DecryptAndDecodeVideo(
      std::move(buffer),
      base::Bind(&MojoDecryptorTest::VideoDecoded, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

}  // namespace media
