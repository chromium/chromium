// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/cardboard/cardboard_render_loop.h"

#include <time.h>
#include <memory>

#include "base/task/bind_post_task.h"
#include "device/vr/android/cardboard/cardboard_image_transport.h"
#include "device/vr/android/cardboard/cardboard_sdk.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom-shared.h"
#include "device/vr/util/transform_utils.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_bindings_autogen_gl.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_android_native_fence_sync.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace device {
namespace {
// TODO(crbug.com/40900871): It's not clear if the display rotation
// should factor into Cardboard's viewport orientation. Initial attempts to
// map them together frequently gave wrong results, whereas statically using
// kLandscapeLeft has the expected effect.
constexpr CardboardViewportOrientation kViewportOrientation = kLandscapeLeft;

// Default downscale factor for computing the recommended WebXR
// render_width/render_height from the 1:1 pixel mapped size.
static constexpr float kRecommendedResolutionScale = 0.7;

constexpr uint64_t kNanosInMs = 1000000;
constexpr uint64_t kNanosInSeconds = 1000 * kNanosInMs;

// Static prediction value used in the hello_cardboard sample.
constexpr uint64_t kPredictionTimeWithoutVsyncNanos = 50 * kNanosInMs;

int64_t GetBootTimeNano() {
  struct timespec res;
  clock_gettime(CLOCK_BOOTTIME, &res);
  return (res.tv_sec * kNanosInSeconds) + res.tv_nsec;
}
}  // namespace

CardboardRenderLoop::CardboardRenderLoop(
    std::unique_ptr<CardboardImageTransportFactory>
        cardboard_image_transport_factory,
    std::unique_ptr<MailboxToSurfaceBridge> mailbox_bridge)
    : base::android::JavaHandlerThread("CardboardRenderLoop"),
      cardboard_image_transport_factory_(
          std::move(cardboard_image_transport_factory)),
      mailbox_bridge_(std::move(mailbox_bridge)),
      webxr_(std::make_unique<WebXrPresentationState>()) {}

CardboardRenderLoop::~CardboardRenderLoop() {
  Stop();
}

void CardboardRenderLoop::GetEnvironmentIntegrationProvider(
    mojo::PendingAssociatedReceiver<
        device::mojom::XREnvironmentIntegrationProvider> environment_provider) {
  // Environment integration is not supported. This call should not
  // be made on this device.
  frame_data_receiver_.ReportBadMessage(
      "Environment integration is not supported.");
}

void CardboardRenderLoop::CreateSession(
    CardboardRequestSessionCallback session_request_callback,
    base::OnceClosure session_shutdown_callback,
    CardboardSdk* cardboard_sdk,
    gfx::AcceleratedWidget drawing_widget,
    const gfx::Size& frame_size,
    display::Display::Rotation display_rotation,
    mojom::XRRuntimeSessionOptionsPtr options) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  CHECK(!session_request_callback_);
  CHECK(!frame_size.IsEmpty());
  DVLOG(1) << __func__;
  cardboard_sdk_ = cardboard_sdk;

  // The initial frame size given here should correspond with the display size.
  cardboard_image_transport_ = cardboard_image_transport_factory_->Create(
      std::move(mailbox_bridge_), frame_size);
  session_request_callback_ = std::move(session_request_callback);
  session_shutdown_callback_ = std::move(session_shutdown_callback);
  texture_size_ = frame_size;

  // Filtering should be done at the higher level, so just enable all requested
  // features.
  enabled_features_.insert(options->required_features.begin(),
                           options->required_features.end());
  enabled_features_.insert(options->optional_features.begin(),
                           options->optional_features.end());

  if (!InitializeGl(drawing_widget)) {
    std::move(session_request_callback_).Run(nullptr);
    return;
  }

  cardboard_image_transport_->Initialize(
      webxr_.get(),
      base::BindOnce(&CardboardRenderLoop::OnCardboardImageTransportReady,
                     weak_ptr_factory_.GetWeakPtr()));

  left_eye_ = mojom::XRView::New();
  left_eye_->eye = mojom::XREye::kLeft;
  left_eye_->viewport =
      gfx::Rect(0, 0, texture_size_.width() / 2, texture_size_.height());

  right_eye_ = mojom::XRView::New();
  right_eye_->eye = mojom::XREye::kRight;
  right_eye_->viewport =
      gfx::Rect(texture_size_.width() / 2, 0, texture_size_.width() / 2,
                texture_size_.height());

  left_eye_->mojo_from_view = gfx::Transform();
  left_eye_->field_of_view =
      cardboard_image_transport_->GetFOV(CardboardEye::kLeft);

  right_eye_->mojo_from_view = gfx::Transform();
  right_eye_->field_of_view =
      cardboard_image_transport_->GetFOV(CardboardEye::kRight);

  head_tracker_ = internal::ScopedCardboardObject<CardboardHeadTracker*>(
      CardboardHeadTracker_create());

  // If the head tracker isn't explicitly resumed after creation it doesn't
  // deliver any poses. Not clear if this is intended, as it's not mentioned in
  // the documentation.
  CardboardHeadTracker_resume(head_tracker_.get());
  CardboardHeadTracker_recenter(head_tracker_.get());
}

