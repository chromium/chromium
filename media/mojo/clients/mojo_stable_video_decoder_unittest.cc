// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_stable_video_decoder.h"

#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "media/base/decoder.h"
#include "media/base/media_util.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/gpu/chromeos/oop_video_decoder.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"

using testing::_;
using testing::InSequence;
using testing::Mock;
using testing::StrictMock;
using testing::WithArgs;

namespace media {

namespace {

std::vector<SupportedVideoDecoderConfig> CreateListOfSupportedConfigs() {
  return {
      {/*profile_min=*/H264PROFILE_MIN, /*profile_max=*/H264PROFILE_MAX,
       /*coded_size_min=*/gfx::Size(320, 180),
       /*coded_size_max=*/gfx::Size(1280, 720), /*allow_encrypted=*/false,
       /*require_encrypted=*/false},
      {/*profile_min=*/VP9PROFILE_MIN, /*profile_max=*/VP9PROFILE_MAX,
       /*coded_size_min=*/gfx::Size(8, 8),
       /*coded_size_max=*/gfx::Size(640, 360), /*allow_encrypted=*/true,
       /*require_encrypted=*/true},
  };
}

VideoDecoderConfig CreateValidSupportedVideoDecoderConfig() {
  // Note: the StableVideoDecoder Mojo interface doesn't support
  // VideoTransformation.
  const VideoDecoderConfig config(
      VideoCodec::kH264, VideoCodecProfile::H264PROFILE_BASELINE,
      VideoDecoderConfig::AlphaMode::kHasAlpha, VideoColorSpace::REC709(),
      VideoTransformation(),
      /*coded_size=*/gfx::Size(640, 368),
      /*visible_rect=*/gfx::Rect(1, 1, 630, 360),
      /*natural_size=*/gfx::Size(1260, 720),
      /*extra_data=*/std::vector<uint8_t>{1, 2, 3},
      EncryptionScheme::kUnencrypted);
  DCHECK(config.IsValidConfig());
  return config;
}

VideoDecoderConfig CreateValidUnsupportedVideoDecoderConfig() {
  // Note: the StableVideoDecoder Mojo interface doesn't support
  // VideoTransformation.
  const VideoDecoderConfig config(
      VideoCodec::kAV1, VideoCodecProfile::AV1PROFILE_PROFILE_MAIN,
      VideoDecoderConfig::AlphaMode::kHasAlpha, VideoColorSpace::REC709(),
      VideoTransformation(),
      /*coded_size=*/gfx::Size(640, 368),
      /*visible_rect=*/gfx::Rect(1, 1, 630, 360),
      /*natural_size=*/gfx::Size(1260, 720),
      /*extra_data=*/std::vector<uint8_t>{1, 2, 3},
      EncryptionScheme::kUnencrypted);
  DCHECK(config.IsValidConfig());
  return config;
}

class MockVideoFrameHandleReleaser
    : public stable::mojom::VideoFrameHandleReleaser {
 public:
  explicit MockVideoFrameHandleReleaser(
      mojo::PendingReceiver<stable::mojom::VideoFrameHandleReleaser>
          video_frame_handle_releaser)
      : video_frame_handle_releaser_receiver_(
            this,
            std::move(video_frame_handle_releaser)) {}
  MockVideoFrameHandleReleaser(const MockVideoFrameHandleReleaser&) = delete;
  MockVideoFrameHandleReleaser& operator=(const MockVideoFrameHandleReleaser&) =
      delete;
  ~MockVideoFrameHandleReleaser() override = default;

  // stable::mojom::VideoFrameHandleReleaser implementation.
  MOCK_METHOD1(ReleaseVideoFrame,
               void(const base::UnguessableToken& release_token));

 private:
  mojo::Receiver<stable::mojom::VideoFrameHandleReleaser>
      video_frame_handle_releaser_receiver_;
};

class MockStableVideoDecoderService : public stable::mojom::StableVideoDecoder {
 public:
  explicit MockStableVideoDecoderService(
      mojo::PendingReceiver<stable::mojom::StableVideoDecoder> pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}
  MockStableVideoDecoderService(const MockStableVideoDecoderService&) = delete;
  MockStableVideoDecoderService& operator=(
      const MockStableVideoDecoderService&) = delete;
  ~MockStableVideoDecoderService() override = default;

