// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/waiting.h"
#include "media/mojo/clients/mojo_audio_decoder.h"
#include "media/mojo/mojom/audio_decoder.mojom.h"
#include "media/mojo/services/mojo_audio_decoder_service.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunCallback;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

const SampleFormat kSampleFormat = kSampleFormatPlanarF32;
const int kChannels = 2;
const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
const int kDefaultSampleRate = 44100;
const int kDefaultFrameSize = 100;
const int kOutputPerDecode = 3;

// Tests MojoAudioDecoder (client) and MojoAudioDecoderService (service).
// To better simulate how they are used in production, the client and service
// are running on two different threads.
class MojoAudioDecoderTest : public ::testing::Test {
 public:
  MojoAudioDecoderTest()
      : input_timestamp_helper_(kDefaultSampleRate),
        service_thread_("Service Thread") {
    input_timestamp_helper_.SetBaseTimestamp(base::TimeDelta());

    service_thread_.Start();
    service_task_runner_ = service_thread_.task_runner();

    // Setup the mojo connection.
    mojo::PendingRemote<mojom::AudioDecoder> remote_audio_decoder;
    service_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MojoAudioDecoderTest::ConnectToService,
                       base::Unretained(this),
                       remote_audio_decoder.InitWithNewPipeAndPassReceiver()));
    mojo_audio_decoder_.reset(
        new MojoAudioDecoder(task_environment_.GetMainThreadTaskRunner(),
                             std::move(remote_audio_decoder)));
  }

  ~MojoAudioDecoderTest() override {
    // Destroy |mojo_audio_decoder_| first so that the service will be
    // destructed. Then stop the service thread. Otherwise we'll leak memory.
    mojo_audio_decoder_.reset();
    service_thread_.Stop();

    RunLoopUntilIdle();
  }

  // Completion callbacks.
  MOCK_METHOD1(OnInitialized, void(bool));
  MOCK_METHOD1(OnOutput, void(scoped_refptr<AudioBuffer>));
  MOCK_METHOD1(OnWaiting, void(WaitingReason));
  MOCK_METHOD1(OnDecoded, void(DecodeStatus));
  MOCK_METHOD0(OnReset, void());

  // Always create a new RunLoop (and destroy the old loop if it exists) before
  // running the loop because we cannot run the same loop more than once.

  void RunLoop() {
    DVLOG(1) << __func__;
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  void RunLoopUntilIdle() {
    DVLOG(1) << __func__;
    run_loop_.reset(new base::RunLoop());
    run_loop_->RunUntilIdle();
  }

  void QuitLoop() {
    DVLOG(1) << __func__;
    run_loop_->QuitWhenIdle();
  }

  void ConnectToService(mojo::PendingReceiver<mojom::AudioDecoder> receiver) {
    DCHECK(service_task_runner_->BelongsToCurrentThread());

    std::unique_ptr<StrictMock<MockAudioDecoder>> mock_audio_decoder(
        new StrictMock<MockAudioDecoder>());
    mock_audio_decoder_ = mock_audio_decoder.get();

    EXPECT_CALL(*mock_audio_decoder_, Initialize_(_, _, _, _, _))
        .WillRepeatedly(DoAll(SaveArg<3>(&output_cb_), SaveArg<4>(&waiting_cb_),
                              RunOnceCallback<2>(true)));
    EXPECT_CALL(*mock_audio_decoder_, Decode(_, _))
        .WillRepeatedly(
            DoAll(InvokeWithoutArgs(this, &MojoAudioDecoderTest::ReturnOutput),
                  RunCallback<1>(DecodeStatus::OK)));
    EXPECT_CALL(*mock_audio_decoder_, Reset_(_))
        .WillRepeatedly(RunOnceCallback<0>());

    mojo::MakeSelfOwnedReceiver(
        std::make_unique<MojoAudioDecoderService>(
            &mojo_cdm_service_context_, std::move(mock_audio_decoder)),
        std::move(receiver));
  }

  void SetWriterCapacity(uint32_t capacity) {
    mojo_audio_decoder_->set_writer_capacity_for_testing(capacity);
  }

  void InitializeAndExpect(bool success) {
    DVLOG(1) << __func__ << ": success=" << success;
    EXPECT_CALL(*this, OnInitialized(success))
        .WillOnce(InvokeWithoutArgs(this, &MojoAudioDecoderTest::QuitLoop));

    AudioDecoderConfig audio_config(kCodecVorbis, kSampleFormat, kChannelLayout,
                                    kDefaultSampleRate, EmptyExtraData(),
                                    EncryptionScheme::kUnencrypted);

    mojo_audio_decoder_->Initialize(
        audio_config, nullptr,
        base::Bind(&MojoAudioDecoderTest::OnInitialized,
                   base::Unretained(this)),
        base::Bind(&MojoAudioDecoderTest::OnOutput, base::Unretained(this)),
        base::Bind(&MojoAudioDecoderTest::OnWaiting, base::Unretained(this)));

    RunLoop();
  }

  void Initialize() { InitializeAndExpect(true); }

  void Decode() {
    scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(100));
    mojo_audio_decoder_->Decode(
        buffer,
        base::Bind(&MojoAudioDecoderTest::OnDecoded, base::Unretained(this)));
  }

  void Reset() {
    mojo_audio_decoder_->Reset(
        base::Bind(&MojoAudioDecoderTest::OnReset, base::Unretained(this)));
  }

  void ResetAndWaitUntilFinish() {
    DVLOG(1) << __func__;
    EXPECT_CALL(*this, OnReset())
        .WillOnce(InvokeWithoutArgs(this, &MojoAudioDecoderTest::QuitLoop));
    Reset();
    RunLoop();
  }

  void ReturnOutput() {
    for (int i = 0; i < kOutputPerDecode; ++i) {
      scoped_refptr<AudioBuffer> audio_buffer = MakeAudioBuffer<float>(
          kSampleFormat, kChannelLayout, kChannels, kDefaultSampleRate, 1.0,
          0.0f, 100, input_timestamp_helper_.GetTimestamp());
      input_timestamp_helper_.AddFrames(kDefaultFrameSize);
      output_cb_.Run(audio_buffer);
    }
  }

  void WaitForKey() { waiting_cb_.Run(WaitingReason::kNoDecryptionKey); }

  void DecodeMultipleTimes(int num_of_decodes) {
    num_of_decodes_ = num_of_decodes;
    KeepDecodingOrQuit();
    RunLoop();
  }

  void KeepDecodingOrQuit() {
    if (decode_count_ >= num_of_decodes_) {
      QuitLoop();
      return;
    }

    decode_count_++;

    InSequence s;  // Make sure OnOutput() and OnDecoded() are called in order.
    EXPECT_CALL(*this, OnOutput(_)).Times(kOutputPerDecode);
    EXPECT_CALL(*this, OnDecoded(DecodeStatus::OK))
        .WillOnce(
            InvokeWithoutArgs(this, &MojoAudioDecoderTest::KeepDecodingOrQuit));
    Decode();
  }

  void DecodeAndReset() {
    InSequence s;  // Make sure all callbacks are fired in order.
    EXPECT_CALL(*this, OnOutput(_)).Times(kOutputPerDecode);
    EXPECT_CALL(*this, OnDecoded(DecodeStatus::OK));
    EXPECT_CALL(*this, OnReset())
        .WillOnce(InvokeWithoutArgs(this, &MojoAudioDecoderTest::QuitLoop));
    Decode();
    Reset();
    RunLoop();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;

  // The MojoAudioDecoder that we are testing.
  std::unique_ptr<MojoAudioDecoder> mojo_audio_decoder_;
  MojoCdmServiceContext mojo_cdm_service_context_;
  AudioDecoder::OutputCB output_cb_;
  WaitingCB waiting_cb_;
  AudioTimestampHelper input_timestamp_helper_;

  // The thread where the service runs. This provides test coverage in an
  // environment similar to what we use in production. Also, some race
  // conditions can only be reproduced when we run the service and client on
  // different threads. See http://crbug.com/646054
  base::Thread service_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> service_task_runner_;

  // Owned by the connection on the service thread.
  MojoAudioDecoderService* mojo_audio_decoder_service_ = nullptr;

  // Service side mock.
  StrictMock<MockAudioDecoder>* mock_audio_decoder_ = nullptr;

  int num_of_decodes_ = 0;
  int decode_count_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoAudioDecoderTest);
};

