// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/arcore/arcore_gl.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <utility>
#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/queue.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "device/vr/android/arcore/ar_image_transport.h"
#include "device/vr/android/arcore/arcore.h"
#include "device/vr/android/arcore/arcore_math_utils.h"
#include "device/vr/android/arcore/arcore_session_utils.h"
#include "device/vr/android/arcore/type_converters.h"
#include "device/vr/android/web_xr_presentation_state.h"
#include "device/vr/public/mojom/pose.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_android_hardware_buffer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace {
// When scheduling the next ARCore update task, aim to have that run this much
// time ahead of when the next camera image is expected to be ready. In case
// the overall system is running slower than ideal, i.e. if the device switches
// from 30fps to 60fps, it'll catch up by this amount every frame until it
// reaches a new steady state.
constexpr base::TimeDelta kUpdateTargetDelta =
    base::TimeDelta::FromMilliseconds(2);

// Maximum delay for scheduling the next ARCore update. This helps ensure
// that there isn't an unreasonable delay due to a bogus estimate if the device
// is paused or unresponsive.
constexpr base::TimeDelta kUpdateMaxDelay =
    base::TimeDelta::FromMilliseconds(30);

const char kInputSourceProfileName[] = "generic-touchscreen";

const gfx::Size kDefaultFrameSize = {1, 1};
const display::Display::Rotation kDefaultRotation = display::Display::ROTATE_0;

}  // namespace

