// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/platform_video_frame_pool.h"

#include <utility>

#include "base/logging.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "media/base/video_util.h"
#include "media/gpu/chromeos/gpu_buffer_layout.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/media_buildflags.h"

namespace media {

namespace {

// The default method to create frames.
scoped_refptr<VideoFrame> DefaultCreateFrame(
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    bool use_protected,
    base::TimeDelta timestamp) {
  scoped_refptr<VideoFrame> frame = CreateGpuMemoryBufferVideoFrame(
      gpu_memory_buffer_factory, format, coded_size, visible_rect, natural_size,
      timestamp,
      use_protected ? gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE
                    : gfx::BufferUsage::SCANOUT_VDA_WRITE);
  if (frame && use_protected) {
    media::VideoFrameMetadata frame_metadata;
    frame_metadata.protected_video = true;
    frame_metadata.hw_protected = true;
    frame->set_metadata(frame_metadata);
  }
  return frame;
}

}  // namespace

PlatformVideoFramePool::PlatformVideoFramePool(
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory)
    : create_frame_cb_(base::BindRepeating(&DefaultCreateFrame)),
      gpu_memory_buffer_factory_(gpu_memory_buffer_factory) {
  DVLOGF(4);
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

PlatformVideoFramePool::~PlatformVideoFramePool() {
  if (parent_task_runner_)
    DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  frames_in_use_.clear();
  free_frames_.clear();
  weak_this_factory_.InvalidateWeakPtrs();
}

// static
gfx::GpuMemoryBufferId PlatformVideoFramePool::GetGpuMemoryBufferId(
    const VideoFrame& frame) {
  DCHECK_EQ(frame.storage_type(),
            VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER);
  DCHECK(frame.GetGpuMemoryBuffer());
  return frame.GetGpuMemoryBuffer()->GetId();
}

scoped_refptr<VideoFrame> PlatformVideoFramePool::GetFrame() {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  if (!frame_layout_) {
    VLOGF(1) << "Please call Initialize() first.";
    return nullptr;
  }

  const VideoPixelFormat format = frame_layout_->fourcc().ToVideoPixelFormat();
  const gfx::Size& coded_size = frame_layout_->size();
  if (free_frames_.empty()) {
    if (GetTotalNumFrames_Locked() >= max_num_frames_)
      return nullptr;

    // We want to be able to re-use |new_frame| if possible even when the pool
    // is re-initialized with a different |visible_rect_|. DRM framebuffers can
    // be re-used if the visible-rect-from-the-origin (a.k.a "usable area") is
    // the same. For example, say we have a |visible_rect_| of (10, 20), 640x360
    // (DRM framebuffer of size 650x380); if we re-initialize the pool with
    // (5, 2), 645x378, we can re-use the resources, but if we re-initialize
    // with (10, 20), 100x100 we cannot (even though it's contained in the
    // former). Hence the use of GetRectSizeFromOrigin() to calculate the
    // visible rect for |new_frame|.
    scoped_refptr<VideoFrame> new_frame =
        create_frame_cb_.Run(gpu_memory_buffer_factory_, format, coded_size,
                             gfx::Rect(GetRectSizeFromOrigin(visible_rect_)),
                             coded_size, use_protected_, base::TimeDelta());
    if (!new_frame)
      return nullptr;

    InsertFreeFrame_Locked(std::move(new_frame));
  }

  DCHECK(!free_frames_.empty());
  scoped_refptr<VideoFrame> origin_frame = std::move(free_frames_.back());
  free_frames_.pop_back();
  DCHECK_EQ(origin_frame->format(), format);
  DCHECK_EQ(origin_frame->coded_size(), coded_size);

  scoped_refptr<VideoFrame> wrapped_frame = VideoFrame::WrapVideoFrame(
      origin_frame, format, visible_rect_, natural_size_);
  DCHECK(wrapped_frame);
  frames_in_use_.emplace(GetGpuMemoryBufferId(*wrapped_frame),
                         origin_frame.get());
  wrapped_frame->AddDestructionObserver(
      base::BindOnce(&PlatformVideoFramePool::OnFrameReleasedThunk, weak_this_,
                     parent_task_runner_, std::move(origin_frame)));

  // Clear all metadata before returning to client, in case origin frame has any
  // unrelated metadata.
  wrapped_frame->clear_metadata();

  // We need to put this metadata in the wrapped frame if we are in protected
  // mode.
  if (use_protected_) {
    media::VideoFrameMetadata frame_metadata;
    frame_metadata.protected_video = true;
    frame_metadata.hw_protected = true;
    wrapped_frame->set_metadata(frame_metadata);
  }

  return wrapped_frame;
}

base::Optional<GpuBufferLayout> PlatformVideoFramePool::Initialize(
    const Fourcc& fourcc,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    size_t max_num_frames,
    bool use_protected) {
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  // Only support the Fourcc that could map to VideoPixelFormat.
  VideoPixelFormat format = fourcc.ToVideoPixelFormat();
  if (format == PIXEL_FORMAT_UNKNOWN) {
    VLOGF(1) << "Unsupported fourcc: " << fourcc.ToString();
    return base::nullopt;
  }

#if !BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  if (use_protected) {
    VLOGF(1) << "Protected buffers unsupported";
    return base::nullopt;
  }
#endif

  // If the frame layout changed we need to allocate new frames so we will clear
  // the pool here. If only the visible rect or natural size changed, we don't
  // need to allocate new frames (unless the change in the visible rect causes a
  // change in the size of the DRM framebuffer, see note below): we'll just
  // update the properties of wrapped frames returned by GetFrame().
  //
  // NOTE: It is assumed layout is determined by |format|, |coded_size|, and
  // possibly |visible_rect|. The reason for the "possibly" is that
  // |visible_rect| is used to compute the size of the DRM framebuffer for
  // hardware overlay purposes. The caveat is that different visible rectangles
  // can map to the same framebuffer size, i.e., all the visible rectangles with
  // the same bottom-right corner map to the same framebuffer size.
  if (!IsSameFormat_Locked(format, coded_size, visible_rect, use_protected)) {
    DVLOGF(4) << "The video frame format is changed. Clearing the pool.";
    free_frames_.clear();

    // Create a temporary frame in order to know VideoFrameLayout that
    // VideoFrame that will be allocated in GetFrame() has.
    auto frame = create_frame_cb_.Run(gpu_memory_buffer_factory_, format,
                                      coded_size, visible_rect, natural_size,
                                      use_protected, base::TimeDelta());
    if (!frame) {
      VLOGF(1) << "Failed to create video frame " << format << " (fourcc "
               << fourcc.ToString() << ")";
      return base::nullopt;
    }
    frame_layout_ = GpuBufferLayout::Create(fourcc, frame->coded_size(),
                                            frame->layout().planes(),
                                            frame->layout().modifier());
  }

  visible_rect_ = visible_rect;
  natural_size_ = natural_size;
  max_num_frames_ = max_num_frames;
  use_protected_ = use_protected;

  // The pool might become available because of |max_num_frames_| increased.
  // Notify the client if so.
  if (frame_available_cb_ && !IsExhausted_Locked())
    std::move(frame_available_cb_).Run();

  return frame_layout_;
}

bool PlatformVideoFramePool::IsExhausted() {
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  return IsExhausted_Locked();
}

bool PlatformVideoFramePool::IsExhausted_Locked() {
  DVLOGF(4);
  lock_.AssertAcquired();

  return free_frames_.empty() && GetTotalNumFrames_Locked() >= max_num_frames_;
}

VideoFrame* PlatformVideoFramePool::UnwrapFrame(
    const VideoFrame& wrapped_frame) {
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  auto it = frames_in_use_.find(GetGpuMemoryBufferId(wrapped_frame));
  return (it == frames_in_use_.end()) ? nullptr : it->second;
}

void PlatformVideoFramePool::NotifyWhenFrameAvailable(base::OnceClosure cb) {
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  if (!IsExhausted_Locked()) {
    parent_task_runner_->PostTask(FROM_HERE, std::move(cb));
    return;
  }

  frame_available_cb_ = std::move(cb);
}

// static
void PlatformVideoFramePool::OnFrameReleasedThunk(
    base::Optional<base::WeakPtr<PlatformVideoFramePool>> pool,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<VideoFrame> origin_frame) {
  DCHECK(pool);
  DVLOGF(4);

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&PlatformVideoFramePool::OnFrameReleased, *pool,
                                std::move(origin_frame)));
}