bool CardboardRenderLoop::InitializeGl(gfx::AcceleratedWidget drawing_widget) {
  DVLOG(1) << __func__;
  DCHECK(task_runner()->BelongsToCurrentThread());
  CHECK(drawing_widget);

  // TODO(crbug.com/40744597): While we actually *can* launch Cardboard
  // with ANGLE support; if we do so, once we try to launch ARCore (which
  // disables it), we end up hitting a crash. We should investigate if this can
  // be resolved to use ANGLE with Cardboard.
  gl::DisableANGLE();

  gl::GLDisplay* display = nullptr;
  if (gl::GetGLImplementation() == gl::kGLImplementationNone) {
    display = gl::init::InitializeGLOneOff(
        /*gpu_preference=*/gl::GpuPreference::kDefault);
    if (!display) {
      DLOG(ERROR) << "gl::init::InitializeGLOneOff failed";
      return false;
    }
  } else {
    display = gl::GetDefaultDisplayEGL();
  }

  scoped_refptr<gl::GLSurface> surface =
      gl::init::CreateViewGLSurface(display, drawing_widget);
  DVLOG(3) << "surface=" << surface.get();
  if (!surface.get()) {
    DLOG(ERROR) << "gl::init::CreateViewGLSurface failed";
    return false;
  }

  scoped_refptr<gl::GLContext> context =
      gl::init::CreateGLContext(nullptr, surface.get(), gl::GLContextAttribs());
  if (!context.get()) {
    DLOG(ERROR) << "gl::init::CreateGLContext failed";
    return false;
  }
  if (!context->MakeCurrent(surface.get())) {
    DLOG(ERROR) << "gl::GLContext::MakeCurrent() failed";
    return false;
  }

  // Swap the surface once so that it will show an empty texture rather than
  // just being transparent.
  surface->SwapBuffers(base::DoNothing(), gfx::FrameData());

  // Assign the surface and context members now that initialization has
  // succeeded.
  surface_ = std::move(surface);
  context_ = std::move(context);

  return true;
}

void CardboardRenderLoop::OnBindingDisconnect() {
  DVLOG(1) << __func__;

  CloseBindingsIfOpen();
  pending_shutdown_ = true;

  // Even if we're currently pending shutdown, it doesn't hurt to ensure that
  // the bindings have been closed; but if we've already called the session
  // shutdown callback and get a binding disconnect before we're destroyed, then
  // we may not have a session_shutdown_callback to call.
  if (session_shutdown_callback_) {
    std::move(session_shutdown_callback_).Run();
  }
}

void CardboardRenderLoop::CloseBindingsIfOpen() {
  DVLOG(1) << __func__;

  frame_data_receiver_.reset();
  session_controller_receiver_.reset();
  presentation_receiver_.reset();
  submit_client_.reset();
}

