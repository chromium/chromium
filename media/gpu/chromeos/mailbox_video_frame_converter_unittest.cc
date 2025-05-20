// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/mailbox_video_frame_converter.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <array>
#include <optional>

#include "base/atomic_sequence_num.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "media/base/format_utils.h"
#include "media/base/media_switches.h"
#include "media/base/simple_sync_token_client.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/video_frame_resource.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/gpu_memory_buffer.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::Expectation;
using ::testing::InSequence;
using ::testing::Matcher;
using ::testing::Mock;
using ::testing::Property;
using ::testing::Ref;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

namespace {

base::ScopedFD GetDummyFD() {
  base::ScopedFD fd(open("/dev/zero", O_RDWR));
  DCHECK(fd.is_valid());
  return fd;
}

gfx::GpuMemoryBufferHandle CreatePixmapHandle(const gfx::Size& size,
                                              gfx::BufferFormat format) {
  std::optional<VideoPixelFormat> video_pixel_format =
      GfxBufferFormatToVideoPixelFormat(format);
  CHECK(video_pixel_format);

  auto data = std::vector<uint8_t>(
      VideoFrame::AllocationSize(*video_pixel_format, size));

  gfx::NativePixmapHandle native_pixmap_handle;
  for (size_t i = 0; i < VideoFrame::NumPlanes(*video_pixel_format); i++) {
    const gfx::Size plane_size_in_bytes =
        VideoFrame::PlaneSize(*video_pixel_format, i, size);
    native_pixmap_handle.planes.emplace_back(plane_size_in_bytes.width(), 0,
                                             plane_size_in_bytes.GetArea(),
                                             GetDummyFD());
  }
  native_pixmap_handle.modifier = gfx::NativePixmapHandle::kNoModifier;
  gfx::GpuMemoryBufferHandle handle(std::move(native_pixmap_handle));
  static base::AtomicSequenceNumber buffer_id_generator;
  handle.id = gfx::GpuMemoryBufferId(buffer_id_generator.GetNext());
  return handle;
}

// IsValidSharedImageInfo() is a custom matcher to help write expectations for
// CreateSharedImage() calls.
// AssertSharedImageInfoIsValid() is a helper for the matcher.
void AssertSharedImageInfoIsValid(const gpu::SharedImageInfo& actual,
                                  const viz::SharedImageFormat& expected_format,
                                  const gfx::Size& expected_size,
                                  bool* result) {
  *result = false;
  ASSERT_EQ(actual.meta.format, expected_format);
  ASSERT_EQ(actual.meta.size, expected_size);
  ASSERT_EQ(actual.debug_label, "MailboxVideoFrameConverter");
  *result = true;
}
MATCHER_P2(IsValidSharedImageInfo, expected_format, expected_size, "") {
  bool valid;
  AssertSharedImageInfoIsValid(arg, expected_format, expected_size, &valid);
  return valid;
}

}  // namespace

class MockSharedImageInterface : public gpu::SharedImageInterface {
 public:
  MockSharedImageInterface() = default;

  // gpu::SharedImageInterface implementation.
  MOCK_METHOD3(CreateSharedImage,
               scoped_refptr<gpu::ClientSharedImage>(
                   const gpu::SharedImageInfo& si_info,
                   gpu::SurfaceHandle surface_handle,
                   std::optional<gpu::SharedImagePoolId> pool_id));
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
  MOCK_METHOD4(
      CreateSharedImageForMLTensor,
      scoped_refptr<gpu::ClientSharedImage>(std::string debug_label,
                                            viz::SharedImageFormat format,
                                            const gfx::Size& size,
                                            gpu::SharedImageUsageSet usage));
  MOCK_METHOD1(CreateSharedImageForSoftwareCompositor,
               scoped_refptr<gpu::ClientSharedImage>(
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
                   gpu::ExportedSharedImage exported_shared_image));
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
  MOCK_METHOD1(GetNativePixmap,
               scoped_refptr<gfx::NativePixmap>(const gpu::Mailbox& mailbox));
  MOCK_METHOD0(GetCapabilities, const gpu::SharedImageCapabilities&());

  scoped_refptr<gpu::SharedImageInterfaceHolder> holder() const {
    return holder_;
  }

