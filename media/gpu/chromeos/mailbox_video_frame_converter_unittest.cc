// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/mailbox_video_frame_converter.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <array>
#include <optional>

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
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

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
                                              viz::SharedImageFormat format) {
  std::optional<VideoPixelFormat> video_pixel_format =
      SharedImageFormatToVideoPixelFormat(format);
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
  MOCK_METHOD0(GenUnverifiedSyncToken, gpu::SyncToken());
  MOCK_METHOD0(GenVerifiedSyncToken, gpu::SyncToken());
  MOCK_METHOD1(VerifySyncToken, void(gpu::SyncToken& sync_token));
  MOCK_METHOD1(CanVerifySyncToken, bool(const gpu::SyncToken& sync_token));
  MOCK_METHOD0(VerifyFlush, void());
  MOCK_METHOD1(WaitSyncToken, void(const gpu::SyncToken& sync_token));
  MOCK_METHOD0(GetCapabilities, const gpu::SharedImageCapabilities&());

  MOCK_CONST_METHOD0(IsLost, bool());
  MOCK_METHOD1(AddGpuChannelLostObserver,
               bool(gpu::GpuChannelLostObserver* observer));
  MOCK_METHOD1(RemoveGpuChannelLostObserver,
               void(gpu::GpuChannelLostObserver* observer));
  MOCK_METHOD0(CrashGpuProcessForTesting, void());

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
    return verified_for_mock_gpu_delegate && verified_for_mock_output_cb &&
           verified_for_mock_frame_destruction_cbs;
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
    converter_ = MailboxVideoFrameConverter::Create(
        std::move(mock_shared_image_interface));
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
        CreatePixmapHandle(kCodedSize, viz::MultiPlaneFormat::kNV12);
    // Setting some default usage in order to get a mappable shared image.
    const auto si_usage = gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY |
                          gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;

    auto shared_image_size =
        needs_detiling ? kCodedSize : gfx::Size(kVisibleRect.size());
    auto shared_image = test_sii_->CreateSharedImage(
        {viz::MultiPlaneFormat::kNV12, shared_image_size, gfx::ColorSpace(),
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
          .WillOnce([&mailboxes_seen_by_gpu_delegate, i, this](
                        const gpu::SharedImageInfo& si_info,
                        gfx::GpuMemoryBufferHandle buffer_handle) {
            auto shared_image = base::MakeRefCounted<gpu::ClientSharedImage>(
                gpu::Mailbox::Generate(), si_info, gpu::SyncToken(),
                mock_shared_image_interface_->holder(), buffer_handle.type);
            mailboxes_seen_by_gpu_delegate[i] = shared_image->mailbox();
            return shared_image;
          });
      EXPECT_CALL(*mock_shared_image_interface_, VerifySyncToken(_)).Times(1);
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
    EXPECT_EQ(converted_frame->metadata().read_lock_fences_enabled, true);
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
  // |mappable_frame|, and the corresponding shared image should be destroyed
  // with it.
  for (size_t i = 0; i < std::size(mappable_frames); i++) {
    EXPECT_CALL(*mock_shared_image_interface_,
                DestroySharedImage(
                    /*sync_token=*/_,
                    /*mailbox=*/Matcher<const gpu::Mailbox&>(
                        mailboxes_seen_by_gpu_delegate[i])))
        .Times(1);
    EXPECT_CALL(*mock_frame_destruction_cbs_[i], Run()).Times(1);
    converted_frames[i].reset();
    ASSERT_TRUE(RunTasksAndVerifyAndClearExpectations());
  }
}

// This test verifies reusing of the shared_images for identical frames.
TEST_P(MailboxVideoFrameConverterWithUnwrappedFramesTest,
       CanConvertIdenticalFramesAndThenHandleTheirRelease) {
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
  auto gmb_handle =
      CreatePixmapHandle(kCodedSize, viz::MultiPlaneFormat::kNV12);
  // Setting some default usage in order to get a mappable shared image.
  const auto si_usage = gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY |
                        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;

  auto shared_image_size =
      needs_detiling ? kCodedSize : gfx::Size(kVisibleRect.size());
  auto shared_image = test_sii_->CreateSharedImage(
      {viz::MultiPlaneFormat::kNV12, shared_image_size, gfx::ColorSpace(),
       gpu::SharedImageUsageSet(si_usage),
       "MailboxVideoFrameConverterWithUnwrappedFramesTest"},
      gpu::kNullSurfaceHandle, gfx::BufferUsage::GPU_READ,
      std::move(gmb_handle));

  // Create 1 frame that will be wrapped multiple times.
  auto video_frame = VideoFrame::WrapMappableSharedImage(
      std::move(shared_image), test_sii_->GenVerifiedSyncToken(),
      base::NullCallback(), kVisibleRect, kNaturalSize, base::TimeDelta());
  ASSERT_TRUE(video_frame);
  video_frame->metadata().needs_detiling = needs_detiling;
  scoped_refptr<FrameResource> original_frame =
      VideoFrameResource::Create(video_frame);
  original_frame->metadata().tracking_token = base::UnguessableToken::Create();

  // Provide a way to retreive the original frame.
  FrameResourceConverter::GetOriginalFrameCB get_original_cb =
      base::BindRepeating(
          // Using dangling raw_ptr in order to not prolong the lifetime of
          // `original_frame` beyond `video_frame` and `original_frame` due to
          // binding arg.
          [](raw_ptr<FrameResource, DisableDanglingPtrDetection> original,
             const base::UnguessableToken&) { return original.get(); },
          base::UnsafeDangling(original_frame.get()));
  converter_->set_get_original_frame_cb(get_original_cb);

  for (size_t i = 0; i < std::size(mappable_frames); i++) {
    scoped_refptr<FrameResource> mappable_frame =
        original_frame->CreateWrappingFrame();
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
      if (i == 0) {
        EXPECT_CALL(
            *mock_shared_image_interface_,
            CreateSharedImage(
                /*si_info=*/IsValidSharedImageInfo(
                    shared_image_format,
                    needs_detiling ? kCodedSize : kVisibleRect.size()),
                /*buffer_handle=*/Matcher<gfx::GpuMemoryBufferHandle>(_)))
            .WillOnce([&mailboxes_seen_by_gpu_delegate, i, this](
                          const gpu::SharedImageInfo& si_info,
                          gfx::GpuMemoryBufferHandle buffer_handle) {
              auto shared_image = base::MakeRefCounted<gpu::ClientSharedImage>(
                  gpu::Mailbox::Generate(), si_info, gpu::SyncToken(),
                  mock_shared_image_interface_->holder(), buffer_handle.type);
              mailboxes_seen_by_gpu_delegate[i] = shared_image->mailbox();
              return shared_image;
            });
        EXPECT_CALL(*mock_shared_image_interface_, VerifySyncToken(_)).Times(1);
      } else {
        EXPECT_CALL(*mock_shared_image_interface_, UpdateSharedImage(
                                                       /*sync_token=*/_,
                                                       /*mailbox=*/_))
            .WillOnce([&mailboxes_seen_by_gpu_delegate, i](
                          const gpu::SyncToken& sync_token,
                          const gpu::Mailbox& mailbox) {
              mailboxes_seen_by_gpu_delegate[i] = mailbox;
            });
        EXPECT_CALL(*mock_shared_image_interface_, GenVerifiedSyncToken())
            .Times(1);
      }
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
    EXPECT_EQ(converted_frame->metadata().read_lock_fences_enabled, true);
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

  EXPECT_EQ(mailboxes_seen_by_gpu_delegate[0],
            mailboxes_seen_by_gpu_delegate[1]);

  // Now let's simulate that the client releases each of the |converted_frames|.
  // |mappable_frame|.
  for (size_t i = 0; i < std::size(mappable_frames); i++) {
    EXPECT_CALL(*mock_frame_destruction_cbs_[i], Run()).Times(1);
    converted_frames[i].reset();
    ASSERT_TRUE(RunTasksAndVerifyAndClearExpectations());
  }

  // Destorying the `original_frame` destroys the shared image.
  EXPECT_CALL(*mock_shared_image_interface_,
              DestroySharedImage(
                  /*sync_token=*/_,
                  /*mailbox=*/Matcher<const gpu::Mailbox&>(
                      mailboxes_seen_by_gpu_delegate[0])))
      .Times(1);
  video_frame.reset();
  original_frame.reset();
  ASSERT_TRUE(RunTasksAndVerifyAndClearExpectations());
}

// This test verifies that shared_image recreates if color space changes.
TEST_P(MailboxVideoFrameConverterWithUnwrappedFramesTest,
       CanRecreateSharedImagesAndThenHandleTheirRelease) {
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
  auto gmb_handle =
      CreatePixmapHandle(kCodedSize, viz::MultiPlaneFormat::kNV12);
  // Setting some default usage in order to get a mappable shared image.
  const auto si_usage = gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY |
                        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;

  auto shared_image_size =
      needs_detiling ? kCodedSize : gfx::Size(kVisibleRect.size());
  auto shared_image = test_sii_->CreateSharedImage(
      {viz::MultiPlaneFormat::kNV12, shared_image_size, gfx::ColorSpace(),
       gpu::SharedImageUsageSet(si_usage),
       "MailboxVideoFrameConverterWithUnwrappedFramesTest"},
      gpu::kNullSurfaceHandle, gfx::BufferUsage::GPU_READ,
      std::move(gmb_handle));

  // Create 1 frame that will be wrapped multiple times.
  auto video_frame = VideoFrame::WrapMappableSharedImage(
      std::move(shared_image), test_sii_->GenVerifiedSyncToken(),
      base::NullCallback(), kVisibleRect, kNaturalSize, base::TimeDelta());
  ASSERT_TRUE(video_frame);
  video_frame->metadata().needs_detiling = needs_detiling;
  scoped_refptr<FrameResource> original_frame =
      VideoFrameResource::Create(video_frame);
  original_frame->metadata().tracking_token = base::UnguessableToken::Create();

  // Provide a way to retreive the original frame.
  FrameResourceConverter::GetOriginalFrameCB get_original_cb =
      base::BindRepeating(
          // Using dangling raw_ptr in order to not prolong the lifetime of
          // `original_frame` beyond `video_frame` and `original_frame` due to
          // binding arg.
          [](raw_ptr<FrameResource, DisableDanglingPtrDetection> original,
             const base::UnguessableToken&) { return original.get(); },
          base::UnsafeDangling(original_frame.get()));
  converter_->set_get_original_frame_cb(get_original_cb);

  for (size_t i = 0; i < std::size(mappable_frames); i++) {
    scoped_refptr<FrameResource> mappable_frame =
        original_frame->CreateWrappingFrame();
    ASSERT_TRUE(mappable_frame);
    // Change color space so shared_image needs to be recreated.
    if (i != 0) {
      mappable_frame->set_color_space(gfx::ColorSpace::CreateHDR10());
    }
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
          .WillOnce([&mailboxes_seen_by_gpu_delegate, i, this](
                        const gpu::SharedImageInfo& si_info,
                        gfx::GpuMemoryBufferHandle buffer_handle) {
            auto shared_image = base::MakeRefCounted<gpu::ClientSharedImage>(
                gpu::Mailbox::Generate(), si_info, gpu::SyncToken(),
                mock_shared_image_interface_->holder(), buffer_handle.type);
            mailboxes_seen_by_gpu_delegate[i] = shared_image->mailbox();
            return shared_image;
          });
      EXPECT_CALL(*mock_shared_image_interface_, VerifySyncToken(_)).Times(1);
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
    EXPECT_EQ(converted_frame->metadata().read_lock_fences_enabled, true);
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
  // |mappable_frame|.
  for (size_t i = 0; i < std::size(mappable_frames); i++) {
    EXPECT_CALL(*mock_frame_destruction_cbs_[i], Run()).Times(1);
    converted_frames[i].reset();
    ASSERT_TRUE(RunTasksAndVerifyAndClearExpectations());
  }

  // Destorying the `original_frame` destroys both shared images.
  EXPECT_CALL(*mock_shared_image_interface_,
              DestroySharedImage(
                  /*sync_token=*/_,
                  /*mailbox=*/Matcher<const gpu::Mailbox&>(
                      mailboxes_seen_by_gpu_delegate[0])))
      .Times(1);
  EXPECT_CALL(*mock_shared_image_interface_,
              DestroySharedImage(
                  /*sync_token=*/_,
                  /*mailbox=*/Matcher<const gpu::Mailbox&>(
                      mailboxes_seen_by_gpu_delegate[1])))
      .Times(1);
  video_frame.reset();
  original_frame.reset();
  ASSERT_TRUE(RunTasksAndVerifyAndClearExpectations());
}

INSTANTIATE_TEST_SUITE_P(,
                         MailboxVideoFrameConverterWithUnwrappedFramesTest,
                         testing::Values(false, true),
                         MailboxVideoFrameConverterWithUnwrappedFramesTest::
                             PrintToStringParamName());

}  // namespace media