  // stable::mojom::StableVideoDecoder implementation.
  //
  // Note: Construct() saves the Mojo endpoints for later usage and to ensure
  // that the connection between the client and the service is not torn down.
  MOCK_METHOD1(GetSupportedConfigs, void(GetSupportedConfigsCallback callback));
  void Construct(
      mojo::PendingAssociatedRemote<stable::mojom::VideoDecoderClient>
          stable_video_decoder_client_remote,
      mojo::PendingRemote<stable::mojom::MediaLog> stable_media_log_remote,
      mojo::PendingReceiver<stable::mojom::VideoFrameHandleReleaser>
          stable_video_frame_handle_releaser_receiver,
      mojo::ScopedDataPipeConsumerHandle decoder_buffer_pipe,
      const gfx::ColorSpace& target_color_space) override {
    video_decoder_client_remote_.Bind(
        std::move(stable_video_decoder_client_remote));
    media_log_remote_.Bind(std::move(stable_media_log_remote));
    mock_video_frame_handle_releaser_ =
        std::make_unique<StrictMock<MockVideoFrameHandleReleaser>>(
            std::move(stable_video_frame_handle_releaser_receiver));
    mojo_decoder_buffer_reader_ = std::make_unique<MojoDecoderBufferReader>(
        std::move(decoder_buffer_pipe));
    DoConstruct(target_color_space);
  }
  MOCK_METHOD4(
      Initialize,
      void(const VideoDecoderConfig& config,
           bool low_delay,
           mojo::PendingRemote<stable::mojom::StableCdmContext> cdm_context,
           InitializeCallback callback));
  MOCK_METHOD2(Decode,
               void(const scoped_refptr<DecoderBuffer>& buffer,
                    DecodeCallback callback));
  MOCK_METHOD1(Reset, void(ResetCallback callback));

  MOCK_METHOD1(DoConstruct, void(const gfx::ColorSpace& target_color_space));

  MojoDecoderBufferReader* mojo_decoder_buffer_reader() const {
    return mojo_decoder_buffer_reader_.get();
  }

 private:
  mojo::Receiver<stable::mojom::StableVideoDecoder> receiver_;

  // |video_decoder_client_remote_| is the client endpoint that's received
  // through the Construct() call.
  mojo::AssociatedRemote<stable::mojom::VideoDecoderClient>
      video_decoder_client_remote_;

  // |media_log_remote_| is the MediaLog endpoint that's received through the
  // Construct() call.
  mojo::Remote<stable::mojom::MediaLog> media_log_remote_;

  // |mock_video_frame_handle_releaser_| receives frame release events from the
  // client.
  std::unique_ptr<StrictMock<MockVideoFrameHandleReleaser>>
      mock_video_frame_handle_releaser_;

