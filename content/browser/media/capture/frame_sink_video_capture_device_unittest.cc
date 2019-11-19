// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/frame_sink_video_capture_device.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/flat_map.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "media/base/video_frame.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

using testing::_;
using testing::ByRef;
using testing::Eq;
using testing::Expectation;
using testing::Ge;
using testing::NiceMock;
using testing::NotNull;
using testing::SaveArg;
using testing::Sequence;
using testing::StrNe;

namespace content {
namespace {

// Threading notes: Throughout these tests, the UI thread (the main test
// thread) represents the executor of all external-to-device operations. This
// means that it represents everything that runs on the UI thread in the browser
// process, plus anything that would run in the VIZ process. The IO thread is
// used as the "device thread" for content::FrameSinkVideoCaptureDevice.
#define DCHECK_ON_DEVICE_THREAD() DCHECK_CURRENTLY_ON(BrowserThread::IO)
#define DCHECK_NOT_ON_DEVICE_THREAD() DCHECK_CURRENTLY_ON(BrowserThread::UI)

// Convenience macro to block the test procedure and run all pending UI tasks.
#define RUN_UI_TASKS() base::RunLoop().RunUntilIdle()

// Convenience macro to post a task to run on the device thread.
#define POST_DEVICE_TASK(closure) \
  base::PostTask(FROM_HERE, {BrowserThread::IO}, closure)

// Convenience macro to block the test procedure until all pending tasks have
// run on the device thread.
#define WAIT_FOR_DEVICE_TASKS()             \
  task_environment_.RunIOThreadUntilIdle(); \
  RUN_UI_TASKS()

// Capture parameters.
constexpr gfx::Size kResolution = gfx::Size(320, 180);
constexpr int kMaxFrameRate = 25;  // It evenly divides 1 million usec.
constexpr base::TimeDelta kMinCapturePeriod = base::TimeDelta::FromMicroseconds(
    base::Time::kMicrosecondsPerSecond / kMaxFrameRate);
constexpr media::VideoPixelFormat kFormat = media::PIXEL_FORMAT_I420;

// Helper to return the capture parameters packaged in a VideoCaptureParams.
media::VideoCaptureParams GetCaptureParams() {
  media::VideoCaptureParams params;
  params.requested_format =
      media::VideoCaptureFormat(kResolution, kMaxFrameRate, kFormat);
  return params;
}

// Mock for the FrameSinkVideoCapturer running in the VIZ process.
class MockFrameSinkVideoCapturer : public viz::mojom::FrameSinkVideoCapturer {
 public:
  MockFrameSinkVideoCapturer() = default;

  bool is_bound() const { return receiver_.is_bound(); }

  void Bind(
      mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer> receiver) {
    DCHECK_NOT_ON_DEVICE_THREAD();
    receiver_.Bind(std::move(receiver));
  }

  MOCK_METHOD2(SetFormat,
               void(media::VideoPixelFormat format,
                    const gfx::ColorSpace& color_space));
  MOCK_METHOD1(SetMinCapturePeriod, void(base::TimeDelta min_period));
  MOCK_METHOD1(SetMinSizeChangePeriod, void(base::TimeDelta));
  MOCK_METHOD3(SetResolutionConstraints,
               void(const gfx::Size& min_size,
                    const gfx::Size& max_size,
                    bool use_fixed_aspect_ratio));
  MOCK_METHOD1(SetAutoThrottlingEnabled, void(bool));
  void ChangeTarget(
      const base::Optional<viz::FrameSinkId>& frame_sink_id) final {
    DCHECK_NOT_ON_DEVICE_THREAD();
    MockChangeTarget(frame_sink_id ? *frame_sink_id : viz::FrameSinkId());
  }
  MOCK_METHOD1(MockChangeTarget, void(const viz::FrameSinkId& frame_sink_id));
  void Start(
      mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumer> consumer) final {
    DCHECK_NOT_ON_DEVICE_THREAD();
    consumer_.Bind(std::move(consumer));
    MockStart(consumer_.get());
  }
  MOCK_METHOD1(MockStart, void(viz::mojom::FrameSinkVideoConsumer* consumer));
  void Stop() final {
    DCHECK_NOT_ON_DEVICE_THREAD();
    consumer_.reset();
    MockStop();
  }
  MOCK_METHOD0(MockStop, void());
  MOCK_METHOD0(RequestRefreshFrame, void());
  MOCK_METHOD2(
      CreateOverlay,
      void(int32_t stacking_index,
           mojo::PendingReceiver<viz::mojom::FrameSinkVideoCaptureOverlay>
               receiver));

