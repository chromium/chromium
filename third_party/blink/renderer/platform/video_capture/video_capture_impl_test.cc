// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/video_capture/video_capture_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/token.h"
#include "build/build_config.h"
#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/scoped_handle.h"
#endif
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/ipc/common/surface_handle.h"
#include "media/base/video_util.h"
#include "media/capture/mojom/video_capture.mojom-blink.h"
#include "media/capture/mojom/video_capture_buffer.mojom-blink.h"
#include "media/capture/mojom/video_capture_types.mojom-blink.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::WithArgs;

namespace blink {

namespace {

// Counts UpdateSharedImage() calls so tests can assert that reusable capture
// slots are flagged as externally updated once per delivered frame.
class CountingSharedImageInterface : public gpu::TestSharedImageInterface {
 public:
  void UpdateSharedImage(const gpu::SyncToken& sync_token,
                         const gpu::Mailbox& mailbox) override {
    ++update_shared_image_count_;
    gpu::TestSharedImageInterface::UpdateSharedImage(sync_token, mailbox);
  }

  scoped_refptr<gpu::ClientSharedImage> ImportSharedImage(
      gpu::ExportedSharedImage exported_shared_image) override {
    ++import_shared_image_count_;
    return gpu::TestSharedImageInterface::ImportSharedImage(
        std::move(exported_shared_image));
  }

  int update_shared_image_count() const { return update_shared_image_count_; }
  int import_shared_image_count() const { return import_shared_image_count_; }

 private:
  ~CountingSharedImageInterface() override = default;

  int update_shared_image_count_ = 0;
  int import_shared_image_count_ = 0;
};

class TestingPlatformSupportWithGpuFactories
    : public IOTaskRunnerTestingPlatformSupport {
 public:
  TestingPlatformSupportWithGpuFactories();
  ~TestingPlatformSupportWithGpuFactories() override;
  media::GpuVideoAcceleratorFactories* GetGpuFactories() override;

  void SetGpuCapabilities(gpu::Capabilities* capabilities);
  void SetSharedImageCapabilities(
      const gpu::SharedImageCapabilities& capabilities);

  // The interface GetGpuFactories() hands out. Tests that feed shared images
  // into the capture path must create them here, so that their mailboxes are
  // known to the interface the code under test will use.
  CountingSharedImageInterface* shared_image_interface() { return sii_.get(); }

  // Hands out a different GpuVideoAcceleratorFactories from here on, which is
  // how VideoCaptureImpl detects that the GPU context was lost and
  // re-established. The SharedImageInterface is deliberately kept, so that
  // mailboxes created before the swap stay valid and the counters above span
  // both sides of it. Supports a single swap: the superseded instance is kept
  // alive because VideoCaptureImpl holds a raw_ptr to it until it is told
  // about the new one.
  void ReplaceGpuFactories();

 private:
  void ConfigureGpuFactories();

  scoped_refptr<CountingSharedImageInterface> sii_;
  std::unique_ptr<media::MockGpuVideoAcceleratorFactories>
      retired_gpu_factories_;
  std::unique_ptr<media::MockGpuVideoAcceleratorFactories> gpu_factories_;
  base::Thread media_thread_;
  raw_ptr<gpu::Capabilities> capabilities_ = nullptr;
};

TestingPlatformSupportWithGpuFactories::TestingPlatformSupportWithGpuFactories()
    : sii_(base::MakeRefCounted<CountingSharedImageInterface>()),
      gpu_factories_(new media::MockGpuVideoAcceleratorFactories(sii_.get())),
      media_thread_("TestingMediaThread") {
  media_thread_.Start();
  ConfigureGpuFactories();
}

void TestingPlatformSupportWithGpuFactories::ConfigureGpuFactories() {
  gpu_factories_->SetVideoFrameOutputFormat(
      media::GpuVideoAcceleratorFactories::OutputFormat::NV12);
  ON_CALL(*gpu_factories_, GetTaskRunner())
      .WillByDefault(testing::Return(media_thread_.task_runner()));
  ON_CALL(*gpu_factories_, ContextCapabilities()).WillByDefault([&]() {
    return capabilities_;
  });
}

void TestingPlatformSupportWithGpuFactories::ReplaceGpuFactories() {
  retired_gpu_factories_ = std::move(gpu_factories_);
  gpu_factories_ =
      std::make_unique<media::MockGpuVideoAcceleratorFactories>(sii_.get());
  ConfigureGpuFactories();
}

TestingPlatformSupportWithGpuFactories::
    ~TestingPlatformSupportWithGpuFactories() {
  media_thread_.Stop();
}

media::GpuVideoAcceleratorFactories*
TestingPlatformSupportWithGpuFactories::GetGpuFactories() {
  return gpu_factories_.get();
}

void TestingPlatformSupportWithGpuFactories::SetGpuCapabilities(
    gpu::Capabilities* capabilities) {
  capabilities_ = capabilities;
}

void TestingPlatformSupportWithGpuFactories::SetSharedImageCapabilities(
    const gpu::SharedImageCapabilities& shared_image_capabilities) {
  sii_->SetCapabilities(shared_image_capabilities);
}

}  // namespace

void RunEmptyFormatsCallback(
    media::mojom::blink::VideoCaptureHost::GetDeviceSupportedFormatsCallback&
        callback) {
  Vector<media::VideoCaptureFormat> formats;
  std::move(callback).Run(formats);
}

ACTION(DoNothing) {}

// Mock implementation of the Mojo Host service.
class MockMojoVideoCaptureHost : public media::mojom::blink::VideoCaptureHost {
 public:
  MockMojoVideoCaptureHost() : released_buffer_count_(0) {
    ON_CALL(*this, GetDeviceSupportedFormatsMock(_, _, _))
        .WillByDefault(WithArgs<2>(RunEmptyFormatsCallback));
    ON_CALL(*this, GetDeviceFormatsInUseMock(_, _, _))
        .WillByDefault(WithArgs<2>(RunEmptyFormatsCallback));
    ON_CALL(*this, ReleaseBuffer(_, _, _))
        .WillByDefault(InvokeWithoutArgs(
            this, &MockMojoVideoCaptureHost::increase_released_buffer_count));
  }
  MockMojoVideoCaptureHost(const MockMojoVideoCaptureHost&) = delete;
  MockMojoVideoCaptureHost& operator=(const MockMojoVideoCaptureHost&) = delete;

  // Start() can't be mocked directly due to move-only |observer|.
  void Start(const base::UnguessableToken& device_id,
             const base::UnguessableToken& session_id,
             const media::VideoCaptureParams& params,
             mojo::PendingRemote<media::mojom::blink::VideoCaptureObserver>
                 observer) override {
    DoStart(device_id, session_id, params);
  }
  MOCK_METHOD3(DoStart,
               void(const base::UnguessableToken&,
                    const base::UnguessableToken&,
                    const media::VideoCaptureParams&));
  MOCK_METHOD1(Stop, void(const base::UnguessableToken&));
  MOCK_METHOD1(Pause, void(const base::UnguessableToken&));
  MOCK_METHOD3(Resume,
               void(const base::UnguessableToken&,
                    const base::UnguessableToken&,
                    const media::VideoCaptureParams&));
  MOCK_METHOD1(RequestRefreshFrame, void(const base::UnguessableToken&));
  MOCK_METHOD3(ReleaseBuffer,
               void(const base::UnguessableToken&,
                    int32_t,
                    const media::VideoCaptureFeedback&));
  MOCK_METHOD3(GetDeviceSupportedFormatsMock,
               void(const base::UnguessableToken&,
                    const base::UnguessableToken&,
                    GetDeviceSupportedFormatsCallback&));
  MOCK_METHOD3(GetDeviceFormatsInUseMock,
               void(const base::UnguessableToken&,
                    const base::UnguessableToken&,
                    GetDeviceFormatsInUseCallback&));
  MOCK_METHOD2(OnFrameDropped,
               void(const base::UnguessableToken&,
                    media::VideoCaptureFrameDropReason));
  MOCK_METHOD2(OnLog, void(const base::UnguessableToken&, const String&));

  void GetDeviceSupportedFormats(
      const base::UnguessableToken& arg1,
      const base::UnguessableToken& arg2,
      GetDeviceSupportedFormatsCallback arg3) override {
    GetDeviceSupportedFormatsMock(arg1, arg2, arg3);
  }

