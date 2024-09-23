// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/decrypting_renderer.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/filters/decrypting_media_resource.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::base::test::RunCallback;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnPointee;
using ::testing::StrictMock;

namespace media {

class CdmContext;
class DemuxerStream;
class MediaLog;

class DecryptingRendererTest : public testing::Test {
 public:
  DecryptingRendererTest() {
    auto renderer = std::make_unique<StrictMock<MockRenderer>>();
    renderer_ = renderer.get();
    decrypting_renderer_ = std::make_unique<DecryptingRenderer>(
        std::move(renderer), &null_media_log_,
        task_environment_.GetMainThreadTaskRunner());

    EXPECT_CALL(cdm_context_, RegisterEventCB(_)).Times(AnyNumber());
    EXPECT_CALL(cdm_context_, GetDecryptor())
        .WillRepeatedly(Return(&decryptor_));
    EXPECT_CALL(decryptor_, CanAlwaysDecrypt())
        .WillRepeatedly(ReturnPointee(&use_aes_decryptor_));
    EXPECT_CALL(decryptor_, CancelDecrypt(_)).Times(AnyNumber());
    EXPECT_CALL(media_resource_, GetAllStreams())
        .WillRepeatedly(Invoke(this, &DecryptingRendererTest::GetAllStreams));
    EXPECT_CALL(media_resource_, GetType())
        .WillRepeatedly(Return(MediaResource::Type::kStream));
  }

  ~DecryptingRendererTest() override {
    // Ensure that the DecryptingRenderer is destructed before other objects
    // that it internally references but does not own.
    decrypting_renderer_.reset();
  }

  void AddStream(DemuxerStream::Type type, bool encrypted) {
    streams_.push_back(CreateMockDemuxerStream(type, encrypted));
  }

  void UseAesDecryptor(bool use_aes_decryptor) {
    use_aes_decryptor_ = use_aes_decryptor;
  }

  std::vector<DemuxerStream*> GetAllStreams() {
    std::vector<DemuxerStream*> streams;

    for (auto& stream : streams_) {
      streams.push_back(stream.get());
    }

    return streams;
  }

 protected:
  // Invoking InitializeRenderer(false) will cause the initialization of the
  // DecryptingRenderer to halt and an error will be propagated to the media
  // pipeline.
  void InitializeDecryptingRendererWithFalse() {
    decrypting_renderer_->InitializeRenderer(false);
  }

  bool use_aes_decryptor_ = false;
  base::test::TaskEnvironment task_environment_;
  base::MockCallback<Renderer::CdmAttachedCB> set_cdm_cb_;
  base::MockOnceCallback<void(PipelineStatus)> renderer_init_cb_;
  NullMediaLog null_media_log_;
  StrictMock<MockCdmContext> cdm_context_;
  StrictMock<MockDecryptor> decryptor_;
  StrictMock<MockMediaResource> media_resource_;
  StrictMock<MockRendererClient> renderer_client_;
  raw_ptr<StrictMock<MockRenderer>, DanglingUntriaged> renderer_;
  std::unique_ptr<DecryptingRenderer> decrypting_renderer_;
  std::vector<std::unique_ptr<StrictMock<MockDemuxerStream>>> streams_;
};

TEST_F(DecryptingRendererTest, ClearStreams_NoCdm) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ false);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ false);

  EXPECT_CALL(*renderer_, OnInitialize(_, _, _))
      .WillOnce(RunOnceCallback<2>(PIPELINE_OK));
  EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_OK)));

  decrypting_renderer_->Initialize(&media_resource_, &renderer_client_,
                                   renderer_init_cb_.Get());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(decrypting_renderer_->HasDecryptingMediaResourceForTesting());
}

TEST_F(DecryptingRendererTest, ClearStreams_AesDecryptor) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ false);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ false);
  UseAesDecryptor(true);

  EXPECT_CALL(*renderer_, OnInitialize(_, _, _))
      .WillOnce(RunOnceCallback<2>(PIPELINE_OK));
  EXPECT_CALL(set_cdm_cb_, Run(true));
  EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_OK)));

  decrypting_renderer_->SetCdm(&cdm_context_, set_cdm_cb_.Get());
  decrypting_renderer_->Initialize(&media_resource_, &renderer_client_,
                                   renderer_init_cb_.Get());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(decrypting_renderer_->HasDecryptingMediaResourceForTesting());
}

TEST_F(DecryptingRendererTest, ClearStreams_OtherCdm) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ false);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ false);

  EXPECT_CALL(*renderer_, OnInitialize(_, _, _))
      .WillOnce(RunOnceCallback<2>(PIPELINE_OK));
  EXPECT_CALL(*renderer_, OnSetCdm(_, _)).WillOnce(RunOnceCallback<1>(true));
  EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_OK)));
  EXPECT_CALL(set_cdm_cb_, Run(true));

  decrypting_renderer_->Initialize(&media_resource_, &renderer_client_,
                                   renderer_init_cb_.Get());
  decrypting_renderer_->SetCdm(&cdm_context_, set_cdm_cb_.Get());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(decrypting_renderer_->HasDecryptingMediaResourceForTesting());
}

