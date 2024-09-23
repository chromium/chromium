// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_WEB_XR_PRESENTATION_STATE_H_
#define DEVICE_VR_ANDROID_WEB_XR_PRESENTATION_STATE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/viz/common/resources/resource_id.h"
#include "device/vr/android/local_texture.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_egl_image.h"

namespace gl {
class GLFence;
}  // namespace gl

namespace viz {
struct BeginFrameArgs;
}  // namespace viz

namespace device {
// When composited by the browser process, WebXR frames go through a three-stage
// pipeline: Animating, Processing, and Rendering. There's also an Idle state
// used as the starting state before Animating and ending state after Rendering.
//
// The stages can overlap, but we enforce that there isn't more than one
// frame in a given non-Idle state at any one time.
//
//       <- GetFrameData
//   Idle
//       SendVSync
//   Animating
//       <- UpdateLayerBounds (optional)
//       <- GetFrameData
//       <- SubmitFrame
//       ProcessOrDefer
//   Processing
//       <- OnWebVrFrameAvailable
//       DrawFrame
//       DrawFrameSubmitWhenReady
//       <= poll prev_frame_completion_fence_
//       DrawFrameSubmitNow
//   Rendering
//       <= prev_frame_completion_fence_ signals
//       DrawFrameSubmitNow (of next frame)
//   Idle
//
// Note that the frame is considered to still be in "Animating" state until
// ProcessOrDefer is called. If the current processing frame isn't done yet
// at the time the incoming SubmitFrame arrives, we defer Processing the frame
// until that finishes.
//
// The renderer may call SubmitFrameMissing instead of SubmitFrame. In that
// case, the frame transitions from Animating back to Idle.
//
//       <- GetFrameData
//   Idle
//       SendVSync
//   Animating
//       <- UpdateLayerBounds (optional)
//       <- GetFrameData
//       <- SubmitFrameMissing
//   Idle
//
//
// When compositing is managed by Viz, the frames go through much the same
// three-stage pipeline, but there are a few noteworthy differences:
//   * An "Animating" frame cannot be processed until it's BeginFrameArgs have
//     been set.
//   * Processing will generally happen synchronously, as most sync points are
//     passed on to be used in the Viz Compositor.
//   * More than one frame may be in the "Rendering" state; and Frames should
//     be transitioned to "Rendering" when they are handed off to the viz
//     Compositor. When the Compositor is no longer using the resources
//     associated with the frame, it can then be transitioned back to Idle.

struct WebXrSharedBuffer {
  WebXrSharedBuffer();
  ~WebXrSharedBuffer();

  gfx::Size size = {0, 0};

  // This owns a single reference to an AHardwareBuffer object.
  base::android::ScopedHardwareBufferHandle scoped_ahb_handle;

  // Resources in the remote GPU process command buffer context
  scoped_refptr<gpu::ClientSharedImage> shared_image;
  gpu::SyncToken sync_token;

  // Resources in the local GL context
  LocalTexture local_texture;
  // This object keeps the image alive while processing a frame. That's
  // required because it owns underlying resources, and must still be
  // alive when the mailbox texture backed by this image is used.
  gl::ScopedEGLImage local_eglimage;

  // The ResourceId that was used to pass this buffer to the Viz Compositor.
  // Id should be set to kInvalidResourceId when it is not in use by the viz
  // compositor (either because the buffer was not passed to it, or because the
  // compositor has told us it is okay to reclaim the resource).
  viz::ResourceId id = viz::kInvalidResourceId;
};

struct WebXrFrame {
  WebXrFrame();

  WebXrFrame(const WebXrFrame&) = delete;
  WebXrFrame& operator=(const WebXrFrame&) = delete;

  ~WebXrFrame();

  bool IsValid() const;
  void Recycle();

  // If true, this frame cannot change state until unlocked. Used to mark
  // processing frames for the critical stage from drawing to Surface until
  // they arrive in OnWebVRFrameAvailable. See also recycle_once_unlocked.
  bool state_locked = false;

  // Start of elements that need to be reset on Recycle

  int16_t index = -1;

  // Set on an animating frame if it is waiting for being able to transition
  // to processing state.
  base::OnceClosure deferred_start_processing;

  // Set if a frame recycle failed due to being locked. The client should check
  // this after unlocking it and retry recycling it at that time.
  bool recycle_once_unlocked = false;

  std::unique_ptr<gl::GLFence> gvr_handoff_fence;

  std::unique_ptr<gl::GLFence> render_completion_fence;

  std::unique_ptr<viz::BeginFrameArgs> begin_frame_args;

  std::vector<gpu::SyncToken> reclaimed_sync_tokens;
  // End of elements that need to be reset on Recycle