 private:
  mojo::Receiver<viz::mojom::FrameSinkVideoCapturer> receiver_{this};
  mojo::Remote<viz::mojom::FrameSinkVideoConsumer> consumer_;
};

// Represents the FrameSinkVideoConsumerFrameCallbacks instance in the VIZ
// process.
class MockFrameSinkVideoConsumerFrameCallbacks
    : public viz::mojom::FrameSinkVideoConsumerFrameCallbacks {
 public:
  MockFrameSinkVideoConsumerFrameCallbacks() = default;

  void Bind(
      mojo::PendingReceiver<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          receiver) {
    DCHECK_NOT_ON_DEVICE_THREAD();
    receiver_.Bind(std::move(receiver));
  }

  MOCK_METHOD0(Done, void());
  MOCK_METHOD1(ProvideFeedback, void(double utilization));

 private:
  mojo::Receiver<viz::mojom::FrameSinkVideoConsumerFrameCallbacks> receiver_{
      this};
};

// Mock for the VideoFrameReceiver, the point-of-injection of video frames into
// the video capture stack. It's mocked methods are called on the device thread.
// Some methods stash objects of interest, which test code must grab via the
// TakeXYZ() utility methods (called on the main thread).
class MockVideoFrameReceiver : public media::VideoFrameReceiver {
 public:
  using Buffer = media::VideoCaptureDevice::Client::Buffer;

  ~MockVideoFrameReceiver() override {
    DCHECK_ON_DEVICE_THREAD();
    EXPECT_TRUE(buffer_handles_.empty());
    EXPECT_TRUE(feedback_ids_.empty());
    EXPECT_TRUE(access_permissions_.empty());
    EXPECT_TRUE(frame_infos_.empty());
  }

  void OnNewBuffer(int buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) final {
    DCHECK_ON_DEVICE_THREAD();
    auto* const raw_pointer = buffer_handle.get();
    buffer_handles_[buffer_id] = std::move(buffer_handle);
    MockOnNewBuffer(buffer_id, raw_pointer);
  }
  MOCK_METHOD2(MockOnNewBuffer,
               void(int buffer_id,
                    media::mojom::VideoBufferHandle* buffer_handle));
  void OnFrameReadyInBuffer(
      int buffer_id,
      int frame_feedback_id,
      std::unique_ptr<Buffer::ScopedAccessPermission> buffer_read_permission,
      media::mojom::VideoFrameInfoPtr frame_info) final {
    DCHECK_ON_DEVICE_THREAD();
    feedback_ids_[buffer_id] = frame_feedback_id;
    auto* const raw_pointer_to_permission = buffer_read_permission.get();
    access_permissions_[buffer_id] = std::move(buffer_read_permission);
    auto* const raw_pointer_to_info = frame_info.get();
    frame_infos_[buffer_id] = std::move(frame_info);
    MockOnFrameReadyInBuffer(buffer_id, frame_feedback_id,
                             raw_pointer_to_permission, raw_pointer_to_info);
  }
  MOCK_METHOD4(MockOnFrameReadyInBuffer,
               void(int buffer_id,
                    int frame_feedback_id,
                    Buffer::ScopedAccessPermission* buffer_read_permission,
                    const media::mojom::VideoFrameInfo* frame_info));
  MOCK_METHOD1(OnBufferRetired, void(int buffer_id));
  MOCK_METHOD1(OnError, void(media::VideoCaptureError error));
  MOCK_METHOD1(OnFrameDropped, void(media::VideoCaptureFrameDropReason reason));
  MOCK_METHOD1(OnLog, void(const std::string& message));
  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD0(OnStopped, void());
  void OnStartedUsingGpuDecode() final { NOTREACHED(); }

  base::ReadOnlySharedMemoryRegion TakeBufferHandle(int buffer_id) {
    DCHECK_NOT_ON_DEVICE_THREAD();
    const auto it = buffer_handles_.find(buffer_id);
    if (it == buffer_handles_.end()) {
      ADD_FAILURE() << "Missing entry for buffer_id=" << buffer_id;
      return base::ReadOnlySharedMemoryRegion();
    }
    CHECK(it->second->is_read_only_shmem_region());
    auto buffer = std::move(it->second->get_read_only_shmem_region());
    buffer_handles_.erase(it);
    return buffer;
  }