namespace device {

ArCoreGl::ArCoreGl(std::unique_ptr<ArImageTransport> ar_image_transport)
    : gl_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      ar_image_transport_(std::move(ar_image_transport)),
      webxr_(std::make_unique<vr::WebXrPresentationState>()) {
  DVLOG(1) << __func__;
}

ArCoreGl::~ArCoreGl() {
  DVLOG(1) << __func__;
  DCHECK(IsOnGlThread());
  ar_image_transport_->DestroySharedBuffers(webxr_.get());
  ar_image_transport_.reset();

  // Make sure mojo bindings are closed before proceeding with member
  // destruction. Specifically, destroying pending_getframedata_
  // must happen after closing bindings, see RunNextGetFrameData()
  // comments.
  CloseBindingsIfOpen();
}

void ArCoreGl::Initialize(
    vr::ArCoreSessionUtils* session_utils,
    ArCoreFactory* arcore_factory,
    gfx::AcceleratedWidget drawing_widget,
    const gfx::Size& frame_size,
    display::Display::Rotation display_rotation,
    const std::vector<device::mojom::XRSessionFeature>& enabled_features,
    base::OnceCallback<void(bool)> callback) {
  DVLOG(3) << __func__;

  DCHECK(IsOnGlThread());
  DCHECK(!is_initialized_);

  transfer_size_ = frame_size;
  camera_image_size_ = frame_size;
  display_rotation_ = display_rotation;
  // TODO(https://crbug.com/953503): start using the list to control the
  // behavior of local and unbounded spaces & send appropriate data back in
  // GetFrameData().
  enabled_features_.insert(enabled_features.begin(), enabled_features.end());
  should_update_display_geometry_ = true;

  if (!InitializeGl(drawing_widget)) {
    std::move(callback).Run(false);
    return;
  }

  // Get the activity context.
  base::android::ScopedJavaLocalRef<jobject> application_context =
      session_utils->GetApplicationContext();
  if (!application_context.obj()) {
    DLOG(ERROR) << "Unable to retrieve the Java context/activity!";
    std::move(callback).Run(false);
    return;
  }

  arcore_ = arcore_factory->Create();
  if (!arcore_->Initialize(application_context, enabled_features_)) {
    DLOG(ERROR) << "ARCore failed to initialize";
    std::move(callback).Run(false);
    return;
  }

  DVLOG(3) << "ar_image_transport_->Initialize()...";
  ar_image_transport_->Initialize(
      webxr_.get(),
      base::BindOnce(&ArCoreGl::OnArImageTransportReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  // Set the texture on ArCore to render the camera. Must be after
  // ar_image_transport_->Initialize().
  arcore_->SetCameraTexture(ar_image_transport_->GetCameraTextureId());
  // Set the Geometry to ensure consistent behaviour.
  arcore_->SetDisplayGeometry(kDefaultFrameSize, kDefaultRotation);
}

void ArCoreGl::OnArImageTransportReady(
    base::OnceCallback<void(bool)> callback) {
  DVLOG(3) << __func__;
  is_initialized_ = true;
  webxr_->NotifyMailboxBridgeReady();
  std::move(callback).Run(true);
}

void ArCoreGl::CreateSession(mojom::VRDisplayInfoPtr display_info,
                             ArCoreGlCreateSessionCallback create_callback,
                             base::OnceClosure shutdown_callback) {
  DVLOG(3) << __func__;

  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);

  session_shutdown_callback_ = std::move(shutdown_callback);

  CloseBindingsIfOpen();

  device::mojom::XRPresentationTransportOptionsPtr transport_options =
      device::mojom::XRPresentationTransportOptions::New();
  transport_options->wait_for_gpu_fence = true;

  if (ar_image_transport_->UseSharedBuffer()) {
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
    ar_image_transport_->SetFrameAvailableCallback(base::BindRepeating(
        &ArCoreGl::OnTransportFrameAvailable, weak_ptr_factory_.GetWeakPtr()));
  }

  auto submit_frame_sink = device::mojom::XRPresentationConnection::New();
  submit_frame_sink->client_receiver =
      submit_client_.BindNewPipeAndPassReceiver();
  submit_frame_sink->provider =
      presentation_receiver_.BindNewPipeAndPassRemote();
  submit_frame_sink->transport_options = std::move(transport_options);

  display_info_ = std::move(display_info);

  std::move(create_callback)
      .Run(frame_data_receiver_.BindNewPipeAndPassRemote(),
           display_info_->Clone(),
           session_controller_receiver_.BindNewPipeAndPassRemote(),
           std::move(submit_frame_sink));

  frame_data_receiver_.set_disconnect_handler(base::BindOnce(
      &ArCoreGl::OnBindingDisconnect, weak_ptr_factory_.GetWeakPtr()));
  session_controller_receiver_.set_disconnect_handler(base::BindOnce(
      &ArCoreGl::OnBindingDisconnect, weak_ptr_factory_.GetWeakPtr()));
}

bool ArCoreGl::InitializeGl(gfx::AcceleratedWidget drawing_widget) {
  DVLOG(3) << __func__;

  DCHECK(IsOnGlThread());
  DCHECK(!is_initialized_);

  if (gl::GetGLImplementation() == gl::kGLImplementationNone &&
      !gl::init::InitializeGLOneOff()) {
    DLOG(ERROR) << "gl::init::InitializeGLOneOff failed";
    return false;
  }

  scoped_refptr<gl::GLSurface> surface =
      gl::init::CreateViewGLSurface(drawing_widget);
  DVLOG(3) << "surface=" << surface.get();
  if (!surface.get()) {
    DLOG(ERROR) << "gl::init::CreateViewGLSurface failed";
    return false;
  }

  gl::GLContextAttribs context_attribs;
  // When using augmented images or certain other ARCore features that involve a
  // frame delay, ARCore's shared EGL context needs to be compatible with ours.
  // Any mismatches result in a EGL_BAD_MATCH error, including different reset
  // notification behavior according to
  // https://www.khronos.org/registry/EGL/specs/eglspec.1.5.pdf page 56.
  // Chromium defaults to lose context on reset when the robustness extension is
  // present, even if robustness features are not requested specifically.
  context_attribs.client_major_es_version = 3;
  context_attribs.client_minor_es_version = 0;
  context_attribs.lose_context_on_reset = false;

  scoped_refptr<gl::GLContext> context =
      gl::init::CreateGLContext(nullptr, surface.get(), context_attribs);
  if (!context.get()) {
    DLOG(ERROR) << "gl::init::CreateGLContext failed";
    return false;
  }
  if (!context->MakeCurrent(surface.get())) {
    DLOG(ERROR) << "gl::GLContext::MakeCurrent() failed";
    return false;
  }

  // Assign the surface and context members now that initialization has
  // succeeded.
  surface_ = std::move(surface);
  context_ = std::move(context);

  DVLOG(3) << "done";
  return true;
}

void ArCoreGl::GetFrameData(
    mojom::XRFrameDataRequestOptionsPtr options,
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  TRACE_EVENT0("gpu", __func__);

  if (webxr_->HaveAnimatingFrame()) {
    DVLOG(3) << __func__ << ": deferring, HaveAnimatingFrame";
    pending_getframedata_ =
        base::BindOnce(&ArCoreGl::GetFrameData, GetWeakPtr(),
                       std::move(options), std::move(callback));
    return;
  }

  DVLOG(3) << __func__ << ": should_update_display_geometry_="
           << should_update_display_geometry_
           << ", transfer_size_=" << transfer_size_.ToString()
           << ", display_rotation_=" << display_rotation_;

  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);

  if (restrict_frame_data_) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (is_paused_) {
    DVLOG(2) << __func__ << ": paused but frame data not restricted. Resuming.";
    Resume();
  }

  // Check if the frame_size and display_rotation updated last frame. If yes,
  // apply the update for this frame. In the current implementation, this should
  // only happen once per session since we don't support mid-session rotation or
  // resize.
  if (should_recalculate_uvs_) {
    // Get the UV transform matrix from ArCore's UV transform.
    uv_transform_ = arcore_->GetCameraUvFromScreenUvTransform();

    DVLOG(3) << __func__ << ": uv_transform_=" << uv_transform_.ToString();

    // We need near/far distances to make a projection matrix. The actual
    // values don't matter, the Renderer will recalculate dependent values
    // based on the application's near/far settngs.
    constexpr float depth_near = 0.1f;
    constexpr float depth_far = 1000.f;
    projection_ = arcore_->GetProjectionMatrix(depth_near, depth_far);
    auto m = projection_.matrix();
    float left = depth_near * (m.get(2, 0) - 1.f) / m.get(0, 0);
    float right = depth_near * (m.get(2, 0) + 1.f) / m.get(0, 0);
    float bottom = depth_near * (m.get(2, 1) - 1.f) / m.get(1, 1);
    float top = depth_near * (m.get(2, 1) + 1.f) / m.get(1, 1);

    // Also calculate the inverse projection which is needed for converting
    // screen touches to world rays.
    bool has_inverse = projection_.GetInverse(&inverse_projection_);
    DCHECK(has_inverse);

    // VRFieldOfView wants positive angles.
    mojom::VRFieldOfViewPtr field_of_view = mojom::VRFieldOfView::New();
    field_of_view->left_degrees = gfx::RadToDeg(atanf(-left / depth_near));
    field_of_view->right_degrees = gfx::RadToDeg(atanf(right / depth_near));
    field_of_view->down_degrees = gfx::RadToDeg(atanf(-bottom / depth_near));
    field_of_view->up_degrees = gfx::RadToDeg(atanf(top / depth_near));
    DVLOG(3) << " fov degrees up=" << field_of_view->up_degrees
             << " down=" << field_of_view->down_degrees
             << " left=" << field_of_view->left_degrees
             << " right=" << field_of_view->right_degrees;

    display_info_->left_eye->field_of_view = std::move(field_of_view);
    display_info_changed_ = true;

    should_recalculate_uvs_ = false;
  }

  // Now check if the frame_size or display_rotation needs to be updated
  // for the next frame. This must happen after the should_recalculate_uvs_
  // check above to ensure it executes with the needed one-frame delay.
  // The delay is needed due to the fact that ArCoreImpl already got a frame
  // and we don't want to calculate uvs for stale frame with new geometry.
  if (should_update_display_geometry_) {
    // Set display geometry before calling Update. It's a pending request that
    // applies to the next frame.
    arcore_->SetDisplayGeometry(camera_image_size_, display_rotation_);

    // Tell the uvs to recalculate on the next animation frame, by which time
    // SetDisplayGeometry will have set the new values in arcore_.
    should_recalculate_uvs_ = true;
    should_update_display_geometry_ = false;
  }

  bool camera_updated = false;
  base::TimeTicks arcore_update_started = base::TimeTicks::Now();
  mojom::VRPosePtr pose = arcore_->Update(&camera_updated);
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta frame_timestamp = arcore_->GetFrameTimestamp();

  DVLOG(3) << __func__ << ": frame_timestamp=" << frame_timestamp;

  if (!arcore_last_frame_timestamp_.is_zero()) {
    arcore_frame_interval_ = frame_timestamp - arcore_last_frame_timestamp_;
    arcore_update_next_expected_ = now + arcore_frame_interval_;
  }
  arcore_last_frame_timestamp_ = frame_timestamp;
  base::TimeDelta arcore_update_elapsed = now - arcore_update_started;
  TRACE_COUNTER1("gpu", "ARCore update elapsed (ms)",
                 arcore_update_elapsed.InMilliseconds());

  if (!camera_updated) {
    DVLOG(1) << "arcore_->Update() failed";
    std::move(callback).Run(nullptr);
    have_camera_image_ = false;
    return;
  }

  // First frame will be requested without a prior call to SetDisplayGeometry -
  // handle this case.
  if (transfer_size_.IsEmpty()) {
    DLOG(ERROR) << "No valid AR frame size provided!";
    std::move(callback).Run(nullptr);
    have_camera_image_ = false;
    return;
  }

  have_camera_image_ = true;
  mojom::XRFrameDataPtr frame_data = mojom::XRFrameData::New();

  // Check if floor height estimate has changed.
  float new_floor_height_estimate = arcore_->GetEstimatedFloorHeight();
  if (!floor_height_estimate_ ||
      *floor_height_estimate_ != new_floor_height_estimate) {
    floor_height_estimate_ = new_floor_height_estimate;

    frame_data->stage_parameters_updated = true;
    frame_data->stage_parameters = mojom::VRStageParameters::New();
    frame_data->stage_parameters->mojo_from_floor = gfx::Transform();
    frame_data->stage_parameters->mojo_from_floor.Translate3d(
        0, (-1 * *floor_height_estimate_), 0);
  }

  frame_data->frame_id = webxr_->StartFrameAnimating();
  DVLOG(2) << __func__ << " frame=" << frame_data->frame_id;
  TRACE_EVENT1("gpu", __func__, "frame", frame_data->frame_id);

  vr::WebXrFrame* xrframe = webxr_->GetAnimatingFrame();
  xrframe->time_pose = now;

  if (display_info_changed_) {
    frame_data->left_eye = display_info_->left_eye.Clone();
    display_info_changed_ = false;
  }

  if (ar_image_transport_->UseSharedBuffer()) {
    // Set up a shared buffer for the renderer to draw into, it'll be sent
    // alongside the frame pose.
    gpu::MailboxHolder buffer_holder = ar_image_transport_->TransferFrame(
        webxr_.get(), transfer_size_, uv_transform_);
    frame_data->buffer_holder = buffer_holder;

    if (IsFeatureEnabled(device::mojom::XRSessionFeature::CAMERA_ACCESS)) {
      gpu::MailboxHolder camera_image_buffer_holder =
          ar_image_transport_->TransferCameraImageFrame(
              webxr_.get(), transfer_size_, uv_transform_);
      frame_data->camera_image_buffer_holder = camera_image_buffer_holder;
    }
  }

  // Create the frame data to return to the renderer.
  if (!pose) {
    DVLOG(1) << __func__ << ": pose unavailable!";
  }

  frame_data->pose = std::move(pose);
  frame_data->time_delta = now - base::TimeTicks();

  fps_meter_.AddFrame(now);
  TRACE_COUNTER1("gpu", "WebXR FPS", fps_meter_.GetFPS());

  // Post a task to finish processing the frame to give a chance for
  // OnScreenTouch() tasks to run and added anchors to be registered.
  gl_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ArCoreGl::ProcessFrame, weak_ptr_factory_.GetWeakPtr(),
                     std::move(options), std::move(frame_data),
                     std::move(callback)));
}

