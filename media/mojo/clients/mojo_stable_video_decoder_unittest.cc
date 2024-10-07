// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/mojo/clients/mojo_stable_video_decoder.h"

#include <sys/mman.h>

#include "base/posix/eintr_wrapper.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
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
using testing::Matcher;
using testing::Mock;
using testing::Return;
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

// Creates a NV12 stable::mojom::VideoFrame with the given dimensions. The
// VideoFrame is backed by FDs that look like dma-bufs (they actually just point
// to memfd shared memory).
stable::mojom::VideoFramePtr CreateTestNV12DecodedFrame(
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  stable::mojom::VideoFramePtr mojo_frame = stable::mojom::VideoFrame::New();
  CHECK(mojo_frame);

  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::NATIVE_PIXMAP;
  gmb_handle.id = gfx::GpuMemoryBufferId(1);

  const size_t y_plane_stride = base::checked_cast<size_t>(coded_size.width());
  const size_t y_plane_size =
      y_plane_stride * base::checked_cast<size_t>(coded_size.height());
  const size_t uv_plane_stride = 2 * ((y_plane_stride + 1) / 2);
  const size_t uv_plane_size =
      uv_plane_stride *
      ((base::checked_cast<size_t>(coded_size.height()) + 1) / 2);

  auto y_fd = base::ScopedFD(memfd_create("nv12_dummy_buffer", 0));
  if (!y_fd.is_valid()) {
    return nullptr;
  }
  if (HANDLE_EINTR(ftruncate(y_fd.get(), y_plane_size + uv_plane_size)) < 0) {
    return nullptr;
  }
  auto uv_fd = base::ScopedFD(HANDLE_EINTR(dup(y_fd.get())));
  if (!uv_fd.is_valid()) {
    return nullptr;
  }

  gfx::NativePixmapPlane y_plane;
  y_plane.stride = base::checked_cast<uint32_t>(y_plane_stride);
  y_plane.offset = 0;
  y_plane.size = base::strict_cast<uint64_t>(y_plane_size);
  y_plane.fd = std::move(y_fd);
  gmb_handle.native_pixmap_handle.planes.push_back(std::move(y_plane));

  gfx::NativePixmapPlane uv_plane;
  uv_plane.stride = base::checked_cast<uint32_t>(uv_plane_stride);
  uv_plane.offset = base::checked_cast<uint64_t>(y_plane_size);
  uv_plane.size = base::checked_cast<uint64_t>(uv_plane_size);
  uv_plane.fd = std::move(uv_fd);
  gmb_handle.native_pixmap_handle.planes.push_back(std::move(uv_plane));

  mojo_frame->format = PIXEL_FORMAT_NV12;
  mojo_frame->coded_size = coded_size;
  mojo_frame->visible_rect = visible_rect;
  mojo_frame->natural_size = natural_size;
  mojo_frame->timestamp = timestamp;
  mojo_frame->metadata.protected_video = false;
  mojo_frame->metadata.hw_protected = false;
  mojo_frame->metadata.needs_detiling = false;
  mojo_frame->gpu_memory_buffer_handle = std::move(gmb_handle);

  return mojo_frame;
}

// IsValidSharedImageInfoForNV12Frame() is a custom matcher to help write
// expectations for CreateSharedImage() calls.
// AssertSharedImageInfoIsValidForNV12Frame() is a helper for the matcher.
void AssertSharedImageInfoIsValidForNV12Frame(
    const gpu::SharedImageInfo& actual,
    const gfx::Rect& expected_visible_rect,
    const gfx::ColorSpace& expected_color_space,
    bool* result) {
  *result = false;
  ASSERT_EQ(actual.meta.format, viz::MultiPlaneFormat::kNV12);
  ASSERT_TRUE(actual.meta.format.PrefersExternalSampler());
  ASSERT_EQ(actual.meta.size, gfx::Size(expected_visible_rect.right(),
                                        expected_visible_rect.bottom()));
  ASSERT_EQ(actual.meta.color_space, expected_color_space);
  ASSERT_EQ(actual.meta.surface_origin, kTopLeft_GrSurfaceOrigin);
  ASSERT_EQ(actual.meta.alpha_type, kPremul_SkAlphaType);
  ASSERT_EQ(actual.meta.usage, gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                   gpu::SHARED_IMAGE_USAGE_SCANOUT);
  ASSERT_EQ(actual.debug_label, "MojoStableVideoDecoder");
  *result = true;
}
MATCHER_P2(IsValidSharedImageInfoForNV12Frame,
           expected_visible_rect,
           expected_color_space,
           "") {
  bool valid;
  AssertSharedImageInfoIsValidForNV12Frame(arg, expected_visible_rect,
                                           expected_color_space, &valid);
  return valid;
}

