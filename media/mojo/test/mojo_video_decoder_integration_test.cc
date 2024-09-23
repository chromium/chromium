// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decoder_status.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_log.h"
#include "media/base/mock_media_log.h"
#include "media/base/simple_sync_token_client.h"
#include "media/base/test_helpers.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "media/base/waiting.h"
#include "media/mojo/clients/mojo_video_decoder.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "media/mojo/services/mojo_media_client.h"
#include "media/mojo/services/mojo_video_decoder_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

namespace {

const int kMaxDecodeRequests = 4;
const int kErrorDataSize = 7;

// A mock VideoDecoder with helpful default functionality.
// TODO(sandersd): Determine how best to merge this with MockVideoDecoder
// declared in mock_filters.h.
class MockVideoDecoder : public VideoDecoder {
 public:
  MockVideoDecoder() {
    // Treat const getters like a NiceMock.
    EXPECT_CALL(*this, GetDecoderType())
        .WillRepeatedly(Return(VideoDecoderType::kTesting));
    EXPECT_CALL(*this, NeedsBitstreamConversion())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*this, CanReadWithoutStalling()).WillRepeatedly(Return(true));
    EXPECT_CALL(*this, GetMaxDecodeRequests())
        .WillRepeatedly(Return(kMaxDecodeRequests));

    // For regular methods, only configure a default action.
    ON_CALL(*this, Decode_(_, _))
        .WillByDefault(Invoke(this, &MockVideoDecoder::DoDecode));
    ON_CALL(*this, Reset_(_))
        .WillByDefault(Invoke(this, &MockVideoDecoder::DoReset));
  }

  MockVideoDecoder(const MockVideoDecoder&) = delete;
  MockVideoDecoder& operator=(const MockVideoDecoder&) = delete;

  // Re-declare as public.
  ~MockVideoDecoder() override {}

  // media::VideoDecoder implementation
  MOCK_CONST_METHOD0(GetDecoderType, VideoDecoderType());

  // Initialize() records values before delegating to the mock method.
  void Initialize(const VideoDecoderConfig& config,
                  bool /* low_delay */,
                  CdmContext* /* cdm_context */,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override {
    config_ = config;
    output_cb_ = output_cb;
    waiting_cb_ = waiting_cb;
    DoInitialize(init_cb);
  }

  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB cb) override {
    Decode_(std::move(buffer), cb);
  }
  MOCK_METHOD2(Decode_, void(scoped_refptr<DecoderBuffer> buffer, DecodeCB&));
  void Reset(base::OnceClosure cb) override { Reset_(cb); }
  MOCK_METHOD1(Reset_, void(base::OnceClosure&));
  MOCK_CONST_METHOD0(NeedsBitstreamConversion, bool());
  MOCK_CONST_METHOD0(CanReadWithoutStalling, bool());
  MOCK_CONST_METHOD0(GetMaxDecodeRequests, int());

  // Mock helpers.
  MOCK_METHOD1(DoInitialize, void(InitCB&));
  VideoFrame::ReleaseMailboxCB GetReleaseMailboxCB() {
    DidGetReleaseMailboxCB();
    return std::move(release_mailbox_cb);
  }

  MOCK_METHOD0(DidGetReleaseMailboxCB, void());

  VideoFrame::ReleaseMailboxCB release_mailbox_cb;

  // Returns an output frame immediately.
  // TODO(sandersd): Extend to support tests of MojoVideoFrame frames.
  void DoDecode(scoped_refptr<DecoderBuffer> buffer, DecodeCB& decode_cb) {
    if (!buffer->end_of_stream()) {
      if (buffer->size() == kErrorDataSize) {
        // This size buffer indicates that decoder should return an error.
        // |decode_cb| must not be called from the same stack.
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(decode_cb),
                                      DecoderStatus::Codes::kFailed));
        return;
      }
      if (buffer->decrypt_config()) {
        // Simulate the case where outputs are only returned when key arrives.
        waiting_cb_.Run(WaitingReason::kNoDecryptionKey);
      } else {
        scoped_refptr<gpu::ClientSharedImage> shared_image =
            gpu::ClientSharedImage::CreateForTesting();
        scoped_refptr<VideoFrame> frame = VideoFrame::WrapSharedImage(
            PIXEL_FORMAT_ARGB, shared_image, gpu::SyncToken(),
            GetReleaseMailboxCB(), config_.coded_size(), config_.visible_rect(),
            config_.natural_size(), buffer->timestamp());
        frame->metadata().power_efficient = true;
        output_cb_.Run(frame);
      }
    }

    // |decode_cb| must not be called from the same stack.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb), DecoderStatus::Codes::kOk));
  }

  void DoReset(base::OnceClosure& reset_cb) {
    // |reset_cb| must not be called from the same stack.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(reset_cb));
  }

  base::WeakPtr<MockVideoDecoder> GetWeakPtr() {
    return weak_this_factory_.GetWeakPtr();
  }

 private:
  VideoDecoderConfig config_;
  OutputCB output_cb_;
  WaitingCB waiting_cb_;
  base::WeakPtrFactory<MockVideoDecoder> weak_this_factory_{this};
};