  // |mojo_decoder_buffer_reader_| wraps the reading end of the data pipe that's
  // received through the Construct() call.
  std::unique_ptr<MojoDecoderBufferReader> mojo_decoder_buffer_reader_;
};

class MockSharedImageInterface : public gpu::SharedImageInterface {
 public:
  // gpu::SharedImageInterface implementation.
  MOCK_METHOD2(
      CreateSharedImage,
      scoped_refptr<gpu::ClientSharedImage>(const gpu::SharedImageInfo& si_info,
                                            gpu::SurfaceHandle surface_handle));
  MOCK_METHOD2(CreateSharedImage,
               scoped_refptr<gpu::ClientSharedImage>(
                   const gpu::SharedImageInfo& si_info,
                   base::span<const uint8_t> pixel_data));
  MOCK_METHOD4(CreateSharedImage,
               scoped_refptr<gpu::ClientSharedImage>(
                   const gpu::SharedImageInfo& si_info,
                   gpu::SurfaceHandle surface_handle,
                   gfx::BufferUsage buffer_usage,
                   gfx::GpuMemoryBufferHandle buffer_handle));
  MOCK_METHOD2(CreateSharedImage,
               scoped_refptr<gpu::ClientSharedImage>(
                   const gpu::SharedImageInfo& si_info,
                   gfx::GpuMemoryBufferHandle buffer_handle));
  MOCK_METHOD1(CreateSharedImage,
               SharedImageMapping(const gpu::SharedImageInfo& si_info));
  MOCK_METHOD4(CreateSharedImage,
               scoped_refptr<gpu::ClientSharedImage>(
                   gfx::GpuMemoryBuffer* gpu_memory_buffer,
                   gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
                   gfx::BufferPlane plane,
                   const gpu::SharedImageInfo& si_info));
  MOCK_METHOD2(UpdateSharedImage,
               void(const gpu::SyncToken& sync_token,
                    const gpu::Mailbox& mailbox));
  MOCK_METHOD3(UpdateSharedImage,
               void(const gpu::SyncToken& sync_token,
                    std::unique_ptr<gfx::GpuFence> acquire_fence,
                    const gpu::Mailbox& mailbox));
  MOCK_METHOD2(DestroySharedImage,
               void(const gpu::SyncToken& sync_token,
                    const gpu::Mailbox& mailbox));
  MOCK_METHOD2(DestroySharedImage,
               void(const gpu::SyncToken& sync_token,
                    scoped_refptr<gpu::ClientSharedImage> client_shared_image));
  MOCK_METHOD1(ImportSharedImage,
               scoped_refptr<gpu::ClientSharedImage>(
                   const gpu::ExportedSharedImage& exported_shared_image));
  MOCK_METHOD6(CreateSwapChain,
               SwapChainSharedImages(viz::SharedImageFormat format,
                                     const gfx::Size& size,
                                     const gfx::ColorSpace& color_space,
                                     GrSurfaceOrigin surface_origin,
                                     SkAlphaType alpha_type,
                                     uint32_t usage));
  MOCK_METHOD2(PresentSwapChain,
               void(const gpu::SyncToken& sync_token,
                    const gpu::Mailbox& mailbox));
  MOCK_METHOD0(GenUnverifiedSyncToken, gpu::SyncToken());
  MOCK_METHOD0(GenVerifiedSyncToken, gpu::SyncToken());
  MOCK_METHOD1(VerifySyncToken, void(gpu::SyncToken& sync_token));
  MOCK_METHOD1(WaitSyncToken, void(const gpu::SyncToken& sync_token));
  MOCK_METHOD0(Flush, void());
  MOCK_METHOD1(GetNativePixmap,
               scoped_refptr<gfx::NativePixmap>(const gpu::Mailbox& mailbox));
  MOCK_METHOD0(GetCapabilities, const gpu::SharedImageCapabilities&());

  scoped_refptr<gpu::SharedImageInterfaceHolder> holder() const {
    return holder_;
  }

 protected:
  ~MockSharedImageInterface() override = default;
};

// TestEndpoints groups a few members that result from creating and initializing
// a MojoStableVideoDecoder so that tests can use them to set expectations
// and/or to poke at them to trigger specific paths.
class TestEndpoints {
 public:
  // Creates a TestEndpoints instance which encapsulates a
  // MojoStableVideoDecoder client that's connected to a
  // MockStableVideoDecoderService. |media_task_runner| is the task runner used
  // for the MojoStableVideoDecoder. The MojoStableVideoDecoder will be
  // destroyed on that task runner.
  explicit TestEndpoints(
      scoped_refptr<base::SequencedTaskRunner> media_task_runner)
      : sii_(base::MakeRefCounted<StrictMock<MockSharedImageInterface>>()),
        gpu_factories_(
            std::make_unique<StrictMock<MockGpuVideoAcceleratorFactories>>(
                sii_.get())),
        client_media_log_(std::make_unique<NullMediaLog>()),
        media_task_runner_(std::move(media_task_runner)) {
    mojo::PendingRemote<stable::mojom::StableVideoDecoder>
        mojo_stable_vd_pending_remote;
    service_ = std::make_unique<StrictMock<MockStableVideoDecoderService>>(
        mojo_stable_vd_pending_remote.InitWithNewPipeAndPassReceiver());
    client_ = std::make_unique<MojoStableVideoDecoder>(
        media_task_runner_, gpu_factories_.get(), client_media_log_.get(),
        std::move(mojo_stable_vd_pending_remote));
  }

