// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/vr/android/arcore/arcore_gl.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <utility>

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/jni_android.h"
#include "base/barrier_callback.h"
#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/angle_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "device/vr/android/arcore/ar_image_transport.h"
#include "device/vr/android/arcore/arcore.h"
#include "device/vr/android/arcore/arcore_math_utils.h"
#include "device/vr/android/arcore/vr_service_type_converters.h"
#include "device/vr/android/web_xr_presentation_state.h"
#include "device/vr/android/xr_java_coordinator.h"
#include "device/vr/public/cpp/xr_frame_sink_client.h"
#include "device/vr/public/mojom/pose.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "device/vr/util/transform_utils.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_android_native_fence_sync.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace {
// TODO(crbug.com/40757470): Some pages can hang if we try to wait for
// the compositor to acknowledge receipt of a frame before moving it to the
// "rendering" state of the state machine. However, not doing so could increase
// the latency of frames under heavy load as we aren't listening to back
// pressure from the compositor. Ideally, this value would be set to false and
// then removed. Investigate why it needs to be true.
const bool kTransitionToRenderingImmediately = true;

const char kInputSourceProfileName[] = "generic-touchscreen";

const gfx::Size kDefaultFrameSize = {1, 1};
const display::Display::Rotation kDefaultRotation = display::Display::ROTATE_0;

// When scheduling calls to GetFrameData, leave some safety margin to run a bit
// early. For the ARCore Update calculation, need to leave time for the update
// itself to finish, not counting time blocked waiting for a frame to be
// available. For the render completion calculation, the margin compensates for
// variations in rendering time. Express this as a fraction of the frame time,
// on the theory that devices capable of 60fps would tend to be faster than
// ones running at 30fps.
const float kScheduleFrametimeMarginForUpdate = 0.2f;
const float kScheduleFrametimeMarginForRender = 0.2f;

const int kSampleWindowSize = 3;

gfx::Transform GetContentTransform(const gfx::RectF& bounds) {
  // Calculate the transform matrix from quad coordinates (range 0..1 with
  // origin at bottom left of the quad) to texture lookup UV coordinates (also
  // range 0..1 with origin at bottom left), where the active viewport uses a
  // subset of the texture range that needs to be magnified to fill the quad.
  // The bounds as used by the UpdateLayerBounds mojo messages appear to use an
  // old WebVR convention with origin at top left, so the Y range needs to be
  // mirrored.
  gfx::Transform transform;
  transform.set_rc(0, 0, bounds.width());
  transform.set_rc(1, 1, bounds.height());
  transform.set_rc(0, 3, bounds.x());
  transform.set_rc(1, 3, 1.f - bounds.y() - bounds.height());
  return transform;
}

gfx::Size GetCameraImageSize(const gfx::Size& in, const gfx::Transform& xform) {
  // The UV transform matrix handles rotation and cropping. Get the
  // post-transform width and height from the transformed rectangle
  // diagonal T*[1, 1] - T*[0, 0]. The offsets in column 3 cancel out,
  // leaving just the scaling factors.
  double x = in.width();
  double y = in.height();
  int width = std::round(std::abs(x * xform.rc(0, 0) + y * xform.rc(1, 0)));
  int height = std::round(std::abs(x * xform.rc(0, 1) + y * xform.rc(1, 1)));

  DVLOG(3) << __func__ << ": uncropped size=" << in.ToString()
           << " cropped/rotated size=" << gfx::Size(width, height).ToString()
           << " uv_transform_=" << xform.ToString();
  return gfx::Size(width, height);
}

}  // namespace

