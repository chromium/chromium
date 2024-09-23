// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/pipeline_status.h"
#include "media/base/test_helpers.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "media/filters/decrypting_media_resource.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::base::test::RunCallback;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;

namespace media {

static constexpr int kFakeBufferSize = 16;
static constexpr char kFakeKeyId[] = "Key ID";
static constexpr char kFakeIv[] = "0123456789abcdef";

// Use anonymous namespace here to prevent the actions to be defined multiple
// times across multiple test files. Sadly we can't use static for them.
namespace {

ACTION_P(ReturnBuffer, buffer) {
  std::move(arg0).Run(
      buffer.get() ? DemuxerStream::kOk : DemuxerStream::kAborted, {buffer});
}

}  // namespace

class DecryptingMediaResourceTest : public testing::Test {
 public:
  DecryptingMediaResourceTest() {
    encrypted_buffer_ = base::MakeRefCounted<DecoderBuffer>(kFakeBufferSize);
    encrypted_buffer_->set_decrypt_config(
        DecryptConfig::CreateCencConfig(kFakeKeyId, kFakeIv, {}));

    EXPECT_CALL(cdm_context_, RegisterEventCB(_)).Times(AnyNumber());
    EXPECT_CALL(cdm_context_, GetDecryptor())
        .WillRepeatedly(Return(&decryptor_));
    EXPECT_CALL(decryptor_, CanAlwaysDecrypt()).WillRepeatedly(Return(true));
    EXPECT_CALL(decryptor_, CancelDecrypt(_)).Times(AnyNumber());
    EXPECT_CALL(demuxer_, GetAllStreams())
        .WillRepeatedly(
            Invoke(this, &DecryptingMediaResourceTest::GetAllStreams));

    decrypting_media_resource_ = std::make_unique<DecryptingMediaResource>(
        &demuxer_, &cdm_context_, &null_media_log_,
        task_environment_.GetMainThreadTaskRunner());
  }

  ~DecryptingMediaResourceTest() override {
    // Ensure that the DecryptingMediaResource is destructed before other
    // objects that it internally references but does not own.
    decrypting_media_resource_.reset();
  }

  bool HasEncryptedStream() {
    for (media::DemuxerStream* stream :
         decrypting_media_resource_->GetAllStreams()) {
      if ((stream->type() == DemuxerStream::AUDIO &&
           stream->audio_decoder_config().is_encrypted()) ||
          (stream->type() == DemuxerStream::VIDEO &&
           stream->video_decoder_config().is_encrypted()))
        return true;
    }

    return false;
  }

  void AddStream(DemuxerStream::Type type, bool encrypted) {
    streams_.push_back(CreateMockDemuxerStream(type, encrypted));
  }

  std::vector<DemuxerStream*> GetAllStreams() {
    std::vector<DemuxerStream*> streams;

    for (auto& stream : streams_) {
      streams.push_back(stream.get());
    }

    return streams;
  }

  MOCK_METHOD2(BufferReady,
               void(DemuxerStream::Status, DemuxerStream::DecoderBufferVector));

 protected:
  base::test::TaskEnvironment task_environment_;
  base::MockCallback<DecryptingMediaResource::InitCB>
      decrypting_media_resource_init_cb_;
  base::MockCallback<WaitingCB> waiting_cb_;
  NullMediaLog null_media_log_;
  StrictMock<MockDecryptor> decryptor_;
  StrictMock<MockDemuxer> demuxer_;
  StrictMock<MockCdmContext> cdm_context_;
  std::unique_ptr<DecryptingMediaResource> decrypting_media_resource_;
  std::vector<std::unique_ptr<StrictMock<MockDemuxerStream>>> streams_;