  void GetDeviceFormatsInUse(const base::UnguessableToken& arg1,
                             const base::UnguessableToken& arg2,
                             GetDeviceFormatsInUseCallback arg3) override {
    GetDeviceFormatsInUseMock(arg1, arg2, arg3);
  }

  int released_buffer_count() const { return released_buffer_count_; }
  void increase_released_buffer_count() { released_buffer_count_++; }

 private:
  int released_buffer_count_;
};

// This class encapsulates a VideoCaptureImpl under test and the necessary
// accessory classes, namely:
// - a MockMojoVideoCaptureHost, mimicking the RendererHost;
// - a few callbacks that are bound when calling operations of VideoCaptureImpl
//  and on which we set expectations.
class VideoCaptureImplTest : public ::testing::Test {
 public:
  struct BufferDescription {
    BufferDescription(
        int buffer_id,
        gfx::Size size,
        media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_I420)
        : buffer_id(buffer_id),
          size(std::move(size)),
          pixel_format(pixel_format) {}

    media::mojom::blink::ReadyBufferPtr ToReadyBuffer(
        const base::TimeTicks now) const {
      media::mojom::blink::VideoFrameInfoPtr info =
          media::mojom::blink::VideoFrameInfo::New();
      media::VideoFrameMetadata metadata;
      metadata.reference_time = now;
      info->timestamp = now - base::TimeTicks();
      info->pixel_format = pixel_format;
      info->coded_size = size;
      info->visible_rect = gfx::Rect(size);
      info->natural_size = size;
      info->color_space = gfx::ColorSpace();
      info->metadata = metadata;
      info->is_premapped = is_premapped;
      return media::mojom::blink::ReadyBuffer::New(buffer_id, std::move(info));
    }

    int buffer_id;
    gfx::Size size;
    media::VideoPixelFormat pixel_format;
    bool is_premapped = false;
  };

  VideoCaptureImplTest()
      : video_capture_impl_(new VideoCaptureImpl(
            session_id_,
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            GetEmptyBrowserInterfaceBroker())) {
    params_small_.requested_format = media::VideoCaptureFormat(
        gfx::Size(176, 144), 30, media::PIXEL_FORMAT_I420);
    params_large_.requested_format = media::VideoCaptureFormat(
        gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);

    video_capture_impl_->SetVideoCaptureHostForTesting(
        &mock_video_capture_host_);

    ON_CALL(mock_video_capture_host_, DoStart(_, _, _)).WillByDefault([this]() {
      video_capture_impl_->OnStateChanged(
          media::mojom::blink::VideoCaptureResult::NewState(
              media::mojom::VideoCaptureState::STARTED));
    });

    platform_->SetGpuCapabilities(&fake_capabilities_);
  }

#if DCHECK_IS_ON()
  ~VideoCaptureImplTest() override { SetIsBeforeThreadCreatedForTest(); }
#endif

  VideoCaptureImplTest(const VideoCaptureImplTest&) = delete;
  VideoCaptureImplTest& operator=(const VideoCaptureImplTest&) = delete;

 protected:
  // These four mocks are used to create callbacks for the different operations.
  MOCK_METHOD2(OnFrameReady,
               void(scoped_refptr<media::VideoFrame>, base::TimeTicks));
  MOCK_METHOD1(OnFrameDropped, void(media::VideoCaptureFrameDropReason));
  MOCK_METHOD1(OnStateUpdate, void(VideoCaptureState));
  MOCK_METHOD1(OnDeviceFormatsInUse,
               void(const Vector<media::VideoCaptureFormat>&));
  MOCK_METHOD1(OnDeviceSupportedFormats,
               void(const Vector<media::VideoCaptureFormat>&));

  void StartCapture(int client_id, const media::VideoCaptureParams& params) {
    const auto state_update_callback = BindRepeating(
        &VideoCaptureImplTest::OnStateUpdate, base::Unretained(this));
    const auto frame_ready_callback = blink::BindRepeating(
        &VideoCaptureImplTest::OnFrameReady, base::Unretained(this));
    const auto frame_dropped_callback = BindRepeating(
        &VideoCaptureImplTest::OnFrameDropped, base::Unretained(this));

    VideoCaptureCallbacks video_capture_callbacks;
    video_capture_callbacks.state_update_cb = state_update_callback;
    video_capture_callbacks.deliver_frame_cb = frame_ready_callback;
    video_capture_callbacks.frame_dropped_cb = frame_dropped_callback;
    video_capture_callbacks.capture_version_cb = base::DoNothing();

    video_capture_impl_->StartCapture(client_id, params,
                                      std::move(video_capture_callbacks));
  }

  void StopCapture(int client_id) {
    video_capture_impl_->StopCapture(client_id);
  }

  void SimulateOnBufferCreated(int buffer_id,
                               const base::UnsafeSharedMemoryRegion& region) {
    video_capture_impl_->OnNewBuffer(
        buffer_id, media::mojom::blink::VideoBufferHandle::NewUnsafeShmemRegion(
                       region.Duplicate()));
  }

  void SimulateReadOnlyBufferCreated(int buffer_id,
                                     base::ReadOnlySharedMemoryRegion region) {
    video_capture_impl_->OnNewBuffer(
        buffer_id,
        media::mojom::blink::VideoBufferHandle::NewReadOnlyShmemRegion(
            std::move(region)));
  }

  void SimulateGpuMemoryBufferCreated(int buffer_id,
                                      gfx::GpuMemoryBufferHandle gmb_handle) {
    video_capture_impl_->OnNewBuffer(
        buffer_id,
        media::mojom::blink::VideoBufferHandle::NewGpuMemoryBufferHandle(
            std::move(gmb_handle)));
  }

  void SimulateSharedImageBufferCreated(
      int buffer_id,
      media::mojom::blink::SharedImageBufferHandleSetPtr handle_set) {
    video_capture_impl_->OnNewBuffer(
        buffer_id, media::mojom::blink::VideoBufferHandle::NewSharedImageHandle(
                       std::move(handle_set)));
  }

  void SimulateBufferReceived(BufferDescription buffer_description) {
    video_capture_impl_->OnBufferReady(
        buffer_description.ToReadyBuffer(base::TimeTicks::Now()));
  }

  void SimulateBufferDestroyed(int buffer_id) {
    video_capture_impl_->OnBufferDestroyed(buffer_id);
  }

  void GetDeviceSupportedFormats() {
    video_capture_impl_->GetDeviceSupportedFormats(
        BindOnce(&VideoCaptureImplTest::OnDeviceSupportedFormats,
                 base::Unretained(this)));
  }

  void GetDeviceFormatsInUse() {
    video_capture_impl_->GetDeviceFormatsInUse(BindOnce(
        &VideoCaptureImplTest::OnDeviceFormatsInUse, base::Unretained(this)));
  }

  void SetSharedImageCapabilities(bool shared_image_d3d) {
    gpu::SharedImageCapabilities shared_image_caps;
    shared_image_caps.shared_image_d3d = shared_image_d3d;
    platform_->SetSharedImageCapabilities(shared_image_caps);
  }

  CountingSharedImageInterface* shared_image_interface() {
    return platform_->shared_image_interface();
  }

  // Hands VideoCaptureImpl the platform's current factories, which is what it
  // does for itself on GPU context loss. Lives here because the fixture, not
  // the TEST_F-derived class, is VideoCaptureImpl's friend.
  void SimulateGpuFactoriesChangeOnIOThread() {
    video_capture_impl_->SetGpuFactoriesHandleOnIOTaskRunner(
        platform_->GetGpuFactories());
  }

  // Drops VideoCaptureImpl's GPU factories, which is the state a renderer is
  // in while the GPU process is gone.
  void SimulateNoGpuFactoriesOnIOThread() {
    video_capture_impl_->SetGpuFactoriesHandleOnIOTaskRunner(nullptr);
  }

  const base::UnguessableToken session_id_ = base::UnguessableToken::Create();
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedTestingPlatformSupport<TestingPlatformSupportWithGpuFactories>
      platform_;
  std::unique_ptr<VideoCaptureImpl> video_capture_impl_;
  MockMojoVideoCaptureHost mock_video_capture_host_;
  media::VideoCaptureParams params_small_;
  media::VideoCaptureParams params_large_;
  base::test::ScopedFeatureList feature_list_;