  ~TestEndpoints() {
    // The |client_| must be destroyed on the |media_task_runner_|, but the
    // TestEndpoints instance can be destroyed on the main test thread.
    // Therefore, we must post a task to destroy the |client_|. However,
    // *|client_| has raw pointers to *|client_media_log_| and
    // *|gpu_factories_|. In order to prevent those pointers from becoming
    // dangling while waiting for the destroy task to run, we also post a task
    // to the |media_task_runner_| to destroy the |client_media_log_| and the
    // |gpu_factories_|. The *|gpu_factories_| has a raw pointer to *|sii_|, so
    // for the same reason, we post a task to the |media_task_runner_| to ensure
    // *|sii_| outlives *|gpu_factories_|.
    media_task_runner_->DeleteSoon(FROM_HERE, std::move(client_));
    media_task_runner_->DeleteSoon(FROM_HERE, std::move(client_media_log_));
    media_task_runner_->DeleteSoon(FROM_HERE, std::move(gpu_factories_));
    media_task_runner_->PostTask(FROM_HERE,
                                 base::DoNothingWithBoundArgs(std::move(sii_)));
  }

  StrictMock<MockSharedImageInterface>* shared_image_interface() const {
    return sii_.get();
  }

  MojoStableVideoDecoder* client() const { return client_.get(); }

  StrictMock<MockStableVideoDecoderService>* service() const {
    return service_.get();
  }

  // Returns a base::MockRepeatingCallback corresponding to the output callback
  // passed to MojoStableVideoDecoder::Initialize().
  StrictMock<base::MockRepeatingCallback<void(scoped_refptr<VideoFrame>)>>*
  client_output_cb() {
    return &client_output_cb_;
  }

  // Returns a base::MockRepeatingCallback corresponding to the waiting callback
  // passed to MojoStableVideoDecoder::Initialize().
  StrictMock<base::MockRepeatingCallback<void(WaitingReason)>>*
  client_waiting_cb() {
    return &client_waiting_cb_;
  }

 private:
  scoped_refptr<StrictMock<MockSharedImageInterface>> sii_;
  std::unique_ptr<StrictMock<MockGpuVideoAcceleratorFactories>> gpu_factories_;
  std::unique_ptr<NullMediaLog> client_media_log_;
  StrictMock<base::MockRepeatingCallback<void(scoped_refptr<VideoFrame>)>>
      client_output_cb_;
  StrictMock<base::MockRepeatingCallback<void(WaitingReason)>>
      client_waiting_cb_;
  std::unique_ptr<MojoStableVideoDecoder> client_;
  std::unique_ptr<StrictMock<MockStableVideoDecoderService>> service_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
};

}  // namespace

class MojoStableVideoDecoderTest : public testing::Test {
 public:
  MojoStableVideoDecoderTest()
      : media_task_runner_(
            base::ThreadPool::CreateSequencedTaskRunner(/*traits=*/{})) {}

  // testing::Test implementation.
  void TearDown() override {
    task_environment_.RunUntilIdle();
    OOPVideoDecoder::ResetGlobalStateForTesting();
  }