// Proxies CreateVideoDecoder() to a callback.
class FakeMojoMediaClient : public MojoMediaClient {
 public:
  using CreateVideoDecoderCB =
      base::RepeatingCallback<std::unique_ptr<VideoDecoder>(MediaLog*)>;

  explicit FakeMojoMediaClient(CreateVideoDecoderCB create_video_decoder_cb)
      : create_video_decoder_cb_(std::move(create_video_decoder_cb)) {}

  FakeMojoMediaClient(const FakeMojoMediaClient&) = delete;
  FakeMojoMediaClient& operator=(const FakeMojoMediaClient&) = delete;

  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      MediaLog* media_log,
      mojom::CommandBufferIdPtr command_buffer_id,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space,
      mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder)
      override {
    return create_video_decoder_cb_.Run(media_log);
  }

 private:
  CreateVideoDecoderCB create_video_decoder_cb_;
};

}  // namespace

class MojoVideoDecoderIntegrationTest : public ::testing::Test {
 public:
  MojoVideoDecoderIntegrationTest()
      : mojo_media_client_(base::BindRepeating(
            &MojoVideoDecoderIntegrationTest::CreateVideoDecoder,
            base::Unretained(this))) {}

  MojoVideoDecoderIntegrationTest(const MojoVideoDecoderIntegrationTest&) =
      delete;
  MojoVideoDecoderIntegrationTest& operator=(
      const MojoVideoDecoderIntegrationTest&) = delete;

  void TearDown() override {
    if (client_) {
      client_.reset();
      RunUntilIdle();
    }
  }

 protected:
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void SetWriterCapacity(uint32_t capacity) { writer_capacity_ = capacity; }

  mojo::PendingRemote<mojom::VideoDecoder> CreateRemoteVideoDecoder() {
    mojo::PendingRemote<mojom::VideoDecoder> remote_video_decoder;
    video_decoder_receivers_.Add(
        std::make_unique<MojoVideoDecoderService>(
            &mojo_media_client_, &mojo_cdm_service_context_,
            mojo::PendingRemote<stable::mojom::StableVideoDecoder>()),
        remote_video_decoder.InitWithNewPipeAndPassReceiver());
    return remote_video_decoder;
  }

  void CreateClient() {
    DCHECK(!client_);
    // TODO(sandersd): Pass a GpuVideoAcceleratorFactories so that the cache can
    // be tested.
    client_ = std::make_unique<MojoVideoDecoder>(
        base::SingleThreadTaskRunner::GetCurrentDefault(), nullptr,
        &client_media_log_, CreateRemoteVideoDecoder(), RequestOverlayInfoCB(),
        gfx::ColorSpace());
    if (writer_capacity_)
      client_->set_writer_capacity_for_testing(writer_capacity_);
  }

  bool Initialize() {
    CreateClient();

    EXPECT_CALL(*decoder_, DoInitialize(_))
        .WillOnce(RunOnceCallback<0>(DecoderStatus::Codes::kOk));

    DecoderStatus result = DecoderStatus::Codes::kOk;
    StrictMock<base::MockCallback<VideoDecoder::InitCB>> init_cb;
    EXPECT_CALL(init_cb, Run(_)).WillOnce(SaveArg<0>(&result));

    EXPECT_EQ(client_->GetDecoderType(), VideoDecoderType::kUnknown);
    client_->Initialize(TestVideoConfig::NormalH264(), false, nullptr,
                        init_cb.Get(), output_cb_.Get(), waiting_cb_.Get());
    RunUntilIdle();

    return result.is_ok();
  }

