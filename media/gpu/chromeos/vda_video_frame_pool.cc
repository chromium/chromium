// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/vda_video_frame_pool.h"

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "media/gpu/chromeos/gpu_buffer_layout.h"
#include "media/gpu/macros.h"

namespace media {

VdaVideoFramePool::VdaVideoFramePool(
    base::WeakPtr<VdaDelegate> vda,
    scoped_refptr<base::SequencedTaskRunner> vda_task_runner)
    : vda_(std::move(vda)), vda_task_runner_(std::move(vda_task_runner)) {
  DVLOGF(3);
  DETACH_FROM_SEQUENCE(parent_sequence_checker_);

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

VdaVideoFramePool::~VdaVideoFramePool() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_sequence_checker_);

  weak_this_factory_.InvalidateWeakPtrs();
}

base::Optional<GpuBufferLayout> VdaVideoFramePool::Initialize(
    const Fourcc& fourcc,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    size_t max_num_frames) {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_sequence_checker_);

  visible_rect_ = visible_rect;
  natural_size_ = natural_size;

  if (max_num_frames_ == max_num_frames && fourcc_ && *fourcc_ == fourcc &&
      coded_size_ == coded_size) {
    DVLOGF(3) << "Arguments related to frame layout are not changed, skip.";
    return layout_;
  }
  max_num_frames_ = max_num_frames;
  fourcc_ = fourcc;
  coded_size_ = coded_size;

  // Clear the pool and reset the layout to prevent previous frames are recycled
  // back to the pool.
  frame_pool_ = {};
  layout_ = base::nullopt;

  // Receive the layout from the callback. |layout_| is accessed on
  // |parent_task_runner_| except OnRequestFramesDone(). However, we block
  // |parent_task_runner_| until OnRequestFramesDone() returns. So we don't need
  // a lock to protect |layout_|.
  // Also it's safe to use base::Unretained() here because we block here, |this|
  // must be alive during the callback.
  base::WaitableEvent done;
  vda_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VdaDelegate::RequestFrames, vda_, fourcc, coded_size,
                     visible_rect, max_num_frames,
                     base::BindOnce(&VdaVideoFramePool::OnRequestFramesDone,
                                    base::Unretained(this), &done),
                     base::BindRepeating(&VdaVideoFramePool::ImportFrameThunk,
                                         parent_task_runner_, weak_this_)));
  done.Wait();

  return layout_;
}

void VdaVideoFramePool::OnRequestFramesDone(
    base::WaitableEvent* done,
    base::Optional<GpuBufferLayout> value) {
  DVLOGF(3);
  // RequestFrames() is blocked on |parent_task_runner_| to wait for this method
  // finishes, so this method must not be run on the same sequence.
  DCHECK(!parent_task_runner_->RunsTasksInCurrentSequence());

  DCHECK(fourcc_);
  DCHECK_EQ(value->fourcc(), *fourcc_);
  DCHECK_GE(value->size().height(), coded_size_.height());
  DCHECK_GE(value->size().width(), coded_size_.width());

  layout_ = value;
  done->Signal();
}

// static
void VdaVideoFramePool::ImportFrameThunk(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::Optional<base::WeakPtr<VdaVideoFramePool>> weak_this,
    scoped_refptr<VideoFrame> frame) {
  DVLOGF(3);
  DCHECK(weak_this);

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&VdaVideoFramePool::ImportFrame, *weak_this,
                                std::move(frame)));
}

void VdaVideoFramePool::ImportFrame(scoped_refptr<VideoFrame> frame) {
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

scoped_refptr<VideoFrame> VdaVideoFramePool::GetFrame() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_sequence_checker_);

  if (IsExhausted())
    return nullptr;

  scoped_refptr<VideoFrame> origin_frame = std::move(frame_pool_.front());
  frame_pool_.pop();

  // Update visible_rect and natural_size.
  scoped_refptr<VideoFrame> wrapped_frame = VideoFrame::WrapVideoFrame(
      origin_frame, origin_frame->format(), visible_rect_, natural_size_);
  DCHECK(wrapped_frame);
  wrapped_frame->AddDestructionObserver(
      base::BindOnce(&VdaVideoFramePool::ImportFrameThunk, parent_task_runner_,
                     weak_this_, std::move(origin_frame)));
  return wrapped_frame;
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

void VdaVideoFramePool::CallFrameAvailableCbIfNeeded() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_sequence_checker_);

  if (frame_available_cb_ && !IsExhausted())
    parent_task_runner_->PostTask(FROM_HERE, std::move(frame_available_cb_));
}

}  // namespace media