bool ArCoreGl::IsSubmitFrameExpected(int16_t frame_index) {
  // submit_client_ could be null when we exit presentation, if there were
  // pending SubmitFrame messages queued.  XRSessionClient::OnExitPresent
  // will clean up state in blink, so it doesn't wait for
  // OnSubmitFrameTransferred or OnSubmitFrameRendered. Similarly,
  // the animating frame state is cleared when exiting presentation,
  // and we should ignore a leftover queued SubmitFrame.
  if (!submit_client_.get() || !webxr_->HaveAnimatingFrame())
    return false;

  vr::WebXrFrame* animating_frame = webxr_->GetAnimatingFrame();
  animating_frame->time_js_submit = base::TimeTicks::Now();

  if (animating_frame->index != frame_index) {
    DVLOG(1) << __func__ << ": wrong frame index, got " << frame_index
             << ", expected " << animating_frame->index;
    mojo::ReportBadMessage("SubmitFrame called with wrong frame index");
    CloseBindingsIfOpen();
    return false;
  }

  // Frame looks valid.
  return true;
}

void ArCoreGl::CopyCameraImageToFramebuffer() {
  DVLOG(2) << __func__;

  // Draw the current camera texture to the output default framebuffer now, if
  // available.
  if (have_camera_image_) {
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
    ar_image_transport_->CopyCameraImageToFramebuffer(camera_image_size_,
                                                      uv_transform_);
    have_camera_image_ = false;
  }

  // We're done with the camera image for this frame, post a task to start the
  // next ARCore update if we had deferred it. This will get the next frame's
  // camera image and pose in parallel while we're waiting for this frame's
  // rendered image.
  if (pending_getframedata_) {
    base::TimeDelta delay = base::TimeDelta();
    if (!arcore_update_next_expected_.is_null()) {
      // Try to schedule the next ARCore update to happen a short time before
      // the camera image is expected to be ready..
      delay = arcore_update_next_expected_ - base::TimeTicks::Now() -
              kUpdateTargetDelta;
      if (delay < base::TimeDelta()) {
        // Negative sleep means we're behind schedule, run immediately.
        delay = base::TimeDelta();
      } else {
        if (delay > kUpdateMaxDelay) {
          DVLOG(1) << __func__ << ": delay " << delay << " too long, clamp to "
                   << kUpdateMaxDelay;
          delay = kUpdateMaxDelay;
        }
      }
    }
    TRACE_COUNTER1("gpu", "ARCore update schedule (ms)",
                   delay.InMilliseconds());
    // RunNextGetFrameData is needed since we must retain ownership of the mojo
    // callback inside the pending_getframedata_ closure.
    gl_thread_task_runner_->PostDelayedTask(
        FROM_HERE, base::BindOnce(&ArCoreGl::RunNextGetFrameData, GetWeakPtr()),
        delay);
  }
}

