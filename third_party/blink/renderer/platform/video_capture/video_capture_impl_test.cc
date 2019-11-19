// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "media/capture/mojom/video_capture.mojom-blink.h"
#include "media/capture/mojom/video_capture_types.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/video_capture/gpu_memory_buffer_test_support.h"
#include "third_party/blink/renderer/platform/video_capture/video_capture_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::WithArgs;

namespace blink {

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
        .WillByDefault(WithArgs<2>(Invoke(RunEmptyFormatsCallback)));
    ON_CALL(*this, GetDeviceFormatsInUseMock(_, _, _))
        .WillByDefault(WithArgs<2>(Invoke(RunEmptyFormatsCallback)));
    ON_CALL(*this, ReleaseBuffer(_, _, _))
        .WillByDefault(InvokeWithoutArgs(
            this, &MockMojoVideoCaptureHost::increase_released_buffer_count));
  }

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
               void(const base::UnguessableToken&, int32_t, double));
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

  DISALLOW_COPY_AND_ASSIGN(MockMojoVideoCaptureHost);
};

// This class encapsulates a VideoCaptureImpl under test and the necessary
// accessory classes, namely:
// - a MockMojoVideoCaptureHost, mimicking the RendererHost;
// - a few callbacks that are bound when calling operations of VideoCaptureImpl
//  and on which we set expectations.
class VideoCaptureImplTest : public ::testing::Test {
 public:
  VideoCaptureImplTest()
      : video_capture_impl_(new VideoCaptureImpl(session_id_)) {
    params_small_.requested_format = media::VideoCaptureFormat(
        gfx::Size(176, 144), 30, media::PIXEL_FORMAT_I420);
    params_large_.requested_format = media::VideoCaptureFormat(
        gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);

    video_capture_impl_->SetVideoCaptureHostForTesting(
        &mock_video_capture_host_);

    ON_CALL(mock_video_capture_host_, DoStart(_, _, _))
        .WillByDefault(InvokeWithoutArgs([this]() {
          video_capture_impl_->OnStateChanged(
              media::mojom::VideoCaptureState::STARTED);
        }));

    video_capture_impl_->SetGpuMemoryBufferSupportForTesting(
        std::make_unique<FakeGpuMemoryBufferSupport>());
  }

 protected:
  // These four mocks are used to create callbacks for the different oeprations.
  MOCK_METHOD2(OnFrameReady,
               void(scoped_refptr<media::VideoFrame>, base::TimeTicks));
  MOCK_METHOD1(OnStateUpdate, void(VideoCaptureState));
  MOCK_METHOD1(OnDeviceFormatsInUse,
               void(const Vector<media::VideoCaptureFormat>&));
  MOCK_METHOD1(OnDeviceSupportedFormats,
               void(const Vector<media::VideoCaptureFormat>&));

  void StartCapture(int client_id, const media::VideoCaptureParams& params) {
    const auto state_update_callback = base::Bind(
        &VideoCaptureImplTest::OnStateUpdate, base::Unretained(this));
    const auto frame_ready_callback =
        base::Bind(&VideoCaptureImplTest::OnFrameReady, base::Unretained(this));

    video_capture_impl_->StartCapture(client_id, params, state_update_callback,
                                      frame_ready_callback);
  }

  void StopCapture(int client_id) {
    video_capture_impl_->StopCapture(client_id);
  }