 protected:
  // Creates and initializes a new MojoStableVideoDecoder connected to a
  // MockStableVideoDecoderService (verifying the initialization expectations
  // along the way). Returns all relevant endpoints or nullptr if initialization
  // did not complete successfully.
  std::unique_ptr<TestEndpoints> CreateAndInitializeMojoStableVideoDecoder(
      const VideoDecoderConfig& config) {
    auto endpoints = std::make_unique<TestEndpoints>(media_task_runner_);

    // The first time Initialize() is called on the MojoStableVideoDecoder in a
    // process, we should get the supported configurations from the service.
    // We'll store the supported configurations callback in
    // |received_get_supported_configs_cb| to reply later.
    //
    // Note that we always set the expectation that GetSupportedConfigs() is
    // called, even though we don't know if each test case is going to be run in
    // its own process. That's because we call
    // OOPVideoDecoder::ResetGlobalStateForTesting() in TearDown() so that
    // singleton state is reset in between test cases.
    stable::mojom::StableVideoDecoder::GetSupportedConfigsCallback
        received_get_supported_configs_cb;
    EXPECT_CALL(*endpoints->service(), GetSupportedConfigs(_))
        .WillOnce(
            [&](stable::mojom::StableVideoDecoder::GetSupportedConfigsCallback
                    callback) {
              received_get_supported_configs_cb = std::move(callback);
            });

    constexpr bool kLowDelayToInitializeWith = false;
    StrictMock<base::MockOnceCallback<void(DecoderStatus)>> init_cb;
    media_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MojoStableVideoDecoder::Initialize,
                                  base::Unretained(endpoints->client()), config,
                                  kLowDelayToInitializeWith,
                                  /*cdm_context=*/nullptr, init_cb.Get(),
                                  endpoints->client_output_cb()->Get(),
                                  endpoints->client_waiting_cb()->Get()));
    task_environment_.RunUntilIdle();
    if (!Mock::VerifyAndClearExpectations(endpoints->service())) {
      return nullptr;
    }
    if (!received_get_supported_configs_cb) {
      ADD_FAILURE() << "Did not receive a valid GetSupportedConfigsCallback";
      return nullptr;
    }

    // Now we'll reply with a list of supported configurations. Depending on
    // whether |config| is supported according to that list, the
    // MojoStableVideoDecoder should do the following:
    //
    // - If |config| is supported, the MojoStableVideoDecoder should then create
    //   the OOPVideoDecoder which should result in a call to Construct() on the
    //   service. Then, it should call OOPVideoDecoder::Initialize() which
    //   should result in an Initialize() call on the service. In this case,
    //   we'll store the Initialize() reply callback that the service sees as
    //   |received_initialize_cb| so that we can call it later.
    //
    // - If |config| is not supported, the MojoStableVideoDecoder should not
    //   create the OOPVideoDecoder, so no messages should be received by the
    //   service. Instead, the MojoStableVideoDecoder should call the
    //   Initialize() callback with kUnsupportedConfig.
    constexpr VideoDecoderType kDecoderTypeToReplyWith =
        VideoDecoderType::kVaapi;
    const std::vector<SupportedVideoDecoderConfig>
        supported_configs_to_reply_with = CreateListOfSupportedConfigs();
    stable::mojom::StableVideoDecoder::InitializeCallback
        received_initialize_cb;
    const bool config_is_supported =
        IsVideoDecoderConfigSupported(supported_configs_to_reply_with, config);
    if (config_is_supported) {
      InSequence sequence;
      EXPECT_CALL(*endpoints->service(), DoConstruct(_));
      EXPECT_CALL(*endpoints->service(),
                  Initialize(_, kLowDelayToInitializeWith, _, _))
          .WillOnce(WithArgs<0, 3>(
              [&](const VideoDecoderConfig& received_config,
                  stable::mojom::StableVideoDecoder::InitializeCallback
                      callback) {
                EXPECT_TRUE(received_config.Matches(config));
                received_initialize_cb = std::move(callback);
              }));
    } else {
      EXPECT_CALL(init_cb,
                  Run(DecoderStatus(DecoderStatus::Codes::kUnsupportedConfig)));
    }
    std::move(received_get_supported_configs_cb)
        .Run(supported_configs_to_reply_with, kDecoderTypeToReplyWith);
    task_environment_.RunUntilIdle();
    if (!Mock::VerifyAndClearExpectations(endpoints->service())) {
      return nullptr;
    }
    if (!Mock::VerifyAndClearExpectations(&init_cb)) {
      return nullptr;
    }
    if (!config_is_supported) {
      // When |config| is not supported, there are no further expectations.
      return nullptr;
    }
    if (!received_initialize_cb) {
      ADD_FAILURE() << "Did not receive a valid InitializeCallback";
      return nullptr;
    }