  gpu::Capabilities fake_capabilities_;
};

TEST_F(VideoCaptureImplTest, Simple) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));

  StartCapture(0, params_small_);
  StopCapture(0);

  histogram_tester.ExpectUniqueSample("Media.VideoCapture.StartOutcome",
                                      VideoCaptureStartOutcome::kStarted, 1);
  histogram_tester.ExpectUniqueSample("Media.VideoCapture.StartErrorCode",
                                      media::VideoCaptureError::kNone, 1);
}

TEST_F(VideoCaptureImplTest, TwoClientsInSequence) {
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED))
      .Times(2);
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));

  StartCapture(0, params_small_);
  StopCapture(0);
  StartCapture(1, params_small_);
  StopCapture(1);
}

TEST_F(VideoCaptureImplTest, LargeAndSmall) {
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED))
      .Times(2);
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_large_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));

  StartCapture(0, params_large_);
  StopCapture(0);
  StartCapture(1, params_small_);
  StopCapture(1);
}

TEST_F(VideoCaptureImplTest, SmallAndLarge) {
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED))
      .Times(2);
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));

  StartCapture(0, params_small_);
  StopCapture(0);
  StartCapture(1, params_large_);
  StopCapture(1);
}

// Checks that a request to GetDeviceSupportedFormats() ends up eventually in
// the provided callback.
TEST_F(VideoCaptureImplTest, GetDeviceFormats) {
  EXPECT_CALL(*this, OnDeviceSupportedFormats(_));
  EXPECT_CALL(mock_video_capture_host_,
              GetDeviceSupportedFormatsMock(_, session_id_, _));

  GetDeviceSupportedFormats();
}

// Checks that two requests to GetDeviceSupportedFormats() end up eventually
// calling the provided callbacks.
TEST_F(VideoCaptureImplTest, TwoClientsGetDeviceFormats) {
  EXPECT_CALL(*this, OnDeviceSupportedFormats(_)).Times(2);
  EXPECT_CALL(mock_video_capture_host_,
              GetDeviceSupportedFormatsMock(_, session_id_, _))
      .Times(2);

  GetDeviceSupportedFormats();
  GetDeviceSupportedFormats();
}

// Checks that a request to GetDeviceFormatsInUse() ends up eventually in the
// provided callback.
TEST_F(VideoCaptureImplTest, GetDeviceFormatsInUse) {
  EXPECT_CALL(*this, OnDeviceFormatsInUse(_));
  EXPECT_CALL(mock_video_capture_host_,
              GetDeviceFormatsInUseMock(_, session_id_, _));

  GetDeviceFormatsInUse();
}

TEST_F(VideoCaptureImplTest, BufferReceived) {
  const int kArbitraryBufferId = 11;

  const size_t frame_size = media::VideoFrame::AllocationSize(
      media::PIXEL_FORMAT_I420, params_small_.requested_format.frame_size);
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(frame_size);
  ASSERT_TRUE(region.IsValid());

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(*this, OnFrameReady(_, _));
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));
  EXPECT_CALL(mock_video_capture_host_, ReleaseBuffer(_, kArbitraryBufferId, _))
      .Times(0);

  StartCapture(0, params_small_);
  SimulateOnBufferCreated(kArbitraryBufferId, region);
  SimulateBufferReceived(BufferDescription(
      kArbitraryBufferId, params_small_.requested_format.frame_size));
  StopCapture(0);
  SimulateBufferDestroyed(kArbitraryBufferId);

  EXPECT_EQ(mock_video_capture_host_.released_buffer_count(), 0);
}

TEST_F(VideoCaptureImplTest, BufferReceived_ReadOnlyShmemRegion) {
  const int kArbitraryBufferId = 11;

  const size_t frame_size = media::VideoFrame::AllocationSize(
      media::PIXEL_FORMAT_I420, params_small_.requested_format.frame_size);
  base::MappedReadOnlyRegion shm =
      base::ReadOnlySharedMemoryRegion::Create(frame_size);
  ASSERT_TRUE(shm.IsValid());

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(*this, OnFrameReady(_, _));
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));
  EXPECT_CALL(mock_video_capture_host_, ReleaseBuffer(_, kArbitraryBufferId, _))
      .Times(0);

  StartCapture(0, params_small_);
  SimulateReadOnlyBufferCreated(kArbitraryBufferId, std::move(shm.region));
  SimulateBufferReceived(BufferDescription(
      kArbitraryBufferId, params_small_.requested_format.frame_size));
  StopCapture(0);
  SimulateBufferDestroyed(kArbitraryBufferId);

  EXPECT_EQ(mock_video_capture_host_.released_buffer_count(), 0);
}

TEST_F(VideoCaptureImplTest, BufferReceived_GpuMemoryBufferHandle) {
  const int kArbitraryBufferId = 11;

  // With GpuMemoryBufferHandle, the buffer handle is received on the IO thread
  // and passed to a media thread to create a SharedImage. After the SharedImage
  // is created and wrapped in a video frame, we pass the video frame back to
  // the IO thread to pass to the clients by calling their frame-ready
  // callbacks.
  base::Thread testing_io_thread("TestingIOThread");
  base::WaitableEvent frame_ready_event;
  scoped_refptr<media::VideoFrame> frame;

  SetSharedImageCapabilities(/* shared_image_d3d = */ true);
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(*this, OnFrameReady(_, _))
      .WillOnce([&](scoped_refptr<media::VideoFrame> f, base::TimeTicks t) {
        // Hold on a reference to the video frame to emulate that we're
        // actively using the buffer.
        frame = f;
        frame_ready_event.Signal();
      });
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));
  EXPECT_CALL(mock_video_capture_host_, ReleaseBuffer(_, kArbitraryBufferId, _))
      .Times(0);

  // The first half of the test: Create and queue the GpuMemoryBufferHandle.
  // VideoCaptureImpl would:
  //   1. create a GpuMemoryBuffer out of the handle on |testing_io_thread|
  //   2. create a SharedImage from the GpuMemoryBuffer on |media_thread_|
  //   3. invoke OnFrameReady callback on |testing_io_thread|
  auto create_and_queue_buffer = [&]() {
    StartCapture(0, params_small_);
    SimulateGpuMemoryBufferCreated(
        kArbitraryBufferId, gpu::TestSharedImageInterface::CreateGMBHandle(
                                viz::MultiPlaneFormat::kNV12,
                                params_small_.requested_format.frame_size));
    SimulateBufferReceived(BufferDescription(
        kArbitraryBufferId, params_small_.requested_format.frame_size,
        media::PIXEL_FORMAT_NV12));
  };

  // The second half of the test: Stop capture and destroy the buffer.
  // Everything should happen on |testing_io_thread| here.
  auto stop_capture_and_destroy_buffer = [&]() {
    StopCapture(0);
    SimulateBufferDestroyed(kArbitraryBufferId);
    // Explicitly destroy |video_capture_impl_| to make sure it's destroyed on
    // the right thread.
    video_capture_impl_.reset();
  };

  testing_io_thread.Start();
  testing_io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(create_and_queue_buffer));

  // Wait until OnFrameReady is called on |testing_io_thread|.
  EXPECT_TRUE(frame_ready_event.TimedWait(base::Seconds(3)));

  testing_io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(stop_capture_and_destroy_buffer));
  testing_io_thread.Stop();

  EXPECT_EQ(mock_video_capture_host_.released_buffer_count(), 0);
}

TEST_F(VideoCaptureImplTest, OnFrameDropped) {
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));
  EXPECT_CALL(*this, OnFrameDropped(_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));

  StartCapture(0, params_small_);
  video_capture_impl_->OnFrameDropped(
      media::VideoCaptureFrameDropReason::kBufferPoolMaxBufferCountExceeded);
  StopCapture(0);
}

TEST_F(VideoCaptureImplTest, BufferReceivedAfterStop) {
  const int kArbitraryBufferId = 12;

  const size_t frame_size = media::VideoFrame::AllocationSize(
      media::PIXEL_FORMAT_I420, params_large_.requested_format.frame_size);
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(frame_size);
  ASSERT_TRUE(region.IsValid());

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(*this, OnFrameReady(_, _)).Times(0);
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_large_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));
  EXPECT_CALL(mock_video_capture_host_,
              ReleaseBuffer(_, kArbitraryBufferId, _));

  StartCapture(0, params_large_);
  SimulateOnBufferCreated(kArbitraryBufferId, region);
  StopCapture(0);
  // A buffer received after StopCapture() triggers an instant ReleaseBuffer().
  SimulateBufferReceived(BufferDescription(
      kArbitraryBufferId, params_large_.requested_format.frame_size));
  SimulateBufferDestroyed(kArbitraryBufferId);

  EXPECT_EQ(mock_video_capture_host_.released_buffer_count(), 1);
}

