// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_decoder_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/media_log.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/mojo/mojom/audio_decoder.mojom.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "media/mojo/services/mojo_media_client.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/functions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallback;
using testing::_;
using testing::StrictMock;

namespace media {

namespace {

class MockAudioDecoderClient : public mojom::AudioDecoderClient {
 public:
  MockAudioDecoderClient() = default;
  ~MockAudioDecoderClient() override = default;

  MOCK_METHOD(void, OnBufferDecoded, (mojom::AudioBufferPtr), (override));
  MOCK_METHOD(void, OnWaiting, (WaitingReason), (override));
};

class TestMockAudioDecoder : public MockAudioDecoder {
 public:
  TestMockAudioDecoder() = default;
  ~TestMockAudioDecoder() override = default;

  base::WeakPtr<TestMockAudioDecoder> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestMockAudioDecoder> weak_factory_{this};
};

using CreateAudioDecoderCB =
    base::RepeatingCallback<std::unique_ptr<AudioDecoder>()>;

class TestMojoMediaClient : public MojoMediaClient {
 public:
  explicit TestMojoMediaClient(CreateAudioDecoderCB create_audio_decoder_cb)
      : create_audio_decoder_cb_(std::move(create_audio_decoder_cb)) {}

  std::unique_ptr<AudioDecoder> CreateAudioDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<MediaLog> media_log) override {
    return create_audio_decoder_cb_.Run();
  }

 private:
  CreateAudioDecoderCB create_audio_decoder_cb_;
};

}  // namespace

class MojoAudioDecoderServiceTest : public testing::Test {
 public:
  MojoAudioDecoderServiceTest()
      : mojo_media_client_(base::BindRepeating(
            &MojoAudioDecoderServiceTest::CreateAudioDecoder,
            base::Unretained(this))) {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &MojoAudioDecoderServiceTest::OnProcessError, base::Unretained(this)));

    service_ = std::make_unique<MojoAudioDecoderService>(
        &mojo_media_client_, &mojo_cdm_service_context_,
        task_environment_.GetMainThreadTaskRunner());

    receiver_ = std::make_unique<mojo::Receiver<mojom::AudioDecoder>>(
        service_.get(), remote_service_.BindNewPipeAndPassReceiver());
  }

  ~MojoAudioDecoderServiceTest() override {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
    if (client_receiver_) {
      client_receiver_->Close();
    }
  }

  std::unique_ptr<AudioDecoder> CreateAudioDecoder() {
    if (should_create_decoder_fail_) {
      return nullptr;
    }
    auto decoder = std::make_unique<StrictMock<TestMockAudioDecoder>>();
    mock_audio_decoder_ = decoder->GetWeakPtr();
    return decoder;
  }

  void OnProcessError(const std::string& error) {
    bad_message_called_ = true;
    if (bad_message_quit_closure_) {
      std::move(bad_message_quit_closure_).Run();
    }
  }

  void ConstructService() {
    mojo::PendingAssociatedRemote<mojom::AudioDecoderClient> client_remote;
    client_receiver_ = mojo::MakeSelfOwnedAssociatedReceiver(
        std::make_unique<MockAudioDecoderClient>(),
        client_remote.InitWithNewEndpointAndPassReceiver());

    mojo::PendingRemote<mojom::MediaLog> media_log_remote;
    std::ignore = media_log_remote.InitWithNewPipeAndPassReceiver();

    remote_service_->Construct(std::move(client_remote),
                               std::move(media_log_remote));
    remote_service_.FlushForTesting();
  }

  bool WaitForBadMessage() {
    if (bad_message_called_) {
      return true;
    }
    base::RunLoop run_loop;
    bad_message_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    return bad_message_called_;
  }

  void InitializeService(const AudioDecoderConfig& config,
                         bool expected_success = true) {
    base::RunLoop run_loop;
    remote_service_->Initialize(
        config, std::nullopt,
        base::BindOnce(
            [](base::OnceClosure quit_closure, bool expected_success,
               const DecoderStatus& status, bool needs_bitstream_conversion,
               AudioDecoderType decoder_type) {
              EXPECT_EQ(status.is_ok(), expected_success);
              std::move(quit_closure).Run();
            },
            run_loop.QuitClosure(), expected_success));
    run_loop.Run();
  }

  void SetDataSource() {
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer, consumer),
              MOJO_RESULT_OK);
    remote_service_->SetDataSource(std::move(consumer));
    remote_service_.FlushForTesting();
  }

  AudioDecoderConfig GetTestConfig() {
    return AudioDecoderConfig(AudioCodec::kVorbis, kSampleFormatPlanarF32,
                              ChannelLayoutConfig::Stereo(), 44100,
                              std::vector<uint8_t>(),
                              EncryptionScheme::kUnencrypted);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  MojoCdmServiceContext mojo_cdm_service_context_;
  TestMojoMediaClient mojo_media_client_;
  std::unique_ptr<MojoAudioDecoderService> service_;
  std::unique_ptr<mojo::Receiver<mojom::AudioDecoder>> receiver_;
  mojo::Remote<mojom::AudioDecoder> remote_service_;

  base::WeakPtr<TestMockAudioDecoder> mock_audio_decoder_;
  bool should_create_decoder_fail_ = false;

  mojo::SelfOwnedAssociatedReceiverRef<mojom::AudioDecoderClient>
      client_receiver_;

  bool bad_message_called_ = false;
  base::OnceClosure bad_message_quit_closure_;
};

