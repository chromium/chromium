// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/fake_video_capture_stack.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/capture/frame_test_util.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video_capture_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace content {

namespace {

gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() {
  gpu::GpuChannelEstablishFactory* factory =
      content::BrowserMainLoop::GetInstance()->gpu_channel_establish_factory();
  if (!factory) {
    return nullptr;
  }

  return factory->GetGpuMemoryBufferManager();
}

}  // namespace

FakeVideoCaptureStack::FakeVideoCaptureStack() = default;

FakeVideoCaptureStack::~FakeVideoCaptureStack() = default;

void FakeVideoCaptureStack::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  frames_.clear();
  last_frame_timestamp_ = base::TimeDelta::Min();
}

// A minimal implementation of VideoFrameReceiver that wraps buffers into
// VideoFrame instances and forwards all relevant callbacks and data to the
// parent FakeVideoCaptureStack. The implemented `media::VideoFrameReceiver`
// methods will be forwarded to a separate thread for processing in order to,
// unblock the thread on which they arrive, and the results of this processing
// will then be posted to the `FakeVideoCaptureStack` owning this instance,
// on the task runner that constructed this instance.
class FakeVideoCaptureStackReceiver final : public media::VideoFrameReceiver {
 public:
  // Creates VideoFrameReceiver that notifies `capture_stack` when new frames
  // arrive (among other things). The `capture_stack` will be called into from
  // the current sequence.
  explicit FakeVideoCaptureStackReceiver(
      base::WeakPtr<FakeVideoCaptureStack> capture_stack)
      : capture_stack_(std::move(capture_stack)),
        pool_(base::MakeRefCounted<base::UnsafeSharedMemoryPool>()),
        capture_stack_task_runner_(
            base::SequencedTaskRunner::GetCurrentDefault()) {
    DETACH_FROM_SEQUENCE(receiver_sequence_checker_);

    // This will DCHECK if we're not currently on UI thread (due to use of
    // `content::BrowserMainLoop::GetInstance()` in the impl.), but that is
    // fine since the tests currently call us on UI thread:
    gmb_manager_ = GetGpuMemoryBufferManager();

    receiver_thread_ =
        std::make_unique<base::Thread>("fake-capture-stack-receiver");
    receiver_thread_->StartAndWaitForTesting();
  }

  void WaitForReceiver() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(capture_stack_sequence_checker_);

    base::RunLoop runLoop(base::RunLoop::Type::kNestableTasksAllowed);

    receiver_thread_->task_runner()->PostTask(FROM_HERE, runLoop.QuitClosure());

