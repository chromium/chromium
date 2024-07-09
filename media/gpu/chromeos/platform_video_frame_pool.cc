// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/platform_video_frame_pool.h"

#include <optional>
#include <utility>

#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/base/video_util.h"
#include "media/gpu/chromeos/gpu_buffer_layout.h"
#include "media/gpu/chromeos/native_pixmap_frame_resource.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/media_buildflags.h"

namespace media {

namespace {

// This needs to be synchronized with the frame type from DefaultCreateFrame().
// There is a runtime CHECK() to validate this.
constexpr VideoFrame::StorageType kDefaultFrameStorageType =
    VideoFrame::STORAGE_DMABUFS;

// The default method to create frames.
CroStatus::Or<scoped_refptr<FrameResource>> DefaultCreateFrame(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    bool use_protected,
    bool use_linear_buffers,
    bool needs_detiling,
    base::TimeDelta timestamp) {
  if (use_protected && use_linear_buffers) {
    VLOGF(1) << "Linear buffers are unsupported when |use_protected| is true.";
    return CroStatus::Codes::kFailedToCreateVideoFrame;
  }

  scoped_refptr<FrameResource> frame = NativePixmapFrameResource::Create(
      format, coded_size, visible_rect, natural_size, timestamp,
      use_protected
          ? gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE
          : (use_linear_buffers ? gfx::BufferUsage::SCANOUT_CPU_READ_WRITE
                                : gfx::BufferUsage::SCANOUT_VDA_WRITE));

  if (!frame)
    return CroStatus::Codes::kFailedToCreateVideoFrame;

  // A SCANOUT usage was requested for the allocated |frame|, so there's a
  // possibility that it can be promoted to overlay, mark it so.
  frame->metadata().allow_overlay = true;
  frame->metadata().protected_video = use_protected;
  frame->metadata().hw_protected = use_protected;

#if defined(ARCH_CPU_ARM_FAMILY)
  if (base::FeatureList::IsEnabled(media::kEnableProtectedVulkanDetiling)) {
    frame->metadata().needs_detiling = needs_detiling;
  }
#endif

  return frame;
}

}  // namespace

PlatformVideoFramePool::PlatformVideoFramePool()
    : create_frame_cb_(base::BindRepeating(&DefaultCreateFrame)),
      frame_storage_type_(kDefaultFrameStorageType) {
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

scoped_refptr<FrameResource> PlatformVideoFramePool::GetFrame() {
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
    //
    // TODO(b/230370976): after https://crrev.com/c/3597211,
    // PlatformVideoFramePool doesn't use a GpuMemoryBufferFactory for
    // allocating dma-bufs which means DRM framebuffers won't be created for a
    // dma-buf at allocation time (instead, it will be created at the moment of
    // creating a SharedImage). That means that we probably don't need to take
    // the |visible_rect_| into account for IsSameFormat_Locked() any more which
    // implies that we can create |new_frame| using gfx::Rect(coded_size) as
    // the visible rectangle.
    CHECK(use_linear_buffers_.has_value());
    CroStatus::Or<scoped_refptr<FrameResource>> new_frame =
        create_frame_cb_.Run(
            format, coded_size, gfx::Rect(GetRectSizeFromOrigin(visible_rect_)),
            coded_size, use_protected_, *use_linear_buffers_,
            frame_layout_->fourcc() == Fourcc(Fourcc::MM21) ||
                frame_layout_->fourcc() == Fourcc(Fourcc::MT2T),
            base::TimeDelta());
    if (!new_frame.has_value()) {
      // TODO(crbug.com/c/1103510) Push the error up instead of dropping it.
      return nullptr;
    }

    CHECK(*new_frame);
    // This passes because |frame_storage_type_| is set to match the StorageType
    // of frames produced by |create_frame_cb_|. When |create_frame_cb_| is set
    // to DefaultCreateFrame(), then |frame_storage_type_| is set to
    // |kDefaultFrameStorageType|, which is hardcoded to match the storage type
    // used by DefaultCreateFrame(). When |create_frame_cb_| has been set by
    // SetCustomAllocator(), then |frame_storage_type_| is expected to be
    // correctly // set by the caller.
    CHECK_EQ((*new_frame)->storage_type(), frame_storage_type_);

    InsertFreeFrame_Locked(std::move(new_frame).value());
  }

  DCHECK(!free_frames_.empty());
  scoped_refptr<FrameResource> origin_frame = std::move(free_frames_.back());
  free_frames_.pop_back();
  DCHECK_EQ(origin_frame->format(), format);
  DCHECK_EQ(origin_frame->coded_size(), coded_size);

  scoped_refptr<FrameResource> wrapped_frame =
      origin_frame->CreateWrappingFrame(visible_rect_, natural_size_);
  DCHECK(wrapped_frame);
  frames_in_use_.emplace(wrapped_frame->GetSharedMemoryId(),
                         origin_frame.get());
  wrapped_frame->AddDestructionObserver(
      base::BindOnce(&PlatformVideoFramePool::OnFrameReleasedThunk, weak_this_,
                     parent_task_runner_, std::move(origin_frame)));

  DCHECK_EQ(wrapped_frame->metadata().protected_video, use_protected_);
  DCHECK_EQ(wrapped_frame->metadata().hw_protected, use_protected_);

  return wrapped_frame;
}

VideoFrame::StorageType PlatformVideoFramePool::GetFrameStorageType() const {
  base::AutoLock auto_lock(lock_);
  return frame_storage_type_;
}

PlatformVideoFramePool* PlatformVideoFramePool::AsPlatformVideoFramePool() {
  return this;
}

CroStatus::Or<GpuBufferLayout> PlatformVideoFramePool::Initialize(
    const Fourcc& fourcc,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    size_t max_num_frames,
    bool use_protected,
    bool use_linear_buffers) {
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  CHECK(!use_linear_buffers_ || *use_linear_buffers_ == use_linear_buffers);
  use_linear_buffers_ = use_linear_buffers;

  // Only support the Fourcc that could map to VideoPixelFormat.
  VideoPixelFormat format = fourcc.ToVideoPixelFormat();
  if (format == PIXEL_FORMAT_UNKNOWN) {
    VLOGF(1) << "Unsupported fourcc: " << fourcc.ToString();
    return CroStatus::Codes::kFourccUnknownFormat;
  }

#if !BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  if (use_protected) {
    VLOGF(1) << "Protected buffers unsupported";
    return CroStatus::Codes::kProtectedContentUnsupported;
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
  //
  // TODO(b/230370976): after https://crrev.com/c/3597211,
  // PlatformVideoFramePool doesn't use a GpuMemoryBufferFactory for allocating
  // dma-bufs which means DRM framebuffers won't be created for a dma-buf at
  // allocation time (instead, it will be created at the moment of creating a
  // SharedImage). That means that we probably don't need to take the
  // |visible_rect| into account for IsSameFormat_Locked() any more.
  if (!IsSameFormat_Locked(format, coded_size, visible_rect, use_protected)) {
    DVLOGF(4) << "The video frame format is changed. Clearing the pool.";
    free_frames_.clear();
    auto maybe_frame = create_frame_cb_.Run(
        format, coded_size, visible_rect, natural_size, use_protected,
        *use_linear_buffers_,
        fourcc == Fourcc(Fourcc::MM21) || fourcc == Fourcc(Fourcc::MT2T),
        base::TimeDelta());
    if (!maybe_frame.has_value())
      return std::move(maybe_frame).error();
    auto frame = std::move(maybe_frame).value();
    frame_layout_ = GpuBufferLayout::Create(fourcc, frame->coded_size(),
                                            frame->layout().planes(),
                                            frame->layout().modifier());
    if (!frame_layout_)
      return CroStatus::Codes::kFailedToGetFrameLayout;
  }

  DCHECK(frame_layout_);

  visible_rect_ = visible_rect;
  natural_size_ = natural_size;
  max_num_frames_ = max_num_frames;
  use_protected_ = use_protected;

  // The pool might become available because of |max_num_frames_| increased.
  // Notify the client if so.
  if (frame_available_cb_ && !IsExhausted_Locked())
    std::move(frame_available_cb_).Run();

  return *frame_layout_;
}

void PlatformVideoFramePool::SetCustomFrameAllocator(
    DmabufVideoFramePool::CreateFrameCB allocator,
    VideoFrame::StorageType frame_storage_type) {
  base::AutoLock auto_lock(lock_);
  create_frame_cb_ = allocator;
  frame_storage_type_ = frame_storage_type;
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

FrameResource* PlatformVideoFramePool::GetOriginalFrame(
    gfx::GenericSharedMemoryId frame_id) {
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  auto it = frames_in_use_.find(frame_id);
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

void PlatformVideoFramePool::ReleaseAllFrames() {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);
  free_frames_.clear();
  frames_in_use_.clear();
  weak_this_factory_.InvalidateWeakPtrs();
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

std::optional<GpuBufferLayout> PlatformVideoFramePool::GetGpuBufferLayout() {
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock auto_lock(lock_);
  return frame_layout_;
}

// static
void PlatformVideoFramePool::OnFrameReleasedThunk(
    std::optional<base::WeakPtr<PlatformVideoFramePool>> pool,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<FrameResource> origin_frame) {
  TRACE_EVENT2("media", "PlatformVideoFramePool::OnFrameReleasedThunk",
               "frame_id", origin_frame->unique_id(), "frame",
               origin_frame->AsHumanReadableString());
  DCHECK(pool);
  DVLOGF(4);

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&PlatformVideoFramePool::OnFrameReleased, *pool,
                                std::move(origin_frame)));
}

void PlatformVideoFramePool::OnFrameReleased(
    scoped_refptr<FrameResource> origin_frame) {
  TRACE_EVENT2("media", "PlatformVideoFramePool::OnFrameReleased", "frame_id",
               origin_frame->unique_id(), "frame",
               origin_frame->AsHumanReadableString());
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DVLOGF(4);
  base::AutoLock auto_lock(lock_);

  gfx::GenericSharedMemoryId frame_id = origin_frame->GetSharedMemoryId();
  auto it = frames_in_use_.find(frame_id);
  CHECK(it != frames_in_use_.end(), base::NotFatalUntil::M130);
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
    scoped_refptr<FrameResource> frame) {
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