void CardboardRenderLoop::OnCardboardImageTransportReady(bool success) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DVLOG(1) << __func__ << ": success=" << success;
  if (!success) {
    std::move(session_request_callback_).Run(nullptr);
    return;
  }

  webxr_->NotifyMailboxBridgeReady();

  // Reset all of our bindings before we assign them.
  CloseBindingsIfOpen();

  device::mojom::XRPresentationTransportOptionsPtr transport_options =
      device::mojom::XRPresentationTransportOptions::New();
  transport_options->wait_for_gpu_fence = true;

  if (CardboardImageTransport::UseSharedBuffer()) {
    DVLOG(2) << __func__
             << ": UseSharedBuffer()=true, DRAW_INTO_TEXTURE_MAILBOX";
    transport_options->transport_method =
        device::mojom::XRPresentationTransportMethod::DRAW_INTO_TEXTURE_MAILBOX;
  } else {
    DVLOG(2) << __func__
             << ": UseSharedBuffer()=false, SUBMIT_AS_MAILBOX_HOLDER";
    transport_options->transport_method =
        device::mojom::XRPresentationTransportMethod::SUBMIT_AS_MAILBOX_HOLDER;
    transport_options->wait_for_transfer_notification = true;
    cardboard_image_transport_->SetFrameAvailableCallback(base::BindRepeating(
        &CardboardRenderLoop::RenderFrame, weak_ptr_factory_.GetWeakPtr()));
  }

  mojom::XRRuntimeSessionResultPtr result =
      device::mojom::XRRuntimeSessionResult::New();
  result->controller = session_controller_receiver_.BindNewPipeAndPassRemote();

  result->session = mojom::XRSession::New();
  auto* session = result->session.get();
  session->data_provider = frame_data_receiver_.BindNewPipeAndPassRemote();
  session->submit_frame_sink = device::mojom::XRPresentationConnection::New();

  auto* submit_frame_sink = session->submit_frame_sink.get();
  submit_frame_sink->client_receiver =
      submit_client_.BindNewPipeAndPassReceiver();
  submit_frame_sink->provider =
      presentation_receiver_.BindNewPipeAndPassRemote();
  submit_frame_sink->transport_options = std::move(transport_options);

  session->enabled_features.assign(enabled_features_.begin(),
                                   enabled_features_.end());

  session->device_config = device::mojom::XRSessionDeviceConfig::New();
  auto* config = session->device_config.get();

  // TODO(crbug.com/40900872): Determine if we should support this.
  config->supports_viewport_scaling = false;

  config->default_framebuffer_scale = kRecommendedResolutionScale;

  config->views.push_back(left_eye_.Clone());
  config->views.push_back(right_eye_.Clone());

  session->enviroment_blend_mode =
      device::mojom::XREnvironmentBlendMode::kOpaque;
  session->interaction_mode = device::mojom::XRInteractionMode::kWorldSpace;

  std::move(session_request_callback_).Run(std::move(result));

  frame_data_receiver_.set_disconnect_handler(
      base::BindOnce(&CardboardRenderLoop::OnBindingDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));
  session_controller_receiver_.set_disconnect_handler(
      base::BindOnce(&CardboardRenderLoop::OnBindingDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));
  presentation_receiver_.set_disconnect_handler(
      base::BindOnce(&CardboardRenderLoop::OnBindingDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));
  submit_client_.set_disconnect_handler(
      base::BindOnce(&CardboardRenderLoop::OnBindingDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CardboardRenderLoop::CleanUp() {
  // This happens on the GL Thread, but we cannot assert that we are on it
  // because the thread is stopping; but this should only be called by our
  // parent.
  weak_ptr_factory_.InvalidateWeakPtrs();

  cardboard_image_transport_->DestroySharedBuffers(webxr_.get());
  cardboard_image_transport_.reset();
  OnBindingDisconnect();

  context_.reset();
  surface_.reset();
}

void CardboardRenderLoop::GetFrameData(
    mojom::XRFrameDataRequestOptionsPtr options,
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  TRACE_EVENT1("gpu", __func__, "frame", webxr_->PeekNextFrameIndex());
  DCHECK(task_runner()->BelongsToCurrentThread());
  CHECK(!texture_size_.IsEmpty());

  if (!CanStartNewAnimatingFrame()) {
    // We bind this as a post task so that whatever processing is run when we
    // attempt to get new frame data can complete before the pending
    // GetFrameData call actually happens.
    pending_getframedata_ = base::BindPostTask(
        task_runner(), base::BindOnce(&CardboardRenderLoop::GetFrameData,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      std::move(options), std::move(callback)));
    return;
  }

  if (restrict_frame_data_) {
    DVLOG(2) << __func__ << ": frame data restricted, returning nullptr.";
    std::move(callback).Run(nullptr);
    return;
  }

  if (is_paused_) {
    DVLOG(2) << __func__ << ": paused but frame data not restricted. Resuming.";
    Resume();
  }

  base::TimeTicks now = base::TimeTicks::Now();
  mojom::XRFrameDataPtr frame_data = mojom::XRFrameData::New();

  frame_data->frame_id = webxr_->StartFrameAnimating();
  WebXrFrame* xr_frame = webxr_->GetAnimatingFrame();

  xr_frame->time_pose = now;
  xr_frame->bounds_left = left_bounds_;
  xr_frame->bounds_right = right_bounds_;

  if (CardboardImageTransport::UseSharedBuffer()) {
    // We aren't modifying the texture that we give to the page, so we just pass
    // in identity for the uv_transform.
    WebXrSharedBuffer* shared_buffer =
        cardboard_image_transport_->TransferFrame(webxr_.get(), texture_size_,
                                                  gfx::Transform());
    CHECK(shared_buffer);
    frame_data->buffer_shared_image = shared_buffer->shared_image->Export();
    frame_data->buffer_sync_token = shared_buffer->sync_token;
  }

  // Get the head pose
  int64_t timestamp_ns = GetBootTimeNano() + kPredictionTimeWithoutVsyncNanos;
  float position[3];
  float orientation[4];
  CardboardHeadTracker_getPose(head_tracker_.get(), timestamp_ns,
                               kViewportOrientation, position, orientation);

  // Translate the head pose into the viewer pose pointer
  // This needs to be inverted because the Cardboard SDK appears to be giving
  // back values that are the inverse of what WebXR expects.
  mojom::VRPosePtr pose = mojom::VRPose::New();
  pose->position = gfx::Point3F(-position[0], -position[1], -position[2]);
  pose->orientation = gfx::Quaternion(-orientation[0], -orientation[1],
                                      -orientation[2], orientation[3]);
  pose->emulated_position = true;

  gfx::Transform mojo_from_viewer = vr_utils::VrPoseToTransform(pose.get());
  frame_data->mojo_from_viewer = std::move(pose);

  // Get the view transform for each eye
  left_eye_->mojo_from_view =
      cardboard_image_transport_->GetMojoFromView(kLeft, mojo_from_viewer);
  right_eye_->mojo_from_view =
      cardboard_image_transport_->GetMojoFromView(kRight, mojo_from_viewer);

  frame_data->views.push_back(left_eye_.Clone());
  frame_data->views.push_back(right_eye_.Clone());

  std::vector<mojom::XRInputSourceStatePtr> input_state;
  input_state.push_back(GetInputSourceState());
  frame_data->input_state = std::move(input_state);

  frame_data->time_delta = now - base::TimeTicks();

  // TODO(crbug.com/40900872): Calculating
  // frame_data->rendering_time_ratio may be necessary for viewport scaling.
  std::move(callback).Run(std::move(frame_data));
}

bool CardboardRenderLoop::IsSubmitFrameExpected(int16_t frame_index) {
  DVLOG(3) << __func__ << ": Frame Index=" << frame_index
           << " submit_client_=" << !!submit_client_.get()
           << " HaveAnimatingFrame()=" << webxr_->HaveAnimatingFrame()
           << " pending_shutdown_=" << pending_shutdown_;
  // submit_client_ could be null when we exit presentation, if there were
  // pending SubmitFrame messages queued.  XRSessionClient::OnExitPresent
  // will clean up state in blink, so it doesn't wait for
  // OnSubmitFrameTransferred or OnSubmitFrameRendered. Similarly,
  // the animating frame state is cleared when exiting presentation,
  // and we should ignore a leftover queued SubmitFrame.
  if (!submit_client_.get() || !webxr_->HaveAnimatingFrame()) {
    return false;
  }

  if (pending_shutdown_) {
    return false;
  }

  WebXrFrame* animating_frame = webxr_->GetAnimatingFrame();
  animating_frame->time_js_submit = base::TimeTicks::Now();

  if (animating_frame->index != frame_index) {
    DVLOG(1) << __func__ << ": wrong frame index, got " << frame_index
             << ", expected " << animating_frame->index;
    presentation_receiver_.ReportBadMessage(
        "SubmitFrame called with wrong frame index");
    OnBindingDisconnect();
    return false;
  }

  // Frame looks valid.
  return true;
}

void CardboardRenderLoop::SubmitFrameMissing(int16_t frame_index,
                                             const gpu::SyncToken& sync_token) {
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);
  DVLOG(2) << __func__ << ": frame=" << frame_index;

  if (!IsSubmitFrameExpected(frame_index)) {
    return;
  }

  webxr_->RecycleUnusedAnimatingFrame();
  cardboard_image_transport_->WaitSyncToken(sync_token);
  FinishFrame(frame_index);

  if (pending_getframedata_) {
    std::move(pending_getframedata_).Run();
  }
}

void CardboardRenderLoop::SubmitFrame(int16_t frame_index,
                                      const gpu::MailboxHolder& mailbox,
                                      base::TimeDelta time_waited) {
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);
  DVLOG(2) << __func__ << ": frame=" << frame_index;
  CHECK(!CardboardImageTransport::UseSharedBuffer());

  if (!IsSubmitFrameExpected(frame_index)) {
    return;
  }

  webxr_->ProcessOrDefer(
      base::BindOnce(&CardboardRenderLoop::ProcessFrameFromMailbox,
                     weak_ptr_factory_.GetWeakPtr(), frame_index, mailbox));
}