    // Now we'll reply to the Initialize() call on the service which should
    // propagate all the way to the |init_cb|, i.e., the Initialize() reply
    // callback passed to the MojoStableVideoDecoder.
    const DecoderStatus kDecoderStatusToReplyWith = DecoderStatus::Codes::kOk;
    constexpr bool kNeedsBitstreamConversionToReplyWith = true;
    constexpr int32_t kMaxDecodeRequestsToReplyWith = 123;
    constexpr bool kNeedsTranscryptionToReplyWith = false;
    EXPECT_CALL(init_cb, Run(kDecoderStatusToReplyWith)).WillOnce([&] {
      EXPECT_EQ(endpoints->client()->NeedsBitstreamConversion(),
                kNeedsBitstreamConversionToReplyWith);
      EXPECT_EQ(endpoints->client()->GetMaxDecodeRequests(),
                kMaxDecodeRequestsToReplyWith);
    });
    std::move(received_initialize_cb)
        .Run(kDecoderStatusToReplyWith, kNeedsBitstreamConversionToReplyWith,
             kMaxDecodeRequestsToReplyWith, kDecoderTypeToReplyWith,
             kNeedsTranscryptionToReplyWith);
    task_environment_.RunUntilIdle();
    if (!Mock::VerifyAndClearExpectations(&init_cb)) {
      return nullptr;
    }

    // Return non-nullptr only if all initialization expectations were met.
    if (HasFailure()) {
      return nullptr;
    }
    return endpoints;
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
};

TEST_F(MojoStableVideoDecoderTest,
       InitializeWithSupportedConfigConstructsStableVideoDecoder) {
  const VideoDecoderConfig config = CreateValidSupportedVideoDecoderConfig();
  std::unique_ptr<TestEndpoints> endpoints =
      CreateAndInitializeMojoStableVideoDecoder(config);
  ASSERT_TRUE(endpoints);
}

TEST_F(MojoStableVideoDecoderTest,
       InitializeWithUnsupportedConfigDoesNotConstructStableVideoDecoder) {
  const VideoDecoderConfig config = CreateValidUnsupportedVideoDecoderConfig();
  std::unique_ptr<TestEndpoints> endpoints =
      CreateAndInitializeMojoStableVideoDecoder(config);
  ASSERT_FALSE(endpoints);
}

TEST_F(MojoStableVideoDecoderTest,
       TwoInitializationsWithSupportedConfigsConstructStableVideoDecoderOnce) {
  // This does the first initialization.
  const VideoDecoderConfig first_config =
      CreateValidSupportedVideoDecoderConfig();
  std::unique_ptr<TestEndpoints> endpoints =
      CreateAndInitializeMojoStableVideoDecoder(first_config);
  ASSERT_TRUE(endpoints);

  // Let's now do the second initialization.
  const VideoDecoderConfig second_config =
      CreateValidSupportedVideoDecoderConfig();
  constexpr bool kLowDelayToInitializeWith = true;

  stable::mojom::StableVideoDecoder::InitializeCallback received_initialize_cb;
  EXPECT_CALL(*endpoints->service(),
              Initialize(_, kLowDelayToInitializeWith, _, _))
      .WillOnce(WithArgs<0, 3>(
          [&](const VideoDecoderConfig& received_config,
              stable::mojom::StableVideoDecoder::InitializeCallback callback) {
            EXPECT_TRUE(received_config.Matches(second_config));
            received_initialize_cb = std::move(callback);
          }));

  StrictMock<base::MockOnceCallback<void(DecoderStatus)>> init_cb;
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MojoStableVideoDecoder::Initialize,
                                base::Unretained(endpoints->client()),
                                second_config, kLowDelayToInitializeWith,
                                /*cdm_context=*/nullptr, init_cb.Get(),
                                endpoints->client_output_cb()->Get(),
                                endpoints->client_waiting_cb()->Get()));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(endpoints->service()));
  ASSERT_TRUE(received_initialize_cb);

  // Now we'll reply to the Initialize() call on the service which should
  // propagate all the way to the |init_cb|, i.e., the Initialize() reply
  // callback passed to the MojoStableVideoDecoder.
  const DecoderStatus kDecoderStatusToReplyWith = DecoderStatus::Codes::kOk;
  constexpr bool kNeedsBitstreamConversionToReplyWith = false;
  constexpr int32_t kMaxDecodeRequestsToReplyWith = 456;
  constexpr VideoDecoderType kDecoderTypeToReplyWith = VideoDecoderType::kVaapi;
  constexpr bool kNeedsTranscryptionToReplyWith = false;
  EXPECT_CALL(init_cb, Run(kDecoderStatusToReplyWith)).WillOnce([&] {
    EXPECT_EQ(endpoints->client()->NeedsBitstreamConversion(),
              kNeedsBitstreamConversionToReplyWith);
    EXPECT_EQ(endpoints->client()->GetMaxDecodeRequests(),
              kMaxDecodeRequestsToReplyWith);
  });
  std::move(received_initialize_cb)
      .Run(kDecoderStatusToReplyWith, kNeedsBitstreamConversionToReplyWith,
           kMaxDecodeRequestsToReplyWith, kDecoderTypeToReplyWith,
           kNeedsTranscryptionToReplyWith);
  task_environment_.RunUntilIdle();
}