  base::TimeTicks time_pose;
  base::TimeTicks time_js_submit;
  base::TimeTicks time_copied;
  gfx::Transform head_pose;

  // In SharedBuffer mode, keep a swap chain.
  std::unique_ptr<WebXrSharedBuffer> shared_buffer;

  std::unique_ptr<WebXrSharedBuffer> camera_image_shared_buffer;

  // Viewport bounds used for rendering, in texture coordinates with uv=(0, 1)
  // corresponding to viewport pixel (0, 0) as set by UpdateLayerBounds.
  //
  // When used by monoscoping ARCore, only the left viewport/bounds are used.
  // Cardboard makes use of both.
  gfx::RectF bounds_left;
  gfx::RectF bounds_right;
};

class WebXrPresentationState {
 public:
  enum class StateMachineType {
    kBrowserComposited,
    kVizComposited,
  };
  // WebXR frames use an arbitrary sequential ID to help catch logic errors
  // involving out-of-order frames. We use an 8-bit unsigned counter, wrapping
  // from 255 back to 0. Elsewhere we use -1 to indicate a non-WebXR frame, so
  // most internal APIs use int16_t to ensure that they can store a full
  // -1..255 value range.
  using FrameIndexType = uint8_t;

  // We have at most one frame animating, one frame being processed,
  // and one frame tracked after submission to GVR.
  static constexpr int kWebXrFrameCount = 3;

  WebXrPresentationState();

  WebXrPresentationState(const WebXrPresentationState&) = delete;
  WebXrPresentationState& operator=(const WebXrPresentationState&) = delete;

  ~WebXrPresentationState();

  void SetStateMachineType(StateMachineType type);

  // State transitions for normal flow
  bool CanStartFrameAnimating();
  FrameIndexType StartFrameAnimating();
  void TransitionFrameAnimatingToProcessing();
  void TransitionFrameProcessingToRendering();
  void EndFrameRendering(WebXrFrame* frame);
  void EndFrameRendering();

  // Shuts down a presentation session. This will recycle any
  // animating or rendering frame. A processing frame cannot be
  // recycled if its state is locked, it will be recycled later
  // once the state unlocks.
  void EndPresentation();

  // Variant transitions, if Renderer didn't call SubmitFrame,
  // or if we want to discard an unwanted incoming frame.
  void RecycleUnusedAnimatingFrame();
  bool RecycleProcessingFrameIfPossible();

  void ProcessOrDefer(base::OnceClosure callback);
  // Call this after state changes that could result in CanProcessFrame
  // becoming true.
  void TryDeferredProcessing();

  bool HaveAnimatingFrame() const { return animating_frame_; }
  WebXrFrame* GetAnimatingFrame() const;
  bool HaveProcessingFrame() const { return processing_frame_; }
  WebXrFrame* GetProcessingFrame() const;
  bool HaveRenderingFrame() const { return rendering_frame_; }
  WebXrFrame* GetRenderingFrame() const;

  bool mailbox_bridge_ready() { return mailbox_bridge_ready_; }
  void NotifyMailboxBridgeReady() { mailbox_bridge_ready_ = true; }

  // The index of the expected next animating frame, intended for logging
  // purposes only. Does not consume or modify the index value.
  FrameIndexType PeekNextFrameIndex() const { return next_frame_index_; }

  // Extracts the shared buffers from all frames, resetting said frames to an
  // invalid state.
  // This is intended for resource cleanup, after EndPresentation was called.
  std::vector<std::unique_ptr<WebXrSharedBuffer>> TakeSharedBuffers();

  // Used by WebVrCanAnimateFrame() to detect when ui_->CanSendWebVrVSync()
  // transitions from false to true, as part of starting the incoming frame
  // timeout.
  bool last_ui_allows_sending_vsync = false;

 private:
  // Checks if we're in a valid state for processing the current animating
  // frame. Invalid states include mailbox_bridge_ready_ being false, or an
  // already existing processing frame that's not done yet.
  bool CanProcessFrame() const;
  std::string DebugState() const;

  std::unique_ptr<WebXrFrame> frames_storage_[kWebXrFrameCount];

  // Index of the next animating WebXR frame.
  FrameIndexType next_frame_index_ = 0;

  StateMachineType state_machine_type_ = StateMachineType::kBrowserComposited;

  raw_ptr<WebXrFrame> animating_frame_ = nullptr;
  raw_ptr<WebXrFrame> processing_frame_ = nullptr;
  raw_ptr<WebXrFrame> rendering_frame_ = nullptr;
  std::vector<raw_ptr<WebXrFrame, VectorExperimental>> rendering_frames_;
  base::queue<raw_ptr<WebXrFrame, CtnExperimental>> idle_frames_;

  bool mailbox_bridge_ready_ = false;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_WEB_XR_PRESENTATION_STATE_H_