    runLoop.Run();
  }

  base::WeakPtr<FakeVideoCaptureStackReceiver> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  FakeVideoCaptureStackReceiver(const FakeVideoCaptureStackReceiver&) = delete;
  FakeVideoCaptureStackReceiver& operator=(
      const FakeVideoCaptureStackReceiver&) = delete;

  ~FakeVideoCaptureStackReceiver() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(capture_stack_sequence_checker_);

    receiver_thread_ = nullptr;
  }

 private:
  using Buffer = media::VideoCaptureDevice::Client::Buffer;

  void OnCaptureConfigurationChanged() override {}

  void OnNewBuffer(int buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(capture_stack_sequence_checker_);

    // Unretained is safe since we own the thread to which we're posting.
    receiver_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &FakeVideoCaptureStackReceiver::OnNewBufferOnReceiverThread,
            base::Unretained(this), buffer_id, std::move(buffer_handle)));
  }

  void OnNewBufferOnReceiverThread(
      int buffer_id,
      media::mojom::VideoBufferHandlePtr buffer_handle) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(receiver_sequence_checker_);

    buffers_[buffer_id] = std::move(buffer_handle);
  }

  scoped_refptr<media::VideoFrame> GetVideoFrameFromSharedMemory(
      media::ReadyFrameInBuffer frame,
      base::ReadOnlySharedMemoryMapping mapping) {
    CHECK(mapping.IsValid());

    const auto& frame_format = media::VideoCaptureFormat(
        frame.frame_info->coded_size, 0.0f, frame.frame_info->pixel_format);
    CHECK_LE(media::VideoFrame::AllocationSize(frame_format.pixel_format,
                                               frame_format.frame_size),
             mapping.size());

    auto video_frame = media::VideoFrame::WrapExternalData(
        frame.frame_info->pixel_format, frame.frame_info->coded_size,
        frame.frame_info->visible_rect, frame.frame_info->visible_rect.size(),
        mapping.GetMemoryAs<const uint8_t>(), mapping.size(),
        frame.frame_info->timestamp);
    CHECK(video_frame);

    video_frame->set_metadata(frame.frame_info->metadata);
    video_frame->set_color_space(frame.frame_info->color_space);

    // This destruction observer will unmap the shared memory when the
    // VideoFrame goes out-of-scope.
    video_frame->AddDestructionObserver(base::BindOnce(
        [](base::ReadOnlySharedMemoryMapping) {}, std::move(mapping)));
    // This destruction observer will notify the video capture device once all
    // downstream code is done using the VideoFrame.
    video_frame->AddDestructionObserver(base::BindOnce(
        [](std::unique_ptr<Buffer::ScopedAccessPermission> access) {},
        std::move(frame.buffer_read_permission)));

    return video_frame;
  }

  scoped_refptr<media::VideoFrame> GetVideoFrameFromGpuMemoryBuffer(
      media::ReadyFrameInBuffer frame,
      const gfx::GpuMemoryBufferHandle& gmb_handle) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(receiver_sequence_checker_);

    CHECK(!gmb_handle.is_null());
    CHECK_EQ(frame.frame_info->pixel_format,
             media::VideoPixelFormat::PIXEL_FORMAT_NV12);

    gpu::GpuMemoryBufferSupport gmb_support;
    std::unique_ptr<gfx::GpuMemoryBuffer> gmb =
        gmb_support.CreateGpuMemoryBufferImplFromHandle(
            gmb_handle.Clone(), frame.frame_info->coded_size,
            gfx::BufferFormat::YUV_420_BIPLANAR,
            gfx::BufferUsage::SCANOUT_VEA_CPU_READ, base::DoNothing(),
            gmb_manager_, pool_);
    CHECK(gmb);

    gfx::Size size = gmb->GetSize();
    auto video_frame = media::VideoFrame::WrapExternalGpuMemoryBuffer(
        frame.frame_info->visible_rect, size, std::move(gmb),
        frame.frame_info->timestamp);
    CHECK(video_frame);

    video_frame->set_metadata(frame.frame_info->metadata);
    video_frame->set_color_space(frame.frame_info->color_space);

    auto mapped_frame = media::ConvertToMemoryMappedFrame(video_frame);
    CHECK(mapped_frame);

    // This destruction observer will notify the video capture device once all
    // downstream code is done using the VideoFrame.
    mapped_frame->AddDestructionObserver(base::BindOnce(
        [](std::unique_ptr<Buffer::ScopedAccessPermission> access) {},
        std::move(frame.buffer_read_permission)));

    return mapped_frame;
  }

  void OnFrameReadyInBuffer(media::ReadyFrameInBuffer frame) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(capture_stack_sequence_checker_);

    // Unretained is safe since we own the thread to which we're posting.
    // This implementation does not forward scaled frames.
    receiver_thread_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&FakeVideoCaptureStackReceiver::
                                      OnFrameReadyInBufferOnReceiverThread,
                                  base::Unretained(this), std::move(frame)));
  }

  void OnFrameReadyInBufferOnReceiverThread(media::ReadyFrameInBuffer frame) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(receiver_sequence_checker_);

    const auto it = buffers_.find(frame.buffer_id);
    CHECK(it != buffers_.end());

    CHECK(it->second->is_read_only_shmem_region() ||
          it->second->is_gpu_memory_buffer_handle());

    scoped_refptr<media::VideoFrame> video_frame = nullptr;
    if (it->second->is_read_only_shmem_region()) {
      video_frame = GetVideoFrameFromSharedMemory(
          std::move(frame), it->second->get_read_only_shmem_region().Map());
    } else {
      video_frame = GetVideoFrameFromGpuMemoryBuffer(
          std::move(frame), it->second->get_gpu_memory_buffer_handle());
    }

    // This implementation does not forward scaled frames.
    capture_stack_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&FakeVideoCaptureStack::OnReceivedFrame,
                                  capture_stack_, std::move(video_frame)));
  }

  void OnBufferRetired(int buffer_id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(capture_stack_sequence_checker_);

    // Unretained is safe since we own the thread to which we're posting.
    receiver_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &FakeVideoCaptureStackReceiver::OnBufferRetiredOnReceiverThread,
            base::Unretained(this), buffer_id));
  }

  void OnBufferRetiredOnReceiverThread(int buffer_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(receiver_sequence_checker_);

    const auto it = buffers_.find(buffer_id);
    CHECK(it != buffers_.end());
    buffers_.erase(it);
  }

  void OnError(media::VideoCaptureError) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(capture_stack_sequence_checker_);

    capture_stack_->SetErrorOccurred();
  }

  void OnFrameDropped(media::VideoCaptureFrameDropReason) override {}

  void OnNewSubCaptureTargetVersion(
      uint32_t sub_capture_target_version) override {}

  void OnFrameWithEmptyRegionCapture() override {}

  void OnLog(const std::string& message) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(capture_stack_sequence_checker_);

    capture_stack_->OnLog(message);
  }

  void OnStarted() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(capture_stack_sequence_checker_);

    capture_stack_->SetStarted();
  }

  void OnStartedUsingGpuDecode() override { NOTREACHED_IN_MIGRATION(); }

  void OnStopped() override {}

  // WeakPtr is thread-safe to copy/move around, but the deref of
  // `capture_stack_` needs to happen on `capture_stack_sequence_checker_`.
  // Since there's one instance where we copy the pointer around that doesn't
  // happen on the right sequence, we cannot use `GUARDED_BY_CONTEXT()` here.
  const base::WeakPtr<FakeVideoCaptureStack> capture_stack_;

  base::flat_map<int, media::mojom::VideoBufferHandlePtr> buffers_
      GUARDED_BY_CONTEXT(receiver_sequence_checker_);

  // Needed to create `gfx::GpuMemoryBuffers` from
  // `gfx::GpuMemoryBufferHandle`s. Will be populated at construction time. Must
  // be obtained on the UI thread.
  raw_ptr<gpu::GpuMemoryBufferManager> gmb_manager_
      GUARDED_BY_CONTEXT(receiver_sequence_checker_) = nullptr;

  // Needed to create `gfx::GpuMemoryBuffers` from
  // `gfx::GpuMemoryBufferHandle`s. Will be populated at construction time.
  scoped_refptr<base::UnsafeSharedMemoryPool> pool_
      GUARDED_BY_CONTEXT(receiver_sequence_checker_);

  // Task runner on which we should be calling into capture stack:
  scoped_refptr<base::SequencedTaskRunner> capture_stack_task_runner_;

  // Thread that will be processing the `media::VideoFrameReceiver` methods.
  // This is needed for Windows if using GpuMemoryBuffers, since mapping GMBs
  // incurs a mojo call and is supposed to be a synchronous operation, which
  // means that the mapping thread will be blocked. If it so happens that the
  // mapping thread is also responsible for processing mojo responses (as is
  // the case if we don't introduce `receiver_thread_`), we will cause a
  // deadlock.
  // As per `base::Thread` documentation, can only be used from a sequence it
  // was started on (in our case, same as `capture_stack_task_runner_`).
  std::unique_ptr<base::Thread> receiver_thread_
      GUARDED_BY_CONTEXT(capture_stack_sequence_checker_);

  // Sequence checker for parts of the class that can be accessed only on
  // the `receiver_thread_`:
  SEQUENCE_CHECKER(receiver_sequence_checker_);

  // Sequence checker for the `receiver_thread_` member variable, since
  // `base::Thread` documents that it can only be accessed from a sequence that
  // started the thread. The name reflects the fact that we're created by
  // `FakeVideoCaptureStack` and that's where we start the thread.
  SEQUENCE_CHECKER(capture_stack_sequence_checker_);

  base::WeakPtrFactory<FakeVideoCaptureStackReceiver> weak_ptr_factory_{this};
};