  void SimulateOnBufferCreated(int buffer_id,
                               const base::UnsafeSharedMemoryRegion& region) {
    video_capture_impl_->OnNewBuffer(
        buffer_id,
        media::mojom::blink::VideoBufferHandle::NewSharedBufferHandle(
            mojo::WrapUnsafeSharedMemoryRegion(region.Duplicate())));
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

  void SimulateBufferReceived(
      int buffer_id,
      const gfx::Size& size,
      media::VideoPixelFormat pixel_format = media::PIXEL_FORMAT_I420) {
    media::mojom::blink::VideoFrameInfoPtr info =
        media::mojom::blink::VideoFrameInfo::New();

    const base::TimeTicks now = base::TimeTicks::Now();
    media::VideoFrameMetadata frame_metadata;
    frame_metadata.SetTimeTicks(media::VideoFrameMetadata::REFERENCE_TIME, now);
    info->metadata = frame_metadata.GetInternalValues().Clone();

    info->timestamp = now - base::TimeTicks();
    info->pixel_format = pixel_format;
    info->coded_size = WebSize(size);
    info->visible_rect = WebRect(gfx::Rect(size));
    info->color_space = gfx::ColorSpace();

    video_capture_impl_->OnBufferReady(buffer_id, std::move(info));
  }

  void SimulateBufferDestroyed(int buffer_id) {
    video_capture_impl_->OnBufferDestroyed(buffer_id);
  }

  void GetDeviceSupportedFormats() {
    const base::Callback<void(const Vector<media::VideoCaptureFormat>&)>
        callback = base::Bind(&VideoCaptureImplTest::OnDeviceSupportedFormats,
                              base::Unretained(this));
    video_capture_impl_->GetDeviceSupportedFormats(callback);
  }

  void GetDeviceFormatsInUse() {
    const base::Callback<void(const Vector<media::VideoCaptureFormat>&)>
        callback = base::Bind(&VideoCaptureImplTest::OnDeviceFormatsInUse,
                              base::Unretained(this));
    video_capture_impl_->GetDeviceFormatsInUse(callback);
  }

  void OnStateChanged(media::mojom::VideoCaptureState state) {
    video_capture_impl_->OnStateChanged(state);
  }

  const base::UnguessableToken session_id_ = base::UnguessableToken::Create();
  base::test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestingPlatformSupportForGpuMemoryBuffer>
      platform_;
  std::unique_ptr<VideoCaptureImpl> video_capture_impl_;
  MockMojoVideoCaptureHost mock_video_capture_host_;
  media::VideoCaptureParams params_small_;
  media::VideoCaptureParams params_large_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImplTest);
};

TEST_F(VideoCaptureImplTest, Simple) {
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));

  StartCapture(0, params_small_);
  StopCapture(0);
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
  SimulateBufferReceived(kArbitraryBufferId,
                         params_small_.requested_format.frame_size);
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
  SimulateBufferReceived(kArbitraryBufferId,
                         params_small_.requested_format.frame_size);
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

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(*this, OnFrameReady(_, _))
      .WillOnce(
          Invoke([&](scoped_refptr<media::VideoFrame> f, base::TimeTicks t) {
            // Hold on a reference to the video frame to emulate that we're
            // actively using the buffer.
            frame = f;
            frame_ready_event.Signal();
          }));
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
    gfx::GpuMemoryBufferHandle gmb_handle;
    gmb_handle.type = gfx::NATIVE_PIXMAP;
    gmb_handle.id = gfx::GpuMemoryBufferId(kArbitraryBufferId);

    StartCapture(0, params_small_);
    SimulateGpuMemoryBufferCreated(kArbitraryBufferId, std::move(gmb_handle));
    SimulateBufferReceived(kArbitraryBufferId,
                           params_small_.requested_format.frame_size,
                           media::PIXEL_FORMAT_NV12);
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
  EXPECT_TRUE(frame_ready_event.TimedWait(base::TimeDelta::FromSeconds(3)));

  testing_io_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(stop_capture_and_destroy_buffer));
  testing_io_thread.Stop();

  EXPECT_EQ(mock_video_capture_host_.released_buffer_count(), 0);
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
  SimulateBufferReceived(kArbitraryBufferId,
                         params_large_.requested_format.frame_size);
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
  SimulateBufferReceived(kArbitraryBufferId,
                         params_large_.requested_format.frame_size);
  SimulateBufferDestroyed(kArbitraryBufferId);

  EXPECT_EQ(mock_video_capture_host_.released_buffer_count(), 1);
}