void CardboardRenderLoop::ProcessFrameFromMailbox(
    int16_t frame_index,
    const gpu::MailboxHolder& mailbox) {
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);
  DVLOG(2) << __func__ << ": frame=" << frame_index;
  CHECK(webxr_->HaveProcessingFrame());
  CHECK(!CardboardImageTransport::UseSharedBuffer());

  // We aren't modifying the texture that we've received from the page, so we
  // just pass in identity.
  cardboard_image_transport_->CopyMailboxToSurfaceAndSwap(
      texture_size_, mailbox, gfx::Transform());

  // Notify the client that we're done with the mailbox so that the underlying
  // image is eligible for destruction.
  submit_client_->OnSubmitFrameTransferred(true);

  // Now wait for cardboard_image_transport_ to call RenderFrame indicating that
  // the image drawn onto the Surface is ready for consumption from the
  // SurfaceTexture.
}

void CardboardRenderLoop::SubmitFrameDrawnIntoTexture(
    int16_t frame_index,
    const gpu::SyncToken& sync_token,
    base::TimeDelta time_waited) {
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);
  DVLOG(2) << __func__ << ": frame=" << frame_index;
  CHECK(CardboardImageTransport::UseSharedBuffer());

  if (!IsSubmitFrameExpected(frame_index)) {
    return;
  }

  // Start processing the frame now if possible. If there's already a current
  // processing frame, defer it until that frame calls TryDeferredProcessing.
  webxr_->ProcessOrDefer(
      base::BindOnce(&CardboardRenderLoop::ProcessFrameDrawnIntoTexture,
                     weak_ptr_factory_.GetWeakPtr(), sync_token));
}