  int TakeFeedbackId(int buffer_id) {
    DCHECK_NOT_ON_DEVICE_THREAD();
    const auto it = feedback_ids_.find(buffer_id);
    if (it == feedback_ids_.end()) {
      ADD_FAILURE() << "Missing entry for buffer_id=" << buffer_id;
      return -1;
    }
    const int feedback_id = it->second;
    feedback_ids_.erase(it);
    return feedback_id;
  }

  void ReleaseAccessPermission(int buffer_id) {
    DCHECK_NOT_ON_DEVICE_THREAD();
    const auto it = access_permissions_.find(buffer_id);
    if (it == access_permissions_.end()) {
      ADD_FAILURE() << "Missing entry for buffer_id=" << buffer_id;
      return;
    }
    access_permissions_.erase(it);
  }

  media::mojom::VideoFrameInfoPtr TakeVideoFrameInfo(int buffer_id) {
    DCHECK_NOT_ON_DEVICE_THREAD();
    const auto it = frame_infos_.find(buffer_id);
    if (it == frame_infos_.end()) {
      ADD_FAILURE() << "Missing entry for buffer_id=" << buffer_id;
      return media::mojom::VideoFrameInfoPtr();
    }
    media::mojom::VideoFrameInfoPtr info = std::move(it->second);
    frame_infos_.erase(it);
    return info;
  }

 private:
  base::flat_map<int, media::mojom::VideoBufferHandlePtr> buffer_handles_;
  base::flat_map<int, int> feedback_ids_;
  base::flat_map<int, std::unique_ptr<Buffer::ScopedAccessPermission>>
      access_permissions_;
  base::flat_map<int, media::mojom::VideoFrameInfoPtr> frame_infos_;
};

// A FrameSinkVideoCaptureDevice, but with CreateCapturer() overridden to bind
// to a MockFrameSinkVideoCapturer instead of the real thing.
class FrameSinkVideoCaptureDeviceForTest : public FrameSinkVideoCaptureDevice {
 public:
  explicit FrameSinkVideoCaptureDeviceForTest(
      MockFrameSinkVideoCapturer* capturer)
      : capturer_(capturer) {}

 protected:
  void CreateCapturer(mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer>
                          receiver) final {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            [](MockFrameSinkVideoCapturer* capturer,
               mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer>
                   receiver) { capturer->Bind(std::move(receiver)); },
            capturer_, std::move(receiver)));
  }

  MockFrameSinkVideoCapturer* const capturer_;
};

// Convenience macros to make a non-blocking FrameSinkVideoCaptureDevice method
// call on the device thread.
#define POST_DEVICE_METHOD_CALL0(method)                                \
  POST_DEVICE_TASK(base::BindOnce(&FrameSinkVideoCaptureDevice::method, \
                                  base::Unretained(device_.get())))
#define POST_DEVICE_METHOD_CALL(method, ...)                            \
  POST_DEVICE_TASK(base::BindOnce(&FrameSinkVideoCaptureDevice::method, \
                                  base::Unretained(device_.get()),      \
                                  __VA_ARGS__))

class FrameSinkVideoCaptureDeviceTest : public testing::Test {
 public:
  FrameSinkVideoCaptureDeviceTest()
      : task_environment_(BrowserTaskEnvironment::REAL_IO_THREAD) {}

  ~FrameSinkVideoCaptureDeviceTest() override { EXPECT_FALSE(device_); }

  void SetUp() override {
    // Create the FrameSinkVideoCaptureDevice on the device thread, and block
    // until complete.
    POST_DEVICE_TASK(base::BindOnce(
        [](FrameSinkVideoCaptureDeviceTest* test) {
          test->device_ = std::make_unique<FrameSinkVideoCaptureDeviceForTest>(
              &test->capturer_);
        },
        this));
    WAIT_FOR_DEVICE_TASKS();
  }

  void TearDown() override {
    // Destroy the FrameSinkVideoCaptureDevice on the device thread, and block
    // until complete.
    POST_DEVICE_TASK(base::BindOnce(
        [](FrameSinkVideoCaptureDeviceTest* test) { test->device_.reset(); },
        this));
    WAIT_FOR_DEVICE_TASKS();
    // Some objects owned by the FrameSinkVideoCaptureDevice may need to be
    // deleted on the UI thread, so run those tasks now.
    RUN_UI_TASKS();
  }