TEST_F(VideoCaptureImplTest, BufferReceivedAfterStop_GpuMemoryBufferHandle) {
  const int kArbitraryBufferId = 12;

  gfx::GpuMemoryBufferHandle gmb_handle;
  gmb_handle.type = gfx::NATIVE_PIXMAP;
  gmb_handle.id = gfx::GpuMemoryBufferId(kArbitraryBufferId);

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
  SimulateBufferReceived(kArbitraryBufferId,
                         params_small_.requested_format.frame_size,
                         media::PIXEL_FORMAT_NV12);
  SimulateBufferDestroyed(kArbitraryBufferId);

  EXPECT_EQ(mock_video_capture_host_.released_buffer_count(), 1);
}

TEST_F(VideoCaptureImplTest, AlreadyStarted) {
  media::VideoCaptureParams params = {};
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED))
      .Times(2);
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_))
      .WillOnce(DoAll(InvokeWithoutArgs([this]() {
                        video_capture_impl_->OnStateChanged(
                            media::mojom::VideoCaptureState::STARTED);
                      }),
                      SaveArg<2>(&params)));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));

  StartCapture(0, params_small_);
  StartCapture(1, params_large_);
  StopCapture(0);
  StopCapture(1);
  DCHECK(params.requested_format == params_small_.requested_format);
}

TEST_F(VideoCaptureImplTest, EndedBeforeStop) {
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));

  StartCapture(0, params_small_);

  OnStateChanged(media::mojom::VideoCaptureState::ENDED);

  StopCapture(0);
}

TEST_F(VideoCaptureImplTest, ErrorBeforeStop) {
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_ERROR));
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_));

  StartCapture(0, params_small_);

  OnStateChanged(media::mojom::VideoCaptureState::FAILED);

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
  SimulateBufferReceived(kArbitraryBufferId,
                         params_small_.requested_format.frame_size);

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(mock_video_capture_host_, RequestRefreshFrame(_));
  video_capture_impl_->OnStateChanged(media::mojom::VideoCaptureState::STARTED);
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(&mock_video_capture_host_);

  // Additional STARTED will cause RequestRefreshFrame a second time.
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(mock_video_capture_host_, RequestRefreshFrame(_));
  video_capture_impl_->OnStateChanged(media::mojom::VideoCaptureState::STARTED);
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
  SimulateBufferReceived(kArbitraryBufferId,
                         params_small_.requested_format.frame_size);

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(mock_video_capture_host_, RequestRefreshFrame(_));
  video_capture_impl_->OnStateChanged(media::mojom::VideoCaptureState::STARTED);
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(&mock_video_capture_host_);

  // Additional STARTED will cause RequestRefreshFrame a second time.
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(mock_video_capture_host_, RequestRefreshFrame(_));
  video_capture_impl_->OnStateChanged(media::mojom::VideoCaptureState::STARTED);
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
  gmb_handle.type = gfx::NATIVE_PIXMAP;
  gmb_handle.id = gfx::GpuMemoryBufferId(kArbitraryBufferId);

  InSequence s;
  EXPECT_CALL(mock_video_capture_host_, DoStart(_, session_id_, params_small_))
      .WillOnce(DoNothing());
  EXPECT_CALL(mock_video_capture_host_,
              ReleaseBuffer(_, kArbitraryBufferId, _));
  StartCapture(0, params_small_);
  SimulateGpuMemoryBufferCreated(kArbitraryBufferId, std::move(gmb_handle));
  SimulateBufferReceived(kArbitraryBufferId,
                         params_small_.requested_format.frame_size,
                         media::PIXEL_FORMAT_NV12);

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(mock_video_capture_host_, RequestRefreshFrame(_));
  video_capture_impl_->OnStateChanged(media::mojom::VideoCaptureState::STARTED);
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(&mock_video_capture_host_);

  // Additional STARTED will cause RequestRefreshFrame a second time.
  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(mock_video_capture_host_, RequestRefreshFrame(_));
  video_capture_impl_->OnStateChanged(media::mojom::VideoCaptureState::STARTED);
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(&mock_video_capture_host_);

  EXPECT_CALL(*this, OnStateUpdate(blink::VIDEO_CAPTURE_STATE_STOPPED));
  EXPECT_CALL(mock_video_capture_host_, Stop(_));
  StopCapture(0);
}

}  // namespace blink