void CardboardRenderLoop::ProcessFrameDrawnIntoTexture(
    const gpu::SyncToken& sync_token) {
  cardboard_image_transport_->CreateGpuFenceForSyncToken(
      sync_token,
      base::BindOnce(&CardboardRenderLoop::OnWebXrTokenSignaled, GetWeakPtr()));

  if (pending_getframedata_) {
    std::move(pending_getframedata_).Run();
  }
}

void CardboardRenderLoop::OnWebXrTokenSignaled(
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  cardboard_image_transport_->ServerWaitForGpuFence(std::move(gpu_fence));
  RenderFrame(gfx::Transform());
}

void CardboardRenderLoop::TransitionProcessingFrameToRendering() {
  if (webxr_->HaveRenderingFrame()) {
    // It's possible, though unlikely, that the previous rendering frame hasn't
    // finished yet, for example if an unusually slow frame is followed by an
    // unusually quick one. In that case, wait for that frame to finish
    // rendering first before proceeding with this one. The state machine
    // doesn't permit two frames to be in rendering state at once. (Also, adding
    // even more GPU work in that condition would be counterproductive.)
    DVLOG(3) << __func__ << ": wait for previous rendering frame to complete";

    FinishRenderingFrame();
  }

  CHECK(!webxr_->HaveRenderingFrame());
  CHECK(webxr_->HaveProcessingFrame());
  auto* frame = webxr_->GetProcessingFrame();
  frame->time_copied = base::TimeTicks::Now();

  frame->render_completion_fence = nullptr;
  webxr_->TransitionFrameProcessingToRendering();

  // We finished processing a frame, unblock a potentially waiting next frame.
  webxr_->TryDeferredProcessing();
}

