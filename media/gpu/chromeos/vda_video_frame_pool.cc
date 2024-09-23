// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/vda_video_frame_pool.h"

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "media/gpu/chromeos/gpu_buffer_layout.h"
#include "media/gpu/macros.h"
#include "media/media_buildflags.h"

namespace media {

VdaVideoFramePool::VdaVideoFramePool(
    base::WeakPtr<VdaDelegate> vda,
    scoped_refptr<base::SequencedTaskRunner> vda_task_runner)
    : vda_(std::move(vda)),
      vda_task_runner_(std::move(vda_task_runner)),
      vda_frame_storage_type_(vda_->GetFrameStorageType()) {
  DVLOGF(3);
  DETACH_FROM_SEQUENCE(parent_sequence_checker_);

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

VdaVideoFramePool::~VdaVideoFramePool() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_sequence_checker_);

  weak_this_factory_.InvalidateWeakPtrs();
}

CroStatus::Or<GpuBufferLayout> VdaVideoFramePool::Initialize(
    const Fourcc& fourcc,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    size_t max_num_frames,
    bool use_protected,
    bool use_linear_buffers) {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_sequence_checker_);

#if !BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
  if (use_protected) {
    LOG(ERROR) << "Cannot allocated protected buffers for VDA";
    return CroStatus::Codes::kProtectedContentUnsupported;
  }
#endif  // !BUILDFLAG(USE_ARC_PROTECTED_MEDIA)

  visible_rect_ = visible_rect;
  natural_size_ = natural_size;

  if (layout_ && max_num_frames_ == max_num_frames && fourcc_ &&
      *fourcc_ == fourcc && coded_size_ == coded_size) {
    DVLOGF(3) << "Arguments related to frame layout are not changed, skip.";
    return *layout_;
  }

  // Invalidate weak pointers so the re-import callbacks of the frames we are
  // about to stop managing do not run and add them back to us.
  weak_this_factory_.InvalidateWeakPtrs();
  weak_this_ = weak_this_factory_.GetWeakPtr();

  // Clear the pool and reset the layout to prevent previous frames are recycled
  // back to the pool.
  frame_pool_ = {};
  max_num_frames_ = 0;
  layout_ = std::nullopt;
  fourcc_ = std::nullopt;
  coded_size_ = gfx::Size();

  CroStatus::Or<GpuBufferLayout> status_or_layout =
      CroStatus::Codes::kResetRequired;
  base::WaitableEvent done;
  vda_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VdaDelegate::RequestFrames, vda_, fourcc, coded_size,
                     visible_rect, max_num_frames,
                     base::BindOnce(&VdaVideoFramePool::OnRequestFramesDone,
                                    &done, &status_or_layout),
                     base::BindRepeating(&VdaVideoFramePool::ImportFrameThunk,
                                         parent_task_runner_, weak_this_)));
  done.Wait();

  if (!status_or_layout.has_value())
    return status_or_layout;

  GpuBufferLayout layout = std::move(status_or_layout).value();
  if (layout.fourcc() != fourcc ||
      layout.size().height() < coded_size.height() ||
      layout.size().width() < coded_size.width()) {
    return CroStatus::Codes::kFailedToGetFrameLayout;
  }

  max_num_frames_ = max_num_frames;
  layout_ = std::move(layout);
  fourcc_ = fourcc;
  coded_size_ = coded_size;
  return *layout_;
}

// static
void VdaVideoFramePool::OnRequestFramesDone(
    base::WaitableEvent* done,
    CroStatus::Or<GpuBufferLayout>* layout,
    CroStatus::Or<GpuBufferLayout> layout_value) {
  DVLOGF(3);

  *layout = std::move(layout_value);
  done->Signal();
}

// static
void VdaVideoFramePool::ImportFrameThunk(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::optional<base::WeakPtr<VdaVideoFramePool>> weak_this,
    scoped_refptr<FrameResource> frame) {
  DVLOGF(3);
  DCHECK(weak_this);

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&VdaVideoFramePool::ImportFrame, *weak_this,
                                std::move(frame)));
}

void VdaVideoFramePool::ImportFrame(scoped_refptr<FrameResource> frame) {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_sequence_checker_);

  if (!layout_ || layout_->fourcc().ToVideoPixelFormat() != frame->format() ||
      layout_->size() != frame->coded_size() ||
      layout_->modifier() != frame->layout().modifier()) {
    return;
  }

  frame_pool_.push(std::move(frame));
  CallFrameAvailableCbIfNeeded();
}

scoped_refptr<FrameResource> VdaVideoFramePool::GetFrame() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_sequence_checker_);

  if (IsExhausted())
    return nullptr;

  scoped_refptr<FrameResource> origin_frame = std::move(frame_pool_.front());
  frame_pool_.pop();

  // Update visible_rect and natural_size.
  scoped_refptr<FrameResource> wrapped_frame =
      origin_frame->CreateWrappingFrame(visible_rect_, natural_size_);
  if (!wrapped_frame) {
    DLOG(WARNING) << __func__ << "Failed to wrap a FrameResource";
    return nullptr;
  }

  wrapped_frame->AddDestructionObserver(
      base::BindOnce(&VdaVideoFramePool::ImportFrameThunk, parent_task_runner_,
                     weak_this_, std::move(origin_frame)));
  return wrapped_frame;
}

VideoFrame::StorageType VdaVideoFramePool::GetFrameStorageType() const {
  return vda_frame_storage_type_;
}

bool VdaVideoFramePool::IsExhausted() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_sequence_checker_);

  return frame_pool_.empty();
}

void VdaVideoFramePool::NotifyWhenFrameAvailable(base::OnceClosure cb) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_sequence_checker_);

  frame_available_cb_ = std::move(cb);
  CallFrameAvailableCbIfNeeded();
}

void VdaVideoFramePool::ReleaseAllFrames() {
  // TODO(jkardatzke): Implement this when we do protected content on Android
  // for Intel platforms. I will do this in a follow up CL, removing the
  // NOREACHED() for now in order to prevent a DCHECK when this occurs.
}

std::optional<GpuBufferLayout> VdaVideoFramePool::GetGpuBufferLayout() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_sequence_checker_);
  return layout_;
}

void VdaVideoFramePool::CallFrameAvailableCbIfNeeded() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_sequence_checker_);

  if (frame_available_cb_ && !IsExhausted())
    parent_task_runner_->PostTask(FROM_HERE, std::move(frame_available_cb_));
}

}  // namespace media