TEST_F(VideoCaptureImplTest, BufferReceivedAfterStop_ReadOnlyShmemRegion) {
  const int kArbitraryBufferId = 12;

  const size_t frame_size = media::VideoFrame::AllocationSize(
      media::PIXEL_FORMAT_I420, params_large_.requested_format.frame_size);
  base::MappedReadOnlyRegion shm =
      base::ReadOnlySharedMemoryRegion::Create(frame_size);
  ASSERT_TRUE(shm.IsValid());

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(*this, OnFrameReady(_, _)).Times(0);
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_large_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));
  EXPECT_CALL(mock_video_capture_host_,
              ReleaseBuffer(_, kArbitraryBufferId, _));

  StartCapture(0, params_large_);
  SimulateReadOnlyBufferCreated(kArbitraryBufferId, std::move(shm.region));
  StopCapture(0);
  // A buffer received after StopCapture() triggers an instant ReleaseBuffer().
  SimulateBufferReceived(BufferDescription(
      kArbitraryBufferId, params_large_.requested_format.frame_size));
  SimulateBufferDestroyed(kArbitraryBufferId);

  EXPECT_EQ(mock_video_capture_host_.released_buffer_count(), 1);
}

TEST_F(VideoCaptureImplTest, BufferReceivedAfterStop_GpuMemoryBufferHandle) {
  const int kArbitraryBufferId = 12;

  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::SHARED_MEMORY_BUFFER;

  SetSharedImageCapabilities(/* shared_image_d3d = */ true);
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(*this, OnFrameReady(_, _)).Times(0);
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_large_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));
  EXPECT_CALL(mock_video_capture_host_,
              ReleaseBuffer(_, kArbitraryBufferId, _));

  StartCapture(0, params_large_);
  SimulateGpuMemoryBufferCreated(kArbitraryBufferId, std::move(gmb_handle));
  StopCapture(0);
  // A buffer received after StopCapture() triggers an instant ReleaseBuffer().
  SimulateBufferReceived(BufferDescription(
      kArbitraryBufferId, params_small_.requested_format.frame_size,
      media::PIXEL_FORMAT_NV12));
  SimulateBufferDestroyed(kArbitraryBufferId);

  EXPECT_EQ(mock_video_capture_host_.released_buffer_count(), 1);
}

TEST_F(VideoCaptureImplTest, AlreadyStarted) {
  base::HistogramTester histogram_tester;

  media::VideoCaptureParams params = {};
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED))
      .Times(2);
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_))
      .WillOnce(DoAll(
          [this]() {
            video_capture_impl_->OnStateChanged(
                media::mojom::blink::VideoCaptureResult::NewState(
                    media::mojom::VideoCaptureState::STARTED));
          },
          SaveArg<2>(&params)));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));

  StartCapture(0, params_small_);
  StartCapture(1, params_large_);
  StopCapture(0);
  StopCapture(1);
  DCHECK(params.requested_format == params_small_.requested_format);

  histogram_tester.ExpectTotalCount("Media.VideoCapture.Start", 1);
  histogram_tester.ExpectTotalCount("Media.VideoCapture.StartOutcome", 1);
  histogram_tester.ExpectUniqueSample("Media.VideoCapture.StartErrorCode",
                                      media::VideoCaptureError::kNone, 1);
}

TEST_F(VideoCaptureImplTest, EndedBeforeStop) {
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));

  StartCapture(0, params_small_);

  video_capture_impl_->OnStateChanged(
      media::mojom::blink::VideoCaptureResult::NewState(
          media::mojom::VideoCaptureState::ENDED));

  StopCapture(0);
}

TEST_F(VideoCaptureImplTest, ErrorBeforeStop) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_ERROR));
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));

  StartCapture(0, params_small_);

  video_capture_impl_->OnStateChanged(
      media::mojom::blink::VideoCaptureResult::NewErrorCode(
          media::VideoCaptureError::kIntentionalErrorRaisedByUnitTest));

  StopCapture(0);

  histogram_tester.ExpectTotalCount("Media.VideoCapture.Start", 1);
  // Successful start before the error, so StartOutcome is kStarted.
  histogram_tester.ExpectUniqueSample("Media.VideoCapture.StartOutcome",
                                      VideoCaptureStartOutcome::kStarted, 1);
  histogram_tester.ExpectUniqueSample("Media.VideoCapture.StartErrorCode",
                                      media::VideoCaptureError::kNone, 1);
}

TEST_F(VideoCaptureImplTest, WinSystemPermissionsErrorUpdatesCorrectState) {
  EXPECT_CALL(*this,
              OnStateUpdate(
                  blink::VIDEO_CAPTURE_STATE_ERROR_SYSTEM_PERMISSIONS_DENIED));
  video_capture_impl_->OnStateChanged(
      media::mojom::blink::VideoCaptureResult::NewErrorCode(
          media::VideoCaptureError::kWinMediaFoundationSystemPermissionDenied));

  StartCapture(0, params_small_);
  StopCapture(0);
}

TEST_F(VideoCaptureImplTest, BufferReceivedBeforeOnStarted) {
  const int kArbitraryBufferId = 16;

  const size_t frame_size = media::VideoFrame::AllocationSize(
      media::PIXEL_FORMAT_I420, params_small_.requested_format.frame_size);
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(frame_size);
  ASSERT_TRUE(region.IsValid());

  InSequence s;
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_))
      .WillOnce(DoNothing());
  EXPECT_CALL(mock_video_capture_host_,
              ReleaseBuffer(_, kArbitraryBufferId, _));
  StartCapture(0, params_small_);
  SimulateOnBufferCreated(kArbitraryBufferId, region);
  SimulateBufferReceived(BufferDescription(
      kArbitraryBufferId, params_small_.requested_format.frame_size));

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(mock_video_capture_host_, RequestRefreshFrame(_));
  video_capture_impl_->OnStateChanged(
      media::mojom::blink::VideoCaptureResult::NewState(
          media::mojom::VideoCaptureState::STARTED));
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(&mock_video_capture_host_);

  // Additional STARTED will cause RequestRefreshFrame a second time.
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(mock_video_capture_host_, RequestRefreshFrame(_));
  video_capture_impl_->OnStateChanged(
      media::mojom::blink::VideoCaptureResult::NewState(
          media::mojom::VideoCaptureState::STARTED));
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(&mock_video_capture_host_);

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));
  StopCapture(0);
}

TEST_F(VideoCaptureImplTest,
       BufferReceivedBeforeOnStarted_ReadOnlyShmemRegion) {
  const int kArbitraryBufferId = 16;

  const size_t frame_size = media::VideoFrame::AllocationSize(
      media::PIXEL_FORMAT_I420, params_small_.requested_format.frame_size);
  base::MappedReadOnlyRegion shm =
      base::ReadOnlySharedMemoryRegion::Create(frame_size);
  ASSERT_TRUE(shm.IsValid());

  InSequence s;
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_))
      .WillOnce(DoNothing());
  EXPECT_CALL(mock_video_capture_host_,
              ReleaseBuffer(_, kArbitraryBufferId, _));
  StartCapture(0, params_small_);
  SimulateReadOnlyBufferCreated(kArbitraryBufferId, std::move(shm.region));
  SimulateBufferReceived(BufferDescription(
      kArbitraryBufferId, params_small_.requested_format.frame_size));

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(mock_video_capture_host_, RequestRefreshFrame(_));
  video_capture_impl_->OnStateChanged(
      media::mojom::blink::VideoCaptureResult::NewState(
          media::mojom::VideoCaptureState::STARTED));
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(&mock_video_capture_host_);

  // Additional STARTED will cause RequestRefreshFrame a second time.
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(mock_video_capture_host_, RequestRefreshFrame(_));
  video_capture_impl_->OnStateChanged(
      media::mojom::blink::VideoCaptureResult::NewState(
          media::mojom::VideoCaptureState::STARTED));
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(&mock_video_capture_host_);

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));
  StopCapture(0);
}