void ArCoreGl::RunNextGetFrameData() {
  DVLOG(3) << __func__;
  DCHECK(pending_getframedata_);
  std::move(pending_getframedata_).Run();
}

void ArCoreGl::FinishFrame(int16_t frame_index) {
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);
  DVLOG(3) << __func__;
  surface_->SwapBuffers(base::DoNothing());

  // If we have a rendering frame (we don't if the app didn't submit one),
  // update statistics.
  if (!webxr_->HaveRenderingFrame())
    return;
  vr::WebXrFrame* frame = webxr_->GetRenderingFrame();
  base::TimeDelta pose_to_submit = frame->time_js_submit - frame->time_pose;
  base::TimeDelta submit_to_swap =
      base::TimeTicks::Now() - frame->time_js_submit;
  TRACE_COUNTER2("gpu", "WebXR frame time (ms)", "javascript",
                 pose_to_submit.InMilliseconds(), "processing",
                 submit_to_swap.InMilliseconds());
}

void ArCoreGl::SubmitFrameMissing(int16_t frame_index,
                                  const gpu::SyncToken& sync_token) {
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);
  DVLOG(2) << __func__;

  if (!IsSubmitFrameExpected(frame_index))
    return;

  webxr_->RecycleUnusedAnimatingFrame();
  ar_image_transport_->WaitSyncToken(sync_token);

  CopyCameraImageToFramebuffer();

  FinishFrame(frame_index);
  DVLOG(3) << __func__ << ": frame=" << frame_index << " SwapBuffers";
}

void ArCoreGl::SubmitFrame(int16_t frame_index,
                           const gpu::MailboxHolder& mailbox,
                           base::TimeDelta time_waited) {
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);
  DVLOG(2) << __func__ << ": frame=" << frame_index;
  DCHECK(!ar_image_transport_->UseSharedBuffer());

  if (!IsSubmitFrameExpected(frame_index))
    return;

  webxr_->ProcessOrDefer(base::BindOnce(&ArCoreGl::ProcessFrameFromMailbox,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        frame_index, mailbox));
}