namespace device {

ArCoreGlCreateSessionResult::ArCoreGlCreateSessionResult(
    mojo::PendingRemote<mojom::XRFrameDataProvider> frame_data_provider,
    mojom::XRViewPtr view,
    mojo::PendingRemote<mojom::XRSessionController> session_controller,
    mojom::XRPresentationConnectionPtr presentation_connection)
    : frame_data_provider(std::move(frame_data_provider)),
      view(std::move(view)),
      session_controller(std::move(session_controller)),
      presentation_connection(std::move(presentation_connection)) {}
ArCoreGlCreateSessionResult::~ArCoreGlCreateSessionResult() = default;
ArCoreGlCreateSessionResult::ArCoreGlCreateSessionResult(
    ArCoreGlCreateSessionResult&& other) = default;

ArCoreGlInitializeResult::ArCoreGlInitializeResult(
    std::unordered_set<device::mojom::XRSessionFeature> enabled_features,
    std::optional<device::mojom::XRDepthConfig> depth_configuration,
    viz::FrameSinkId frame_sink_id)
    : enabled_features(enabled_features),
      depth_configuration(depth_configuration),
      frame_sink_id(frame_sink_id) {}
ArCoreGlInitializeResult::ArCoreGlInitializeResult(
    ArCoreGlInitializeResult&& other) = default;
ArCoreGlInitializeResult::~ArCoreGlInitializeResult() = default;

// The ArCompositor is currently only supported if we're using shared buffers.
ArCoreGl::ArCoreGl(std::unique_ptr<ArImageTransport> ar_image_transport)
    : gl_thread_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      ar_image_transport_(std::move(ar_image_transport)),
      use_ar_compositor_(ArImageTransport::UseSharedBuffer()),
      webxr_(std::make_unique<WebXrPresentationState>()),
      average_camera_frametime_(kSampleWindowSize),
      average_animate_time_(kSampleWindowSize),
      average_process_time_(kSampleWindowSize),
      average_render_time_(kSampleWindowSize) {
  DVLOG(1) << __func__;
}

ArCoreGl::~ArCoreGl() {
  DVLOG(1) << __func__;
  DCHECK(IsOnGlThread());
  ar_image_transport_->DestroySharedBuffers(webxr_.get());
  ar_image_transport_.reset();

  // If anyone is still waiting for our initialization to finish, let them know
  // that it failed.
  if (initialized_callback_)
    std::move(initialized_callback_)
        .Run(base::unexpected(ArCoreGlInitializeError::kFailure));

  // Make sure mojo bindings are closed before proceeding with member
  // destruction. Specifically, destroying pending_getframedata_
  // must happen after closing bindings, see pending_getframedata_
  // comments in the header file.
  CloseBindingsIfOpen();
}

bool ArCoreGl::CanRenderDOMContent() {
  return use_ar_compositor_;
}

void ArCoreGl::Initialize(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread_task_runner,
    XrJavaCoordinator* session_utils,
    ArCoreFactory* arcore_factory,
    XrFrameSinkClient* xr_frame_sink_client,
    gfx::AcceleratedWidget drawing_widget,
    gpu::SurfaceHandle surface_handle,
    ui::WindowAndroid* root_window,
    const gfx::Size& frame_size,
    display::Display::Rotation display_rotation,
    const std::unordered_set<device::mojom::XRSessionFeature>&
        required_features,
    const std::unordered_set<device::mojom::XRSessionFeature>&
        optional_features,
    const std::vector<device::mojom::XRTrackedImagePtr>& tracked_images,
    device::mojom::XRDepthOptionsPtr depth_options,
    ArCoreGlInitializeCallback callback) {
  DVLOG(3) << __func__;

  DCHECK(IsOnGlThread());
  DCHECK(!is_initialized_);

  DCHECK(!frame_size.IsEmpty());

  transfer_size_ = frame_size;
  screen_size_ = frame_size;
  // The camera image size will be updated once we have a valid camera frame
  // from ARCore.
  camera_image_size_ = {0, 0};
  display_rotation_ = display_rotation;
  recalculate_uvs_and_projection_ = true;

  // ARCore is monoscopic and does not have an associated eye.
  view_.eye = mojom::XREye::kNone;
  view_.viewport = gfx::Rect(0, 0, frame_size.width(), frame_size.height());
  // We don't have the transform or field of view until the first frame, so set
  // the transform to identity and field of view to zero. The XR Session
  // creation callback only needs the viewport size for the framebuffer.
  // mojo_from_view is set on every frame in ArCoreGl::GetFrameData and
  // field_of_view is set in ArCoreGl::RecalculateUvsAndProjection, which is
  // called one time on the first frame.
  view_.mojo_from_view = gfx::Transform();
  view_.field_of_view = mojom::VRFieldOfView::New(0.0f, 0.0f, 0.0f, 0.0f);

  // If we're using the ArCompositor, we need to initialize GL without the
  // drawing_widget. (Since the ArCompositor accesses the surface through a
  // different mechanism than the drawing_widget it's okay to set it null here).
  if (use_ar_compositor_) {
    drawing_widget = gfx::kNullAcceleratedWidget;
  }
  if (!InitializeGl(drawing_widget)) {
    std::move(callback).Run(
        base::unexpected(ArCoreGlInitializeError::kFailure));
    return;
  }

  // Get the activity context.
  base::android::ScopedJavaLocalRef<jobject> application_context =
      session_utils->GetCurrentActivityContext();
  if (!application_context.obj()) {
    DLOG(ERROR) << "Unable to retrieve the Java context/activity!";
    std::move(callback).Run(
        base::unexpected(ArCoreGlInitializeError::kFailure));
    return;
  }

  std::optional<ArCore::DepthSensingConfiguration> depth_sensing_config;
  if (depth_options) {
    depth_sensing_config = ArCore::DepthSensingConfiguration(
        depth_options->usage_preferences,
        depth_options->data_format_preferences);
  }

  device::DomOverlaySetup dom_setup = device::DomOverlaySetup::kNone;
  if (CanRenderDOMContent()) {
    if (base::Contains(required_features,
                       device::mojom::XRSessionFeature::DOM_OVERLAY)) {
      dom_setup = device::DomOverlaySetup::kRequired;
    } else if (base::Contains(optional_features,
                              device::mojom::XRSessionFeature::DOM_OVERLAY)) {
      dom_setup = device::DomOverlaySetup::kOptional;
    }
  }

  arcore_ = arcore_factory->Create();
  std::optional<ArCore::InitializeResult> maybe_initialize_result =
      arcore_->Initialize(application_context, required_features,
                          optional_features, tracked_images,
                          std::move(depth_sensing_config));
  if (!maybe_initialize_result) {
    DLOG(ERROR) << "ARCore failed to initialize";
    std::move(callback).Run(
        base::unexpected(ArCoreGlInitializeError::kFailure));
    return;
  }

  initialized_callback_ = std::move(callback);

  // TODO(crbug.com/41453315): start using the list to control the
  // behavior of local and unbounded spaces & send appropriate data back in
  // GetFrameData().
  enabled_features_ = maybe_initialize_result->enabled_features;
  depth_configuration_ = maybe_initialize_result->depth_configuration;

  DVLOG(3) << "ar_image_transport_->Initialize()...";
  ar_image_transport_->Initialize(
      webxr_.get(), base::BindOnce(&ArCoreGl::OnArImageTransportReady,
                                   weak_ptr_factory_.GetWeakPtr()));

  if (use_ar_compositor_) {
    InitializeArCompositor(main_thread_task_runner, surface_handle, root_window,
                           xr_frame_sink_client, dom_setup);
    webxr_->SetStateMachineType(
        WebXrPresentationState::StateMachineType::kVizComposited);
  } else {
    webxr_->SetStateMachineType(
        WebXrPresentationState::StateMachineType::kBrowserComposited);
  }

  // Set the texture on ArCore to render the camera. Must be after
  // ar_image_transport_->Initialize().
  arcore_->SetCameraTexture(ar_image_transport_->GetCameraTextureId());
  // Set the Geometry to ensure consistent behaviour.
  arcore_->SetDisplayGeometry(kDefaultFrameSize, kDefaultRotation);
}

void ArCoreGl::InitializeArCompositor(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread_task_runner,
    gpu::SurfaceHandle surface_handle,
    ui::WindowAndroid* root_window,
    XrFrameSinkClient* xr_frame_sink_client,
    device::DomOverlaySetup dom_setup) {
  ArCompositorFrameSink::BeginFrameCallback begin_frame_callback =
      base::BindRepeating(&ArCoreGl::OnBeginFrame,
                          weak_ptr_factory_.GetWeakPtr());

  // This callback is called to acknowledge receipt of a submitted frame. Since
  // we will only submit the frame if it's processing, and there's no other
  // work to do, we can just directly call to transition it.
  ArCompositorFrameSink::CompositorReceivedFrameCallback
      compositor_received_frame_callback = base::DoNothing();

  if (!kTransitionToRenderingImmediately) {
    compositor_received_frame_callback =
        base::BindRepeating(&ArCoreGl::TransitionProcessingFrameToRendering,
                            weak_ptr_factory_.GetWeakPtr());
  }

  ArCompositorFrameSink::RenderingFinishedCallback rendering_finished_callback =
      base::BindRepeating(&ArCoreGl::FinishRenderingFrame,
                          weak_ptr_factory_.GetWeakPtr());

  // Once we can issue a new frame, there's no other checks we need to do
  // except to see if the pipeline has been unblocked, so let that callback
  // just directly try to unblock the pipeline.
  ArCompositorFrameSink::CanIssueNewFrameCallback can_issue_new_frame_callback =
      base::BindRepeating(&ArCoreGl::TryRunPendingGetFrameData,
                          weak_ptr_factory_.GetWeakPtr());

  ar_compositor_ = std::make_unique<ArCompositorFrameSink>(
      gl_thread_task_runner_, begin_frame_callback,
      compositor_received_frame_callback, rendering_finished_callback,
      can_issue_new_frame_callback);

  ar_compositor_->Initialize(
      main_thread_task_runner, surface_handle, root_window, screen_size_,
      xr_frame_sink_client, dom_setup,
      base::BindOnce(&ArCoreGl::OnArCompositorInitialized,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ArCoreGl::OnBindingDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArCoreGl::OnArImageTransportReady(bool success) {
  DVLOG(1) << __func__ << ": success=" << success;
  if (!success) {
    std::move(initialized_callback_)
        .Run(base::unexpected(ArCoreGlInitializeError::kRetryableFailure));
    return;
  }
  is_image_transport_ready_ = true;
  OnInitialized();
}

void ArCoreGl::OnArCompositorInitialized(bool initialized) {
  DVLOG(1) << __func__ << " intialized=" << initialized;
  if (!initialized) {
    std::move(initialized_callback_)
        .Run(base::unexpected(ArCoreGlInitializeError::kFailure));
    return;
  }

  // Note that this erasing is acceptable, as the ArCompositor would not return
  // that it was initialized successfully if it was told that initializing the
  // DOM was required.
  if (CanRenderDOMContent() &&
      IsFeatureEnabled(device::mojom::XRSessionFeature::DOM_OVERLAY) &&
      !ar_compositor_->CanCompositeDomContent()) {
    enabled_features_.erase(device::mojom::XRSessionFeature::DOM_OVERLAY);
  }

  OnInitialized();
}

void ArCoreGl::OnInitialized() {
  DVLOG(1) << __func__;
  if (!is_image_transport_ready_ ||
      (use_ar_compositor_ &&
       !(ar_compositor_ && ar_compositor_->IsInitialized())))
    return;

  // Assert that if we're using SharedBuffer transport, we've got an
  // ArCompositor, and that we don't have it if we aren't using SharedBuffers.
  DCHECK_EQ(!!ar_compositor_, ArImageTransport::UseSharedBuffer());

  is_initialized_ = true;
  webxr_->NotifyMailboxBridgeReady();
  viz::FrameSinkId frame_sink_id =
      ar_compositor_ ? ar_compositor_->FrameSinkId() : viz::FrameSinkId();

  std::move(initialized_callback_)
      .Run(ArCoreGlInitializeResult(enabled_features_, depth_configuration_,
                                    frame_sink_id));
}

void ArCoreGl::CreateSession(ArCoreGlCreateSessionCallback create_callback,
                             base::OnceClosure shutdown_callback) {
  DVLOG(3) << __func__;

  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);

  session_shutdown_callback_ = std::move(shutdown_callback);

  CloseBindingsIfOpen();

  device::mojom::XRPresentationTransportOptionsPtr transport_options =
      device::mojom::XRPresentationTransportOptions::New();
  transport_options->wait_for_gpu_fence = true;

  if (ArImageTransport::UseSharedBuffer()) {
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

  ArCoreGlCreateSessionResult result(
      frame_data_receiver_.BindNewPipeAndPassRemote(), view_.Clone(),
      session_controller_receiver_.BindNewPipeAndPassRemote(),
      std::move(submit_frame_sink));

  std::move(create_callback).Run(std::move(result));

  frame_data_receiver_.set_disconnect_handler(base::BindOnce(
      &ArCoreGl::OnBindingDisconnect, weak_ptr_factory_.GetWeakPtr()));
  session_controller_receiver_.set_disconnect_handler(base::BindOnce(
      &ArCoreGl::OnBindingDisconnect, weak_ptr_factory_.GetWeakPtr()));
}

bool ArCoreGl::InitializeGl(gfx::AcceleratedWidget drawing_widget) {
  DVLOG(3) << __func__;

  DCHECK(IsOnGlThread());
  DCHECK(!is_initialized_);

  // ARCore provides the camera image as a native GL texture and doesn't support
  // ANGLE, so disable it.
  // TODO(crbug.com/40744597): support ANGLE with cardboard?
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

  DCHECK(gl::GetGLImplementation() != gl::kGLImplementationEGLANGLE);

  // If we weren't provided with a drawing_widget, then we need to set up the
  // surface for Offscreen usage.
  scoped_refptr<gl::GLSurface> surface;
  if (drawing_widget != gfx::kNullAcceleratedWidget) {
    surface = gl::init::CreateViewGLSurface(display, drawing_widget);
  } else {
    surface = gl::init::CreateOffscreenGLSurface(display, {0, 0});
  }
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

void ArCoreGl::TryRunPendingGetFrameData() {
  // ArCompositor doesn't wait on a fence, and so accurately reporting it's
  // times relies heavily on scheduling as soon as we can; if we're not using
  // the ArCompositor, we should call SchedulePendingGetFrameData isntead.
  DCHECK(ar_compositor_);

  if (CanStartNewAnimatingFrame() && pending_getframedata_) {
    // We'll post a task to run the pending getframedata so that whoever called
    // us can finish any processing they need to do before we *actually* start
    // running the new GetFrameData.
    gl_thread_task_runner_->PostTask(FROM_HERE,
                                     std::move(pending_getframedata_));
  }
}

bool ArCoreGl::CanStartNewAnimatingFrame() {
  if (pending_shutdown_)
    return false;

  if (webxr_->HaveAnimatingFrame()) {
    DVLOG(3) << __func__ << ": deferring, HaveAnimatingFrame";
    return false;
  }

  if (!webxr_->CanStartFrameAnimating()) {
    DVLOG(3) << __func__ << ": deferring, no available frames in swapchain";
    return false;
  }

  // If we're using the ArCompositor, we need to ask it if we can start a new
  // frame. If we aren't, then we may need to wait for the previous frames to
  // finish.
  if (ar_compositor_) {
    return ar_compositor_->CanIssueBeginFrame();
  } else {
    if (webxr_->HaveProcessingFrame() && webxr_->HaveRenderingFrame()) {
      // If there are already two frames in flight, ensure that the rendering
      // frame completes first before starting a new animating frame. It may be
      // complete already, in that case just collect its statistics. (Don't wait
      // if there's a rendering frame but no processing frame.)
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
  }

  return true;
}

void ArCoreGl::RecalculateUvsAndProjection() {
  // Get the UV transform matrix from ArCore's UV transform.
  uv_transform_ = arcore_->GetCameraUvFromScreenUvTransform();
  DVLOG(3) << __func__ << ": uv_transform_=" << uv_transform_.ToString();

  // We need near/far distances to make a projection matrix. The actual
  // values don't matter, the Renderer will recalculate dependent values
  // based on the application's near/far settngs.
  constexpr float depth_near = 0.1f;
  constexpr float depth_far = 1000.f;
  projection_ = arcore_->GetProjectionMatrix(depth_near, depth_far);
  float left = depth_near * (projection_.rc(2, 0) - 1.f) / projection_.rc(0, 0);
  float right =
      depth_near * (projection_.rc(2, 0) + 1.f) / projection_.rc(0, 0);
  float bottom =
      depth_near * (projection_.rc(2, 1) - 1.f) / projection_.rc(1, 1);
  float top = depth_near * (projection_.rc(2, 1) + 1.f) / projection_.rc(1, 1);
  DVLOG(3) << __func__ << ": projection_=" << projection_.ToString();

  // Also calculate the inverse projection which is needed for converting
  // screen touches to world rays.
  inverse_projection_ = projection_.GetCheckedInverse();

  // VRFieldOfView wants positive angles.
  mojom::VRFieldOfViewPtr field_of_view = mojom::VRFieldOfView::New();
  field_of_view->left_degrees = base::RadToDeg(atanf(-left / depth_near));
  field_of_view->right_degrees = base::RadToDeg(atanf(right / depth_near));
  field_of_view->down_degrees = base::RadToDeg(atanf(-bottom / depth_near));
  field_of_view->up_degrees = base::RadToDeg(atanf(top / depth_near));
  DVLOG(3) << " fov degrees up=" << field_of_view->up_degrees
           << " down=" << field_of_view->down_degrees
           << " left=" << field_of_view->left_degrees
           << " right=" << field_of_view->right_degrees;

  view_.field_of_view = std::move(field_of_view);
}

void ArCoreGl::GetFrameData(
    mojom::XRFrameDataRequestOptionsPtr options,
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  TRACE_EVENT1("gpu", __func__, "frame", webxr_->PeekNextFrameIndex());

  if (!CanStartNewAnimatingFrame()) {
    pending_getframedata_ =
        base::BindOnce(&ArCoreGl::GetFrameData, GetWeakPtr(),
                       std::move(options), std::move(callback));
    return;
  }

  DVLOG(3) << __func__ << ": recalculate_uvs_and_projection_="
           << recalculate_uvs_and_projection_
           << ", transfer_size_=" << transfer_size_.ToString()
           << ", display_rotation_=" << display_rotation_;

  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);
  DCHECK(!transfer_size_.IsEmpty());

  if (restrict_frame_data_) {
    DVLOG(2) << __func__ << ": frame data restricted, returning nullptr.";
    std::move(callback).Run(nullptr);
    return;
  }

  if (is_paused_) {
    DVLOG(2) << __func__ << ": paused but frame data not restricted. Resuming.";
    Resume();
  }

  // If this is the first frame, update the display geometry. This is applied
  // to the next Update call which happens below in this method.
  if (recalculate_uvs_and_projection_) {
    // Set display geometry before calling Update. It's a pending request that
    // applies to the next frame.
    arcore_->SetDisplayGeometry(screen_size_, display_rotation_);
  }

  bool camera_updated = false;
  base::TimeTicks arcore_update_started = base::TimeTicks::Now();
  mojom::VRPosePtr pose = arcore_->Update(&camera_updated);
  base::TimeTicks now = base::TimeTicks::Now();
  last_arcore_update_time_ = now;

  // Track the camera frame timestamp interval in preparation for handling with
  // frame rate variations within ARCore's configured frame rate range. Not yet
  // used for timing purposes since we currently only request 30fps. (Note that
  // the frame timestamp has an unspecified and camera-specific time base.)
  base::TimeDelta frame_timestamp = arcore_->GetFrameTimestamp();
  if (!last_arcore_frame_timestamp_.is_zero()) {
    base::TimeDelta delta = frame_timestamp - last_arcore_frame_timestamp_;
    average_camera_frametime_.AddSample(delta);
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("xr.debug"),
                   "ARCore camera frame interval (ms)", delta.InMilliseconds());
  }
  last_arcore_frame_timestamp_ = frame_timestamp;

  base::TimeDelta arcore_update_elapsed = now - arcore_update_started;
  TRACE_COUNTER1("gpu", "ARCore update elapsed (ms)",
                 arcore_update_elapsed.InMilliseconds());

  if (!camera_updated) {
    DVLOG(1) << "arcore_->Update() failed";
    std::move(callback).Run(nullptr);
    have_camera_image_ = false;
    return;
  }

  have_camera_image_ = true;
  mojom::XRFrameDataPtr frame_data = mojom::XRFrameData::New();

  if (recalculate_uvs_and_projection_) {
    // Now that ARCore's Update() is complete, we can get the UV transform
    // and projection matrix. This is only needed once since the screen
    // geometry won't change for the duration of the session. (Changes
    // to the transfer size are handled separately, cf. UpdateLayerBounds())
    RecalculateUvsAndProjection();
    recalculate_uvs_and_projection_ = false;
  }

  camera_image_size_ =
      GetCameraImageSize(arcore_->GetUncroppedCameraImageSize(), uv_transform_);
  DCHECK(!camera_image_size_.IsEmpty());

  frame_data->frame_id = webxr_->StartFrameAnimating();
  DVLOG(3) << __func__ << " frame=" << frame_data->frame_id;
  TRACE_EVENT1("gpu", __func__, "frame", frame_data->frame_id);

  WebXrFrame* xrframe = webxr_->GetAnimatingFrame();

  // Now that we are guaranteed to actually have an animating frame,
  // Warn the compositor that we're expecting to have data to submit this frame.
  if (ar_compositor_) {
    base::TimeDelta frametime = EstimatedArCoreFrameTime();
    base::TimeDelta render_margin =
        kScheduleFrametimeMarginForRender * frametime;
    base::TimeTicks deadline =
        base::TimeTicks::Now() + (frametime - render_margin);
    ar_compositor_->RequestBeginFrame(frametime, deadline);
  }

  xrframe->time_pose = now;
  xrframe->bounds_left = viewport_bounds_;

  if (ArImageTransport::UseSharedBuffer()) {
    // Whether or not a handle to the shared buffer is passed to blink, the GPU
    // will need the camera, so always copy it over, and then decide if we are
    // also sending the camera frame to the renderer.
    // Note that even though the buffers are re-used this does not leak data
    // as the decision of whether or not the renderer gets camera frames is made
    // on a per-session and not a per-frame basis.
    WebXrSharedBuffer* shared_buffer =
        ar_image_transport_->TransferCameraImageFrame(
            webxr_.get(), camera_image_size_, uv_transform_);
    CHECK(shared_buffer);

    if (IsFeatureEnabled(device::mojom::XRSessionFeature::CAMERA_ACCESS)) {
      frame_data->camera_image_buffer_shared_image =
          shared_buffer->shared_image->Export();
      frame_data->camera_image_buffer_sync_token = shared_buffer->sync_token;
      frame_data->camera_image_size = camera_image_size_;
    }
  }

  // Check if floor height estimate has changed.
  float new_floor_height_estimate = arcore_->GetEstimatedFloorHeight();
  if (!floor_height_estimate_ ||
      *floor_height_estimate_ != new_floor_height_estimate) {
    floor_height_estimate_ = new_floor_height_estimate;

    if (!stage_parameters_) {
      stage_parameters_ = mojom::VRStageParameters::New();
    }
    stage_parameters_->mojo_from_floor = gfx::Transform();
    stage_parameters_->mojo_from_floor.Translate3d(
        0, (-1 * *floor_height_estimate_), 0);

    stage_parameters_id_++;
  }

  // Only send updates to the stage parameters if the session's stage parameters
  // id is different.
  frame_data->stage_parameters_id = stage_parameters_id_;
  if (!options || options->stage_parameters_id != stage_parameters_id_) {
    frame_data->stage_parameters = stage_parameters_.Clone();
  }

  if (ArImageTransport::UseSharedBuffer()) {
    // Set up a shared buffer for the renderer to draw into, it'll be sent
    // alongside the frame pose.
    WebXrSharedBuffer* shared_buffer = ar_image_transport_->TransferFrame(
        webxr_.get(), transfer_size_, uv_transform_);
    CHECK(shared_buffer);
    frame_data->buffer_shared_image = shared_buffer->shared_image->Export();
    frame_data->buffer_sync_token = shared_buffer->sync_token;
  }

  // Create the frame data to return to the renderer.
  if (!pose) {
    DVLOG(1) << __func__ << ": pose unavailable!";
  }

  if (pose) {
    // The pose returned by ArCoreImpl::Update populates both the orientation
    // and position if there is a pose.
    DCHECK(pose->orientation);
    DCHECK(pose->position);

    // The view properties besides the transform are calculated by
    // ArCoreGl::RecalculateUvsAndProjection() as needed. IF we don't have a
    // pose, the transform from the previous frame is used.
    view_.mojo_from_view = vr_utils::VrPoseToTransform(pose.get());
  }

  frame_data->views.push_back(view_.Clone());
  frame_data->mojo_from_viewer = std::move(pose);
  frame_data->time_delta = now - base::TimeTicks();
  if (rendering_time_ratio_ > 0) {
    frame_data->rendering_time_ratio = rendering_time_ratio_;
  }

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

  if (pending_shutdown_)
    return false;

  WebXrFrame* animating_frame = webxr_->GetAnimatingFrame();
  animating_frame->time_js_submit = base::TimeTicks::Now();
  average_animate_time_.AddSample(animating_frame->time_js_submit -
                                  animating_frame->time_pose);

  if (animating_frame->index != frame_index) {
    DVLOG(1) << __func__ << ": wrong frame index, got " << frame_index
             << ", expected " << animating_frame->index;
    presentation_receiver_.ReportBadMessage(
        "SubmitFrame called with wrong frame index");
    CloseBindingsIfOpen();
    pending_shutdown_ = true;
    return false;
  }

  // Frame looks valid.
  return true;
}

void ArCoreGl::CopyCameraImageToFramebuffer() {
  DVLOG(3) << __func__;
  DCHECK(!ArImageTransport::UseSharedBuffer());

  // Draw the current camera texture to the output default framebuffer now, if
  // available.
  if (have_camera_image_) {
    ar_image_transport_->CopyCameraImageToFramebuffer(
        /*framebuffer=*/0, screen_size_, uv_transform_);
    have_camera_image_ = false;
  }

  // We're done with the camera image for this frame, post a task to start the
  // next animating frame and its ARCore update if we had deferred it.
  if (pending_getframedata_ && !ar_compositor_) {
    ScheduleGetFrameData();
  }
}

base::TimeDelta ArCoreGl::EstimatedArCoreFrameTime() {
  // ARCore may be operating in a variable frame rate mode where it adjusts
  // the frame rate during the session, for example to increase exposure time
  // in low light conditions.
  //
  // We need an estimated frame time for scheduling purposes. We have an average
  // camera frame time delta from ARCore updates, but this may be a multiple of
  // the nominal frame time. For example, if ARCore has the camera configured to
  // use 60fps, but the application is only submitting frames at 30fps and only
  // using every second camera frame, we don't have a reliable way of detecting
  // that. However, this still seems OK as long as the frame rate is stable.
  ArCore::MinMaxRange range = arcore_->GetTargetFramerateRange();
  DCHECK_GT(range.min, 0.f);
  DCHECK_GT(range.max, 0.f);
  DCHECK_GE(range.max, range.min);

  // The min frame time corresponds to the max frame rate and vice versa.
  base::TimeDelta min_frametime = base::Seconds(1.0f / range.max);
  base::TimeDelta max_frametime = base::Seconds(1.0f / range.min);

  base::TimeDelta frametime =
      average_camera_frametime_.GetAverageOrDefault(min_frametime);

  // Ensure that the returned value is within ARCore's nominal frame time range.
  // This helps avoid underestimating the frame rate if the app is too slow
  // to reach the minimum target FPS value.
  return std::clamp(frametime, min_frametime, max_frametime);
}

base::TimeDelta ArCoreGl::WaitTimeForArCoreUpdate() {
  // ARCore update will block if called before a new camera frame is available.
  // Estimate when this is.
  base::TimeDelta frametime = EstimatedArCoreFrameTime();
  base::TimeTicks next_update = last_arcore_update_time_ + frametime;
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta wait =
      next_update - now - kScheduleFrametimeMarginForUpdate * frametime;
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("xr.debug"),
                 "GetFrameData update wait (ms)", wait.InMilliseconds());
  return wait;
}

base::TimeDelta ArCoreGl::WaitTimeForRenderCompletion() {
  DCHECK(webxr_->HaveRenderingFrame());

  // If there's a current rendering frame, estimate when it's going to finish
  // rendering, then try to schedule the next update to match.
  auto* rendering_frame = webxr_->GetRenderingFrame();
  base::TimeDelta avg_animate = average_animate_time_.GetAverage();
  base::TimeDelta avg_render = average_render_time_.GetAverage();

  // The time averages may not have any samples yet, in that case they return
  // zero. That's OK, it just means we expect them to finish immediately and
  // don't delay.
  base::TimeTicks expected_render_complete =
      rendering_frame->time_copied + avg_render;
  base::TimeDelta render_margin =
      kScheduleFrametimeMarginForRender * EstimatedArCoreFrameTime();
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta render_wait = expected_render_complete - now - render_margin;

  DVLOG(3) << __func__
           << " expected_render_complete=" << expected_render_complete
           << " render_margin=" << render_margin << " now=" << now
           << " render_wait" << render_wait
           << " time_copied=" << rendering_frame->time_copied
           << " avg_render=" << avg_render << " avg_animate=" << avg_animate;
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("xr.debug"),
                 "GetFrameData render wait (ms)", render_wait.InMilliseconds());
  // Once the next frame starts animating, we won't be able to finish processing
  // it until the current frame finishes rendering. If rendering is slower than
  // the animating (JS processing) time, increase the delay to compensate.
  if (avg_animate < avg_render) {
    render_wait += avg_render - avg_animate;
  }
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("xr.debug"),
                 "GetFrameData adjusted render wait (ms)",
                 render_wait.InMilliseconds());
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("xr.debug"),
                 "Avg animating time (ms)", avg_animate.InMilliseconds());
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("xr.debug"),
                 "Avg processing time (ms)",
                 average_process_time_.GetAverage().InMilliseconds());
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("xr.debug"),
                 "Avg rendering time (ms)", avg_render.InMilliseconds());
  return render_wait;
}