TEST_F(DecryptingRendererTest, EncryptedStreams_NoCdm) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ true);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ true);

  decrypting_renderer_->Initialize(&media_resource_, &renderer_client_,
                                   renderer_init_cb_.Get());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(decrypting_renderer_->HasDecryptingMediaResourceForTesting());
}

TEST_F(DecryptingRendererTest, EncryptedStreams_AesDecryptor) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ true);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ true);
  UseAesDecryptor(true);

  EXPECT_CALL(*renderer_, OnInitialize(_, _, _))
      .WillOnce(RunOnceCallback<2>(PIPELINE_OK));
  EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_OK)));
  EXPECT_CALL(set_cdm_cb_, Run(true));

  decrypting_renderer_->Initialize(&media_resource_, &renderer_client_,
                                   renderer_init_cb_.Get());
  decrypting_renderer_->SetCdm(&cdm_context_, set_cdm_cb_.Get());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(decrypting_renderer_->HasDecryptingMediaResourceForTesting());
}

TEST_F(DecryptingRendererTest, EncryptedStreams_OtherCdm) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ true);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ true);

  EXPECT_CALL(*renderer_, OnInitialize(_, _, _))
      .WillOnce(RunOnceCallback<2>(PIPELINE_OK));
  EXPECT_CALL(*renderer_, OnSetCdm(_, _)).WillOnce(RunOnceCallback<1>(true));
  EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_OK)));
  EXPECT_CALL(set_cdm_cb_, Run(true));

  decrypting_renderer_->Initialize(&media_resource_, &renderer_client_,
                                   renderer_init_cb_.Get());
  decrypting_renderer_->SetCdm(&cdm_context_, set_cdm_cb_.Get());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(decrypting_renderer_->HasDecryptingMediaResourceForTesting());
}

TEST_F(DecryptingRendererTest, EncryptedStreams_AesDecryptor_CdmSetBeforeInit) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ true);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ true);
  UseAesDecryptor(true);

  EXPECT_CALL(*renderer_, OnInitialize(_, _, _))
      .WillOnce(RunOnceCallback<2>(PIPELINE_OK));
  EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_OK)));
  EXPECT_CALL(set_cdm_cb_, Run(true));

  decrypting_renderer_->SetCdm(&cdm_context_, set_cdm_cb_.Get());
  decrypting_renderer_->Initialize(&media_resource_, &renderer_client_,
                                   renderer_init_cb_.Get());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(decrypting_renderer_->HasDecryptingMediaResourceForTesting());
}

TEST_F(DecryptingRendererTest, EncryptedStreams_OtherCdm_CdmSetBeforeInit) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ true);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ true);

  EXPECT_CALL(*renderer_, OnInitialize(_, _, _))
      .WillOnce(RunOnceCallback<2>(PIPELINE_OK));
  EXPECT_CALL(*renderer_, OnSetCdm(_, _)).WillOnce(RunOnceCallback<1>(true));
  EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_OK)));
  EXPECT_CALL(set_cdm_cb_, Run(true));

  decrypting_renderer_->SetCdm(&cdm_context_, set_cdm_cb_.Get());
  decrypting_renderer_->Initialize(&media_resource_, &renderer_client_,
                                   renderer_init_cb_.Get());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(decrypting_renderer_->HasDecryptingMediaResourceForTesting());
}

TEST_F(DecryptingRendererTest, EncryptedAndClearStream_OtherCdm) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ false);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ true);

  EXPECT_CALL(*renderer_, OnInitialize(_, _, _))
      .WillOnce(RunOnceCallback<2>(PIPELINE_OK));
  EXPECT_CALL(*renderer_, OnSetCdm(_, _)).WillOnce(RunOnceCallback<1>(true));
  EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_OK)));
  EXPECT_CALL(set_cdm_cb_, Run(true));

  decrypting_renderer_->Initialize(&media_resource_, &renderer_client_,
                                   renderer_init_cb_.Get());
  decrypting_renderer_->SetCdm(&cdm_context_, set_cdm_cb_.Get());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(decrypting_renderer_->HasDecryptingMediaResourceForTesting());
}

TEST_F(DecryptingRendererTest, DecryptingMediaResourceInitFails) {
  AddStream(DemuxerStream::AUDIO, /* encrypted = */ false);
  AddStream(DemuxerStream::VIDEO, /* encrypted = */ true);
  UseAesDecryptor(true);

  EXPECT_CALL(renderer_init_cb_,
              Run(HasStatusCode(PIPELINE_ERROR_INITIALIZATION_FAILED)));

  decrypting_renderer_->Initialize(&media_resource_, &renderer_client_,
                                   renderer_init_cb_.Get());
  task_environment_.RunUntilIdle();

  // Cause a PIPELINE_ERROR_INITIALIZATION_FAILED error to be passed as a
  // parameter to the initialization callback.
  InitializeDecryptingRendererWithFalse();
}

}  // namespace media