TEST_F(MojoStableVideoDecoderTest, Decode) {
  const VideoDecoderConfig config = CreateValidSupportedVideoDecoderConfig();
  std::unique_ptr<TestEndpoints> endpoints =
      CreateAndInitializeMojoStableVideoDecoder(config);
  ASSERT_TRUE(endpoints);

  constexpr uint8_t kEncodedData[] = {1, 2, 3};
  constexpr base::TimeDelta kTimestamp = base::Milliseconds(128u);
  constexpr base::TimeDelta kDuration = base::Milliseconds(16u);
  constexpr base::TimeDelta kFrontDiscardPadding = base::Milliseconds(2u);
  constexpr base::TimeDelta kBackDiscardPadding = base::Milliseconds(5u);
  constexpr bool kIsKeyFrame = true;
  constexpr uint64_t kSecureHandle = 42;
  scoped_refptr<DecoderBuffer> decoder_buffer_to_send =
      DecoderBuffer::CopyFrom(kEncodedData, std::size(kEncodedData));
  ASSERT_TRUE(decoder_buffer_to_send);
  decoder_buffer_to_send->set_timestamp(kTimestamp);
  decoder_buffer_to_send->set_duration(kDuration);
  decoder_buffer_to_send->set_discard_padding(
      std::make_pair(kFrontDiscardPadding, kBackDiscardPadding));
  decoder_buffer_to_send->set_is_key_frame(kIsKeyFrame);
  decoder_buffer_to_send->WritableSideData().secure_handle = kSecureHandle;

  // First, we'll call MojoStableVideoDecoder::Decode() and store both the
  // DecoderBuffer (without the encoded data) and the Decode() reply callback as
  // seen by the service.
  mojom::DecoderBufferPtr received_mojo_decoder_buffer;
  StrictMock<base::MockOnceCallback<void(DecoderStatus)>> decode_cb_to_send;
  stable::mojom::StableVideoDecoder::DecodeCallback received_decode_cb;
  EXPECT_CALL(*endpoints->service(), Decode(_, _))
      .WillOnce(
          [&](const scoped_refptr<DecoderBuffer>& buffer,
              stable::mojom::StableVideoDecoder::DecodeCallback callback) {
            ASSERT_TRUE(buffer);
            received_mojo_decoder_buffer = mojom::DecoderBuffer::From(*buffer);
            ASSERT_TRUE(received_mojo_decoder_buffer);
            received_decode_cb = std::move(callback);
          });
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MojoStableVideoDecoder::Decode,
                     base::Unretained(endpoints->client()),
                     decoder_buffer_to_send, decode_cb_to_send.Get()));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(endpoints->service()));

  // Next, we'll retrieve the encoded data and ensure it matches what we sent.
  scoped_refptr<DecoderBuffer> received_decoder_buffer_with_data;
  endpoints->service()->mojo_decoder_buffer_reader()->ReadDecoderBuffer(
      std::move(received_mojo_decoder_buffer),
      base::BindOnce(
          [](scoped_refptr<DecoderBuffer>* dst_buffer,
             scoped_refptr<DecoderBuffer> buffer) {
            *dst_buffer = std::move(buffer);
          },
          &received_decoder_buffer_with_data));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(received_decoder_buffer_with_data);
  // We want to check that the |received_decoder_buffer_with_data| matches the
  // |decoder_buffer_to_send| except for the timestamp: that's because the
  // OOPVideoDecoder sends DecoderBuffers to the service with fake timestamps.
  // Hence, before calling MatchesForTesting(), let's restore the real
  // timestamp.
  ASSERT_FALSE(received_decoder_buffer_with_data->end_of_stream());
  EXPECT_NE(received_decoder_buffer_with_data->timestamp(), kTimestamp);
  received_decoder_buffer_with_data->set_timestamp(kTimestamp);
  EXPECT_TRUE(received_decoder_buffer_with_data->MatchesForTesting(
      *decoder_buffer_to_send));

  // Finally, we'll pretend that the service replies to the Decode() request.
  // This reply should be received as a call to the callback passed to
  // MojoStableVideoDecoder::Decode().
  const DecoderStatus kDecoderStatusToReplyWith = DecoderStatus::Codes::kOk;
  EXPECT_CALL(decode_cb_to_send, Run(kDecoderStatusToReplyWith))
      .WillOnce([&]() {
        EXPECT_TRUE(media_task_runner_->RunsTasksInCurrentSequence());
      });
  std::move(received_decode_cb).Run(kDecoderStatusToReplyWith);
  task_environment_.RunUntilIdle();

  // Note: the VideoDecoder::Decode() API takes a scoped_refptr<DecoderBuffer>
  // instead of a scoped_refptr<const DecoderBuffer>, so in theory, the
  // implementation can change the DecoderBuffer in unexpected ways. The
  // OOPVideoDecoder does change the DecoderBuffer internally, but it should
  // restore it to its original state before returning from Decode(). The
  // following expectations test that.
  EXPECT_EQ(decoder_buffer_to_send->timestamp(), kTimestamp);
  EXPECT_EQ(decoder_buffer_to_send->duration(), kDuration);
  EXPECT_EQ(decoder_buffer_to_send->discard_padding().first,
            kFrontDiscardPadding);
  EXPECT_EQ(decoder_buffer_to_send->discard_padding().second,
            kBackDiscardPadding);
  EXPECT_EQ(decoder_buffer_to_send->is_key_frame(), kIsKeyFrame);
  ASSERT_EQ(decoder_buffer_to_send->size(), std::size(kEncodedData));
  EXPECT_EQ(base::make_span(decoder_buffer_to_send->data(),
                            decoder_buffer_to_send->size()),
            base::make_span(kEncodedData, std::size(kEncodedData)));
  ASSERT_TRUE(decoder_buffer_to_send->side_data().has_value());
  EXPECT_EQ(decoder_buffer_to_send->side_data()->secure_handle, kSecureHandle);
}