TEST_F(VideoCaptureImplTest,
       BufferReceivedBeforeOnStarted_GpuMemoryBufferHandle) {
  const int kArbitraryBufferId = 16;

  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::SHARED_MEMORY_BUFFER;

  InSequence s;
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_))
      .WillOnce(DoNothing());
  EXPECT_CALL(mock_video_capture_host_,
              ReleaseBuffer(_, kArbitraryBufferId, _));
  StartCapture(0, params_small_);
  SimulateGpuMemoryBufferCreated(kArbitraryBufferId, std::move(gmb_handle));
  SimulateBufferReceived(BufferDescription(
      kArbitraryBufferId, params_small_.requested_format.frame_size,
      media::PIXEL_FORMAT_NV12));

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(mock_video_capture_host_, RequestRefreshFrame(_));
  video_capture_impl_->OnStateChanged(
      media::mojom::blink::VideoCaptureResult::NewState(
          media::mojom::VideoCaptureState::STARTED));
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(&mock_video_capture_host_);

  // Additional STARTED will cause RequestRefreshFrame a second time.
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(mock_video_capture_host_, RequestRefreshFrame(_));
  video_capture_impl_->OnStateChanged(
      media::mojom::blink::VideoCaptureResult::NewState(
          media::mojom::VideoCaptureState::STARTED));
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(&mock_video_capture_host_);

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));
  StopCapture(0);
}

TEST_F(VideoCaptureImplTest, StartTimeout) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*this,
              OnStateUpdate(blink::VIDEO_CAPTURE_STATE_ERROR_START_TIMEOUT));
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));

  ON_CALL(mock_video_capture_host_, DoStart(_, _, _)).WillByDefault([]() {
    // Do nothing.
  });

  StartCapture(0, params_small_);
  task_environment_.FastForwardBy(VideoCaptureImpl::kCaptureStartTimeout);

  histogram_tester.ExpectTotalCount("Media.VideoCapture.Start", 1);
  histogram_tester.ExpectUniqueSample("Media.VideoCapture.StartOutcome",
                                      VideoCaptureStartOutcome::kTimedout, 1);
  histogram_tester.ExpectUniqueSample(
      "Media.VideoCapture.StartErrorCode",
      media::VideoCaptureError::kVideoCaptureImplTimedOutOnStart, 1);
}

TEST_F(VideoCaptureImplTest, ErrorBeforeStart) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_ERROR));
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));
  ON_CALL(mock_video_capture_host_, DoStart(_, _, _)).WillByDefault([this]() {
    // Go straight to Failed. Do not pass Go. Do not collect £200.
    video_capture_impl_->OnStateChanged(
        media::mojom::blink::VideoCaptureResult::NewErrorCode(
            media::VideoCaptureError::kIntentionalErrorRaisedByUnitTest));
  });

  StartCapture(0, params_small_);

  histogram_tester.ExpectTotalCount("Media.VideoCapture.Start", 1);
  histogram_tester.ExpectUniqueSample("Media.VideoCapture.StartOutcome",
                                      VideoCaptureStartOutcome::kFailed, 1);
  histogram_tester.ExpectUniqueSample(
      "Media.VideoCapture.StartErrorCode",
      media::VideoCaptureError::kIntentionalErrorRaisedByUnitTest, 1);
}

#if BUILDFLAG(IS_WIN)
// This is windows-only scenario.
TEST_F(VideoCaptureImplTest, FallbacksToPremappedGmbsWhenNotSupported) {
  const int kArbitraryBufferId = 11;

  // With GpuMemoryBufferHandle, the buffer handle is received on the IO thread
  // and passed to a media thread to create a SharedImage. After the SharedImage
  // is created and wrapped in a video frame, we pass the video frame back to
  // the IO thread to pass to the clients by calling their frame-ready
  // callbacks.
  base::Thread testing_io_thread("TestingIOThread");
  base::WaitableEvent frame_ready_event;
  media::VideoCaptureFeedback feedback;

  SetSharedImageCapabilities(/* shared_image_d3d = */ false);
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));
  EXPECT_CALL(mock_video_capture_host_, ReleaseBuffer(_, kArbitraryBufferId, _))
      .WillOnce([&](const base::UnguessableToken& device_id, int32_t buffer_id,
                    const media::VideoCaptureFeedback& fb) {
        // Hold on a reference to the video frame to emulate that we're
        // actively using the buffer.
        feedback = fb;
        frame_ready_event.Signal();
      });

  // The first half of the test: Create and queue the GpuMemoryBufferHandle.
  // VideoCaptureImpl would:
  //   1. create a GpuMemoryBuffer out of the handle on |testing_io_thread|
  //   2. create a SharedImage from the GpuMemoryBuffer on |media_thread_|
  //   3. invoke OnFrameReady callback on |testing_io_thread|
  auto create_and_queue_buffer = [&]() {
    gfx::GpuMemoryBufferHandle gmb_handle;
    gmb_handle.type = gfx::SHARED_MEMORY_BUFFER;

    StartCapture(0, params_small_);
    SimulateGpuMemoryBufferCreated(kArbitraryBufferId, std::move(gmb_handle));
    SimulateBufferReceived(BufferDescription(
        kArbitraryBufferId, params_small_.requested_format.frame_size,
        media::PIXEL_FORMAT_NV12));
  };

  // The second half of the test: Stop capture and destroy the buffer.
  // Everything should happen on |testing_io_thread| here.
  auto stop_capture_and_destroy_buffer = [&]() {
    StopCapture(0);
    SimulateBufferDestroyed(kArbitraryBufferId);
    // Explicitly destroy |video_capture_impl_| to make sure it's destroyed on
    // the right thread.
    video_capture_impl_.reset();
  };

  testing_io_thread.Start();
  testing_io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(create_and_queue_buffer));

  // Wait until OnFrameReady is called on |testing_io_thread|.
  EXPECT_TRUE(frame_ready_event.TimedWait(base::Seconds(3)));

  EXPECT_TRUE(feedback.require_mapped_frame);

  testing_io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(stop_capture_and_destroy_buffer));
  testing_io_thread.Stop();

  EXPECT_EQ(mock_video_capture_host_.released_buffer_count(), 0);
}