void ArCoreGl::ScheduleGetFrameData() {
  DVLOG(3) << __func__;
  DCHECK(!ar_compositor_);
  DCHECK(pending_getframedata_);

  base::TimeDelta delay = base::TimeDelta();
  if (!last_arcore_update_time_.is_null()) {
    base::TimeDelta update_wait = WaitTimeForArCoreUpdate();
    if (update_wait > delay) {
      delay = update_wait;
    }

    if (webxr_->HaveRenderingFrame()) {
      base::TimeDelta render_wait = WaitTimeForRenderCompletion();
      if (render_wait > delay) {
        delay = render_wait;
      }
    }
  }

  TRACE_COUNTER1("xr", "ARCore schedule delay (ms)", delay.InMilliseconds());
  if (delay.is_zero()) {
    // If there's no wait time, run immediately, not as posted task, to minimize
    // delay. There shouldn't be any pending work we'd need to yield for.
    RunPendingGetFrameData();
  } else {
    DVLOG(3) << __func__ << " delay=" << delay;
    // Run the next frame update as a posted task. This uses a helper method
    // since it's not safe to have a closure owned by the task runner, see
    // comments on pending_getframedata_ in the header file.
    gl_thread_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ArCoreGl::RunPendingGetFrameData,
                       weak_ptr_factory_.GetWeakPtr()),
        delay);
  }
}