TEST_F(MojoStableVideoDecoderTest, Reset) {
  const VideoDecoderConfig config = CreateValidSupportedVideoDecoderConfig();
  std::unique_ptr<TestEndpoints> endpoints =
      CreateAndInitializeMojoStableVideoDecoder(config);
  ASSERT_TRUE(endpoints);

  // First, we'll call MojoStableVideoDecoder::Reset() and store the Reset()
  // reply callback as seen by the service to call it later.
  StrictMock<base::MockOnceCallback<void()>> reset_cb_to_send;
  stable::mojom::StableVideoDecoder::ResetCallback received_reset_cb;
  EXPECT_CALL(*endpoints->service(), Reset(_))
      .WillOnce([&](stable::mojom::StableVideoDecoder::ResetCallback callback) {
        received_reset_cb = std::move(callback);
      });
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MojoStableVideoDecoder::Reset,
                                base::Unretained(endpoints->client()),
                                reset_cb_to_send.Get()));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(endpoints->service()));

  // Now we can pretend that the service replies to the Reset() reply callback
  // which should get propagated all the way to the |reset_cb_to_send|.
  EXPECT_CALL(reset_cb_to_send, Run()).WillOnce([&]() {
    EXPECT_TRUE(media_task_runner_->RunsTasksInCurrentSequence());
  });
  std::move(received_reset_cb).Run();
  task_environment_.RunUntilIdle();
}

}  // namespace media