// Windows-only scenario. Shared-image camera buffers carry a premapped region
// inside the exported image's buffer handle (see D3D11SharedImageBufferTracker
// in media/capture). When mappable shared images are not supported, premapped
// frames must be wrapped from that region instead of being dropped.
TEST_F(VideoCaptureImplTest, FallbacksToPremappedSharedImagesWhenNotSupported) {
  const int kGmbBufferId = 11;
  const int kSharedImageBufferId = 12;
  const gfx::Size kFrameSize = params_small_.requested_format.frame_size;
  constexpr uint8_t kFrameDataMarker = 0x5a;

  base::Thread testing_io_thread("TestingIOThread");
  base::WaitableEvent frame_ready_event;
  media::VideoCaptureFeedback feedback;

  // Fill a region with a marker byte and attach it to a fake DXGI handle,
  // mirroring how the capture pool exports the slot's premapped region inside
  // the shared image's buffer handle.
  auto premapped_region = base::UnsafeSharedMemoryRegion::Create(
      kFrameSize.width() * kFrameSize.height() * 3 / 2);
  auto premapped_mapping = premapped_region.Map();
  for (auto& byte : premapped_mapping.GetMemoryAsSpan<uint8_t>()) {
    byte = kFrameDataMarker;
  }

  gfx::DXGIHandle dxgi_handle(base::win::ScopedHandle(::CreateEvent(
      nullptr, /*bManualReset=*/TRUE, /*bInitialState=*/FALSE, nullptr)));
  ASSERT_TRUE(dxgi_handle.IsValid());
  dxgi_handle = dxgi_handle.CloneWithRegion(premapped_region.Duplicate());

  scoped_refptr<gpu::ClientSharedImage> client_shared_image;
  {
    auto sii = base::MakeRefCounted<gpu::TestSharedImageInterface>();
    client_shared_image = sii->CreateSharedImage(
        {viz::MultiPlaneFormat::kNV12, kFrameSize, gfx::ColorSpace(),
         gpu::SharedImageUsageSet(gpu::SHARED_IMAGE_USAGE_DISPLAY_READ),
         "VideoCaptureTestSharedImage"},
        gpu::kNullSurfaceHandle, gfx::BufferUsage::SCANOUT_VEA_CPU_READ,
        gfx::GpuMemoryBufferHandle(std::move(dxgi_handle)));
    ASSERT_TRUE(client_shared_image);
  }

  auto handle_set = media::mojom::blink::SharedImageBufferHandleSet::New(
      client_shared_image->Export(/*with_buffer_handle=*/true),
      client_shared_image->creation_sync_token(),
      /*is_rewritten_in_place=*/true);

  SetSharedImageCapabilities(/*shared_image_d3d=*/false);
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));
  // The GMB delivery fails while the GPU process reports no D3D shared image
  // support, which flips VideoCaptureImpl into the premapped-frames mode.
  // When the dropped GMB frame is released back, the mode is active, so the
  // shared-image buffer is delivered then.
  EXPECT_CALL(mock_video_capture_host_, ReleaseBuffer(_, kGmbBufferId, _))
      .WillOnce([&, handle_set = std::move(handle_set)](
                    const base::UnguessableToken& device_id, int32_t buffer_id,
                    const media::VideoCaptureFeedback& fb) mutable {
        SimulateSharedImageBufferCreated(kSharedImageBufferId,
                                         std::move(handle_set));
        BufferDescription buffer_description(kSharedImageBufferId, kFrameSize,
                                             media::PIXEL_FORMAT_NV12);
        buffer_description.is_premapped = true;
        SimulateBufferReceived(std::move(buffer_description));
      });
  // The frame is released back once all consumers are done with it; the
  // feedback then carries the premapped-frames requirement.
  EXPECT_CALL(mock_video_capture_host_,
              ReleaseBuffer(_, kSharedImageBufferId, _))
      .WillOnce([&](const base::UnguessableToken& device_id, int32_t buffer_id,
                    const media::VideoCaptureFeedback& fb) {
        feedback = fb;
        frame_ready_event.Signal();
      });

  // The delivered frame must wrap the premapped region's data.
  EXPECT_CALL(*this, OnFrameReady(_, _))
      .WillOnce([&](scoped_refptr<media::VideoFrame> frame,
                    base::TimeTicks frame_ready_time) {
        EXPECT_EQ(frame->visible_data(0)[0], kFrameDataMarker);
      });

  auto create_and_queue_buffer = [&]() {
    StartCapture(0, params_small_);
    // Deliver a GMB frame first; with D3D shared images reported as
    // unsupported its bind fails on the media thread and the premapped-frames
    // mode is activated (see FallbacksToPremappedGmbsWhenNotSupported). The
    // shared-image buffer is then delivered from the ReleaseBuffer callback
    // above, once the mode is active.
    gfx::GpuMemoryBufferHandle gmb_handle;
    gmb_handle.type = gfx::SHARED_MEMORY_BUFFER;
    SimulateGpuMemoryBufferCreated(kGmbBufferId, std::move(gmb_handle));
    SimulateBufferReceived(
        BufferDescription(kGmbBufferId, kFrameSize, media::PIXEL_FORMAT_NV12));
  };

  auto stop_capture_and_destroy_buffer = [&]() {
    StopCapture(0);
    SimulateBufferDestroyed(kGmbBufferId);
    SimulateBufferDestroyed(kSharedImageBufferId);
    // Explicitly destroy |video_capture_impl_| to make sure it's destroyed on
    // the right thread.
    video_capture_impl_.reset();
  };

  testing_io_thread.Start();
  testing_io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(create_and_queue_buffer));

  // Wait until OnFrameReady is called on |testing_io_thread|.
  EXPECT_TRUE(frame_ready_event.TimedWait(base::Seconds(3)));

  testing_io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(stop_capture_and_destroy_buffer));
  testing_io_thread.Stop();

  EXPECT_TRUE(feedback.require_mapped_frame);
}

// Same no-GPU path as above, but for a shared image exported without a buffer
// handle: there is no premapped region to wrap, and
// CloneGpuMemoryBufferHandle() CHECKs on such images, so the frame has to be
// dropped rather than wrapped.
TEST_F(VideoCaptureImplTest, DropsPremappedFrameWithoutBufferHandle) {
  const int kArbitraryBufferId = 11;
  const gfx::Size kFrameSize = params_small_.requested_format.frame_size;

  auto client_shared_image = shared_image_interface()->CreateSharedImage(
      {viz::MultiPlaneFormat::kNV12, kFrameSize, gfx::ColorSpace(),
       gpu::SharedImageUsageSet(gpu::SHARED_IMAGE_USAGE_DISPLAY_READ),
       "VideoCaptureTestSharedImage"},
      gpu::kNullSurfaceHandle);
  ASSERT_TRUE(client_shared_image);
  ASSERT_EQ(client_shared_image->GetGpuMemoryBufferType(), gfx::EMPTY_BUFFER);

  auto handle_set = media::mojom::blink::SharedImageBufferHandleSet::New(
      client_shared_image->Export(), client_shared_image->creation_sync_token(),
      /*is_rewritten_in_place=*/true);

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));
  // The frame is dropped, not delivered - and the process stays alive.
  EXPECT_CALL(*this, OnFrameReady(_, _)).Times(0);

  StartCapture(0, params_small_);
  // No GPU factories, so delivery takes the Windows software fallback branch.
  SimulateNoGpuFactoriesOnIOThread();
  SimulateSharedImageBufferCreated(kArbitraryBufferId, std::move(handle_set));
  BufferDescription buffer_description(kArbitraryBufferId, kFrameSize,
                                       media::PIXEL_FORMAT_NV12);
  buffer_description.is_premapped = true;
  SimulateBufferReceived(std::move(buffer_description));

  StopCapture(0);
  SimulateBufferDestroyed(kArbitraryBufferId);
}

// Software consumers (e.g. WebRTC's software encoder) read a GPU frame through
// ConvertToMemoryMappedFrame() only if it is a mappable shared image, which is
// then served from the premapped region without a GPU readback. An opaque
// shared image frame would instead be read back through the raster interface
// for every frame, leaving the premapped region unused.
TEST_F(VideoCaptureImplTest, WrapsPremappedSharedImageAsMappableFrame) {
  const int kArbitraryBufferId = 11;
  const gfx::Size kFrameSize = params_small_.requested_format.frame_size;
  constexpr uint8_t kFrameDataMarker = 0x5a;

  // Like the capture pool, export the slot's premapped region inside the
  // shared image's buffer handle.
  auto premapped_region = base::UnsafeSharedMemoryRegion::Create(
      kFrameSize.width() * kFrameSize.height() * 2);
  auto premapped_mapping = premapped_region.Map();
  for (auto& byte : premapped_mapping.GetMemoryAsSpan<uint8_t>()) {
    byte = kFrameDataMarker;
  }
  gfx::DXGIHandle dxgi_handle(base::win::ScopedHandle(::CreateEvent(
      nullptr, /*bManualReset=*/TRUE, /*bInitialState=*/FALSE, nullptr)));
  ASSERT_TRUE(dxgi_handle.IsValid());
  dxgi_handle = dxgi_handle.CloneWithRegion(premapped_region.Duplicate());

  auto client_shared_image = shared_image_interface()->CreateSharedImage(
      {viz::MultiPlaneFormat::kNV12, kFrameSize, gfx::ColorSpace(),
       gpu::SharedImageUsageSet(gpu::SHARED_IMAGE_USAGE_DISPLAY_READ),
       "VideoCaptureTestSharedImage"},
      gpu::kNullSurfaceHandle, gfx::BufferUsage::SCANOUT_VEA_CPU_READ,
      gfx::GpuMemoryBufferHandle(std::move(dxgi_handle)));
  ASSERT_TRUE(client_shared_image);

  auto handle_set = media::mojom::blink::SharedImageBufferHandleSet::New(
      client_shared_image->Export(/*with_buffer_handle=*/true),
      client_shared_image->creation_sync_token(),
      /*is_rewritten_in_place=*/true);

  base::Thread testing_io_thread("TestingIOThread");
  base::WaitableEvent frame_ready_event;
  scoped_refptr<media::VideoFrame> frame;

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(*this, OnFrameReady(_, _))
      .WillOnce([&](scoped_refptr<media::VideoFrame> f, base::TimeTicks t) {
        frame = f;
        frame_ready_event.Signal();
      });
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));
  EXPECT_CALL(mock_video_capture_host_, ReleaseBuffer(_, kArbitraryBufferId, _))
      .Times(0);

  auto create_and_queue_buffer = [&]() {
    StartCapture(0, params_small_);
    SimulateSharedImageBufferCreated(kArbitraryBufferId, std::move(handle_set));
    BufferDescription buffer_description(kArbitraryBufferId, kFrameSize,
                                         media::PIXEL_FORMAT_NV12);
    buffer_description.is_premapped = true;
    SimulateBufferReceived(std::move(buffer_description));
  };
  auto stop_capture_and_destroy_buffer = [&]() {
    StopCapture(0);
    SimulateBufferDestroyed(kArbitraryBufferId);
    video_capture_impl_.reset();
  };

  testing_io_thread.Start();
  testing_io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(create_and_queue_buffer));

  EXPECT_TRUE(frame_ready_event.TimedWait(base::Seconds(3)));
  ASSERT_TRUE(frame);

  // Mapping is served from the premapped region, not by a GPU readback. The
  // results are checked after the teardown so that a failure does not skip it.
  const bool is_mappable = frame->HasMappableSharedImage();
  scoped_refptr<media::VideoFrame> mapped_frame;
  uint8_t first_byte = 0;
  if (is_mappable) {
    mapped_frame = media::ConvertToMemoryMappedFrame(frame);
    if (mapped_frame) {
      first_byte = mapped_frame->visible_data(0)[0];
    }
  }

  testing_io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(stop_capture_and_destroy_buffer));
  testing_io_thread.Stop();

  EXPECT_TRUE(is_mappable);
  EXPECT_TRUE(mapped_frame);
  EXPECT_EQ(first_byte, kFrameDataMarker);
}
#endif