  // Starts-up the FrameSinkVideoCaptureDevice: Sets a frame sink target,
  // creates a capturer, sets the capture parameters; and checks that the mock
  // capturer receives the correct mojo method calls.
  void AllocateAndStartSynchronouslyWithExpectations(
      std::unique_ptr<media::VideoFrameReceiver> receiver) {
    EXPECT_CALL(capturer_, SetFormat(kFormat, _));
    EXPECT_CALL(capturer_, SetMinCapturePeriod(kMinCapturePeriod));
    EXPECT_CALL(capturer_,
                SetResolutionConstraints(kResolution, kResolution, _));
    constexpr viz::FrameSinkId frame_sink_id(1, 1);
    EXPECT_CALL(capturer_, MockChangeTarget(frame_sink_id));
    EXPECT_CALL(capturer_, MockStart(NotNull()));

    EXPECT_FALSE(capturer_.is_bound());
    POST_DEVICE_METHOD_CALL(OnTargetChanged, frame_sink_id);
    POST_DEVICE_METHOD_CALL(AllocateAndStartWithReceiver, GetCaptureParams(),
                            std::move(receiver));
    WAIT_FOR_DEVICE_TASKS();
    RUN_UI_TASKS();  // Run the task to create the capturer.
    EXPECT_TRUE(capturer_.is_bound());
    WAIT_FOR_DEVICE_TASKS();  // Run the task where the interface is bound, etc.
  }

  // Stops the FrameSinkVideoCaptureDevice and optionally checks that the mock
  // capturer received the Stop() call.
  void StopAndDeAllocateSynchronouslyWithExpectations(
      bool capturer_stopped_also) {
    EXPECT_CALL(capturer_, MockStop()).Times(capturer_stopped_also ? 1 : 0);
    POST_DEVICE_METHOD_CALL0(StopAndDeAllocate);
    WAIT_FOR_DEVICE_TASKS();
  }

  // Simulates what the VIZ capturer would do: Allocates a shared memory buffer,
  // populates it with video content, and calls OnFrameCaptured().
  void SimulateFrameCapture(
      int frame_number,
      MockFrameSinkVideoConsumerFrameCallbacks* callbacks) {
    // Allocate a buffer and fill it with values based on |frame_number|.
    base::MappedReadOnlyRegion region = mojo::CreateReadOnlySharedMemoryRegion(
        media::VideoFrame::AllocationSize(kFormat, kResolution));
    CHECK(region.IsValid());
    memset(region.mapping.memory(), GetFrameFillValue(frame_number),
           region.mapping.size());

    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks_remote;
    callbacks->Bind(callbacks_remote.InitWithNewPipeAndPassReceiver());
    // |callbacks_remote| is bound on the main thread, so it needs to be
    // re-bound to the device thread before calling OnFrameCaptured().
    POST_DEVICE_TASK(base::BindOnce(
        [](FrameSinkVideoCaptureDevice* device,
           base::ReadOnlySharedMemoryRegion data, int frame_number,
           mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
               callbacks_remote) {
          device->OnFrameCaptured(
              std::move(data),
              media::mojom::VideoFrameInfo::New(
                  kMinCapturePeriod * frame_number,
                  base::Value(base::Value::Type::DICTIONARY), kFormat,
                  kResolution, gfx::Rect(kResolution),
                  gfx::ColorSpace::CreateREC709(), nullptr),
              gfx::Rect(kResolution), std::move(callbacks_remote));
        },
        base::Unretained(device_.get()), std::move(region.region), frame_number,
        std::move(callbacks_remote)));
  }

  // Returns a byte value based on the given |frame_number|.
  static constexpr uint8_t GetFrameFillValue(int frame_number) {
    return (frame_number % 0x3f) << 2;
  }

  // Returns true if the |buffer| is filled with the correct byte value for the
  // given |frame_number|.
  static bool IsExpectedBufferContentForFrame(
      int frame_number,
      base::ReadOnlySharedMemoryRegion buffer) {
    const auto mapping = buffer.Map();
    const size_t frame_allocation_size =
        media::VideoFrame::AllocationSize(kFormat, kResolution);
    CHECK_LE(frame_allocation_size, mapping.size());
    const uint8_t* src = mapping.GetMemoryAs<const uint8_t>();
    const uint8_t expected_value = GetFrameFillValue(frame_number);
    for (size_t i = 0; i < frame_allocation_size; ++i) {
      if (src[i] != expected_value) {
        return false;
      }
    }
    return true;
  }