void ArCoreGl::RunPendingGetFrameData() {
  DVLOG(3) << __func__;
  std::move(pending_getframedata_).Run();
}

void ArCoreGl::FinishRenderingFrame(WebXrFrame* frame) {
  DCHECK(frame || webxr_->HaveRenderingFrame());
  if (!frame) {
    frame = webxr_->GetRenderingFrame();
  }
  DVLOG(3) << __func__ << " frame=" << frame->index;

  TRACE_EVENT1("gpu", __func__, "frame", frame->index);

  // Even though we may be told that the frame is done, it may still actually
  // be in use. In this case, we'll have received sync tokens to wait on until
  // the GPU is *actually* done with the resources associated with the frame.
  if (!frame->reclaimed_sync_tokens.empty()) {
    auto barrier_callback =
        base::BarrierCallback<std::unique_ptr<gfx::GpuFence>>(
            frame->reclaimed_sync_tokens.size(),
            base::BindOnce(&ArCoreGl::OnReclaimedGpuFenceAvailable,
                           GetWeakPtr(), frame));
    // We'll have to wait until the latest fence resolves and any earlier waits
    // will simply become no-ops if they are waited on after that fence, so we
    // don't need to try to do anything fancy with regards to the ordering of
    // the tokens.
    for (const auto& reclaimed_sync_token : frame->reclaimed_sync_tokens) {
      ar_image_transport_->WaitSyncToken(reclaimed_sync_token);
      ar_image_transport_->CreateGpuFenceForSyncToken(reclaimed_sync_token,
                                                      barrier_callback);
    }
    frame->reclaimed_sync_tokens.clear();
  } else {
    // We didn't have any frame tokens, so just finish up this frame now.
    if (!frame->render_completion_fence) {
      frame->render_completion_fence = gl::GLFence::CreateForGpuFence();
    }
    ClearRenderingFrame(frame);
  }
}