  // Constant buffer to be returned by the input demuxer streams and
  // |decryptor_|.
  scoped_refptr<DecoderBuffer> encrypted_buffer_;
};

TEST_F(DecryptingMediaResourceTest, ClearStreams) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ false);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ false);

  EXPECT_CALL(decrypting_media_resource_init_cb_, Run(true));

  decrypting_media_resource_->Initialize(
      decrypting_media_resource_init_cb_.Get(), waiting_cb_.Get());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(
      decrypting_media_resource_->DecryptingDemuxerStreamCountForTesting(), 2);
  EXPECT_FALSE(HasEncryptedStream());
}

TEST_F(DecryptingMediaResourceTest, EncryptedStreams) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ true);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ true);

  EXPECT_CALL(decrypting_media_resource_init_cb_, Run(true));

  decrypting_media_resource_->Initialize(
      decrypting_media_resource_init_cb_.Get(), waiting_cb_.Get());
  task_environment_.RunUntilIdle();

  // When using an AesDecryptor we preemptively wrap our streams with a
  // DecryptingDemuxerStream, regardless of encryption. With this in mind, we
  // should have three DecryptingDemuxerStreams.
  EXPECT_EQ(
      decrypting_media_resource_->DecryptingDemuxerStreamCountForTesting(), 2);

  // All of the streams that we get from our DecryptingMediaResource, NOT the
  // internal MediaResource implementation, should be clear.
  EXPECT_FALSE(HasEncryptedStream());
}

TEST_F(DecryptingMediaResourceTest, MixedStreams) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ false);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ true);

  EXPECT_CALL(decrypting_media_resource_init_cb_, Run(true));

  decrypting_media_resource_->Initialize(
      decrypting_media_resource_init_cb_.Get(), waiting_cb_.Get());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(
      decrypting_media_resource_->DecryptingDemuxerStreamCountForTesting(), 2);
  EXPECT_FALSE(HasEncryptedStream());
}

TEST_F(DecryptingMediaResourceTest,
       OneDecryptingDemuxerStreamFailsInitialization) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ false);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ true);

  // The first DecryptingDemuxerStream will fail to initialize, causing the
  // callback to be run with a value of false. The second
  // DecryptingDemuxerStream will succeed but never invoke the callback.
  EXPECT_CALL(cdm_context_, GetDecryptor())
      .WillOnce(Return(nullptr))
      .WillRepeatedly(Return(&decryptor_));
  EXPECT_CALL(decrypting_media_resource_init_cb_, Run(false));

  decrypting_media_resource_->Initialize(
      decrypting_media_resource_init_cb_.Get(), waiting_cb_.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(DecryptingMediaResourceTest,
       BothDecryptingDemuxerStreamsFailInitialization) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ false);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ true);

  // Both DecryptingDemuxerStreams will fail to initialize but the callback
  // should still only be invoked a single time.
  EXPECT_CALL(cdm_context_, GetDecryptor()).WillRepeatedly(Return(nullptr));
  EXPECT_CALL(decrypting_media_resource_init_cb_, Run(false));

  decrypting_media_resource_->Initialize(
      decrypting_media_resource_init_cb_.Get(), waiting_cb_.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(DecryptingMediaResourceTest, WaitingCallback) {
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ true);

  EXPECT_CALL(*streams_.front(), OnRead(_))
      .WillRepeatedly(ReturnBuffer(encrypted_buffer_));
  EXPECT_CALL(decryptor_, Decrypt(_, encrypted_buffer_, _))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<2>(
          Decryptor::kNoKey, scoped_refptr<DecoderBuffer>()));
  EXPECT_CALL(decrypting_media_resource_init_cb_, Run(true));
  EXPECT_CALL(waiting_cb_, Run(WaitingReason::kNoDecryptionKey));

  decrypting_media_resource_->Initialize(
      decrypting_media_resource_init_cb_.Get(), waiting_cb_.Get());
  decrypting_media_resource_->GetAllStreams().front()->Read(
      1, base::BindOnce(&DecryptingMediaResourceTest::BufferReady,
                        base::Unretained(this)));
  task_environment_.RunUntilIdle();
}

}  // namespace media