 protected:
  // See the threading notes at top of this file.
  BrowserTaskEnvironment task_environment_;

  NiceMock<MockFrameSinkVideoCapturer> capturer_;
  std::unique_ptr<FrameSinkVideoCaptureDevice> device_;
};

// Tests a normal session, progressing through the start, frame capture, and
// stop phases.
TEST_F(FrameSinkVideoCaptureDeviceTest, CapturesAndDeliversFrames) {
  auto receiver_ptr = std::make_unique<MockVideoFrameReceiver>();
  auto* const receiver = receiver_ptr.get();
  EXPECT_CALL(*receiver, OnStarted());
  EXPECT_CALL(*receiver, OnError(_)).Times(0);

  AllocateAndStartSynchronouslyWithExpectations(std::move(receiver_ptr));
  // From this point, there is no reason the capturer should be re-started.
  EXPECT_CALL(capturer_, MockStart(_)).Times(0);

  // Run 24 frames through the pipeline, one at a time. Then, run 24 more, two
  // at a time. Then, run 24 more, three at a time.
  constexpr int kNumFramesToDeliver = 24;
  constexpr int kMaxSimultaneousFrames = 3;
  int next_frame_number = 0;
  for (int in_flight_count = 1; in_flight_count <= kMaxSimultaneousFrames;
       ++in_flight_count) {
    for (int iteration = 0; iteration < kNumFramesToDeliver; ++iteration) {
      int buffer_ids[kMaxSimultaneousFrames] = {-1};
      MockFrameSinkVideoConsumerFrameCallbacks
          callbackses[kMaxSimultaneousFrames];

      // Simulate |in_flight_count| frame captures and expect the frames to be
      // delivered to the VideoFrameReceiver.
      const int first_frame_number = next_frame_number;
      for (int i = 0; i < in_flight_count; ++i) {
        Expectation new_buffer_called =
            EXPECT_CALL(*receiver, MockOnNewBuffer(Ge(0), NotNull()))
                .WillOnce(SaveArg<0>(&buffer_ids[i]));
        EXPECT_CALL(*receiver,
                    MockOnFrameReadyInBuffer(Eq(ByRef(buffer_ids[i])), Ge(0),
                                             NotNull(), NotNull()))
            .After(new_buffer_called);
        SimulateFrameCapture(next_frame_number, &callbackses[i]);
        ++next_frame_number;
        WAIT_FOR_DEVICE_TASKS();
      }

      // Confirm the VideoFrameReceiver was provided the correct buffer and
      // VideoFrameInfo struct for each frame in this batch.
      for (int frame_number = first_frame_number;
           frame_number < next_frame_number; ++frame_number) {
        const int buffer_id = buffer_ids[frame_number - first_frame_number];

        auto buffer = receiver->TakeBufferHandle(buffer_id);
        ASSERT_TRUE(buffer.IsValid());
        EXPECT_TRUE(
            IsExpectedBufferContentForFrame(frame_number, std::move(buffer)));

        const auto info = receiver->TakeVideoFrameInfo(buffer_id);
        ASSERT_TRUE(info);
        EXPECT_EQ(kMinCapturePeriod * frame_number, info->timestamp);
        EXPECT_EQ(kFormat, info->pixel_format);
        EXPECT_EQ(kResolution, info->coded_size);
        EXPECT_EQ(gfx::Rect(kResolution), info->visible_rect);
      }

      // Simulate the receiver providing the feedback and done notifications for
      // each frame and expect the FrameSinkVideoCaptureDevice to process these
      // notifications.
      for (int frame_number = first_frame_number;
           frame_number < next_frame_number; ++frame_number) {
        const int buffer_id = buffer_ids[frame_number - first_frame_number];
        MockFrameSinkVideoConsumerFrameCallbacks& callbacks =
            callbackses[frame_number - first_frame_number];

        const double fake_utilization =
            static_cast<double>(frame_number) / kNumFramesToDeliver;
        EXPECT_CALL(callbacks, ProvideFeedback(fake_utilization));
        EXPECT_CALL(callbacks, Done());
        EXPECT_CALL(*receiver, OnBufferRetired(buffer_id));

        const int feedback_id = receiver->TakeFeedbackId(buffer_id);
        POST_DEVICE_METHOD_CALL(OnUtilizationReport, feedback_id,
                                fake_utilization);
        receiver->ReleaseAccessPermission(buffer_id);
        WAIT_FOR_DEVICE_TASKS();
      }
    }
  }

  StopAndDeAllocateSynchronouslyWithExpectations(true /* capturer will stop */);
}