std::unique_ptr<media::VideoFrameReceiver>
FakeVideoCaptureStack::CreateFrameReceiver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!receiver_);

  auto result = std::make_unique<FakeVideoCaptureStackReceiver>(
      weak_ptr_factory_.GetWeakPtr());

  receiver_ = result->GetWeakPtr();

  return result;
}

SkBitmap FakeVideoCaptureStack::NextCapturedFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!frames_.empty());
  media::VideoFrame& frame = *(frames_.front());
  SkBitmap bitmap = FrameTestUtil::ConvertToBitmap(frame);
  frames_.pop_front();
  return bitmap;
}

void FakeVideoCaptureStack::ClearCapturedFramesQueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  frames_.clear();
}

void FakeVideoCaptureStack::ExpectHasLogMessages() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  EXPECT_FALSE(log_messages_.empty());
  while (!log_messages_.empty()) {
    VLOG(1) << "Next log message: " << log_messages_.front();
    log_messages_.pop_front();
  }
}

void FakeVideoCaptureStack::ExpectNoLogMessages() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  while (!log_messages_.empty()) {
    ADD_FAILURE() << "Unexpected log message: " << log_messages_.front();
    log_messages_.pop_front();
  }
}

void FakeVideoCaptureStack::OnReceivedFrame(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Frame timestamps should be monotionically increasing.
  EXPECT_LT(last_frame_timestamp_, frame->timestamp());
  last_frame_timestamp_ = frame->timestamp();

  EXPECT_TRUE(frame->ColorSpace().IsValid());

  if (on_frame_received_) {
    on_frame_received_.Run(frame.get());
  }

  frames_.emplace_back(std::move(frame));
}

// Returns true if the device called VideoFrameReceiver::OnStarted().
bool FakeVideoCaptureStack::Started() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  WaitForReceiver();

  return started_;
}

// Returns true if the device called VideoFrameReceiver::OnError().
bool FakeVideoCaptureStack::ErrorOccurred() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  WaitForReceiver();

  return error_occurred_;
}

// Accessors to capture frame queue.
bool FakeVideoCaptureStack::HasCapturedFrames() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  WaitForReceiver();

  return !frames_.empty();
}

void FakeVideoCaptureStack::WaitForReceiver() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!receiver_) {
    return;
  }

  receiver_->WaitForReceiver();
}

}  // namespace content