// IsValidNV12NativeGpuMemoryBufferHandle() is a custom matcher to help write
// expectations for CreateSharedImage() calls.
// AssertNV12NativeGpuMemoryBufferHandleIsValid() is a helper for the matcher.
void AssertNV12NativeGpuMemoryBufferHandleIsValid(
    const gfx::GpuMemoryBufferHandle& actual,
    const gfx::Size& coded_size,
    bool* result) {
  *result = false;
  ASSERT_EQ(actual.type, gfx::NATIVE_PIXMAP);
  ASSERT_EQ(actual.native_pixmap_handle.planes.size(), 2u);
  ASSERT_EQ(actual.native_pixmap_handle.planes[0].stride,
            base::checked_cast<uint32_t>(coded_size.width()));
  ASSERT_EQ(actual.native_pixmap_handle.planes[0].offset, 0u);
  ASSERT_EQ(actual.native_pixmap_handle.planes[0].size,
            base::checked_cast<uint64_t>(coded_size.GetArea()));
  ASSERT_TRUE(actual.native_pixmap_handle.planes[0].fd.is_valid());
  ASSERT_EQ(actual.native_pixmap_handle.planes[1].stride,
            base::checked_cast<uint32_t>(coded_size.width()));
  ASSERT_EQ(actual.native_pixmap_handle.planes[1].offset,
            base::checked_cast<uint64_t>(coded_size.GetArea()));
  ASSERT_EQ(actual.native_pixmap_handle.planes[1].size,
            base::checked_cast<uint64_t>(coded_size.GetArea() / 2));
  ASSERT_TRUE(actual.native_pixmap_handle.planes[1].fd.is_valid());
  *result = true;
}
MATCHER_P(IsValidNV12NativeGpuMemoryBufferHandle, coded_size, "") {
  bool valid;
  AssertNV12NativeGpuMemoryBufferHandleIsValid(arg, coded_size, &valid);
  return valid;
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

  stable::mojom::VideoDecoderClient* video_decoder_client_remote() const {
    return video_decoder_client_remote_.get();
  }

  StrictMock<MockVideoFrameHandleReleaser>* mock_video_frame_handle_releaser()
      const {
    return mock_video_frame_handle_releaser_.get();
  }

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

}  // namespace

// NOTE: This needs to be outside of an anonymous namespace to allow it to be
// friended by SharedImageInterface.
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
                                     gpu::SharedImageUsageSet usage));
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

namespace {

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

  void DestroyClientSide() {
    // The |client_| must be destroyed on the |media_task_runner_|, but
    // DestroyClientSide() runs on the the main test thread. Therefore, we must
    // post a task to destroy the |client_|. However, *|client_| has raw
    // pointers to *|client_media_log_| and *|gpu_factories_|. In order to
    // prevent those pointers from becoming dangling while waiting for the
    // destroy task to run, we also post a task to the |media_task_runner_| to
    // destroy the |client_media_log_| and the |gpu_factories_|. The
    // *|gpu_factories_| has a raw pointer to *|sii_|, so for the same reason,
    // we post a task to the |media_task_runner_| to ensure
    // *|sii_| outlives *|gpu_factories_|.
    if (client_) {
      media_task_runner_->DeleteSoon(FROM_HERE, std::move(client_));
    }
    if (client_media_log_) {
      media_task_runner_->DeleteSoon(FROM_HERE, std::move(client_media_log_));
    }
    if (gpu_factories_) {
      media_task_runner_->DeleteSoon(FROM_HERE, std::move(gpu_factories_));
    }
    if (sii_) {
      media_task_runner_->PostTask(
          FROM_HERE, base::DoNothingWithBoundArgs(std::move(sii_)));
    }
  }

  ~TestEndpoints() { DestroyClientSide(); }

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

  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
};

TEST_F(MojoStableVideoDecoderTest,
       InitializeWithSupportedConfigConstructsStableVideoDecoder) {
  const VideoDecoderConfig config = CreateValidSupportedVideoDecoderConfig();
  std::unique_ptr<TestEndpoints> endpoints =
      CreateAndInitializeMojoStableVideoDecoder(config);
  ASSERT_TRUE(endpoints);

  // No frames were output by the service, so the decode latency histogram
  // should be empty.
  histogram_tester_.ExpectTotalCount(
      kMojoStableVideoDecoderDecodeLatencyHistogram,
      /*expected_count=*/0u);
}