// Tests that a client request to Suspend() should stop consumption and ignore
// all refresh requests. Likewise, a client request to Resume() will
// re-establish consumption and allow refresh requests to propagate to the
// capturer again.
TEST_F(FrameSinkVideoCaptureDeviceTest, SuspendsAndResumes) {
  AllocateAndStartSynchronouslyWithExpectations(
      std::make_unique<NiceMock<MockVideoFrameReceiver>>());

  // A started device should have started the capturer, and any refresh frame
  // requests from the client should be propagated to it.
  {
    EXPECT_CALL(capturer_, RequestRefreshFrame());
    POST_DEVICE_METHOD_CALL0(RequestRefreshFrame);
    WAIT_FOR_DEVICE_TASKS();
  }

  // Simulate a client request that capture be suspended. The capturer should
  // receive a Stop() message.
  {
    EXPECT_CALL(capturer_, MockStart(_)).Times(0);
    EXPECT_CALL(capturer_, MockStop());
    POST_DEVICE_METHOD_CALL0(MaybeSuspend);
    WAIT_FOR_DEVICE_TASKS();
  }

  // A suspended device should not propagate any refresh frame requests.
  {
    EXPECT_CALL(capturer_, RequestRefreshFrame()).Times(0);
    POST_DEVICE_METHOD_CALL0(RequestRefreshFrame);
    WAIT_FOR_DEVICE_TASKS();
  }

  // Simulate a client request that capture be resumed. The capturer should
  // receive a Start() message.
  {
    EXPECT_CALL(capturer_, MockStart(NotNull()));
    EXPECT_CALL(capturer_, MockStop()).Times(0);
    POST_DEVICE_METHOD_CALL0(Resume);
    WAIT_FOR_DEVICE_TASKS();
  }

  // Now refresh frame requests should propagate again.
  {
    EXPECT_CALL(capturer_, RequestRefreshFrame());
    POST_DEVICE_METHOD_CALL0(RequestRefreshFrame);
    WAIT_FOR_DEVICE_TASKS();
  }

  StopAndDeAllocateSynchronouslyWithExpectations(true /* capturer will stop */);
}

// Tests that the FrameSinkVideoCaptureDevice will shutdown on a fatal error and
// refuse to be started again.
TEST_F(FrameSinkVideoCaptureDeviceTest, ShutsDownOnFatalError) {
  auto receiver_ptr = std::make_unique<MockVideoFrameReceiver>();
  auto* receiver = receiver_ptr.get();
  Sequence sequence;
  EXPECT_CALL(*receiver, OnStarted()).InSequence(sequence);
  EXPECT_CALL(*receiver, OnLog(StrNe(""))).InSequence(sequence);
  EXPECT_CALL(*receiver, OnError(_)).InSequence(sequence);

  AllocateAndStartSynchronouslyWithExpectations(std::move(receiver_ptr));

  // Notify the device that the target frame sink was lost. This should stop
  // consumption, unbind the capturer, log an error with the VideoFrameReceiver,
  // and destroy the VideoFrameReceiver.
  {
    EXPECT_CALL(capturer_, MockChangeTarget(viz::FrameSinkId()));
    EXPECT_CALL(capturer_, MockStop());
    POST_DEVICE_METHOD_CALL0(OnTargetPermanentlyLost);
    WAIT_FOR_DEVICE_TASKS();
  }

  // Shutdown the device. However, the fatal error already stopped consumption,
  // so don't expect the capturer to be stopped again.
  StopAndDeAllocateSynchronouslyWithExpectations(false);

  // Now, any further attempts to start the FrameSinkVideoCaptureDevice again
  // should fail. The VideoFrameReceiver will be provided the same error
  // message.
  receiver_ptr = std::make_unique<MockVideoFrameReceiver>();
  receiver = receiver_ptr.get();
  {
    EXPECT_CALL(*receiver, OnStarted()).Times(0);
    EXPECT_CALL(*receiver, OnLog(StrNe("")));
    EXPECT_CALL(*receiver, OnError(_));
    EXPECT_CALL(capturer_, MockStart(_)).Times(0);

    POST_DEVICE_METHOD_CALL(AllocateAndStartWithReceiver, GetCaptureParams(),
                            std::move(receiver_ptr));
    WAIT_FOR_DEVICE_TASKS();
  }
}

}  // namespace
}  // namespace content