// Shared images exported without a buffer handle (e.g. by the texture virtual
// device) have no mappable buffer; the GPU path must still wrap them without
// touching the premapped-memory machinery.
TEST_F(VideoCaptureImplTest, WrapsSharedImageWithoutBufferHandle) {
  const int kArbitraryBufferId = 11;
  const gfx::Size kFrameSize(64, 64);

  // Like the GpuMemoryBuffer path, slots are wrapped on the media thread and
  // the frame is handed back to the IO thread, so wait for the round trip.
  base::Thread testing_io_thread("TestingIOThread");
  base::WaitableEvent frame_ready_event;
  scoped_refptr<media::VideoFrame> frame;

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(*this, OnFrameReady(_, _))
      .WillOnce([&](scoped_refptr<media::VideoFrame> f, base::TimeTicks t) {
        frame = f;
        frame_ready_event.Signal();
      });
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));
  EXPECT_CALL(mock_video_capture_host_, ReleaseBuffer(_, kArbitraryBufferId, _))
      .Times(0);

  // Created through the interface the capture path will use, so that flagging
  // the slot as externally updated finds the mailbox. Exported without a
  // buffer handle, so the image has no mappable buffer.
  auto client_shared_image = shared_image_interface()->CreateSharedImage(
      {viz::MultiPlaneFormat::kNV12, kFrameSize, gfx::ColorSpace(),
       gpu::SharedImageUsageSet(gpu::SHARED_IMAGE_USAGE_DISPLAY_READ),
       "VideoCaptureTestSharedImage"},
      gpu::kNullSurfaceHandle);
  ASSERT_TRUE(client_shared_image);
  ASSERT_EQ(client_shared_image->GetGpuMemoryBufferType(), gfx::EMPTY_BUFFER);

  auto handle_set = media::mojom::blink::SharedImageBufferHandleSet::New(
      client_shared_image->Export(), client_shared_image->creation_sync_token(),
      /*is_rewritten_in_place=*/true);

  auto create_and_queue_buffer = [&]() {
    StartCapture(0, params_small_);
    SimulateSharedImageBufferCreated(kArbitraryBufferId, std::move(handle_set));
    SimulateBufferReceived(BufferDescription(kArbitraryBufferId, kFrameSize));
  };
  auto stop_capture_and_destroy_buffer = [&]() {
    StopCapture(0);
    SimulateBufferDestroyed(kArbitraryBufferId);
    video_capture_impl_.reset();
  };

  testing_io_thread.Start();
  testing_io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(create_and_queue_buffer));

  EXPECT_TRUE(frame_ready_event.TimedWait(base::Seconds(3)));

  // A capture slot is written by the capture process outside this process's
  // command buffer, so each delivered frame must flag the backing as updated.
  EXPECT_EQ(shared_image_interface()->update_shared_image_count(), 1);
  // Frames must be backed by an owning import, so that the backing survives
  // the capturer destroying its own reference when the pool evicts the slot.
  EXPECT_EQ(shared_image_interface()->import_shared_image_count(), 1);

  testing_io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(stop_capture_and_destroy_buffer));
  testing_io_thread.Stop();

  EXPECT_EQ(mock_video_capture_host_.released_buffer_count(), 0);
}

TEST_F(VideoCaptureImplTest, WrapsSharedImageDirectlyWithoutExternalUpdate) {
  const int kArbitraryBufferId = 11;
  const gfx::Size kFrameSize(64, 64);

  base::Thread testing_io_thread("TestingIOThread");
  base::WaitableEvent frame_ready_event;
  scoped_refptr<media::VideoFrame> frame;

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(*this, OnFrameReady(_, _))
      .WillOnce([&](scoped_refptr<media::VideoFrame> f, base::TimeTicks t) {
        frame = f;
        frame_ready_event.Signal();
      });
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));
  EXPECT_CALL(mock_video_capture_host_, ReleaseBuffer(_, kArbitraryBufferId, _))
      .Times(0);

  auto client_shared_image = shared_image_interface()->CreateSharedImage(
      {viz::MultiPlaneFormat::kNV12, kFrameSize, gfx::ColorSpace(),
       gpu::SharedImageUsageSet(gpu::SHARED_IMAGE_USAGE_DISPLAY_READ),
       "VideoCaptureTestSharedImage"},
      gpu::kNullSurfaceHandle);
  ASSERT_TRUE(client_shared_image);
  // The texture virtual device leaves is_rewritten_in_place false.
  auto handle_set = media::mojom::blink::SharedImageBufferHandleSet::New(
      client_shared_image->Export(), client_shared_image->creation_sync_token(),
      /*is_rewritten_in_place=*/false);

  testing_io_thread.Start();
  testing_io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        StartCapture(0, params_small_);
        SimulateSharedImageBufferCreated(kArbitraryBufferId,
                                         std::move(handle_set));
        SimulateBufferReceived(
            BufferDescription(kArbitraryBufferId, kFrameSize));
      }));

  EXPECT_TRUE(frame_ready_event.TimedWait(base::Seconds(3)));

  // The frame wraps the handle set's shared image and sync token directly.
  ASSERT_TRUE(frame);
  EXPECT_EQ(frame->shared_image()->mailbox(), client_shared_image->mailbox());
  EXPECT_EQ(frame->acquire_sync_token(),
            client_shared_image->creation_sync_token());
  // Neither an externally-updated notification nor an owning import was
  // requested.
  EXPECT_EQ(shared_image_interface()->update_shared_image_count(), 0);
  EXPECT_EQ(shared_image_interface()->import_shared_image_count(), 0);

  testing_io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        StopCapture(0);
        SimulateBufferDestroyed(kArbitraryBufferId);
        video_capture_impl_.reset();
      }));
  testing_io_thread.Stop();

  EXPECT_EQ(mock_video_capture_host_.released_buffer_count(), 0);
}