TEST_F(MojoAudioDecoderTest, Initialize_Success) {
  Initialize();
}

TEST_F(MojoAudioDecoderTest, Reinitialize_Success) {
  Initialize();
  DecodeMultipleTimes(10);
  ResetAndWaitUntilFinish();

  // Reinitialize MojoAudioDecoder.
  Initialize();
}

// Makes sure all callbacks and client calls are called in order. See
// http://crbug.com/646054
TEST_F(MojoAudioDecoderTest, Decode_MultipleTimes) {
  Initialize();

  // Choose a large number of decodes per test on purpose to expose potential
  // out of order delivery of mojo messages. See http://crbug.com/646054
  DecodeMultipleTimes(100);
}

TEST_F(MojoAudioDecoderTest, Reset_DuringDecode) {
  Initialize();
  DecodeAndReset();
}

TEST_F(MojoAudioDecoderTest, Reset_DuringDecode_ChunkedWrite) {
  // Use a small writer capacity to force chunked write.
  SetWriterCapacity(10);
  Initialize();
  DecodeAndReset();
}

TEST_F(MojoAudioDecoderTest, WaitingForKey) {
  Initialize();
  EXPECT_CALL(*mock_audio_decoder_, Decode(_, _))
      .WillOnce(
          DoAll(InvokeWithoutArgs(this, &MojoAudioDecoderTest::WaitForKey),
                RunCallback<1>(DecodeStatus::OK)));
  EXPECT_CALL(*this, OnWaiting(WaitingReason::kNoDecryptionKey)).Times(1);
  EXPECT_CALL(*this, OnDecoded(DecodeStatus::OK))
      .WillOnce(InvokeWithoutArgs(this, &MojoAudioDecoderTest::QuitLoop));
  Decode();
  RunLoop();
}

// TODO(xhwang): Add more tests.

}  // namespace media