  DecoderStatus Decode(scoped_refptr<DecoderBuffer> buffer,
                       VideoFrame::ReleaseMailboxCB release_cb =
                           VideoFrame::ReleaseMailboxCB()) {
    DecoderStatus result(DecoderStatus::Codes::kFailed);

    if (!buffer->end_of_stream()) {
      decoder_->release_mailbox_cb = std::move(release_cb);
      EXPECT_CALL(*decoder_, DidGetReleaseMailboxCB());
    }
    EXPECT_CALL(*decoder_, Decode_(_, _));

    StrictMock<base::MockCallback<VideoDecoder::DecodeCB>> decode_cb;
    EXPECT_CALL(decode_cb, Run(_)).WillOnce(SaveArg<0>(&result));

    client_->Decode(buffer, decode_cb.Get());
    RunUntilIdle();

    return result;
  }

  scoped_refptr<DecoderBuffer> CreateKeyframe(int64_t timestamp_ms) {
    // Use 32 bytes to simulated chunked write (with capacity 10; see below).
    std::vector<uint8_t> data(32, 0);

    scoped_refptr<DecoderBuffer> buffer = DecoderBuffer::CopyFrom(data);

    buffer->set_timestamp(base::Milliseconds(timestamp_ms));
    buffer->set_duration(base::Milliseconds(10));
    buffer->set_is_key_frame(true);

    return buffer;
  }

  scoped_refptr<DecoderBuffer> CreateErrorFrame(int64_t timestamp_ms) {
    std::vector<uint8_t> data(kErrorDataSize, 0);

    scoped_refptr<DecoderBuffer> buffer = DecoderBuffer::CopyFrom(data);

    buffer->set_timestamp(base::Milliseconds(timestamp_ms));
    buffer->set_duration(base::Milliseconds(10));
    buffer->set_is_key_frame(true);

    return buffer;
  }

  // TODO(xhwang): Add a function to help create encrypted video buffers in
  // media/base/test_helpers.h.
  scoped_refptr<DecoderBuffer> CreateEncryptedKeyframe(int64_t timestamp_ms) {
    auto buffer = CreateKeyframe(timestamp_ms);

    const uint8_t kFakeKeyId[] = {0x4b, 0x65, 0x79, 0x20, 0x49, 0x44};
    const uint8_t kFakeIv[DecryptConfig::kDecryptionKeySize] = {0};
    buffer->set_decrypt_config(DecryptConfig::CreateCencConfig(
        std::string(reinterpret_cast<const char*>(kFakeKeyId),
                    std::size(kFakeKeyId)),
        std::string(reinterpret_cast<const char*>(kFakeIv), std::size(kFakeIv)),
        {}));

    return buffer;
  }

  // Callback that |client_| will deliver VideoFrames to.
  StrictMock<base::MockCallback<VideoDecoder::OutputCB>> output_cb_;

  StrictMock<base::MockCallback<WaitingCB>> waiting_cb_;

  // MojoVideoDecoder (client) under test.
  std::unique_ptr<MojoVideoDecoder> client_;

  // MediaLog that |client_| will deliver log events to.
  StrictMock<MockMediaLog> client_media_log_;

  // VideoDecoder (impl used by service) under test.
  // |decoder_owner_| owns the decoder until ownership is transferred to the
  // |MojoVideoDecoderService|. |decoder_| references it for the duration of its
  // lifetime.
  std::unique_ptr<MockVideoDecoder> decoder_owner_ =
      std::make_unique<MockVideoDecoder>();
  base::WeakPtr<MockVideoDecoder> decoder_ = decoder_owner_->GetWeakPtr();

  // MediaLog that the service has provided to |decoder_|. This should be
  // proxied to |client_media_log_|.
  raw_ptr<MediaLog, AcrossTasksDanglingUntriaged> decoder_media_log_ = nullptr;

  mojo::UniqueReceiverSet<mojom::VideoDecoder> video_decoder_receivers_;

 private:
  // Passes |decoder_| to the service.
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(MediaLog* media_log) {
    DCHECK(!decoder_media_log_);
    decoder_media_log_ = media_log;
    return std::move(decoder_owner_);
  }

  base::test::TaskEnvironment task_environment_;

  // Capacity that will be used for the MojoDecoderBufferWriter.
  uint32_t writer_capacity_ = 0;

  MojoCdmServiceContext mojo_cdm_service_context_;

  // Provides |decoder_| to the service.
  FakeMojoMediaClient mojo_media_client_;
};

TEST_F(MojoVideoDecoderIntegrationTest, CreateAndDestroy) {}

TEST_F(MojoVideoDecoderIntegrationTest, GetSupportedConfigs) {
  mojo::Remote<mojom::VideoDecoder> remote_video_decoder(
      CreateRemoteVideoDecoder());
  StrictMock<
      base::MockCallback<mojom::VideoDecoder::GetSupportedConfigsCallback>>
      callback;

  // TODO(sandersd): Expect there to be an entry.
  EXPECT_CALL(callback, Run(_, _));
  remote_video_decoder->GetSupportedConfigs(callback.Get());
  RunUntilIdle();
}