TEST_F(MojoStableVideoDecoderTest,
       InitializeWithUnsupportedConfigDoesNotConstructStableVideoDecoder) {
  const VideoDecoderConfig config = CreateValidUnsupportedVideoDecoderConfig();
  std::unique_ptr<TestEndpoints> endpoints =
      CreateAndInitializeMojoStableVideoDecoder(config);
  ASSERT_FALSE(endpoints);

  // No frames were output by the service, so the decode latency histogram
  // should be empty.
  histogram_tester_.ExpectTotalCount(
      kMojoStableVideoDecoderDecodeLatencyHistogram,
      /*expected_count=*/0u);
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

  // No frames were output by the service, so the decode latency histogram
  // should be empty.
  histogram_tester_.ExpectTotalCount(
      kMojoStableVideoDecoderDecodeLatencyHistogram,
      /*expected_count=*/0u);
}

// This test sends four frames to the service for decoding and expects to
// receive four decoded frames. The order and properties of these decoded frames
// plus the timing for releasing them ensures we exercise some of the difficult
// lifetime corner cases (more details inside the test).
TEST_F(MojoStableVideoDecoderTest, Decode) {
  const VideoDecoderConfig config = CreateValidSupportedVideoDecoderConfig();
  std::unique_ptr<TestEndpoints> endpoints =
      CreateAndInitializeMojoStableVideoDecoder(config);
  ASSERT_TRUE(endpoints);

  constexpr base::TimeDelta kRealTimestamps[] = {
      base::Milliseconds(128u),
      base::Milliseconds(144u),
      base::Milliseconds(160u),
      base::Milliseconds(176u),
  };
  base::TimeDelta received_fake_timestamps[std::size(kRealTimestamps)] = {};

  // First there's the Decode() portion of the test. This just sends a Decode()
  // request for each frame and waits for the decode callback for each request
  // to be called. In this portion, no decoded frames are sent by the service,
  // but this creates enough state in the OOPVideoDecoder so that decoded frames
  // are accepted in a later portion of the test.
  for (size_t i = 0; i < std::size(kRealTimestamps); i++) {
    constexpr uint8_t kEncodedData[] = {1, 2, 3};
    const base::TimeDelta kTimestamp = kRealTimestamps[i];
    constexpr base::TimeDelta kDuration = base::Milliseconds(16u);
    constexpr base::TimeDelta kFrontDiscardPadding = base::Milliseconds(2u);
    constexpr base::TimeDelta kBackDiscardPadding = base::Milliseconds(5u);
    constexpr bool kIsKeyFrame = true;
    constexpr uint64_t kSecureHandle = 42;

    scoped_refptr<DecoderBuffer> decoder_buffer_to_send =
        DecoderBuffer::CopyFrom(kEncodedData);
    ASSERT_TRUE(decoder_buffer_to_send);
    decoder_buffer_to_send->set_timestamp(kTimestamp);
    decoder_buffer_to_send->set_duration(kDuration);
    decoder_buffer_to_send->set_discard_padding(
        std::make_pair(kFrontDiscardPadding, kBackDiscardPadding));
    decoder_buffer_to_send->set_is_key_frame(kIsKeyFrame);
    decoder_buffer_to_send->WritableSideData().secure_handle = kSecureHandle;

    // First, we'll call MojoStableVideoDecoder::Decode() and store both the
    // DecoderBuffer (without the encoded data) and the Decode() reply callback
    // as seen by the service.
    mojom::DecoderBufferPtr received_mojo_decoder_buffer;
    StrictMock<base::MockOnceCallback<void(DecoderStatus)>> decode_cb_to_send;
    stable::mojom::StableVideoDecoder::DecodeCallback received_decode_cb;
    EXPECT_CALL(*endpoints->service(), Decode(_, _))
        .WillOnce(
            [&](const scoped_refptr<DecoderBuffer>& buffer,
                stable::mojom::StableVideoDecoder::DecodeCallback callback) {
              ASSERT_TRUE(buffer);
              received_mojo_decoder_buffer =
                  mojom::DecoderBuffer::From(*buffer);
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
    received_fake_timestamps[i] =
        received_decoder_buffer_with_data->timestamp();
    received_decoder_buffer_with_data->set_timestamp(kTimestamp);
    EXPECT_TRUE(received_decoder_buffer_with_data->MatchesForTesting(
        *decoder_buffer_to_send));

    // Next, we'll pretend that the service replies to the Decode() request.
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
    EXPECT_EQ(decoder_buffer_to_send->side_data()->secure_handle,
              kSecureHandle);
  }

  // At this point, no decoded frames have been output by the service, so the
  // decode latency histogram should have nothing yet.
  histogram_tester_.ExpectTotalCount(
      kMojoStableVideoDecoderDecodeLatencyHistogram,
      /*expected_count=*/0u);

  // Now on to the second portion of the test: the service will start outputting
  // decoded frames to the client. Let's suppose the service sends the first
  // decoded frame. When the MojoStableVideoDecoder receives it, it should
  // create a SharedImage, wrap it in a gpu::Mailbox-backed VideoFrame, and
  // output it through the output callback. We'll hold on to the output
  // VideoFrame as |received_decoded_video_frame_1| to release it later.
  constexpr gfx::Size kDecodedFrame1CodedSize(1280, 720);
  constexpr gfx::Rect kDecodedFrame1VisibleRect(10, 10, 1000, 700);
  constexpr gfx::Size kDecodedFrame1NaturalSize(2000, 800);
  constexpr bool kCanReadWithoutStallingAfterFrame1 = true;
  const base::UnguessableToken kDecodedFrame1ReleaseToken =
      base::UnguessableToken::Create();
  const gpu::Mailbox kDecodedFrame1Mailbox = gpu::Mailbox::Generate();
  const gpu::SyncToken kDecodedFrame1SharedImageSyncToken(
      /*namespace_id=*/gpu::GPU_IO,
      /*command_buffer_id=*/gpu::CommandBufferId::FromUnsafeValue(1u),
      /*release_count=*/5u);
  stable::mojom::VideoFramePtr decoded_video_frame_to_send_1 =
      CreateTestNV12DecodedFrame(
          kDecodedFrame1CodedSize, kDecodedFrame1VisibleRect,
          kDecodedFrame1NaturalSize, received_fake_timestamps[0]);
  ASSERT_TRUE(decoded_video_frame_to_send_1);
  scoped_refptr<VideoFrame> received_decoded_video_frame_1;
  bool received_can_read_without_stalling_after_frame_1;
  {
    InSequence sequence;
    // Note: the Matcher<gfx::GpuMemoryBufferHandle> is needed to disambiguate
    // among all the CreateSharedImage() overloads.
    EXPECT_CALL(
        *endpoints->shared_image_interface(),
        CreateSharedImage(IsValidSharedImageInfoForNV12Frame(
                              kDecodedFrame1VisibleRect, gfx::ColorSpace()),
                          Matcher<gfx::GpuMemoryBufferHandle>(
                              IsValidNV12NativeGpuMemoryBufferHandle(
                                  kDecodedFrame1CodedSize))))
        .WillOnce([&](const gpu::SharedImageInfo& si_info,
                      gfx::GpuMemoryBufferHandle buffer_handle) {
          return base::MakeRefCounted<gpu::ClientSharedImage>(
              kDecodedFrame1Mailbox, si_info.meta, gpu::SyncToken(),
              gpu::GpuMemoryBufferHandleInfo(
                  std::move(buffer_handle), si_info.meta.format,
                  kDecodedFrame1CodedSize, gfx::BufferUsage::SCANOUT_VDA_WRITE),
              endpoints->shared_image_interface()->holder());
        });
    EXPECT_CALL(*endpoints->shared_image_interface(), GenUnverifiedSyncToken())
        .WillOnce(Return(kDecodedFrame1SharedImageSyncToken));
    EXPECT_CALL(*endpoints->client_output_cb(), Run(_))
        .WillOnce([&](scoped_refptr<VideoFrame> video_frame) {
          received_decoded_video_frame_1 = std::move(video_frame);
          received_can_read_without_stalling_after_frame_1 =
              endpoints->client()->CanReadWithoutStalling();
        });
  }
  endpoints->service()->video_decoder_client_remote()->OnVideoFrameDecoded(
      decoded_video_frame_to_send_1.Clone(), kCanReadWithoutStallingAfterFrame1,
      kDecodedFrame1ReleaseToken);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(
      Mock::VerifyAndClearExpectations(endpoints->shared_image_interface()));
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(endpoints->client_output_cb()));
  ASSERT_TRUE(received_decoded_video_frame_1);
  EXPECT_EQ(received_decoded_video_frame_1->format(), PIXEL_FORMAT_NV12);
  // Note: the output VideoFrame's coded size should match the SharedImage's
  // size, not the coded size of the frame received from the service.
  EXPECT_EQ(received_decoded_video_frame_1->coded_size(),
            gfx::Size(kDecodedFrame1VisibleRect.right(),
                      kDecodedFrame1VisibleRect.bottom()));
  EXPECT_EQ(received_decoded_video_frame_1->visible_rect(),
            kDecodedFrame1VisibleRect);
  EXPECT_EQ(received_decoded_video_frame_1->natural_size(),
            kDecodedFrame1NaturalSize);
  EXPECT_EQ(received_decoded_video_frame_1->ColorSpace(), gfx::ColorSpace());
  ASSERT_TRUE(received_decoded_video_frame_1->HasSharedImage());
  EXPECT_EQ(received_decoded_video_frame_1->shared_image()->mailbox(),
            kDecodedFrame1Mailbox);
  EXPECT_EQ(received_decoded_video_frame_1->acquire_sync_token(),
            kDecodedFrame1SharedImageSyncToken);
  EXPECT_TRUE(
      received_decoded_video_frame_1->metadata().read_lock_fences_enabled);
  EXPECT_EQ(received_can_read_without_stalling_after_frame_1,
            kCanReadWithoutStallingAfterFrame1);
  histogram_tester_.ExpectTotalCount(
      kMojoStableVideoDecoderDecodeLatencyHistogram,
      /*expected_count=*/1u);

  // Now the service will send the second decoded frame. This decoded frame has
  // the same underlying dma-buf as the first frame. Furthermore, the coded size
  // and the visible rectangle are such that the SharedImage can be re-used.
  // We'll hold on to the output VideoFrame as |received_decoded_video_frame_2|
  // to release it later.
  constexpr gfx::Size kDecodedFrame2CodedSize = kDecodedFrame1CodedSize;
  constexpr gfx::Rect kDecodedFrame2VisibleRect(20, 30, 990, 680);
  constexpr gfx::Size kDecodedFrame2NaturalSize(3000, 900);
  constexpr bool kCanReadWithoutStallingAfterFrame2 = false;
  const base::UnguessableToken kDecodedFrame2ReleaseToken =
      base::UnguessableToken::Create();
  const gpu::SyncToken kDecodedFrame2SharedImageSyncToken(
      /*namespace_id=*/gpu::GPU_IO,
      /*command_buffer_id=*/gpu::CommandBufferId::FromUnsafeValue(1u),
      /*release_count=*/8u);
  stable::mojom::VideoFramePtr decoded_video_frame_to_send_2 =
      decoded_video_frame_to_send_1.Clone();
  ASSERT_TRUE(decoded_video_frame_to_send_2);
  decoded_video_frame_to_send_2->coded_size = kDecodedFrame2CodedSize;
  decoded_video_frame_to_send_2->visible_rect = kDecodedFrame2VisibleRect;
  decoded_video_frame_to_send_2->natural_size = kDecodedFrame2NaturalSize;
  decoded_video_frame_to_send_2->timestamp = received_fake_timestamps[1];
  scoped_refptr<VideoFrame> received_decoded_video_frame_2;
  bool received_can_read_without_stalling_after_frame_2;
  {
    InSequence sequence;
    EXPECT_CALL(*endpoints->shared_image_interface(),
                UpdateSharedImage(gpu::SyncToken(), kDecodedFrame1Mailbox));
    EXPECT_CALL(*endpoints->shared_image_interface(), GenUnverifiedSyncToken())
        .WillOnce(Return(kDecodedFrame2SharedImageSyncToken));
    EXPECT_CALL(*endpoints->client_output_cb(), Run(_))
        .WillOnce([&](scoped_refptr<VideoFrame> video_frame) {
          received_decoded_video_frame_2 = std::move(video_frame);
          received_can_read_without_stalling_after_frame_2 =
              endpoints->client()->CanReadWithoutStalling();
        });
  }
  endpoints->service()->video_decoder_client_remote()->OnVideoFrameDecoded(
      decoded_video_frame_to_send_2.Clone(), kCanReadWithoutStallingAfterFrame2,
      kDecodedFrame2ReleaseToken);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(
      Mock::VerifyAndClearExpectations(endpoints->shared_image_interface()));
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(endpoints->client_output_cb()));
  ASSERT_TRUE(received_decoded_video_frame_2);
  EXPECT_EQ(received_decoded_video_frame_2->format(), PIXEL_FORMAT_NV12);
  // Note: the output VideoFrame's coded size should match the SharedImage's
  // size, not the coded size of the frame received from the service, and the
  // SharedImage for this frame is the same as for the first frame.
  EXPECT_EQ(received_decoded_video_frame_2->coded_size(),
            gfx::Size(kDecodedFrame1VisibleRect.right(),
                      kDecodedFrame1VisibleRect.bottom()));
  EXPECT_EQ(received_decoded_video_frame_2->visible_rect(),
            kDecodedFrame2VisibleRect);
  EXPECT_EQ(received_decoded_video_frame_2->natural_size(),
            kDecodedFrame2NaturalSize);
  EXPECT_EQ(received_decoded_video_frame_2->ColorSpace(), gfx::ColorSpace());
  ASSERT_TRUE(received_decoded_video_frame_2->HasSharedImage());
  EXPECT_EQ(received_decoded_video_frame_2->shared_image()->mailbox(),
            kDecodedFrame1Mailbox);
  EXPECT_EQ(received_decoded_video_frame_2->acquire_sync_token(),
            kDecodedFrame2SharedImageSyncToken);
  EXPECT_TRUE(
      received_decoded_video_frame_2->metadata().read_lock_fences_enabled);
  EXPECT_EQ(received_can_read_without_stalling_after_frame_2,
            kCanReadWithoutStallingAfterFrame2);
  histogram_tester_.ExpectTotalCount(
      kMojoStableVideoDecoderDecodeLatencyHistogram,
      /*expected_count=*/2u);

  // Now, the service sends the third decoded frame. This frame has the same
  // underlying dma-buf as the previous frames but a different color space. This
  // means that the SharedImage for the previous two frames can't be re-used.
  // We'll hold on to the output VideoFrame as |received_decoded_video_frame_3|
  // to release it later.
  constexpr gfx::ColorSpace kDecodedFrame3ColorSpace =
      gfx::ColorSpace::CreateREC709();
  constexpr bool kCanReadWithoutStallingAfterFrame3 = true;
  const base::UnguessableToken kDecodedFrame3ReleaseToken =
      base::UnguessableToken::Create();
  const gpu::Mailbox kDecodedFrame3Mailbox = gpu::Mailbox::Generate();
  const gpu::SyncToken kDecodedFrame3SharedImageSyncToken(
      /*namespace_id=*/gpu::GPU_IO,
      /*command_buffer_id=*/gpu::CommandBufferId::FromUnsafeValue(1u),
      /*release_count=*/10u);
  stable::mojom::VideoFramePtr decoded_video_frame_to_send_3 =
      decoded_video_frame_to_send_2.Clone();
  ASSERT_TRUE(decoded_video_frame_to_send_3);
  decoded_video_frame_to_send_3->timestamp = received_fake_timestamps[2];
  decoded_video_frame_to_send_3->color_space = kDecodedFrame3ColorSpace;
  scoped_refptr<VideoFrame> received_decoded_video_frame_3;
  bool received_can_read_without_stalling_after_frame_3;
  {
    InSequence sequence;
    EXPECT_CALL(*endpoints->shared_image_interface(),
                CreateSharedImage(
                    IsValidSharedImageInfoForNV12Frame(
                        kDecodedFrame2VisibleRect, kDecodedFrame3ColorSpace),
                    Matcher<gfx::GpuMemoryBufferHandle>(
                        IsValidNV12NativeGpuMemoryBufferHandle(
                            kDecodedFrame1CodedSize))))
        .WillOnce([&](const gpu::SharedImageInfo& si_info,
                      gfx::GpuMemoryBufferHandle buffer_handle) {
          return base::MakeRefCounted<gpu::ClientSharedImage>(
              kDecodedFrame3Mailbox, si_info.meta, gpu::SyncToken(),
              gpu::GpuMemoryBufferHandleInfo(
                  std::move(buffer_handle), si_info.meta.format,
                  kDecodedFrame2CodedSize, gfx::BufferUsage::SCANOUT_VDA_WRITE),
              endpoints->shared_image_interface()->holder());
        });
    EXPECT_CALL(*endpoints->shared_image_interface(), GenUnverifiedSyncToken())
        .WillOnce(Return(kDecodedFrame3SharedImageSyncToken));
    EXPECT_CALL(*endpoints->client_output_cb(), Run(_))
        .WillOnce([&](scoped_refptr<VideoFrame> video_frame) {
          received_decoded_video_frame_3 = std::move(video_frame);
          received_can_read_without_stalling_after_frame_3 =
              endpoints->client()->CanReadWithoutStalling();
        });
  }
  endpoints->service()->video_decoder_client_remote()->OnVideoFrameDecoded(
      std::move(decoded_video_frame_to_send_3),
      kCanReadWithoutStallingAfterFrame3, kDecodedFrame3ReleaseToken);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(
      Mock::VerifyAndClearExpectations(endpoints->shared_image_interface()));
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(endpoints->client_output_cb()));
  ASSERT_TRUE(received_decoded_video_frame_3);
  EXPECT_EQ(received_decoded_video_frame_3->format(), PIXEL_FORMAT_NV12);
  // Note: the output VideoFrame's coded size should match the SharedImage's
  // size, not the coded size of the frame received from the service.
  EXPECT_EQ(received_decoded_video_frame_3->coded_size(),
            gfx::Size(kDecodedFrame2VisibleRect.right(),
                      kDecodedFrame2VisibleRect.bottom()));
  EXPECT_EQ(received_decoded_video_frame_3->visible_rect(),
            kDecodedFrame2VisibleRect);
  EXPECT_EQ(received_decoded_video_frame_3->natural_size(),
            kDecodedFrame2NaturalSize);
  EXPECT_EQ(received_decoded_video_frame_3->ColorSpace(),
            kDecodedFrame3ColorSpace);
  ASSERT_TRUE(received_decoded_video_frame_3->HasSharedImage());
  EXPECT_EQ(received_decoded_video_frame_3->shared_image()->mailbox(),
            kDecodedFrame3Mailbox);
  EXPECT_EQ(received_decoded_video_frame_3->acquire_sync_token(),
            kDecodedFrame3SharedImageSyncToken);
  EXPECT_TRUE(
      received_decoded_video_frame_3->metadata().read_lock_fences_enabled);
  EXPECT_EQ(received_can_read_without_stalling_after_frame_3,
            kCanReadWithoutStallingAfterFrame3);
  histogram_tester_.ExpectTotalCount(
      kMojoStableVideoDecoderDecodeLatencyHistogram,
      /*expected_count=*/3u);

  // Now the service will send the fourth decoded frame. This frame changes the
  // coded size, so the OOPVideoDecoder should forget all previous buffers. That
  // means that the only reference remaining to the second SharedImage created
  // above will now be held by the |received_decoded_video_frame_3| (this is
  // important for later). Also, the MojoStableVideoDecoder should create a new
  // SharedImage. We'll hold on to the output VideoFrame as
  // |received_decoded_video_frame_4| to release it later.
  constexpr gfx::Size kDecodedFrame4CodedSize(640, 368);
  constexpr gfx::Rect kDecodedFrame4VisibleRect(640, 360);
  constexpr gfx::Size kDecodedFrame4NaturalSize(640, 360);
  constexpr bool kCanReadWithoutStallingAfterFrame4 = false;
  const base::UnguessableToken kDecodedFrame4ReleaseToken =
      base::UnguessableToken::Create();
  const gpu::Mailbox kDecodedFrame4Mailbox = gpu::Mailbox::Generate();
  const gpu::SyncToken kDecodedFrame4SharedImageSyncToken(
      /*namespace_id=*/gpu::GPU_IO,
      /*command_buffer_id=*/gpu::CommandBufferId::FromUnsafeValue(1u),
      /*release_count=*/15u);
  stable::mojom::VideoFramePtr decoded_video_frame_to_send_4 =
      CreateTestNV12DecodedFrame(
          kDecodedFrame4CodedSize, kDecodedFrame4VisibleRect,
          kDecodedFrame4NaturalSize, received_fake_timestamps[3]);
  ASSERT_TRUE(decoded_video_frame_to_send_4);
  scoped_refptr<VideoFrame> received_decoded_video_frame_4;
  bool received_can_read_without_stalling_after_frame_4;
  {
    InSequence sequence;
    EXPECT_CALL(
        *endpoints->shared_image_interface(),
        CreateSharedImage(IsValidSharedImageInfoForNV12Frame(
                              kDecodedFrame4VisibleRect, gfx::ColorSpace()),
                          Matcher<gfx::GpuMemoryBufferHandle>(
                              IsValidNV12NativeGpuMemoryBufferHandle(
                                  kDecodedFrame4CodedSize))))
        .WillOnce([&](const gpu::SharedImageInfo& si_info,
                      gfx::GpuMemoryBufferHandle buffer_handle) {
          return base::MakeRefCounted<gpu::ClientSharedImage>(
              kDecodedFrame4Mailbox, si_info.meta, gpu::SyncToken(),
              gpu::GpuMemoryBufferHandleInfo(
                  std::move(buffer_handle), si_info.meta.format,
                  kDecodedFrame4CodedSize, gfx::BufferUsage::SCANOUT_VDA_WRITE),
              endpoints->shared_image_interface()->holder());
        });
    EXPECT_CALL(*endpoints->shared_image_interface(), GenUnverifiedSyncToken())
        .WillOnce(Return(kDecodedFrame4SharedImageSyncToken));
    EXPECT_CALL(*endpoints->client_output_cb(), Run(_))
        .WillOnce([&](scoped_refptr<VideoFrame> video_frame) {
          received_decoded_video_frame_4 = std::move(video_frame);
          received_can_read_without_stalling_after_frame_4 =
              endpoints->client()->CanReadWithoutStalling();
        });
  }
  endpoints->service()->video_decoder_client_remote()->OnVideoFrameDecoded(
      std::move(decoded_video_frame_to_send_4),
      kCanReadWithoutStallingAfterFrame4, kDecodedFrame4ReleaseToken);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(
      Mock::VerifyAndClearExpectations(endpoints->shared_image_interface()));
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(endpoints->client_output_cb()));
  ASSERT_TRUE(received_decoded_video_frame_4);
  EXPECT_EQ(received_decoded_video_frame_4->format(), PIXEL_FORMAT_NV12);
  // Note: the output VideoFrame's coded size should match the SharedImage's
  // size, not the coded size of the frame received from the service.
  EXPECT_EQ(received_decoded_video_frame_4->coded_size(),
            gfx::Size(kDecodedFrame4VisibleRect.right(),
                      kDecodedFrame4VisibleRect.bottom()));
  EXPECT_EQ(received_decoded_video_frame_4->visible_rect(),
            kDecodedFrame4VisibleRect);
  EXPECT_EQ(received_decoded_video_frame_4->natural_size(),
            kDecodedFrame4NaturalSize);
  EXPECT_EQ(received_decoded_video_frame_4->ColorSpace(), gfx::ColorSpace());
  ASSERT_TRUE(received_decoded_video_frame_4->HasSharedImage());
  EXPECT_EQ(received_decoded_video_frame_4->shared_image()->mailbox(),
            kDecodedFrame4Mailbox);
  EXPECT_EQ(received_decoded_video_frame_4->acquire_sync_token(),
            kDecodedFrame4SharedImageSyncToken);
  EXPECT_TRUE(
      received_decoded_video_frame_4->metadata().read_lock_fences_enabled);
  EXPECT_EQ(received_can_read_without_stalling_after_frame_4,
            kCanReadWithoutStallingAfterFrame4);
  histogram_tester_.ExpectTotalCount(
      kMojoStableVideoDecoderDecodeLatencyHistogram,
      /*expected_count=*/4u);

  // Now the third portion of the test: we'll release the decoded frames. Let's
  // release the first decoded frame. Since the SharedImage is still being used
  // by the second decoded frame (which we're not going to release yet), we
  // don't expect a call to DestroySharedImage() at this point. We only expect
  // the service to be notified that the first decoded frame is no longer
  // in use.
  EXPECT_CALL(*endpoints->service()->mock_video_frame_handle_releaser(),
              ReleaseVideoFrame(kDecodedFrame1ReleaseToken));
  received_decoded_video_frame_1.reset();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(
      endpoints->service()->mock_video_frame_handle_releaser()));

  // Now we're going to release the second decoded frame. This frame should be
  // the only one holding a reference to the first SharedImage, so releasing it
  // should result in a call to DestroySharedImage(). Note that there is no
  // expected ordering between the call to DestroySharedImage() and the
  // ReleaseVideoFrame() call. That's because frames should not be released
  // until the client is completely done with them (because
  // read_lock_fences_enabled is set to true). Therefore, it doesn't matter if
  // the service recycles the underlying buffer before the corresponding
  // SharedImage is destroyed.
  EXPECT_CALL(*endpoints->shared_image_interface(),
              DestroySharedImage(gpu::SyncToken(), kDecodedFrame1Mailbox));
  EXPECT_CALL(*endpoints->service()->mock_video_frame_handle_releaser(),
              ReleaseVideoFrame(kDecodedFrame2ReleaseToken));
  received_decoded_video_frame_2.reset();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(
      Mock::VerifyAndClearExpectations(endpoints->shared_image_interface()));
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(
      endpoints->service()->mock_video_frame_handle_releaser()));

  // Next, let's release the third decoded frame. As described above, this frame
  // should be the only one holding a reference to the second SharedImage, so
  // releasing it should result in a call to DestroySharedImage().
  EXPECT_CALL(*endpoints->shared_image_interface(),
              DestroySharedImage(gpu::SyncToken(), kDecodedFrame3Mailbox));
  EXPECT_CALL(*endpoints->service()->mock_video_frame_handle_releaser(),
              ReleaseVideoFrame(kDecodedFrame3ReleaseToken));
  received_decoded_video_frame_3.reset();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(
      Mock::VerifyAndClearExpectations(endpoints->shared_image_interface()));
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(
      endpoints->service()->mock_video_frame_handle_releaser()));

  // Finally, we'll do something tricky: first, we'll destroy the client side of
  // things. In particular, that involves destroying the MojoStableVideoDecoder
  // and releasing a reference to the MockSharedImageInterface. Then, we'll
  // release |received_decoded_video_frame_4|. This should still cause
  // DestroySharedImage() to be called because the
  // |received_decoded_video_frame_4| indirectly should hold a reference to the
  // MockSharedImageInterface. The service, however, should not get notified
  // that the frame was released because the client no longer exists.
  StrictMock<MockSharedImageInterface>* const sii =
      endpoints->shared_image_interface();
  endpoints->DestroyClientSide();
  task_environment_.RunUntilIdle();
  EXPECT_CALL(*sii,
              DestroySharedImage(gpu::SyncToken(), kDecodedFrame4Mailbox));
  received_decoded_video_frame_4.reset();
  task_environment_.RunUntilIdle();
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

  // No frames were output by the service, so the decode latency histogram
  // should be empty.
  histogram_tester_.ExpectTotalCount(
      kMojoStableVideoDecoderDecodeLatencyHistogram,
      /*expected_count=*/0u);
}

}  // namespace media