void ArCoreGl::ProcessFrameFromMailbox(int16_t frame_index,
                                       const gpu::MailboxHolder& mailbox) {
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);
  DVLOG(2) << __func__ << ": frame=" << frame_index;
  DCHECK(webxr_->HaveProcessingFrame());
  DCHECK(!ar_image_transport_->UseSharedBuffer());

  ar_image_transport_->CopyMailboxToSurfaceAndSwap(transfer_size_, mailbox);
  // Notify the client that we're done with the mailbox so that the underlying
  // image is eligible for destruction.
  submit_client_->OnSubmitFrameTransferred(true);

  CopyCameraImageToFramebuffer();

  // Now wait for ar_image_transport_ to call OnTransportFrameAvailable
  // indicating that the image drawn onto the Surface is ready for consumption
  // from the SurfaceTexture.
}

void ArCoreGl::OnTransportFrameAvailable(const gfx::Transform& uv_transform) {
  DVLOG(2) << __func__;
  DCHECK(!ar_image_transport_->UseSharedBuffer());
  DCHECK(webxr_->HaveProcessingFrame());
  int16_t frame_index = webxr_->GetProcessingFrame()->index;
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);
  webxr_->GetProcessingFrame()->time_copied = base::TimeTicks::Now();
  webxr_->TransitionFrameProcessingToRendering();

  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
  ar_image_transport_->CopyDrawnImageToFramebuffer(
      webxr_.get(), camera_image_size_, uv_transform);

  FinishFrame(frame_index);

  webxr_->EndFrameRendering();

  if (submit_client_) {
    // Create a local GpuFence and pass it to the Renderer via IPC.
    std::unique_ptr<gl::GLFence> gl_fence = gl::GLFence::CreateForGpuFence();
    std::unique_ptr<gfx::GpuFence> gpu_fence2 = gl_fence->GetGpuFence();
    submit_client_->OnSubmitFrameGpuFence(
        gpu_fence2->GetGpuFenceHandle().Clone());
  }
  // We finished processing a frame, unblock a potentially waiting next frame.
  webxr_->TryDeferredProcessing();
}

void ArCoreGl::SubmitFrameWithTextureHandle(
    int16_t frame_index,
    mojo::PlatformHandle texture_handle) {
  NOTIMPLEMENTED();
}

void ArCoreGl::SubmitFrameDrawnIntoTexture(int16_t frame_index,
                                           const gpu::SyncToken& sync_token,
                                           base::TimeDelta time_waited) {
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);
  DVLOG(2) << __func__ << ": frame=" << frame_index;
  DCHECK(ar_image_transport_->UseSharedBuffer());

  if (!IsSubmitFrameExpected(frame_index))
    return;

  // Start processing the frame now if possible. If there's already a current
  // processing frame, defer it until that frame calls TryDeferredProcessing.
  webxr_->ProcessOrDefer(base::BindOnce(&ArCoreGl::ProcessFrameDrawnIntoTexture,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        frame_index, sync_token));
}

void ArCoreGl::ProcessFrameDrawnIntoTexture(int16_t frame_index,
                                            const gpu::SyncToken& sync_token) {
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);

  DCHECK(webxr_->HaveProcessingFrame());
  DCHECK(ar_image_transport_->UseSharedBuffer());
  CopyCameraImageToFramebuffer();

  ar_image_transport_->CreateGpuFenceForSyncToken(
      sync_token, base::BindOnce(&ArCoreGl::OnWebXrTokenSignaled, GetWeakPtr(),
                                 frame_index));
}

void ArCoreGl::OnWebXrTokenSignaled(int16_t frame_index,
                                    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);
  DVLOG(3) << __func__ << ": frame=" << frame_index;

  ar_image_transport_->ServerWaitForGpuFence(std::move(gpu_fence));

  DCHECK(webxr_->HaveProcessingFrame());
  DCHECK(ar_image_transport_->UseSharedBuffer());
  webxr_->GetProcessingFrame()->time_copied = base::TimeTicks::Now();
  webxr_->TransitionFrameProcessingToRendering();

  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
  ar_image_transport_->CopyDrawnImageToFramebuffer(
      webxr_.get(), camera_image_size_, shared_buffer_transform_);

  FinishFrame(frame_index);

  webxr_->EndFrameRendering();

  if (submit_client_) {
    // Create a local GpuFence and pass it to the Renderer via IPC.
    std::unique_ptr<gl::GLFence> gl_fence = gl::GLFence::CreateForGpuFence();
    std::unique_ptr<gfx::GpuFence> gpu_fence2 = gl_fence->GetGpuFence();
    submit_client_->OnSubmitFrameGpuFence(
        gpu_fence2->GetGpuFenceHandle().Clone());
  }
  // We finished processing a frame, unblock a potentially waiting next frame.
  webxr_->TryDeferredProcessing();
}

void ArCoreGl::UpdateLayerBounds(int16_t frame_index,
                                 const gfx::RectF& left_bounds,
                                 const gfx::RectF& right_bounds,
                                 const gfx::Size& source_size) {
  DVLOG(2) << __func__ << " source_size=" << source_size.ToString();

  transfer_size_ = source_size;
}