TEST_F(MojoAudioDecoderServiceTest, Construct_Success) {
  ConstructService();
  EXPECT_FALSE(bad_message_called_);
  EXPECT_NE(mock_audio_decoder_, nullptr);
}

TEST_F(MojoAudioDecoderServiceTest, Construct_Duplicate) {
  ConstructService();
  EXPECT_FALSE(bad_message_called_);

  // Call Construct again manually to avoid overwriting client_receiver_
  // and causing a leak.
  mojo::PendingAssociatedRemote<mojom::AudioDecoderClient> client_remote2;
  auto client_receiver2 = mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<MockAudioDecoderClient>(),
      client_remote2.InitWithNewEndpointAndPassReceiver());

  mojo::PendingRemote<mojom::MediaLog> media_log_remote2;
  std::ignore = media_log_remote2.InitWithNewPipeAndPassReceiver();

  remote_service_->Construct(std::move(client_remote2),
                             std::move(media_log_remote2));
  remote_service_.FlushForTesting();

  ASSERT_TRUE(WaitForBadMessage());

  if (client_receiver2) {
    client_receiver2->Close();
  }
}

TEST_F(MojoAudioDecoderServiceTest, Initialize_BeforeConstruct) {
  // Call Initialize before Construct.
  InitializeService(GetTestConfig(), /*expected_success=*/false);
  ASSERT_TRUE(WaitForBadMessage());
}

TEST_F(MojoAudioDecoderServiceTest, SetDataSource_Duplicate) {
  ConstructService();
  SetDataSource();
  EXPECT_FALSE(bad_message_called_);

  // Call SetDataSource again.
  SetDataSource();
  ASSERT_TRUE(WaitForBadMessage());
}

TEST_F(MojoAudioDecoderServiceTest, Initialize_Success) {
  ConstructService();
  EXPECT_CALL(*mock_audio_decoder_, Initialize_(_, _, _, _, _))
      .WillOnce(RunOnceCallback<2>(DecoderStatus::Codes::kOk));
  InitializeService(GetTestConfig(), /*expected_success=*/true);
  EXPECT_FALSE(bad_message_called_);
}

TEST_F(MojoAudioDecoderServiceTest, Initialize_Failure_ResetsDecoder) {
  ConstructService();
  EXPECT_CALL(*mock_audio_decoder_, Initialize_(_, _, _, _, _))
      .WillOnce(
          RunOnceCallback<2>(DecoderStatus::Codes::kFailedToCreateDecoder));
  InitializeService(GetTestConfig(), /*expected_success=*/false);
  EXPECT_FALSE(bad_message_called_);

  // Subsequent Initialize should fail with bad message because decoder_ was
  // reset.
  InitializeService(GetTestConfig(), /*expected_success=*/false);
  ASSERT_TRUE(WaitForBadMessage());
}

TEST_F(MojoAudioDecoderServiceTest, Decode_BeforeConstruct) {
  base::RunLoop run_loop;
  remote_service_->Decode(
      mojom::DecoderBuffer::NewEos(mojom::EosDecoderBuffer::New()),
      base::BindOnce(
          [](base::OnceClosure quit_closure, const DecoderStatus& status) {
            EXPECT_FALSE(status.is_ok());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
  ASSERT_TRUE(WaitForBadMessage());
}

TEST_F(MojoAudioDecoderServiceTest, Reset_BeforeConstruct) {
  base::RunLoop run_loop;
  remote_service_->Reset(base::BindOnce(
      [](base::OnceClosure quit_closure) { std::move(quit_closure).Run(); },
      run_loop.QuitClosure()));
  run_loop.Run();
  ASSERT_TRUE(WaitForBadMessage());
}

TEST_F(MojoAudioDecoderServiceTest, Decode_BeforeSetDataSource) {
  ConstructService();
  EXPECT_CALL(*mock_audio_decoder_, Initialize_(_, _, _, _, _))
      .WillOnce(RunOnceCallback<2>(DecoderStatus::Codes::kOk));
  InitializeService(GetTestConfig(), /*expected_success=*/true);
  EXPECT_FALSE(bad_message_called_);

  base::RunLoop run_loop;
  remote_service_->Decode(
      mojom::DecoderBuffer::NewEos(mojom::EosDecoderBuffer::New()),
      base::BindOnce(
          [](base::OnceClosure quit_closure, const DecoderStatus& status) {
            EXPECT_FALSE(status.is_ok());
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));
  run_loop.Run();
  ASSERT_TRUE(WaitForBadMessage());
}

TEST_F(MojoAudioDecoderServiceTest, Reset_BeforeSetDataSource) {
  ConstructService();
  EXPECT_CALL(*mock_audio_decoder_, Initialize_(_, _, _, _, _))
      .WillOnce(RunOnceCallback<2>(DecoderStatus::Codes::kOk));
  InitializeService(GetTestConfig(), /*expected_success=*/true);
  EXPECT_FALSE(bad_message_called_);

  base::RunLoop run_loop;
  remote_service_->Reset(base::BindOnce(
      [](base::OnceClosure quit_closure) { std::move(quit_closure).Run(); },
      run_loop.QuitClosure()));
  run_loop.Run();
  ASSERT_TRUE(WaitForBadMessage());
}

}  // namespace media