void ArCoreGl::OnReclaimedGpuFenceAvailable(
    WebXrFrame* frame,
    std::vector<std::unique_ptr<gfx::GpuFence>> gpu_fences) {
  TRACE_EVENT1("gpu", __func__, "frame", frame->index);
  DVLOG(3) << __func__ << ": frame=" << frame->index;

  for (auto& gpu_fence : gpu_fences) {
    ar_image_transport_->ServerWaitForGpuFence(std::move(gpu_fence));
  }

  // The ServerWait above is enough that we could re-use this frame now, since
  // its usage is now appropriately synchronized; however, we have no way of
  // getting the time that the gpu fence triggered, which we need for the
  // rendered frame stats that drive dynamic viewport scaling.
  // TODO(crbug.com/40754792): It appears as though we are actually
  // placing/waiting on this fence after the frame *after* this current frame.
  frame->render_completion_fence = gl::GLFence::CreateForGpuFence();

  ClearRenderingFrame(frame);
}

void ArCoreGl::ClearRenderingFrame(WebXrFrame* frame) {
  TRACE_EVENT1("gpu", __func__, "frame", frame->index);
  DVLOG(3) << __func__ << ": frame=" << frame->index;

  // Ensure that we're totally finished with the rendering frame, then collect
  // stats and move the frame out of the rendering path.
  DVLOG(3) << __func__ << ": client wait start";
  frame->render_completion_fence->ClientWait();
  DVLOG(3) << __func__ << ": client wait done";

  GetRenderedFrameStats(frame);
  webxr_->EndFrameRendering(frame);

  if (ar_compositor_) {
    // Now that we have finished rendering, it's possible our pipeline has been
    // unblocked.
    TryRunPendingGetFrameData();
  }
}