void ArCoreGl::GetEnvironmentIntegrationProvider(
    mojo::PendingAssociatedReceiver<
        device::mojom::XREnvironmentIntegrationProvider> environment_provider) {
  DVLOG(3) << __func__;

  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);

  environment_receiver_.reset();
  environment_receiver_.Bind(std::move(environment_provider));
  environment_receiver_.set_disconnect_handler(base::BindOnce(
      &ArCoreGl::OnBindingDisconnect, weak_ptr_factory_.GetWeakPtr()));
}

void ArCoreGl::SetInputSourceButtonListener(
    mojo::PendingAssociatedRemote<device::mojom::XRInputSourceButtonListener>) {
  // Input eventing is not supported. This call should not
  // be made on this device.
  mojo::ReportBadMessage("Input eventing is not supported.");
}

void ArCoreGl::SubscribeToHitTest(
    mojom::XRNativeOriginInformationPtr native_origin_information,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    mojom::XRRayPtr ray,
    mojom::XREnvironmentIntegrationProvider::SubscribeToHitTestCallback
        callback) {
  DVLOG(2) << __func__ << ": ray origin=" << ray->origin.ToString()
           << ", ray direction=" << ray->direction.ToString();

  // Input source state information is known to ArCoreGl and not to ArCore -
  // check if we recognize the input source id.

  if (native_origin_information->is_input_source_id()) {
    DVLOG(1) << __func__
             << ": ARCore device supports only transient input sources for "
                "now. Rejecting subscription request.";
    std::move(callback).Run(
        device::mojom::SubscribeToHitTestResult::FAILURE_GENERIC, 0);
    return;
  }

  base::Optional<uint64_t> maybe_subscription_id = arcore_->SubscribeToHitTest(
      std::move(native_origin_information), entity_types, std::move(ray));

  if (maybe_subscription_id) {
    DVLOG(2) << __func__ << ": subscription_id=" << *maybe_subscription_id;
    std::move(callback).Run(device::mojom::SubscribeToHitTestResult::SUCCESS,
                            *maybe_subscription_id);
  } else {
    DVLOG(1) << __func__ << ": subscription failed";
    std::move(callback).Run(
        device::mojom::SubscribeToHitTestResult::FAILURE_GENERIC, 0);
  }
}

void ArCoreGl::SubscribeToHitTestForTransientInput(
    const std::string& profile_name,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    mojom::XRRayPtr ray,
    mojom::XREnvironmentIntegrationProvider::
        SubscribeToHitTestForTransientInputCallback callback) {
  DVLOG(2) << __func__ << ": ray origin=" << ray->origin.ToString()
           << ", ray direction=" << ray->direction.ToString();

  base::Optional<uint64_t> maybe_subscription_id =
      arcore_->SubscribeToHitTestForTransientInput(profile_name, entity_types,
                                                   std::move(ray));

  if (maybe_subscription_id) {
    DVLOG(2) << __func__ << ": subscription_id=" << *maybe_subscription_id;
    std::move(callback).Run(device::mojom::SubscribeToHitTestResult::SUCCESS,
                            *maybe_subscription_id);
  } else {
    DVLOG(1) << __func__ << ": subscription failed";
    std::move(callback).Run(
        device::mojom::SubscribeToHitTestResult::FAILURE_GENERIC, 0);
  }
}

void ArCoreGl::UnsubscribeFromHitTest(uint64_t subscription_id) {
  DVLOG(2) << __func__;

  arcore_->UnsubscribeFromHitTest(subscription_id);
}

void ArCoreGl::CreateAnchor(
    mojom::XRNativeOriginInformationPtr native_origin_information,
    const device::Pose& native_origin_from_anchor,
    CreateAnchorCallback callback) {
  DVLOG(2) << __func__;

  DCHECK(native_origin_information);

  arcore_->CreateAnchor(*native_origin_information, native_origin_from_anchor,
                        std::move(callback));
}

void ArCoreGl::CreatePlaneAnchor(
    mojom::XRNativeOriginInformationPtr native_origin_information,
    const device::Pose& native_origin_from_anchor,
    uint64_t plane_id,
    CreatePlaneAnchorCallback callback) {
  DVLOG(2) << __func__ << ": plane_id=" << plane_id;

  DCHECK(native_origin_information);
  DCHECK(plane_id);

  arcore_->CreatePlaneAttachedAnchor(*native_origin_information,
                                     native_origin_from_anchor, plane_id,
                                     std::move(callback));
}

void ArCoreGl::DetachAnchor(uint64_t anchor_id) {
  DVLOG(2) << __func__;

  arcore_->DetachAnchor(anchor_id);
}

void ArCoreGl::SetFrameDataRestricted(bool frame_data_restricted) {
  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);

  DVLOG(3) << __func__ << ": frame_data_restricted=" << frame_data_restricted;
  restrict_frame_data_ = frame_data_restricted;
  if (restrict_frame_data_) {
    Pause();
  } else {
    Resume();
  }
}

