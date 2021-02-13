// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/web_xr_presentation_state.h"

#include <iomanip>
#include <sstream>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_android_hardware_buffer.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_image_egl.h"

namespace device {

WebXrSharedBuffer::WebXrSharedBuffer() = default;
WebXrSharedBuffer::~WebXrSharedBuffer() = default;

WebXrFrame::WebXrFrame() = default;

WebXrFrame::~WebXrFrame() = default;

bool WebXrFrame::IsValid() const {
  return index >= 0;
}

void WebXrFrame::Recycle() {
  DCHECK(!state_locked);
  index = -1;
  deferred_start_processing.Reset();
  recycle_once_unlocked = false;
  gvr_handoff_fence.reset();
}

WebXrPresentationState::WebXrPresentationState() {
  for (auto& frame : frames_storage_) {
    // Create frames in "idle" state.
    frame = std::make_unique<WebXrFrame>();
    idle_frames_.push(frame.get());
  }
}

WebXrPresentationState::~WebXrPresentationState() {}

WebXrFrame* WebXrPresentationState::GetAnimatingFrame() const {
  DCHECK(HaveAnimatingFrame());
  DCHECK(animating_frame_->IsValid());
  return animating_frame_;
}

WebXrFrame* WebXrPresentationState::GetProcessingFrame() const {
  DCHECK(HaveProcessingFrame());
  DCHECK(processing_frame_->IsValid());
  return processing_frame_;
}

WebXrFrame* WebXrPresentationState::GetRenderingFrame() const {
  DCHECK(HaveRenderingFrame());
  DCHECK(rendering_frame_->IsValid());
  return rendering_frame_;
}

std::string WebXrPresentationState::DebugState() const {
  std::ostringstream ss;

  ss << "[";
  if (HaveAnimatingFrame()) {
    ss << std::setw(3) << GetAnimatingFrame()->index;
  } else {
    ss << "---";
  }
  ss << "|";
  if (HaveProcessingFrame()) {
    ss << std::setw(3) << GetProcessingFrame()->index;
  } else {
    ss << "---";
  }
  ss << "|";
  if (HaveRenderingFrame()) {
    ss << std::setw(3) << GetRenderingFrame()->index;
  } else {
    ss << "---";
  }
  ss << "]";

  return ss.str();
}

WebXrPresentationState::FrameIndexType
WebXrPresentationState::StartFrameAnimating() {
  DCHECK(!HaveAnimatingFrame());
  DCHECK(!idle_frames_.empty());
  animating_frame_ = idle_frames_.front();
  idle_frames_.pop();

  animating_frame_->index = next_frame_index_++;

  DVLOG(3) << DebugState() << __func__;
  return animating_frame_->index;
}

void WebXrPresentationState::TransitionFrameAnimatingToProcessing() {
  DVLOG(3) << DebugState() << __func__;
  DCHECK(HaveAnimatingFrame());
  DCHECK(animating_frame_->IsValid());
  DCHECK(!animating_frame_->state_locked);
  DCHECK(!HaveProcessingFrame());
  processing_frame_ = animating_frame_;
  animating_frame_ = nullptr;
  DVLOG(3) << DebugState() << __func__;
}

void WebXrPresentationState::RecycleUnusedAnimatingFrame() {
  DCHECK(HaveAnimatingFrame());
  animating_frame_->Recycle();
  idle_frames_.push(animating_frame_);
  animating_frame_ = nullptr;
  DVLOG(3) << DebugState() << __func__;
}

void WebXrPresentationState::TransitionFrameProcessingToRendering() {
  DVLOG(3) << DebugState() << __func__;
  DCHECK(HaveProcessingFrame());
  DCHECK(processing_frame_->IsValid());
  DCHECK(!processing_frame_->state_locked);
  DCHECK(!HaveRenderingFrame());
  rendering_frame_ = processing_frame_;
  processing_frame_ = nullptr;
  DVLOG(3) << DebugState() << __func__;
}

void WebXrPresentationState::EndFrameRendering() {
  DVLOG(3) << DebugState() << __func__;
  DCHECK(HaveRenderingFrame());
  DCHECK(rendering_frame_->IsValid());
  rendering_frame_->Recycle();
  idle_frames_.push(rendering_frame_);
  rendering_frame_ = nullptr;
  DVLOG(3) << DebugState() << __func__;
}

bool WebXrPresentationState::RecycleProcessingFrameIfPossible() {
  DCHECK(HaveProcessingFrame());
  bool can_cancel = !processing_frame_->state_locked;
  if (can_cancel) {
    processing_frame_->Recycle();
    idle_frames_.push(processing_frame_);
    processing_frame_ = nullptr;
  } else {
    processing_frame_->recycle_once_unlocked = true;
  }
  DVLOG(3) << DebugState() << __func__;
  return can_cancel;
}

std::vector<std::unique_ptr<WebXrSharedBuffer>>
WebXrPresentationState::TakeSharedBuffers() {
  std::vector<std::unique_ptr<WebXrSharedBuffer>> shared_buffers;
  for (auto& frame : frames_storage_) {
    if (frame->shared_buffer)
      shared_buffers.emplace_back(std::move(frame->shared_buffer));
    if (frame->camera_image_shared_buffer)
      shared_buffers.emplace_back(std::move(frame->camera_image_shared_buffer));
  }
  return shared_buffers;
}

void WebXrPresentationState::EndPresentation() {
  TRACE_EVENT0("gpu", __FUNCTION__);

  if (HaveRenderingFrame()) {
    rendering_frame_->Recycle();
    idle_frames_.push(rendering_frame_);
    rendering_frame_ = nullptr;
  }
  if (HaveProcessingFrame()) {
    RecycleProcessingFrameIfPossible();
  }
  if (HaveAnimatingFrame()) {
    RecycleUnusedAnimatingFrame();
  }

  last_ui_allows_sending_vsync = false;
}

bool WebXrPresentationState::CanProcessFrame() const {
  if (!mailbox_bridge_ready_) {
    DVLOG(2) << __FUNCTION__ << ": waiting for mailbox bridge";
    return false;
  }
  if (processing_frame_) {
    DVLOG(2) << __FUNCTION__ << ": waiting for previous processing frame";
    return false;
  }

  return true;
}

void WebXrPresentationState::ProcessOrDefer(base::OnceClosure callback) {
  DCHECK(animating_frame_ && !animating_frame_->deferred_start_processing);
  if (CanProcessFrame()) {
    TransitionFrameAnimatingToProcessing();
    std::move(callback).Run();
  } else {
    DVLOG(2) << "Deferring processing frame, not ready";
    animating_frame_->deferred_start_processing = std::move(callback);
  }
}

void WebXrPresentationState::TryDeferredProcessing() {
  if (!animating_frame_ || !animating_frame_->deferred_start_processing ||
      !CanProcessFrame()) {
    return;
  }
  DVLOG(2) << "Running deferred SubmitFrame";
  // Run synchronously, not via PostTask, to ensure we don't
  // get a new SendVSync scheduling in between.
  TransitionFrameAnimatingToProcessing();

  // After the above call, the frame that was in animating_frame_ is now in
  // processing_frame_.
  std::move(processing_frame_->deferred_start_processing).Run();
}

}  // namespace device
