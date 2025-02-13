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
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/gpu_memory_buffer.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::Expectation;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Property;
using ::testing::Ref;
using ::testing::Return;
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

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::NATIVE_PIXMAP;

  static base::AtomicSequenceNumber buffer_id_generator;
  handle.id = gfx::GpuMemoryBufferId(buffer_id_generator.GetNext());

  for (size_t i = 0; i < VideoFrame::NumPlanes(*video_pixel_format); i++) {
    const gfx::Size plane_size_in_bytes =
        VideoFrame::PlaneSize(*video_pixel_format, i, size);
    handle.native_pixmap_handle.planes.emplace_back(
        plane_size_in_bytes.width(), 0, plane_size_in_bytes.GetArea(),
        GetDummyFD());
  }
  handle.native_pixmap_handle.modifier = gfx::NativePixmapHandle::kNoModifier;
  return handle;
}

class MockGpuDelegate : public MailboxVideoFrameConverter::GpuDelegate {
 public:
  MOCK_METHOD0(Initialize, bool());
  MOCK_METHOD0(GetCapabilities, std::optional<gpu::SharedImageCapabilities>());
  MOCK_METHOD5(
      CreateSharedImage,
      scoped_refptr<gpu::ClientSharedImage>(gfx::GpuMemoryBufferHandle handle,
                                            viz::SharedImageFormat format,
                                            const gfx::Size& size,
                                            const gfx::ColorSpace& color_space,
                                            gpu::SharedImageUsageSet usage));
  MOCK_METHOD1(UpdateSharedImage,
               std::optional<gpu::SyncToken>(const gpu::Mailbox& mailbox));
  MOCK_METHOD2(WaitOnSyncTokenAndReleaseFrame,
               bool(scoped_refptr<FrameResource> frame,
                    const gpu::SyncToken& sync_token));
};

}  // namespace

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
        Mock::VerifyAndClearExpectations(mock_gpu_delegate_);
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
    mock_gpu_delegate_ = nullptr;
    converter_.reset();
    RunTasksAndVerifyAndClearExpectations();
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  raw_ptr<StrictMock<MockGpuDelegate>> mock_gpu_delegate_;

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
    auto mock_gpu_delegate = std::make_unique<StrictMock<MockGpuDelegate>>();
    mock_gpu_delegate_ = mock_gpu_delegate.get();
    converter_ =
        base::WrapUnique<FrameResourceConverter>(new MailboxVideoFrameConverter(
            /*gpu_task_runner=*/base::ThreadPool::CreateSingleThreadTaskRunner(
                {}),
            std::move(mock_gpu_delegate)));
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
      EXPECT_CALL(*mock_gpu_delegate_, Initialize()).WillOnce(Return(true));
      viz::SharedImageFormat shared_image_format = viz::MultiPlaneFormat::kNV12;
      shared_image_format.SetPrefersExternalSampler();
      EXPECT_CALL(
          *mock_gpu_delegate_,
          CreateSharedImage(
              /*handle=*/_, shared_image_format,
              /*size=*/needs_detiling ? kCodedSize : kVisibleRect.size(),
              /*color_space=*/_,
              /*usage=*/_))
          .WillOnce([&mailboxes_seen_by_gpu_delegate, i]() {
            auto shared_image = gpu::ClientSharedImage::CreateForTesting();
            mailboxes_seen_by_gpu_delegate[i] = shared_image->mailbox();
            return shared_image;
          });
      EXPECT_CALL(mock_output_cb_, Run(_))
          .WillOnce(SaveArg<0>(&converted_frames[i]));
    }

    EXPECT_CALL(*mock_gpu_delegate_, GetCapabilities())
        .WillRepeatedly(Return(gpu::SharedImageCapabilities()));
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
        EXPECT_CALL(*mock_gpu_delegate_,
                    WaitOnSyncTokenAndReleaseFrame(
                        Property(&scoped_refptr<FrameResource>::get,
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
