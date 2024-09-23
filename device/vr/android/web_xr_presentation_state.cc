// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/web_xr_presentation_state.h"

#include <iomanip>
#include <sstream>
#include <vector>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "ui/gl/gl_fence.h"

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
  DCHECK(reclaimed_sync_tokens.empty());
  index = -1;
  deferred_start_processing.Reset();
  recycle_once_unlocked = false;
  gvr_handoff_fence.reset();
  begin_frame_args.reset();
}

WebXrPresentationState::WebXrPresentationState() {
  for (auto& frame : frames_storage_) {
    // Create frames in "idle" state.
    frame = std::make_unique<WebXrFrame>();
    idle_frames_.push(frame.get());
  }
}

WebXrPresentationState::~WebXrPresentationState() {}

void WebXrPresentationState::SetStateMachineType(StateMachineType type) {
  state_machine_type_ = type;
}

bool WebXrPresentationState::CanStartFrameAnimating() {
  return !idle_frames_.empty();
}

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
  switch (state_machine_type_) {
    case StateMachineType::kBrowserComposited: {
      if (HaveRenderingFrame()) {
        ss << std::setw(3) << GetRenderingFrame()->index;
      } else {
        ss << "---";
      }
      break;
    }
    case StateMachineType::kVizComposited: {
      if (rendering_frames_.size() > 0) {
        for (size_t i = 0; i < rendering_frames_.size(); i++) {
          auto* frame = rendering_frames_[i].get();
          ss << std::setw(3) << frame->index;
          ss << "(" << frame->shared_buffer->id << ", "
             << frame->camera_image_shared_buffer->id << ")";
          // Append a "," separator, unless this is the last element to list.
          if (i != rendering_frames_.size() - 1) {
            ss << ",";
          }
        }
      } else {
        ss << "---";
      }
      break;
    }
  }
  ss << "]";

  return ss.str();
}

WebXrPresentationState::FrameIndexType
WebXrPresentationState::StartFrameAnimating() {
  DCHECK(!HaveAnimatingFrame());
  DCHECK(!idle_frames_.empty());
  animating_frame_ = idle_frames_.front().get();
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
  idle_frames_.push(animating_frame_.get());
  animating_frame_ = nullptr;
  DVLOG(3) << DebugState() << __func__;
}

void WebXrPresentationState::TransitionFrameProcessingToRendering() {
  DVLOG(3) << DebugState() << __func__;
  DCHECK(HaveProcessingFrame());
  DCHECK(processing_frame_->IsValid());
  DCHECK(!processing_frame_->state_locked);

  switch (state_machine_type_) {
    // In the BrowserComposited StateMachine we can only have one RenderingFrame
    // at a time. Assert that we don't have one, and then assign it to the slot.
    case StateMachineType::kBrowserComposited: {
      DCHECK(!HaveRenderingFrame());
      rendering_frame_ = processing_frame_;
      break;
    }
    // In the VizComposited StateMachine multiple frames may be "Rendering",
    // where "Rendering" means that the frame has been passed to the viz
    // compositor. We need to wait until the viz compositor is done with a frame
    // before it can be recycled.
    case StateMachineType::kVizComposited: {
      rendering_frames_.push_back(processing_frame_.get());
      break;
    }
  }

  processing_frame_ = nullptr;
  DVLOG(3) << DebugState() << __func__;
}

void WebXrPresentationState::EndFrameRendering(WebXrFrame* frame) {
  DCHECK(frame);
  // If we have a rendering frame, that means we should be in the
  // BrowserComposited mode. In that case, the caller may have called us with
  // the result of GetRenderingFrame() to simplify code-paths for operations
  // that they need to do with the Frame. Ensure that if we have a rendering
  // frame, we were called with it, and then process in that mode.
  if (HaveRenderingFrame()) {
    DCHECK_EQ(frame, rendering_frame_);
    EndFrameRendering();
    return;
  }

  // In this case, we don't have a RenderingFrame. If we're not running the viz
  // composited state machine, this is an error. If we are, then there should
  // be exactly one instance of this frame in the rendering_frames_ list.
  // Remove it from the list, and then recycle the frame.
  DVLOG(3) << DebugState() << __func__;
  DCHECK_EQ(state_machine_type_, StateMachineType::kVizComposited);
  auto erased = std::erase_if(rendering_frames_,
                              [frame](const WebXrFrame* rendering_frame) {
                                return frame == rendering_frame;
                              });

  // Not using DCHECK_EQ as the compiler doesn't pick the right type to compare.
  DCHECK(erased == 1);
  frame->Recycle();
  idle_frames_.push(frame);
  DVLOG(3) << DebugState() << __func__;
}

void WebXrPresentationState::EndFrameRendering() {
  DVLOG(3) << DebugState() << __func__;
  DCHECK_EQ(state_machine_type_, StateMachineType::kBrowserComposited);
  DCHECK(HaveRenderingFrame());
  DCHECK(rendering_frame_->IsValid());
  rendering_frame_->Recycle();
  idle_frames_.push(rendering_frame_.get());
  rendering_frame_ = nullptr;
  DVLOG(3) << DebugState() << __func__;
}

bool WebXrPresentationState::RecycleProcessingFrameIfPossible() {
  DCHECK(HaveProcessingFrame());
  bool can_cancel = !processing_frame_->state_locked;
  if (can_cancel) {
    processing_frame_->Recycle();
    idle_frames_.push(processing_frame_.get());
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
    idle_frames_.push(rendering_frame_.get());
    rendering_frame_ = nullptr;
  }
  for (device::WebXrFrame* frame : rendering_frames_) {
    frame->Recycle();
    idle_frames_.push(frame);
  }
  rendering_frames_.clear();
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

  // If we're running the viz composited state machine, we need to keep a frame
  // in the "Animating" state until we have our BeginFrameArgs.
  if (state_machine_type_ == StateMachineType::kVizComposited &&
      !animating_frame_->begin_frame_args) {
    DVLOG(2) << __FUNCTION__ << ": waiting for BeginFrameArgs";
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