void CardboardRenderLoop::RenderFrame(const gfx::Transform& uv_transform) {
  DVLOG(2) << __func__;
  CHECK(webxr_->HaveProcessingFrame());
  int16_t frame_index = webxr_->GetProcessingFrame()->index;
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);

  TransitionProcessingFrameToRendering();

  cardboard_image_transport_->Render(webxr_.get(), /*framebuffer=*/0);

  FinishFrame(frame_index);

  if (submit_client_) {
    // Create a local GpuFence and pass it to the Renderer via IPC.
    std::unique_ptr<gl::GLFence> gl_fence = gl::GLFence::CreateForGpuFence();
    std::unique_ptr<gfx::GpuFence> gpu_fence2 = gl_fence->GetGpuFence();
    submit_client_->OnSubmitFrameGpuFence(
        gpu_fence2->GetGpuFenceHandle().Clone());
  }

  if (pending_getframedata_) {
    std::move(pending_getframedata_).Run();
  }
}

void CardboardRenderLoop::FinishRenderingFrame(WebXrFrame* frame) {
  CHECK(frame || webxr_->HaveRenderingFrame());
  if (!frame) {
    frame = webxr_->GetRenderingFrame();
  }

  if (!frame->render_completion_fence) {
    frame->render_completion_fence = gl::GLFence::CreateForGpuFence();
  }
  ClearRenderingFrame(frame);
}

void CardboardRenderLoop::ClearRenderingFrame(WebXrFrame* frame) {
  TRACE_EVENT1("gpu", __func__, "frame", frame->index);
  DVLOG(3) << __func__ << ": frame=" << frame->index;

  // Ensure that we're totally finished with the rendering frame, then collect
  // stats and move the frame out of the rendering path.
  DVLOG(3) << __func__ << ": client wait start";
  frame->render_completion_fence->ClientWait();
  DVLOG(3) << __func__ << ": client wait done";

  webxr_->EndFrameRendering(frame);
}

void CardboardRenderLoop::FinishFrame(int16_t frame_index) {
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);
  DVLOG(3) << __func__;

  surface_->SwapBuffers(base::DoNothing(), gfx::FrameData());

  // If we have a rendering frame we need to create a GLFence
  if (!webxr_->HaveRenderingFrame()) {
    return;
  }

  WebXrFrame* frame = webxr_->GetRenderingFrame();
  frame->render_completion_fence = gl::GLFence::CreateForGpuFence();
}