void ArCoreGl::FinishFrame(int16_t frame_index) {
  // SharedBuffer mode handles it's transitions/rendering separately from this.
  DCHECK(!ar_compositor_);

  TRACE_EVENT1("gpu", __func__, "frame", frame_index);
  DVLOG(3) << __func__;
  surface_->SwapBuffers(base::DoNothing(), gfx::FrameData());

  // If we have a rendering frame (we don't if the app didn't submit one),
  // update statistics.
  if (!webxr_->HaveRenderingFrame())
    return;
  WebXrFrame* frame = webxr_->GetRenderingFrame();

  frame->render_completion_fence = gl::GLFence::CreateForGpuFence();
}

void ArCoreGl::GetRenderedFrameStats(WebXrFrame* frame) {
  DVLOG(2) << __func__;
  DCHECK(frame || webxr_->HaveRenderingFrame());
  if (!frame) {
    frame = webxr_->GetRenderingFrame();
  }
  base::TimeTicks now = base::TimeTicks::Now();

  // Get the time when rendering completed from the render completion fence.
  //
  // This is an overestimate in AR compositor mode because the fence
  // completes one frame late. The average_render_time_ calculation should use
  // the WritesDone time reported via OnBeginFrame's timing_data instead, but
  // those aren't guaranteed to be available. See also the GPU load
  // estimate in rendering_time_ratio_ which uses a different calculation.
  // TODO(crbug.com/40877379): revisit this calculation?
  base::TimeTicks completion_time = now;
  DCHECK(frame->render_completion_fence);
  completion_time = static_cast<gl::GLFenceAndroidNativeFenceSync*>(
                        frame->render_completion_fence.get())
                        ->GetStatusChangeTime();

  if (completion_time.is_null()) {
    // The fence status change time is best effort and may be unavailable.
    // In that case, use wallclock time.
    DVLOG(3) << __func__ << ": got null completion time, using wallclock";
    completion_time = now;
  }

  base::TimeDelta pose_to_submit = frame->time_js_submit - frame->time_pose;
  base::TimeDelta submit_to_swap = completion_time - frame->time_js_submit;
  TRACE_COUNTER2("gpu", "WebXR frame time (ms)", "javascript",
                 pose_to_submit.InMilliseconds(), "post-submit",
                 submit_to_swap.InMilliseconds());

  average_render_time_.AddSample(completion_time - frame->time_copied);

  base::TimeDelta copied_to_completion = completion_time - frame->time_copied;
  if (ar_compositor_) {
    // AR compositor mode uses timing data received in OnBeginFrame() for the
    // GPU load estimate.
    DVLOG(3) << __func__ << " time_js_submit=" << frame->time_js_submit
             << " time_pose=" << frame->time_pose
             << " time_copied=" << frame->time_copied
             << " copied_to_completion=" << copied_to_completion;
  } else {
    // Save a GPU load estimate for use in GetFrameData. This is somewhat
    // arbitrary, use the most recent rendering time divided by the nominal
    // frame time.
    base::TimeDelta arcore_frametime = EstimatedArCoreFrameTime();
    DCHECK(!arcore_frametime.is_zero());

    rendering_time_ratio_ = copied_to_completion / arcore_frametime;
    TRACE_COUNTER1("xr", "WebXR rendering time ratio (%)",
                   rendering_time_ratio_ * 100);

    DVLOG(3) << __func__ << " time_js_submit=" << frame->time_js_submit
             << " time_pose=" << frame->time_pose
             << " time_copied=" << frame->time_copied
             << " copied_to_completion=" << copied_to_completion
             << " arcore_frametime=" << arcore_frametime
             << " rendering_time_ratio_=" << rendering_time_ratio_;
  }

  // Add Animating/Processing/Rendering async annotations to event traces.

  // Trace IDs need to be unique. Since frame->index is an 8-bit wrapping value,
  // use a separate arbitrary value. This is the only place we create this kind
  // of trace, so there's no need to keep the ID in sync with other code
  // locations. Use a static value so that IDs don't get reused across sessions.
  static uint32_t frame_id_for_tracing = 0;
  uint32_t trace_id = ++frame_id_for_tracing;

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "xr", "Animating", trace_id, frame->time_pose, "frame", frame->index);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP1("xr", "Animating", trace_id,
                                                 frame->time_js_submit, "frame",
                                                 frame->index);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1("xr", "Processing", trace_id,
                                                   frame->time_js_submit,
                                                   "frame", frame->index);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP1(
      "xr", "Processing", trace_id, frame->time_copied, "frame", frame->index);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "xr", "Rendering", trace_id, frame->time_copied, "frame", frame->index);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP1(
      "xr", "Rendering", trace_id, completion_time, "frame", frame->index);
}

void ArCoreGl::SubmitFrameMissing(int16_t frame_index,
                                  const gpu::SyncToken& sync_token) {
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);
  DVLOG(2) << __func__;

  if (!IsSubmitFrameExpected(frame_index))
    return;

  if (ar_compositor_) {
    ar_image_transport_->WaitSyncToken(sync_token);

    if (have_camera_image_) {
      webxr_->ProcessOrDefer(base::BindOnce(
          &ArCoreGl::SubmitVizFrame, weak_ptr_factory_.GetWeakPtr(),
          frame_index, ArCompositorFrameSink::FrameType::kMissingWebXrContent));
    } else {
      DVLOG(1) << __func__ << ": frame=" << frame_index
               << " Had No Camera Image";
      // Note that we can't just recycle here, as although we've told the
      // compositor that we're going to have a frame, we may not have gotten the
      // begin frame args back from it yet that we need to tell it we need to
      // recycle the frame. Note that we can't recycle our frame yet (and just
      // set a flag to indicate to OnBeginFrame to call DidNotProduceFrame), as
      // we need to keep the animating frame to prevent a new frame from being
      // queued up while we wait for the BeginFrameArgs.
      webxr_->ProcessOrDefer(base::BindOnce(&ArCoreGl::DidNotProduceVizFrame,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            frame_index));
    }
  } else {
    webxr_->RecycleUnusedAnimatingFrame();
    ar_image_transport_->WaitSyncToken(sync_token);
    CopyCameraImageToFramebuffer();
    FinishFrame(frame_index);
    DVLOG(3) << __func__ << ": frame=" << frame_index << " SwapBuffers";
  }
}