TEST_F(MojoVideoDecoderIntegrationTest, Initialize) {
  ASSERT_TRUE(Initialize());
  EXPECT_EQ(client_->GetDecoderType(), VideoDecoderType::kTesting);
  EXPECT_EQ(client_->NeedsBitstreamConversion(), false);
  EXPECT_EQ(client_->CanReadWithoutStalling(), true);
  EXPECT_EQ(client_->GetMaxDecodeRequests(), kMaxDecodeRequests);
}

TEST_F(MojoVideoDecoderIntegrationTest, InitializeFailNoDecoder) {
  CreateClient();

  StrictMock<base::MockCallback<VideoDecoder::InitCB>> init_cb;
  EXPECT_CALL(init_cb,
              Run(HasStatusCode(DecoderStatus::Codes::kFailedToCreateDecoder)));

  // Clear |decoder_| so that Initialize() should fail.
  decoder_owner_.reset();
  client_->Initialize(TestVideoConfig::NormalH264(), false, nullptr,
                      init_cb.Get(), output_cb_.Get(), waiting_cb_.Get());
  RunUntilIdle();
}

TEST_F(MojoVideoDecoderIntegrationTest, InitializeFailNoCdm) {
  CreateClient();

  StrictMock<base::MockCallback<VideoDecoder::InitCB>> init_cb;
  EXPECT_CALL(
      init_cb,
      Run(HasStatusCode(DecoderStatus::Codes::kUnsupportedEncryptionMode)));

  // CdmContext* (3rd parameter) is not provided but the VideoDecoderConfig
  // specifies encrypted video, so Initialize() should fail.
  client_->Initialize(TestVideoConfig::NormalEncrypted(), false, nullptr,
                      init_cb.Get(), output_cb_.Get(), waiting_cb_.Get());
  RunUntilIdle();
}

TEST_F(MojoVideoDecoderIntegrationTest, MediaLogIsProxied) {
  ASSERT_TRUE(Initialize());
  EXPECT_MEDIA_LOG_ON(client_media_log_, HasSubstr("\"test\""));
  MEDIA_LOG(DEBUG, decoder_media_log_) << "test";
  RunUntilIdle();
}

TEST_F(MojoVideoDecoderIntegrationTest, WaitingForKey) {
  ASSERT_TRUE(Initialize());

  auto buffer = CreateEncryptedKeyframe(0);
  StrictMock<base::MockCallback<VideoDecoder::DecodeCB>> decode_cb;

  EXPECT_CALL(*decoder_, Decode_(_, _));
  EXPECT_CALL(waiting_cb_, Run(WaitingReason::kNoDecryptionKey));
  EXPECT_CALL(decode_cb, Run(IsOkStatus()));

  client_->Decode(buffer, decode_cb.Get());
  RunUntilIdle();
}

TEST_F(MojoVideoDecoderIntegrationTest, Decode) {
  ASSERT_TRUE(Initialize());

  EXPECT_CALL(output_cb_, Run(_));
  ASSERT_TRUE(Decode(CreateKeyframe(0)).is_ok());
  Mock::VerifyAndClearExpectations(&output_cb_);

  ASSERT_TRUE(Decode(DecoderBuffer::CreateEOSBuffer()).is_ok());
}

TEST_F(MojoVideoDecoderIntegrationTest, Release) {
  ASSERT_TRUE(Initialize());

  StrictMock<base::MockCallback<VideoFrame::ReleaseMailboxCB>> release_cb;
  scoped_refptr<VideoFrame> frame;

  EXPECT_CALL(output_cb_, Run(_)).WillOnce(SaveArg<0>(&frame));
  ASSERT_TRUE(Decode(CreateKeyframe(0), release_cb.Get()).is_ok());
  Mock::VerifyAndClearExpectations(&output_cb_);

  EXPECT_CALL(release_cb, Run(_));
  gpu::SyncToken release_sync_token(gpu::CommandBufferNamespace::GPU_IO,
                                    gpu::CommandBufferId(),
                                    /*release_count=*/1u);
  release_sync_token.SetVerifyFlush();
  SimpleSyncTokenClient client(release_sync_token);
  frame->UpdateReleaseSyncToken(&client);
  frame = nullptr;
  RunUntilIdle();
}