bool CardboardRenderLoop::CanStartNewAnimatingFrame() {
  if (pending_shutdown_) {
    return false;
  }

  if (webxr_->HaveAnimatingFrame()) {
    DVLOG(3) << __func__ << ": deferring, HaveAnimatingFrame";
    return false;
  }

  if (!webxr_->CanStartFrameAnimating()) {
    DVLOG(3) << __func__ << ": deferring, no available frames in swapchain";
    return false;
  }

  // If there are already two frames in flight, ensure that the rendering frame
  // completes first before starting a new animating frame. It may be complete
  // already, in that case just collect its statistics. (Don't wait if there's a
  // rendering frame but no processing frame.)
  if (webxr_->HaveProcessingFrame() && webxr_->HaveRenderingFrame()) {
    DVLOG(2) << __func__ << ": wait, have processing&rendering frames";
    FinishRenderingFrame();
  }

  // If there is still a rendering frame (we didn't wait for it), check
  // if it's complete. If yes, collect its statistics now so that the GPU
  // time estimate for the upcoming frame is up to date.
  if (webxr_->HaveRenderingFrame()) {
    auto* frame = webxr_->GetRenderingFrame();
    if (frame->render_completion_fence &&
        frame->render_completion_fence->HasCompleted()) {
      FinishRenderingFrame();
    }
  }

  return true;
}

void CardboardRenderLoop::UpdateLayerBounds(int16_t frame_index,
                                            const gfx::RectF& left_bounds,
                                            const gfx::RectF& right_bounds,
                                            const gfx::Size& source_size) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DVLOG(2) << __func__ << " source_size=" << source_size.ToString()
           << " left_bounds=" << left_bounds.ToString()
           << " right_bounds=" << right_bounds.ToString();

  // The first UpdateLayerBounds may arrive early, when there's
  // no animating frame yet. In that case, just save it in
  // `left_bounds_`/`right_bounds_` so that it's applied to the next animating
  // frame.
  if (webxr_->HaveAnimatingFrame()) {
    webxr_->GetAnimatingFrame()->bounds_left = left_bounds;
    webxr_->GetAnimatingFrame()->bounds_right = right_bounds;
  }

  left_bounds_ = left_bounds;
  right_bounds_ = right_bounds;

  // TODO(crbug.com/40900879): This was lifted from ArCoreGl which does
  // a very similar thing, but both cases actually use this texture_size_ to
  // render with and there isn't a corresponding item on the
  // WebXrPresentationState. Replacing the assignment below with a CHECK did not
  // trigger in my limited testing.
  // Early setting of `texture_size_` is OK since that's only used by the
  // animating frame. Processing/rendering frames use the bounds from
  // WebXRPresentationState.
  texture_size_ = source_size;
}

void CardboardRenderLoop::SetFrameDataRestricted(bool frame_data_restricted) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  DVLOG(3) << __func__ << ": frame_data_restricted=" << frame_data_restricted;
  restrict_frame_data_ = frame_data_restricted;
  if (restrict_frame_data_) {
    Pause();
  } else {
    Resume();
  }
}

void CardboardRenderLoop::OnTriggerEvent(bool pressed) {
  DVLOG(2) << __func__ << ": pressed=" << pressed;

  if (pressed) {
    trigger_pressed_ = true;
  } else if (trigger_pressed_) {
    trigger_pressed_ = false;
    trigger_clicked_ = true;
  }
}

device::mojom::XRInputSourceStatePtr
CardboardRenderLoop::GetInputSourceState() {
  device::mojom::XRInputSourceStatePtr state =
      device::mojom::XRInputSourceState::New();
  // Only one gaze input source to worry about, so it can have a static id.
  state->source_id = 1;

  // Report any trigger state changes made since the last call and reset the
  // state here.
  state->primary_input_pressed = trigger_pressed_;
  state->primary_input_clicked = trigger_clicked_;
  trigger_clicked_ = false;

  state->description = device::mojom::XRInputSourceDescription::New();

  // It's a gaze-cursor-based device.
  state->description->target_ray_mode = device::mojom::XRTargetRayMode::GAZING;
  state->emulated_position = true;

  // No implicit handedness
  state->description->handedness = device::mojom::XRHandedness::NONE;

  // Pointer and grip transforms are omitted since this is a gaze-based source.

  return state;
}

void CardboardRenderLoop::Pause() {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DVLOG(1) << __func__;

  CardboardHeadTracker_pause(head_tracker_.get());
  is_paused_ = true;
}

void CardboardRenderLoop::Resume() {
  DCHECK(task_runner()->BelongsToCurrentThread());
  DVLOG(1) << __func__;

  CardboardHeadTracker_resume(head_tracker_.get());
  is_paused_ = false;
}

}  // namespace device