void ArCoreGl::DidNotProduceVizFrame(int16_t frame_index) {
  DCHECK(ar_compositor_);
  DCHECK(webxr_->HaveAnimatingFrame());
  DCHECK(webxr_->GetAnimatingFrame()->begin_frame_args);
  DVLOG(1) << __func__ << ": frame=" << frame_index;

  // We need to use the Animating Frame for the BeginFrameArgs before we
  // recycle it.
  ar_compositor_->DidNotProduceFrame(webxr_->GetAnimatingFrame());

  webxr_->RecycleUnusedAnimatingFrame();

  // Now that our animating frame has been cleared, see if we need to
  // unblock the pipeline. Note that if we have a CameraImage, we still
  // run our normal pipeline, so do not need to unblock in that case.
  TryRunPendingGetFrameData();
}

void ArCoreGl::SubmitFrame(int16_t frame_index,
                           const gpu::MailboxHolder& mailbox,
                           base::TimeDelta time_waited) {
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);
  DVLOG(2) << __func__ << ": frame=" << frame_index;
  DCHECK(!ArImageTransport::UseSharedBuffer());

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
  DCHECK(!ArImageTransport::UseSharedBuffer());

  // Use only the active bounds of the viewport, converting the
  // bounds UV boundaries to a transform. See also OnWebXrTokenSignaled().
  gfx::Transform transform =
      GetContentTransform(webxr_->GetProcessingFrame()->bounds_left);
  ar_image_transport_->CopyMailboxToSurfaceAndSwap(transfer_size_, mailbox,
                                                   transform);

  // Notify the client that we're done with the mailbox so that the underlying
  // image is eligible for destruction.
  submit_client_->OnSubmitFrameTransferred(true);

  CopyCameraImageToFramebuffer();

  // Now wait for ar_image_transport_ to call OnTransportFrameAvailable
  // indicating that the image drawn onto the Surface is ready for consumption
  // from the SurfaceTexture.
}

void ArCoreGl::TransitionProcessingFrameToRendering() {
  if (webxr_->HaveRenderingFrame()) {
    // It's possible, though unlikely, that the previous rendering frame hasn't
    // finished yet, for example if an unusually slow frame is followed by an
    // unusually quick one. In that case, wait for that frame to finish
    // rendering first before proceeding with this one. The state machine
    // doesn't permit two frames to be in rendering state at once. (Also, adding
    // even more GPU work in that condition would be counterproductive.)
    DVLOG(3) << __func__ << ": wait for previous rendering frame to complete";

    // When we have an ArCompositor we should be "pooling" our RenderingFrames
    // and storing multiple of them. As such, the check for having a single
    // rendering frame, should be false.
    DCHECK(!ar_compositor_);
    FinishRenderingFrame();
  }

  DCHECK(!webxr_->HaveRenderingFrame());
  DCHECK(webxr_->HaveProcessingFrame());
  auto* frame = webxr_->GetProcessingFrame();
  frame->time_copied = base::TimeTicks::Now();
  average_process_time_.AddSample(frame->time_copied - frame->time_js_submit);

  frame->render_completion_fence = nullptr;
  webxr_->TransitionFrameProcessingToRendering();

  // We finished processing a frame, unblock a potentially waiting next frame.
  webxr_->TryDeferredProcessing();

  // In shared buffer mode, submitting our frame (i.e. moving it to "Rendering")
  // may have unblocked the pipeline. Try to schedule it now.
  if (ar_compositor_) {
    TryRunPendingGetFrameData();
  }
}

void ArCoreGl::OnTransportFrameAvailable(const gfx::Transform& uv_transform) {
  DVLOG(2) << __func__;
  DCHECK(!ArImageTransport::UseSharedBuffer());
  DCHECK(webxr_->HaveProcessingFrame());
  int16_t frame_index = webxr_->GetProcessingFrame()->index;
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);

  TransitionProcessingFrameToRendering();

  // Now copy the received SurfaceTexture image to the framebuffer.
  // Don't use the viewport bounds here, those already got applied
  // when copying the mailbox image to the transfer Surface
  // in ProcessFrameFromMailbox.
  ar_image_transport_->CopyDrawnImageToFramebuffer(
      webxr_.get(), /*framebuffer=*/0, screen_size_, uv_transform);

  FinishFrame(frame_index);

  if (submit_client_) {
    // Create a local GpuFence and pass it to the Renderer via IPC.
    std::unique_ptr<gl::GLFence> gl_fence = gl::GLFence::CreateForGpuFence();
    std::unique_ptr<gfx::GpuFence> gpu_fence2 = gl_fence->GetGpuFence();
    submit_client_->OnSubmitFrameGpuFence(
        gpu_fence2->GetGpuFenceHandle().Clone());
  }
}

void ArCoreGl::SubmitFrameDrawnIntoTexture(int16_t frame_index,
                                           const gpu::SyncToken& sync_token,
                                           base::TimeDelta time_waited) {
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);
  DVLOG(2) << __func__ << ": frame=" << frame_index;
  DCHECK(ArImageTransport::UseSharedBuffer());
  DCHECK(ar_compositor_);

  if (!IsSubmitFrameExpected(frame_index))
    return;

  // The previous sync token has been consumed by the renderer process, so we
  // need to set this one for use by the compositor.
  webxr_->GetAnimatingFrame()->shared_buffer->sync_token = sync_token;
  webxr_->GetAnimatingFrame()->camera_image_shared_buffer->sync_token =
      sync_token;

  // Start processing the frame now if possible. If there's already a current
  // processing frame, defer it until that frame calls TryDeferredProcessing.
  webxr_->ProcessOrDefer(base::BindOnce(
      &ArCoreGl::SubmitVizFrame, weak_ptr_factory_.GetWeakPtr(), frame_index,
      ArCompositorFrameSink::FrameType::kHasWebXrContent));
}

void ArCoreGl::SubmitVizFrame(int16_t frame_index,
                              ArCompositorFrameSink::FrameType frame_type) {
  // Because this call may have been deferred, we need to re-check that the
  // we didn't get a shutdown triggered in the meantime.
  if (pending_shutdown_)
    return;
  TRACE_EVENT1("gpu", __func__, "frame", frame_index);
  DCHECK(webxr_->HaveProcessingFrame());
  DCHECK(ar_compositor_);

  DVLOG(3) << __func__ << " Submitting Frame to compositor: " << frame_index;
  auto* frame = webxr_->GetProcessingFrame();
  ar_compositor_->SubmitFrame(frame, frame_type);

  if (have_camera_image_) {
    have_camera_image_ = false;
  }

  if (kTransitionToRenderingImmediately) {
    TransitionProcessingFrameToRendering();
  }

  if (submit_client_ &&
      frame_type == ArCompositorFrameSink::FrameType::kHasWebXrContent) {
    // Create a local GpuFence and pass it to the Renderer via IPC.
    std::unique_ptr<gl::GLFence> gl_fence = gl::GLFence::CreateForGpuFence();
    std::unique_ptr<gfx::GpuFence> gpu_fence2 = gl_fence->GetGpuFence();
    submit_client_->OnSubmitFrameGpuFence(
        gpu_fence2->GetGpuFenceHandle().Clone());
  }

  // Now that we were transitioned to Processing (and are done with our work),
  // see if there's any work pending in the pipeline.
  TryRunPendingGetFrameData();
}