void ArCoreGl::ProcessFrame(
    mojom::XRFrameDataRequestOptionsPtr options,
    mojom::XRFrameDataPtr frame_data,
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  DVLOG(3) << __func__ << " frame=" << frame_data->frame_id << ", pose valid? "
           << (frame_data->pose ? true : false);

  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);

  if (frame_data->pose) {
    DCHECK(frame_data->pose->position);
    DCHECK(frame_data->pose->orientation);

    frame_data->input_state = GetInputSourceStates();

    device::Pose mojo_from_viewer(*frame_data->pose->position,
                                  *frame_data->pose->orientation);

    // Get results for hit test subscriptions.
    frame_data->hit_test_subscription_results =
        arcore_->GetHitTestSubscriptionResults(mojo_from_viewer.ToTransform(),
                                               *frame_data->input_state);

    arcore_->ProcessAnchorCreationRequests(
        mojo_from_viewer.ToTransform(), *frame_data->input_state,
        frame_data->time_delta + base::TimeTicks());
  }

  // Get anchors data, including anchors created this frame.
  frame_data->anchors_data = arcore_->GetAnchorsData();

  // Get planes data if it was requested.
  if (IsFeatureEnabled(device::mojom::XRSessionFeature::PLANE_DETECTION)) {
    frame_data->detected_planes_data = arcore_->GetDetectedPlanesData();
  }

  // Get lighting estimation data if it was requested.
  if (options && options->include_lighting_estimation_data) {
    frame_data->light_estimation_data = arcore_->GetLightEstimationData();
  }

  if (IsFeatureEnabled(device::mojom::XRSessionFeature::DEPTH)) {
    frame_data->depth_data = arcore_->GetDepthData();
  }

  // Running this callback after resolving all the hit-test requests ensures
  // that we satisfy the guarantee of the WebXR hit-test spec - that the
  // hit-test promise resolves immediately prior to the frame for which it is
  // valid.
  std::move(callback).Run(std::move(frame_data));
}

void ArCoreGl::OnScreenTouch(bool is_primary,
                             bool touching,
                             int32_t pointer_id,
                             const gfx::PointF& touch_point) {
  DVLOG(2) << __func__ << ": is_primary=" << is_primary
           << ", pointer_id=" << pointer_id << ", touching=" << touching
           << ", touch_point=" << touch_point.ToString();

  if (!base::Contains(pointer_id_to_input_source_id_, pointer_id)) {
    // assign ID
    DCHECK(next_input_source_id_ != 0) << "ID equal to 0 cannot be used!";
    pointer_id_to_input_source_id_[pointer_id] = next_input_source_id_;

    DVLOG(3)
        << __func__
        << " : pointer id not previously recognized, assigned input source id="
        << next_input_source_id_;

    // Overflow is defined behavior for unsigned integers, just make sure that
    // we never send out ID = 0.
    next_input_source_id_++;
    if (next_input_source_id_ == 0) {
      next_input_source_id_ = 1;
    }
  }

  uint32_t inputSourceId = pointer_id_to_input_source_id_[pointer_id];
  ScreenTouchEvent& screen_touch_event = screen_touch_events_[inputSourceId];

  screen_touch_event.pointer_id = pointer_id;
  screen_touch_event.is_primary = is_primary;
  screen_touch_event.screen_last_touch = touch_point;
  screen_touch_event.screen_touch_active = touching;
  if (touching) {
    screen_touch_event.screen_touch_pending = true;
  }
}