 protected:
  ~MockSharedImageInterface() override = default;
};

class MailboxVideoFrameConverterTest : public ::testing::Test {
 public:
  MailboxVideoFrameConverterTest() = default;
  MailboxVideoFrameConverterTest(const MailboxVideoFrameConverterTest&) =
      delete;
  MailboxVideoFrameConverterTest& operator=(
      const MailboxVideoFrameConverterTest&) = delete;
  ~MailboxVideoFrameConverterTest() override = default;

  virtual bool RunTasksAndVerifyAndClearExpectations() {
    task_environment_.RunUntilIdle();
    const bool verified_for_mock_gpu_delegate =
        Mock::VerifyAndClearExpectations(mock_shared_image_interface_);
    const bool verified_for_mock_output_cb =
        Mock::VerifyAndClearExpectations(&mock_output_cb_);
    bool verified_for_mock_frame_destruction_cbs = true;
    for (auto& cb : mock_frame_destruction_cbs_) {
      verified_for_mock_frame_destruction_cbs =
          Mock::VerifyAndClearExpectations(cb.get()) &&
          verified_for_mock_frame_destruction_cbs;
    }
    bool verified_for_mock_release_cb =
        Mock::VerifyAndClearExpectations(&mock_release_cb_);
    return verified_for_mock_gpu_delegate && verified_for_mock_output_cb &&
           verified_for_mock_frame_destruction_cbs &&
           verified_for_mock_release_cb;
  }

  void SetUp() override {
    // Before starting the test, make sure nothing unexpected happened at
    // construction time.
    ASSERT_TRUE(RunTasksAndVerifyAndClearExpectations());
  }