TEST_F(MojoVideoDecoderIntegrationTest, ReleaseAfterShutdown) {
  ASSERT_TRUE(Initialize());

  StrictMock<base::MockCallback<VideoFrame::ReleaseMailboxCB>> release_cb;
  scoped_refptr<VideoFrame> frame;

  EXPECT_CALL(output_cb_, Run(_)).WillOnce(SaveArg<0>(&frame));
  ASSERT_TRUE(Decode(CreateKeyframe(0), release_cb.Get()).is_ok());
  Mock::VerifyAndClearExpectations(&output_cb_);

  client_.reset();
  RunUntilIdle();

  EXPECT_CALL(release_cb, Run(_));
  frame = nullptr;
  RunUntilIdle();
}

TEST_F(MojoVideoDecoderIntegrationTest, ResetDuringDecode) {
  ASSERT_TRUE(Initialize());

  StrictMock<base::MockCallback<VideoDecoder::DecodeCB>> decode_cb;
  StrictMock<base::MockCallback<base::OnceClosure>> reset_cb;

  EXPECT_CALL(*decoder_, DidGetReleaseMailboxCB()).Times(AtLeast(0));
  EXPECT_CALL(output_cb_, Run(_)).Times(kMaxDecodeRequests);
  EXPECT_CALL(*decoder_, Decode_(_, _)).Times(kMaxDecodeRequests);
  EXPECT_CALL(*decoder_, Reset_(_));

  InSequence s;  // Make sure all callbacks are fired in order.
  EXPECT_CALL(decode_cb, Run(_)).Times(kMaxDecodeRequests);
  EXPECT_CALL(reset_cb, Run());

  int64_t timestamp_ms = 0;
  for (int j = 0; j < kMaxDecodeRequests; ++j) {
    client_->Decode(CreateKeyframe(timestamp_ms++), decode_cb.Get());
  }

  client_->Reset(reset_cb.Get());

  RunUntilIdle();
}

TEST_F(MojoVideoDecoderIntegrationTest, ResetDuringDecode_ChunkedWrite) {
  // Use a small writer capacity to force chunked write.
  SetWriterCapacity(10);
  ASSERT_TRUE(Initialize());

  VideoFrame::ReleaseMailboxCB release_cb = VideoFrame::ReleaseMailboxCB();
  StrictMock<base::MockCallback<VideoDecoder::DecodeCB>> decode_cb;
  StrictMock<base::MockCallback<base::OnceClosure>> reset_cb;

  EXPECT_CALL(*decoder_, DidGetReleaseMailboxCB()).Times(AtLeast(0));
  EXPECT_CALL(output_cb_, Run(_)).Times(kMaxDecodeRequests);
  EXPECT_CALL(*decoder_, Decode_(_, _)).Times(kMaxDecodeRequests);
  EXPECT_CALL(*decoder_, Reset_(_));

  InSequence s;  // Make sure all callbacks are fired in order.
  EXPECT_CALL(decode_cb, Run(_)).Times(kMaxDecodeRequests);
  EXPECT_CALL(reset_cb, Run());

  int64_t timestamp_ms = 0;
  for (int j = 0; j < kMaxDecodeRequests; ++j) {
    client_->Decode(CreateKeyframe(timestamp_ms++), decode_cb.Get());
  }

  client_->Reset(reset_cb.Get());

  RunUntilIdle();
}

TEST_F(MojoVideoDecoderIntegrationTest, CanReadWithoutStallingAfterReset) {
  ASSERT_TRUE(Initialize());

  StrictMock<base::MockCallback<VideoDecoder::DecodeCB>> decode_cb;
  StrictMock<base::MockCallback<base::OnceClosure>> reset_cb;

  EXPECT_CALL(*decoder_, DidGetReleaseMailboxCB()).Times(AtLeast(0));
  EXPECT_CALL(output_cb_, Run(_)).Times(1);
  EXPECT_CALL(*decoder_, CanReadWithoutStalling())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*decoder_, Decode_(_, _)).Times(1);
  EXPECT_CALL(*decoder_, Reset_(_));

  EXPECT_TRUE(client_->CanReadWithoutStalling());

  InSequence s;  // Make sure all callbacks are fired in order.
  EXPECT_CALL(decode_cb, Run(_)).Times(1);
  EXPECT_CALL(reset_cb, Run());

  client_->Decode(CreateKeyframe(0), decode_cb.Get());
  RunUntilIdle();

  EXPECT_FALSE(client_->CanReadWithoutStalling());
  client_->Reset(reset_cb.Get());

  RunUntilIdle();
  EXPECT_TRUE(client_->CanReadWithoutStalling());
}

}  // namespace media