std::vector<mojom::XRInputSourceStatePtr> ArCoreGl::GetInputSourceStates() {
  DVLOG(3) << __func__;

  std::vector<mojom::XRInputSourceStatePtr> result;

  for (auto& id_and_touch_event : screen_touch_events_) {
    bool is_primary = id_and_touch_event.second.is_primary;
    bool screen_touch_pending = id_and_touch_event.second.screen_touch_pending;
    bool screen_touch_active = id_and_touch_event.second.screen_touch_active;
    gfx::PointF screen_last_touch = id_and_touch_event.second.screen_last_touch;

    DVLOG(3) << __func__
             << " : pointer for input source id=" << id_and_touch_event.first
             << ", pointer_id=" << id_and_touch_event.second.pointer_id
             << ", active=" << screen_touch_active
             << ", pending=" << screen_touch_pending;

    // If there's no active screen touch, and no unreported past click
    // event, don't report a device.
    if (!screen_touch_pending && !screen_touch_active) {
      continue;
    }

    device::mojom::XRInputSourceStatePtr state =
        device::mojom::XRInputSourceState::New();

    state->source_id = id_and_touch_event.first;

    state->is_auxiliary = !is_primary;

    state->primary_input_pressed = screen_touch_active;

    // If the touch is not active but pending, it means that it was clicked
    // within a single frame.
    if (!screen_touch_active && screen_touch_pending) {
      state->primary_input_clicked = true;

      // Clear screen_touch_pending for this input source - we have consumed it.
      id_and_touch_event.second.screen_touch_pending = false;
    }

    // Save the touch point for use in Blink's XR input event deduplication.
    state->overlay_pointer_position = screen_last_touch;

    state->description = device::mojom::XRInputSourceDescription::New();

    state->description->handedness = device::mojom::XRHandedness::NONE;

    state->description->target_ray_mode =
        device::mojom::XRTargetRayMode::TAPPING;

    state->description->profiles.push_back(kInputSourceProfileName);

    // Controller doesn't have a measured position.
    state->emulated_position = true;

    // The Renderer code ignores state->grip for TAPPING (screen-based) target
    // ray mode, so we don't bother filling it in here. If this does get used at
    // some point in the future, this should be set to the inverse of the
    // pose rigid transform.

    // Get a viewer-space ray from screen-space coordinates by applying the
    // inverse of the projection matrix. Z coordinate of -1 means the point will
    // be projected onto the projection matrix near plane. See also
    // third_party/blink/renderer/modules/xr/xr_view.cc's UnprojectPointer.
    const float x_normalized =
        screen_last_touch.x() / camera_image_size_.width() * 2.f - 1.f;
    const float y_normalized =
        (1.f - screen_last_touch.y() / camera_image_size_.height()) * 2.f - 1.f;
    gfx::Point3F touch_point(x_normalized, y_normalized, -1.f);
    DVLOG(3) << __func__ << ": touch_point=" << touch_point.ToString();
    inverse_projection_.TransformPoint(&touch_point);
    DVLOG(3) << __func__ << ": unprojected=" << touch_point.ToString();

    // Ray points along -Z in ray space, so we need to flip it to get
    // the +Z axis unit vector.
    gfx::Vector3dF ray_backwards(-touch_point.x(), -touch_point.y(),
                                 -touch_point.z());
    gfx::Vector3dF new_z;
    bool can_normalize = ray_backwards.GetNormalized(&new_z);
    DCHECK(can_normalize);

    // Complete the ray-space basis by adding X and Y unit
    // vectors based on cross products.
    const gfx::Vector3dF kUp(0.f, 1.f, 0.f);
    gfx::Vector3dF new_x(kUp);
    new_x.Cross(new_z);
    new_x.GetNormalized(&new_x);
    gfx::Vector3dF new_y(new_z);
    new_y.Cross(new_x);
    new_y.GetNormalized(&new_y);

    // Fill in the transform matrix in row-major order. The first three columns
    // contain the basis vectors, the fourth column the position offset.
    gfx::Transform viewer_from_pointer(
        new_x.x(), new_y.x(), new_z.x(), touch_point.x(),  // row 1
        new_x.y(), new_y.y(), new_z.y(), touch_point.y(),  // row 2
        new_x.z(), new_y.z(), new_z.z(), touch_point.z(),  // row 3
        0, 0, 0, 1);
    DVLOG(3) << __func__ << ": viewer_from_pointer=\n"
             << viewer_from_pointer.ToString();

    state->description->input_from_pointer = viewer_from_pointer;

    // Create the gamepad object and modify necessary fields.
    state->gamepad = device::Gamepad{};
    state->gamepad->connected = true;
    state->gamepad->id[0] = '\0';
    state->gamepad->timestamp =
        base::TimeTicks::Now().since_origin().InMicroseconds();

    state->gamepad->axes_length = 2;
    state->gamepad->axes[0] = x_normalized;
    state->gamepad->axes[1] =
        -y_normalized;  //  Gamepad's Y axis is actually
                        //  inverted (1.0 means "backward").

    state->gamepad->buttons_length = 3;  // 2 placeholders + the real one
    // Default-constructed buttons are already valid placeholders.
    state->gamepad->buttons[2].touched = true;
    state->gamepad->buttons[2].value = 1.0;
    state->gamepad->mapping = device::GamepadMapping::kNone;
    state->gamepad->hand = device::GamepadHand::kNone;

    result.push_back(std::move(state));
  }

  // All the input source IDs that are no longer touching need to remain unused
  // for at least one frame. For now, we always assign new ID for input source
  // so there's no need to remember the IDs that have to be put on hold. Just
  // clean up all the no longer touching pointers:
  std::unordered_map<uint32_t, ScreenTouchEvent> still_touching_events;
  for (const auto& screen_touch_event : screen_touch_events_) {
    if (!screen_touch_event.second.screen_touch_active) {
      // This pointer is no longer touching - remove it from the mapping, do not
      // consider it as still touching:
      pointer_id_to_input_source_id_.erase(
          screen_touch_event.second.pointer_id);
    } else {
      still_touching_events.insert(screen_touch_event);
    }
  }

  screen_touch_events_.swap(still_touching_events);

  return result;
}

bool ArCoreGl::IsFeatureEnabled(mojom::XRSessionFeature feature) {
  return base::Contains(enabled_features_, feature);
}

void ArCoreGl::Pause() {
  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);
  DVLOG(1) << __func__;

  arcore_->Pause();
  is_paused_ = true;
}

void ArCoreGl::Resume() {
  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);
  DVLOG(1) << __func__;

  arcore_->Resume();
  is_paused_ = false;
}

void ArCoreGl::OnBindingDisconnect() {
  DVLOG(3) << __func__;

  CloseBindingsIfOpen();

  std::move(session_shutdown_callback_).Run();
}

void ArCoreGl::CloseBindingsIfOpen() {
  DVLOG(3) << __func__;

  environment_receiver_.reset();
  frame_data_receiver_.reset();
  session_controller_receiver_.reset();
  presentation_receiver_.reset();
}

bool ArCoreGl::IsOnGlThread() const {
  return gl_thread_task_runner_->BelongsToCurrentThread();
}

base::WeakPtr<ArCoreGl> ArCoreGl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace device