  void TearDown() override {
    mock_shared_image_interface_ = nullptr;
    converter_.reset();
    RunTasksAndVerifyAndClearExpectations();
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  raw_ptr<StrictMock<MockSharedImageInterface>> mock_shared_image_interface_;

  // Note: we intentionally make all the mock callbacks members of the test
  // fixture instead of limiting their lifetime to each test. The reason is that
  // we don't want to crash if there are pending tasks at the end of a test that
  // use these callbacks.
  StrictMock<base::MockRepeatingCallback<void(scoped_refptr<VideoFrame>)>>
      mock_output_cb_;
  // |mock_frame_destruction_cbs_| are callbacks added as destruction observers
  // to VideoFrames.
  std::vector<std::unique_ptr<StrictMock<base::MockOnceCallback<void()>>>>
      mock_frame_destruction_cbs_;
  StrictMock<
      base::MockRepeatingCallback<bool(scoped_refptr<FrameResource> frame,
                                       const gpu::SyncToken& sync_token)>>
      mock_release_cb_;

  std::unique_ptr<FrameResourceConverter> converter_;
};

class MailboxVideoFrameConverterWithUnwrappedFramesTest
    : public MailboxVideoFrameConverterTest,
      public ::testing::WithParamInterface<bool> {
 public:
  MailboxVideoFrameConverterWithUnwrappedFramesTest() {
    test_sii_ = base::MakeRefCounted<gpu::TestSharedImageInterface>();
    auto mock_shared_image_interface =
        base::MakeRefCounted<StrictMock<MockSharedImageInterface>>();
    mock_shared_image_interface_ = mock_shared_image_interface.get();
    converter_ =
        base::WrapUnique<FrameResourceConverter>(new MailboxVideoFrameConverter(
            std::move(mock_shared_image_interface), mock_release_cb_.Get()));
    converter_->Initialize(
        /*parent_task_runner=*/base::SingleThreadTaskRunner::
            GetCurrentDefault(),
        mock_output_cb_.Get());
  }

  MailboxVideoFrameConverterWithUnwrappedFramesTest(
      const MailboxVideoFrameConverterWithUnwrappedFramesTest&) = delete;
  MailboxVideoFrameConverterWithUnwrappedFramesTest& operator=(
      const MailboxVideoFrameConverterWithUnwrappedFramesTest&) = delete;
  ~MailboxVideoFrameConverterWithUnwrappedFramesTest() override = default;

  scoped_refptr<gpu::TestSharedImageInterface> test_sii_;
  gpu::SharedImageCapabilities si_cap_;

  struct PrintToStringParamName {
    template <class ParamType>
    std::string operator()(
        const testing::TestParamInfo<ParamType>& info) const {
      return base::StringPrintf("%s", (info.param) ? "Tiled" : "Linear");
    }
  };
};

// This test verifies a typical path for a MailboxVideoFrameConverter configured
// to receive frames that don't need to be unwrapped. This mode is used for
// out-of-process video decoding. The MailboxVideoFrameConverter is fed two
// Mappable frames and after it outputs the two corresponding Mailbox
// frames, we verify that the MailboxVideoFrameConverter can handle those frames
// being released by the client. Note that in this mode, SharedImages are not
// expected to be re-used because in out-of-process video decoding, we always
// create a new frame with a unique ID for every Mappable frame we receive
// from the video decoder process.
TEST_P(MailboxVideoFrameConverterWithUnwrappedFramesTest,
       CanConvertMultipleFramesAndThenHandleTheirRelease) {
  constexpr gfx::Size kCodedSize(640, 368);
  constexpr gfx::Rect kVisibleRect(600, 300);
  constexpr gfx::Size kNaturalSize(1200, 600);
  const bool needs_detiling = GetParam();

  // |mappable_frames| are the frames backed by Mappable shared images. In real
  // usage, those are the frames that the hardware decoder decodes to and (when
  // the MailboxVideoFrameConverter is configured to handle unwrapped frames)
  // that the OOPVideoDecoder receives from the remote video decoder process.
  // The OOPVideoDecoder transfers ownership of these frames to the
  // MailboxVideoFrameConverter, but we keep raw pointers around in order to use
  // them in test assertions.
  std::array<FrameResource*, 2> mappable_frames;

  // |mailboxes_seen_by_gpu_delegate| are the Mailboxes generated for each of
  // the |mappable_frames|. These Mailboxes are generated in the
  // MailboxVideoFrameConverter and supplied to the GpuDelegate to create the
  // SharedImage for the the VideoFrame.
  std::array<gpu::Mailbox, std::size(mappable_frames)>
      mailboxes_seen_by_gpu_delegate;

  for (size_t i = 0; i < std::size(mappable_frames); i++) {
    mock_frame_destruction_cbs_.emplace_back(
        std::make_unique<StrictMock<base::MockOnceCallback<void()>>>());
  }

  // |converted_frames| are the outputs of the MailboxVideoFrameConverter.
  std::array<scoped_refptr<VideoFrame>, std::size(mappable_frames)>
      converted_frames;

  // Let's now feed each of the |mappable_frames| to the
  // MailboxVideoFrameConverter and verify that the GpuDelegate gets used
  // correctly.
  for (size_t i = 0; i < std::size(mappable_frames); i++) {
    auto gmb_handle =
        CreatePixmapHandle(kCodedSize, gfx::BufferFormat::YUV_420_BIPLANAR);
    // Setting some default usage in order to get a mappable shared image.
    const auto si_usage = gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY |
                          gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;

    auto shared_image = test_sii_->CreateSharedImage(
        {viz::MultiPlaneFormat::kNV12, kCodedSize, gfx::ColorSpace(),
         gpu::SharedImageUsageSet(si_usage),
         "MailboxVideoFrameConverterWithUnwrappedFramesTest"},
        gpu::kNullSurfaceHandle, gfx::BufferUsage::GPU_READ,
        std::move(gmb_handle));

    auto video_frame = VideoFrame::WrapMappableSharedImage(
        std::move(shared_image), test_sii_->GenVerifiedSyncToken(),
        base::NullCallback(), kVisibleRect, kNaturalSize, base::TimeDelta());
    ASSERT_TRUE(video_frame);
    video_frame->metadata().needs_detiling = needs_detiling;
    scoped_refptr<FrameResource> mappable_frame =
        VideoFrameResource::Create(video_frame);
    ASSERT_TRUE(mappable_frame);
    mappable_frame->AddDestructionObserver(
        mock_frame_destruction_cbs_[i]->Get());
    mappable_frames[i] = mappable_frame.get();

    {
      InSequence sequence;
      viz::SharedImageFormat shared_image_format = viz::MultiPlaneFormat::kNV12;
      shared_image_format.SetPrefersExternalSampler();
      // Note: the Matcher<gfx::GpuMemoryBufferHandle> is needed to disambiguate
      // among all the CreateSharedImage() overloads.
      EXPECT_CALL(*mock_shared_image_interface_,
                  CreateSharedImage(
                      /*si_info=*/IsValidSharedImageInfo(
                          shared_image_format,
                          needs_detiling ? kCodedSize : kVisibleRect.size()),
                      /*buffer_handle=*/Matcher<gfx::GpuMemoryBufferHandle>(_)))
          .WillOnce([&mailboxes_seen_by_gpu_delegate, i](
                        const gpu::SharedImageInfo& si_info,
                        gfx::GpuMemoryBufferHandle buffer_handle) {
            auto shared_image = gpu::ClientSharedImage::CreateForTesting();
            mailboxes_seen_by_gpu_delegate[i] = shared_image->mailbox();
            return shared_image;
          });
      EXPECT_CALL(mock_output_cb_, Run(_))
          .WillOnce(SaveArg<0>(&converted_frames[i]));
    }

    EXPECT_CALL(*mock_shared_image_interface_, GetCapabilities())
        .WillRepeatedly(ReturnRef(si_cap_));
    // Note: after this, the MailboxVideoFrameConverter should have full
    // ownership of the *|mappable_frame|.
    converter_->ConvertFrame(std::move(mappable_frame));
    ASSERT_TRUE(RunTasksAndVerifyAndClearExpectations());
    ASSERT_TRUE(converted_frames[i]);
    scoped_refptr<const VideoFrame> converted_frame = converted_frames[i];
    EXPECT_EQ(converted_frame->storage_type(), VideoFrame::STORAGE_OPAQUE);
    EXPECT_EQ(converted_frame->format(), mappable_frames[i]->format());
    if (needs_detiling) {
      EXPECT_EQ(converted_frame->coded_size(), kCodedSize);
    } else {
      EXPECT_EQ(converted_frame->coded_size(),
                mappable_frames[i]->visible_rect().size());
    }
    EXPECT_EQ(converted_frame->visible_rect(),
              mappable_frames[i]->visible_rect());
    EXPECT_EQ(converted_frame->natural_size(),
              mappable_frames[i]->natural_size());
    ASSERT_TRUE(converted_frame->HasSharedImage());
    EXPECT_EQ(converted_frame->shared_image()->mailbox(),
              mailboxes_seen_by_gpu_delegate[i]);
  }

  EXPECT_NE(mailboxes_seen_by_gpu_delegate[0],
            mailboxes_seen_by_gpu_delegate[1]);

  // Now let's simulate that the client releases each of the |converted_frames|.
  // The GpuDelegate should be invoked to wait on the right SyncToken. Then,
  // since the MailboxVideoFrameConverter should be the sole owner of the
  // corresponding Mappable FrameResource, the SharedImage for that frame
  // should be destroyed and the destruction observer registered above for that
  // frame should be invoked.
  for (size_t i = 0; i < std::size(mappable_frames); i++) {
    const gpu::SyncToken release_sync_token(
        gpu::CommandBufferNamespace::GPU_IO,
        gpu::CommandBufferId::FromUnsafeValue(1u), /*release_count=*/(1u + i));
    Expectation wait_on_sync_token_and_release =
        EXPECT_CALL(mock_release_cb_,
                    Run(Property(&scoped_refptr<FrameResource>::get,
                                 mappable_frames[i]),
                        release_sync_token))
            .WillOnce(Return(true));
    EXPECT_CALL(*mock_frame_destruction_cbs_[i], Run())
        .After(wait_on_sync_token_and_release);
    SimpleSyncTokenClient sync_token_client(release_sync_token);
    converted_frames[i]->UpdateReleaseSyncToken(&sync_token_client);
    converted_frames[i].reset();
    ASSERT_TRUE(RunTasksAndVerifyAndClearExpectations());
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         MailboxVideoFrameConverterWithUnwrappedFramesTest,
                         testing::Values(false, true),
                         MailboxVideoFrameConverterWithUnwrappedFramesTest::
                             PrintToStringParamName());

}  // namespace media