void ArCoreGl::UpdateLayerBounds(
    int16_t frame_index,
    const gfx::RectF& left_bounds,
    [[maybe_unused]] const gfx::RectF& right_bounds,
    const gfx::Size& source_size) {
  DVLOG(2) << __func__ << " source_size=" << source_size.ToString()
           << " left_bounds=" << left_bounds.ToString();

  // The first UpdateLayerBounds may arrive early, when there's
  // no animating frame yet. In that case, just save it in viewport_bounds_
  // so that it's applied to the next animating frame.
  if (webxr_->HaveAnimatingFrame()) {
    // Handheld AR mode is monoscopic and only uses the left bounds, thus the
    // [[maybe_unused]] on `right_bounds`.
    webxr_->GetAnimatingFrame()->bounds_left = left_bounds;
  }
  viewport_bounds_ = left_bounds;

  // Early setting of transfer_size_ is OK since that's only used by the
  // animating frame. Processing/rendering frames use the bounds from
  // WebXRPresentationState.
  transfer_size_ = source_size;

  // Note that this resize only affects the WebXR content. The camera image will
  // always take up the full screen size, which means that compositing will
  // still take up the full screen size, so we don't need to notify the ar
  // compositor about any change in size. (The WebXR content gets appropriately
  // scaled for this Fullscreen rendering).
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

  if (native_origin_information->is_input_source_space_info()) {
    DVLOG(1) << __func__
             << ": ARCore device supports only transient input sources for "
                "now. Rejecting subscription request.";
    std::move(callback).Run(
        device::mojom::SubscribeToHitTestResult::FAILURE_GENERIC, 0);
    return;
  }

  std::optional<uint64_t> maybe_subscription_id = arcore_->SubscribeToHitTest(
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

  std::optional<uint64_t> maybe_subscription_id =
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
  // Because this call may have been deferred, we need to re-check that the
  // we didn't get a shutdown triggered in the meantime.
  if (pending_shutdown_)
    return;
  DVLOG(3) << __func__ << " frame=" << frame_data->frame_id << ", pose valid? "
           << (frame_data->mojo_from_viewer ? true : false);

  DCHECK(IsOnGlThread());
  DCHECK(is_initialized_);

  if (frame_data->mojo_from_viewer) {
    DCHECK(frame_data->mojo_from_viewer->position);
    DCHECK(frame_data->mojo_from_viewer->orientation);

    frame_data->input_state = GetInputSourceStates();

    device::Pose mojo_from_viewer(*frame_data->mojo_from_viewer->position,
                                  *frame_data->mojo_from_viewer->orientation);

    // Get results for hit test subscriptions.
    frame_data->hit_test_subscription_results =
        arcore_->GetHitTestSubscriptionResults(mojo_from_viewer.ToTransform(),
                                               *frame_data->input_state);

    if (IsFeatureEnabled(device::mojom::XRSessionFeature::ANCHORS)) {
      arcore_->ProcessAnchorCreationRequests(
          mojo_from_viewer.ToTransform(), *frame_data->input_state,
          frame_data->time_delta + base::TimeTicks());
    }
  }

  // Get anchors data, including anchors created this frame.
  if (IsFeatureEnabled(device::mojom::XRSessionFeature::ANCHORS)) {
    frame_data->anchors_data = arcore_->GetAnchorsData();
  }

  // Get planes data if it was requested.
  if (IsFeatureEnabled(device::mojom::XRSessionFeature::PLANE_DETECTION)) {
    frame_data->detected_planes_data = arcore_->GetDetectedPlanesData();
  }

  // Get lighting estimation data if it was requested.
  if (options && options->include_lighting_estimation_data) {
    frame_data->light_estimation_data = arcore_->GetLightEstimationData();
  }

  if (IsFeatureEnabled(device::mojom::XRSessionFeature::DEPTH)) {
    // We only return a single view.
    CHECK(frame_data->views.size() > 0);
    frame_data->views[0]->depth_data = arcore_->GetDepthData();
  }

  if (IsFeatureEnabled(device::mojom::XRSessionFeature::IMAGE_TRACKING)) {
    frame_data->tracked_images = arcore_->GetTrackedImages();
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
    if (IsFeatureEnabled(device::mojom::XRSessionFeature::DOM_OVERLAY)) {
      state->overlay_pointer_position = screen_last_touch;
    }

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
        screen_last_touch.x() / screen_size_.width() * 2.f - 1.f;
    const float y_normalized =
        (1.f - screen_last_touch.y() / screen_size_.height()) * 2.f - 1.f;
    gfx::Point3F touch_point(x_normalized, y_normalized, -1.f);
    DVLOG(3) << __func__ << ": touch_point=" << touch_point.ToString();
    touch_point = inverse_projection_.MapPoint(touch_point);
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
    auto viewer_from_pointer = gfx::Transform::RowMajor(
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

  // This call appears to fix a spurious ARCoreError "texture names are not
  // set" aka AR_ERROR_TEXTURE_NOT_SET. The documentation mentions that
  // the texture contents aren't valid across pause/resume, but it's unclear
  // why that also makes the registered texture name invalid.
  arcore_->SetCameraTexture(ar_image_transport_->GetCameraTextureId());
}

void ArCoreGl::OnBindingDisconnect() {
  DVLOG(3) << __func__;

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

void ArCoreGl::OnBeginFrame(const viz::BeginFrameArgs& args,
                            const viz::FrameTimingDetailsMap& frame_timing) {
  // With the ExternalBeginFrameController driving our compositing, we shouldn't
  // request any frames unless we actually have a frame to animate.
  DCHECK(webxr_->HaveAnimatingFrame());

  TRACE_EVENT1("gpu", __func__, "frame", webxr_->GetAnimatingFrame()->index);
  DVLOG(3) << __func__;
  webxr_->GetAnimatingFrame()->begin_frame_args =
      std::make_unique<viz::BeginFrameArgs>(args);

  // If we have information about frame timing from completed frames, use that
  // to update GPU load heuristics. Typically, there will be one reported old
  // frame for each OnBeginFrame once it reaches a steady state.
  for (auto& timing_data : frame_timing) {
    const viz::FrameTimingDetails& details = timing_data.second;
    base::TimeTicks writes_done =
        details.presentation_feedback.writes_done_timestamp;

    // The GPU driver isn't required to support writes_done timestamps, so
    // this data may be unavailable. In that case, don't update the rendering
    // time ratio estimate. This disables dynamic viewport scaling since that
    // feature is only active when the ratio is nonzero.
    if (writes_done.is_null())
      continue;

    // For the GPU load, use the drawing time (draw start to render completion)
    // divided by the nominal frame time.

    base::TimeDelta delta = writes_done - details.draw_start_timestamp;
    base::TimeDelta arcore_frametime = EstimatedArCoreFrameTime();
    DCHECK(!arcore_frametime.is_zero());
    rendering_time_ratio_ = delta / arcore_frametime;

    DVLOG(3) << __func__ << ": frame_token=" << timing_data.first
             << " draw_start_timestamp=" << details.draw_start_timestamp
             << " writes_done="
             << details.presentation_feedback.writes_done_timestamp
             << " delta=" << delta
             << " rendering_time_ratio_=" << rendering_time_ratio_;
    TRACE_COUNTER1("xr", "WebXR rendering time ratio (%)",
                   rendering_time_ratio_ * 100);
  }

  webxr_->TryDeferredProcessing();
}

}  // namespace device