// BufferContexts outlive a GPU context loss (|client_buffers_| is only cleared
// on stop), so the owning import cached for a slot belongs to the old, dead
// SharedImageInterface and must be retaken once the context is back. Mirrors
// the recreate-on-change behaviour of the GpuMemoryBuffer path.
TEST_F(VideoCaptureImplTest, ReimportsSharedImageAfterGpuContextChange) {
  const int kArbitraryBufferId = 11;
  const gfx::Size kFrameSize(64, 64);

  base::Thread testing_io_thread("TestingIOThread");
  base::WaitableEvent first_frame_ready;
  base::WaitableEvent second_frame_ready;
  scoped_refptr<media::VideoFrame> frame;

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(*this, OnFrameReady(_, _))
      .WillOnce([&](scoped_refptr<media::VideoFrame> f, base::TimeTicks t) {
        frame = f;
        first_frame_ready.Signal();
      })
      .WillOnce([&](scoped_refptr<media::VideoFrame> f, base::TimeTicks t) {
        frame = f;
        second_frame_ready.Signal();
      });
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));

  auto client_shared_image = shared_image_interface()->CreateSharedImage(
      {viz::MultiPlaneFormat::kNV12, kFrameSize, gfx::ColorSpace(),
       gpu::SharedImageUsageSet(gpu::SHARED_IMAGE_USAGE_DISPLAY_READ),
       "VideoCaptureTestSharedImage"},
      gpu::kNullSurfaceHandle);
  ASSERT_TRUE(client_shared_image);

  auto handle_set = media::mojom::blink::SharedImageBufferHandleSet::New(
      client_shared_image->Export(), client_shared_image->creation_sync_token(),
      /*is_rewritten_in_place=*/true);

  testing_io_thread.Start();
  testing_io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        StartCapture(0, params_small_);
        SimulateSharedImageBufferCreated(kArbitraryBufferId,
                                         std::move(handle_set));
        SimulateBufferReceived(
            BufferDescription(kArbitraryBufferId, kFrameSize));
      }));
  EXPECT_TRUE(first_frame_ready.TimedWait(base::Seconds(3)));
  EXPECT_EQ(shared_image_interface()->import_shared_image_count(), 1);

  // Lose and re-establish the GPU context: a different factories instance is
  // exactly what VideoCaptureImpl keys the recovery off.
  platform_->ReplaceGpuFactories();
  testing_io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        SimulateGpuFactoriesChangeOnIOThread();
        // Same buffer id, so the BufferContext - and its cached import - is
        // reused rather than rebuilt.
        SimulateBufferReceived(
            BufferDescription(kArbitraryBufferId, kFrameSize));
      }));
  EXPECT_TRUE(second_frame_ready.TimedWait(base::Seconds(3)));

  // The second frame must not reuse the import taken against the dead
  // interface.
  EXPECT_EQ(shared_image_interface()->import_shared_image_count(), 2);

  testing_io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        frame.reset();
        StopCapture(0);
        SimulateBufferDestroyed(kArbitraryBufferId);
        video_capture_impl_.reset();
      }));
  testing_io_thread.Stop();
}

// Dropping a shared-image frame must return the buffer to the capturer. The
// release callback holds a reference to the BufferContext until it runs, which
// the reference-count DCHECKs in OnAllClientsFinishedConsumingFrame() and
// OnBufferDestroyed() have to tolerate for this buffer type.
TEST_F(VideoCaptureImplTest, SharedImageFrameReleaseReturnsBuffer) {
  const int kArbitraryBufferId = 11;
  const gfx::Size kFrameSize(64, 64);

  base::Thread testing_io_thread("TestingIOThread");
  base::WaitableEvent frame_ready_event;
  base::WaitableEvent buffer_released_event;
  scoped_refptr<media::VideoFrame> frame;

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(*this, OnFrameReady(_, _))
      .WillOnce([&](scoped_refptr<media::VideoFrame> f, base::TimeTicks t) {
        frame = std::move(f);
        frame_ready_event.Signal();
      });
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));
  EXPECT_CALL(mock_video_capture_host_, ReleaseBuffer(_, kArbitraryBufferId, _))
      .WillOnce(InvokeWithoutArgs([&]() { buffer_released_event.Signal(); }));

  auto client_shared_image = shared_image_interface()->CreateSharedImage(
      {viz::MultiPlaneFormat::kNV12, kFrameSize, gfx::ColorSpace(),
       gpu::SharedImageUsageSet(gpu::SHARED_IMAGE_USAGE_DISPLAY_READ),
       "VideoCaptureTestSharedImage"},
      gpu::kNullSurfaceHandle);
  ASSERT_TRUE(client_shared_image);

  auto handle_set = media::mojom::blink::SharedImageBufferHandleSet::New(
      client_shared_image->Export(), client_shared_image->creation_sync_token(),
      /*is_rewritten_in_place=*/true);

  testing_io_thread.Start();
  testing_io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        StartCapture(0, params_small_);
        SimulateSharedImageBufferCreated(kArbitraryBufferId,
                                         std::move(handle_set));
        SimulateBufferReceived(
            BufferDescription(kArbitraryBufferId, kFrameSize));
      }));
  EXPECT_TRUE(frame_ready_event.TimedWait(base::Seconds(3)));

  // Occupy the media thread so that the release callback's hop to it is still
  // queued - and so still holding its BufferContext reference - while the IO
  // thread runs OnAllClientsFinishedConsumingFrame(). Without this the two
  // race and the extra reference is usually already gone.
  //
  // Blocking the media thread here is only safe because nothing between
  // frame.reset() and the ReleaseBuffer() below needs it: that path runs
  // entirely on the IO thread. If a future change adds a media thread hop in
  // that window, this deadlocks and surfaces as the TimedWait() below expiring
  // after 3 seconds rather than as an obvious hang.
  //
  // The events are ref-counted rather than stack-allocated: the media thread
  // belongs to |platform_| and is only joined during fixture teardown, after
  // this function returns. Under load the parked task may not be picked up
  // until then, which would make stack events a use-after-free.
  using RefCountedEvent = base::RefCountedData<base::WaitableEvent>;
  auto media_thread_parked = base::MakeRefCounted<RefCountedEvent>();
  auto unblock_media_thread = base::MakeRefCounted<RefCountedEvent>();
  platform_->GetGpuFactories()->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](scoped_refptr<RefCountedEvent> parked,
                        scoped_refptr<RefCountedEvent> unblock) {
                       parked->data.Signal();
                       unblock->data.Wait();
                     },
                     media_thread_parked, unblock_media_thread));
  // The parked task must have actually started, otherwise the interleaving
  // this test depends on is not guaranteed and it would assert nothing.
  EXPECT_TRUE(media_thread_parked->data.TimedWait(base::Seconds(3)));

  testing_io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { frame.reset(); }));
  EXPECT_TRUE(buffer_released_event.TimedWait(base::Seconds(3)));
  // Released unconditionally, including when a wait above failed, so that the
  // parked task can never block fixture teardown.
  unblock_media_thread->data.Signal();

  testing_io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        StopCapture(0);
        SimulateBufferDestroyed(kArbitraryBufferId);
        video_capture_impl_.reset();
      }));
  testing_io_thread.Stop();
}

TEST_F(VideoCaptureImplTest, WinCameraBusyErrorUpdatesCorrectState) {
  EXPECT_CALL(*this,
              OnStateUpdate(blink::VIDEO_CAPTURE_STATE_ERROR_CAMERA_BUSY));
  video_capture_impl_->OnStateChanged(
      media::mojom::blink::VideoCaptureResult::NewErrorCode(
          media::VideoCaptureError::kWinMediaFoundationCameraBusy));

  StartCapture(0, params_small_);
  StopCapture(0);
}

TEST_F(VideoCaptureImplTest, LoggingErrorCodeForGetUserMediaRequestWorks) {
  base::HistogramTester histogram_tester;
  media::VideoCaptureParams camera_params = params_small_;
  camera_params.request_type = media::CaptureSourceRequestType::kGetUserMedia;
  StartCapture(0, camera_params);

  histogram_tester.ExpectUniqueSample(
      "Media.VideoCapture.StartErrorCode.GetUserMedia",
      media::VideoCaptureError::kNone, 1);
  histogram_tester.ExpectTotalCount(
      "Media.VideoCapture.StartErrorCode.GetDisplayMedia", 0);
  StopCapture(0);
}

TEST_F(VideoCaptureImplTest, LoggingErrorCodeForGetDisplayMediaRequestWorks) {
  base::HistogramTester histogram_tester;
  media::VideoCaptureParams screen_params = params_large_;
  screen_params.request_type =
      media::CaptureSourceRequestType::kGetDisplayMedia;
  StartCapture(0, screen_params);

  histogram_tester.ExpectUniqueSample(
      "Media.VideoCapture.StartErrorCode.GetDisplayMedia",
      media::VideoCaptureError::kNone, 1);
  histogram_tester.ExpectTotalCount(
      "Media.VideoCapture.StartErrorCode.GetUserMedia", 0);
  StopCapture(0);
}

}  // namespace blink