void PlatformVideoFramePool::OnFrameReleased(
    scoped_refptr<VideoFrame> origin_frame) {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  gfx::GpuMemoryBufferId frame_id = GetGpuMemoryBufferId(*origin_frame);
  auto it = frames_in_use_.find(frame_id);
  DCHECK(it != frames_in_use_.end());
  frames_in_use_.erase(it);

  if (IsSameFormat_Locked(origin_frame->format(), origin_frame->coded_size(),
                          origin_frame->visible_rect(),
                          origin_frame->metadata().hw_protected)) {
    InsertFreeFrame_Locked(std::move(origin_frame));
  }

  if (frame_available_cb_ && !IsExhausted_Locked())
    std::move(frame_available_cb_).Run();
}

void PlatformVideoFramePool::InsertFreeFrame_Locked(
    scoped_refptr<VideoFrame> frame) {
  DCHECK(frame);
  DVLOGF(4);
  lock_.AssertAcquired();

  if (GetTotalNumFrames_Locked() < max_num_frames_)
    free_frames_.push_back(std::move(frame));
}

size_t PlatformVideoFramePool::GetTotalNumFrames_Locked() const {
  DVLOGF(4);
  lock_.AssertAcquired();

  return free_frames_.size() + frames_in_use_.size();
}

bool PlatformVideoFramePool::IsSameFormat_Locked(VideoPixelFormat format,
                                                 const gfx::Size& coded_size,
                                                 const gfx::Rect& visible_rect,
                                                 bool use_protected) const {
  DVLOGF(4);
  lock_.AssertAcquired();

  return frame_layout_ &&
         frame_layout_->fourcc().ToVideoPixelFormat() == format &&
         frame_layout_->size() == coded_size &&
         GetRectSizeFromOrigin(visible_rect_) ==
             GetRectSizeFromOrigin(visible_rect) &&
         use_protected_ == use_protected;
}

size_t PlatformVideoFramePool::GetPoolSizeForTesting() {
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  return free_frames_.size();
}

}  // namespace media
