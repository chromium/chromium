// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_session.h"

#include "base/auto_reset.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_frame_request_callback.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/screen_orientation/screen_orientation.h"
#include "third_party/blink/renderer/modules/xr/xr.h"
#include "third_party/blink/renderer/modules/xr/xr_canvas_input_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_device.h"
#include "third_party/blink/renderer/modules/xr/xr_frame.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_of_reference.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_of_reference_options.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_hit_result.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source_event.h"
#include "third_party/blink/renderer/modules/xr/xr_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_presentation_context.h"
#include "third_party/blink/renderer/modules/xr/xr_session_event.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_layer.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

namespace {

const char kSessionEnded[] = "XRSession has already ended.";

const char kUnknownFrameOfReference[] = "Unknown frame of reference type.";

const char kNonEmulatedStageNotSupported[] =
    "This device does not support a non-emulated 'stage' frame of reference.";

const double kDegToRad = M_PI / 180.0;

// TODO(bajones): This is something that we probably want to make configurable.
const double kMagicWindowVerticalFieldOfView = 75.0f * M_PI / 180.0f;

void UpdateViewFromEyeParameters(
    XRView* view,
    const device::mojom::blink::VREyeParametersPtr& eye,
    double depth_near,
    double depth_far) {
  const device::mojom::blink::VRFieldOfViewPtr& fov = eye->fieldOfView;

  view->UpdateProjectionMatrixFromFoV(
      fov->upDegrees * kDegToRad, fov->downDegrees * kDegToRad,
      fov->leftDegrees * kDegToRad, fov->rightDegrees * kDegToRad, depth_near,
      depth_far);

  view->UpdateOffset(eye->offset[0], eye->offset[1], eye->offset[2]);
}

}  // namespace

class XRSession::XRSessionResizeObserverDelegate final
    : public ResizeObserver::Delegate {
 public:
  explicit XRSessionResizeObserverDelegate(XRSession* session)
      : session_(session) {
    DCHECK(session);
  }
  ~XRSessionResizeObserverDelegate() override = default;

  void OnResize(
      const HeapVector<Member<ResizeObserverEntry>>& entries) override {
    DCHECK_EQ(1u, entries.size());
    session_->UpdateCanvasDimensions(entries[0]->target());
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(session_);
    ResizeObserver::Delegate::Trace(visitor);
  }

 private:
  Member<XRSession> session_;
};

XRSession::XRSession(
    XRDevice* device,
    device::mojom::blink::XRSessionClientRequest client_request,
    bool immersive,
    bool environment_integration,
    XRPresentationContext* output_context,
    EnvironmentBlendMode environment_blend_mode)
    : device_(device),
      immersive_(immersive),
      environment_integration_(environment_integration),
      output_context_(output_context),
      client_binding_(this, std::move(client_request)),
      callback_collection_(new XRFrameRequestCallbackCollection(
          device->xr()->GetExecutionContext())) {
  blurred_ = !HasAppropriateFocus();

  // When an output context is provided, monitor it for resize events.
  if (output_context_) {
    HTMLCanvasElement* canvas = outputContext()->canvas();
    if (canvas) {
      resize_observer_ = ResizeObserver::Create(
          canvas->GetDocument(), new XRSessionResizeObserverDelegate(this));
      resize_observer_->observe(canvas);

      // Begin processing input events on the output context's canvas.
      if (!immersive_) {
        canvas_input_provider_ = new XRCanvasInputProvider(this, canvas);
      }

      // Get the initial canvas dimensions
      UpdateCanvasDimensions(canvas);
    }
  }

  switch (environment_blend_mode) {
    case kBlendModeOpaque:
      blend_mode_string_ = "opaque";
      break;
    case kBlendModeAdditive:
      blend_mode_string_ = "additive";
      break;
    case kBlendModeAlphaBlend:
      blend_mode_string_ = "alpha-blend";
      break;
    default:
      NOTREACHED() << "Unknown environment blend mode: "
                   << environment_blend_mode;
  }
}

void XRSession::setDepthNear(double value) {
  if (depth_near_ != value) {
    update_views_next_frame_ = true;
    depth_near_ = value;
  }
}

void XRSession::setDepthFar(double value) {
  if (depth_far_ != value) {
    update_views_next_frame_ = true;
    depth_far_ = value;
  }
}

void XRSession::setBaseLayer(XRLayer* value) {
  base_layer_ = value;
  // Make sure that the layer's drawing buffer is updated to the right size
  // if this is a non-immersive session.
  if (!immersive_ && base_layer_) {
    base_layer_->OnResize();
  }
}

void XRSession::SetNonImmersiveProjectionMatrix(
    const WTF::Vector<float>& projection_matrix) {
  DCHECK_EQ(projection_matrix.size(), 16lu);

  non_immersive_projection_matrix_ = projection_matrix;
  // It is about as expensive to check equality as to just
  // update the views, so just update.
  update_views_next_frame_ = true;
}

ExecutionContext* XRSession::GetExecutionContext() const {
  return device_->xr()->GetExecutionContext();
}

const AtomicString& XRSession::InterfaceName() const {
  return EventTargetNames::XRSession;
}

ScriptPromise XRSession::requestFrameOfReference(
    ScriptState* script_state,
    const String& type,
    const XRFrameOfReferenceOptions& options) {
  if (ended_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                           kSessionEnded));
  }

  XRFrameOfReference* frameOfRef = nullptr;
  if (type == "head-model") {
    frameOfRef =
        new XRFrameOfReference(this, XRFrameOfReference::kTypeHeadModel);
  } else if (type == "eye-level") {
    frameOfRef =
        new XRFrameOfReference(this, XRFrameOfReference::kTypeEyeLevel);
  } else if (type == "stage") {
    if (!options.disableStageEmulation()) {
      frameOfRef = new XRFrameOfReference(this, XRFrameOfReference::kTypeStage);
      frameOfRef->UseEmulatedHeight(options.stageEmulationHeight());
    } else if (display_info_ && display_info_->stageParameters) {
      frameOfRef = new XRFrameOfReference(this, XRFrameOfReference::kTypeStage);
    } else {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(DOMExceptionCode::kNotSupportedError,
                               kNonEmulatedStageNotSupported));
    }
  }

  if (!frameOfRef) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                           kUnknownFrameOfReference));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Resolve(frameOfRef);

  return promise;
}

int XRSession::requestAnimationFrame(V8XRFrameRequestCallback* callback) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  // Don't allow any new frame requests once the session is ended.
  if (ended_)
    return 0;

  // Don't allow frames to be scheduled if there's no layers attached to the
  // session. That would allow tracking with no associated visuals.
  if (!base_layer_)
    return 0;

  int id = callback_collection_->RegisterCallback(callback);
  if (!pending_frame_) {
    // Kick off a request for a new XR frame.
    device_->frameProvider()->RequestFrame(this);
    pending_frame_ = true;
  }
  return id;
}

void XRSession::cancelAnimationFrame(int id) {
  callback_collection_->CancelCallback(id);
}

HeapVector<Member<XRInputSource>> XRSession::getInputSources() const {
  Document* doc = To<Document>(GetExecutionContext());
  if (!did_log_getInputSources_ && doc) {
    ukm::builders::XR_WebXR(device_->GetSourceId())
        .SetDidGetXRInputSources(1)
        .Record(doc->UkmRecorder());
    did_log_getInputSources_ = true;
  }

  HeapVector<Member<XRInputSource>> source_array;
  for (const auto& input_source : input_sources_.Values()) {
    source_array.push_back(input_source);
  }

  if (canvas_input_provider_) {
    XRInputSource* input_source = canvas_input_provider_->GetInputSource();
    if (input_source) {
      source_array.push_back(input_source);
    }
  }

  return source_array;
}

ScriptPromise XRSession::requestHitTest(ScriptState* script_state,
                                        NotShared<DOMFloat32Array> origin,
                                        NotShared<DOMFloat32Array> direction,
                                        XRCoordinateSystem* coordinate_system) {
  if (ended_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                           kSessionEnded));
  }

  if (!coordinate_system) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(),
                          "The coordinateSystem parameter is empty."));
  }

  if (origin.View()->length() != 3 || direction.View()->length() != 3) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kNotSupportedError,
                                           "Invalid ray!"));
  }

  // TODO(https://crbug.com/846411): use coordinate_system.

  // TODO(https://crbug.com/843376): Reject the promise if device doesn't
  // support the hit-test API.
  device::mojom::blink::XRRayPtr ray = device::mojom::blink::XRRay::New();

  ray->origin = gfx::mojom::blink::Point3F::New();
  ray->origin->x = origin.View()->Data()[0];
  ray->origin->y = origin.View()->Data()[1];
  ray->origin->z = origin.View()->Data()[2];

  ray->direction = gfx::mojom::blink::Vector3dF::New();
  ray->direction->x = direction.View()->Data()[0];
  ray->direction->y = direction.View()->Data()[1];
  ray->direction->z = direction.View()->Data()[2];

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // TODO(https://crbug.com/845520): Promise should be rejected if session
  // is deleted.
  device_->xrEnvironmentProviderPtr()->RequestHitTest(
      std::move(ray),
      WTF::Bind(&XRSession::OnHitTestResults, WrapWeakPersistent(this),
                WrapPersistent(resolver)));

  return promise;
}

void XRSession::OnHitTestResults(
    ScriptPromiseResolver* resolver,
    base::Optional<WTF::Vector<device::mojom::blink::XRHitResultPtr>> results) {
  if (!results) {
    resolver->Reject();
    return;
  }

  HeapVector<Member<XRHitResult>> hit_results;
  for (const auto& mojom_result : results.value()) {
    XRHitResult* hit_result = new XRHitResult(TransformationMatrix::Create(
        mojom_result->hit_matrix[0], mojom_result->hit_matrix[1],
        mojom_result->hit_matrix[2], mojom_result->hit_matrix[3],
        mojom_result->hit_matrix[4], mojom_result->hit_matrix[5],
        mojom_result->hit_matrix[6], mojom_result->hit_matrix[7],
        mojom_result->hit_matrix[8], mojom_result->hit_matrix[9],
        mojom_result->hit_matrix[10], mojom_result->hit_matrix[11],
        mojom_result->hit_matrix[12], mojom_result->hit_matrix[13],
        mojom_result->hit_matrix[14], mojom_result->hit_matrix[15]));
    hit_results.push_back(hit_result);
  }
  resolver->Resolve(hit_results);
}

ScriptPromise XRSession::end(ScriptState* script_state) {
  // Don't allow a session to end twice.
  if (ended_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                           kSessionEnded));
  }

  ForceEnd();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // TODO(bajones): If there's any work that needs to be done asynchronously on
  // session end it should be completed before this promise is resolved.

  resolver->Resolve();
  return promise;
}

void XRSession::ForceEnd() {
  // Detach this session from the device.
  ended_ = true;
  pending_frame_ = false;

  if (canvas_input_provider_) {
    canvas_input_provider_->Stop();
    canvas_input_provider_ = nullptr;
  }

  // If this session is the active immersive session for the device, notify the
  // frameProvider that it's ended.
  if (device_->frameProvider()->immersive_session() == this) {
    device_->frameProvider()->OnImmersiveSessionEnded();
  }

  DispatchEvent(*XRSessionEvent::Create(EventTypeNames::end, this));
}

double XRSession::NativeFramebufferScale() const {
  if (immersive_) {
    double scale = display_info_->webxr_default_framebuffer_scale;
    DCHECK(scale);

    // Return the inverse of the default scale, since that's what we'll need to
    // multiply the default size by to get back to the native size.
    return 1.0 / scale;
  }
  return 1.0;
}

DoubleSize XRSession::DefaultFramebufferSize() const {
  if (!immersive_) {
    return OutputCanvasSize();
  }

  double width = (display_info_->leftEye->renderWidth +
                  display_info_->rightEye->renderWidth);
  double height = std::max(display_info_->leftEye->renderHeight,
                           display_info_->rightEye->renderHeight);

  double scale = display_info_->webxr_default_framebuffer_scale;
  return DoubleSize(width * scale, height * scale);
}

DoubleSize XRSession::OutputCanvasSize() const {
  if (!output_context_) {
    return DoubleSize();
  }

  return DoubleSize(output_width_, output_height_);
}

void XRSession::OnFocus() {
  if (!blurred_)
    return;

  blurred_ = false;
  DispatchEvent(*XRSessionEvent::Create(EventTypeNames::focus, this));
}

void XRSession::OnBlur() {
  if (blurred_)
    return;

  blurred_ = true;
  DispatchEvent(*XRSessionEvent::Create(EventTypeNames::blur, this));
}

// Immersive sessions may still not be blurred in headset even if the page isn't
// focused.  This prevents the in-headset experience from freezing on an
// external display headset when the user clicks on another tab.
bool XRSession::HasAppropriateFocus() {
  return immersive_ ? has_device_focus_
                    : has_device_focus_ && device_->HasFrameFocus();
}

void XRSession::OnFocusChanged() {
  if (HasAppropriateFocus()) {
    OnFocus();
  } else {
    OnBlur();
  }
}

void XRSession::OnFrame(
    double timestamp,
    std::unique_ptr<TransformationMatrix> base_pose_matrix,
    const base::Optional<gpu::MailboxHolder>& output_mailbox_holder,
    const base::Optional<gpu::MailboxHolder>& background_mailbox_holder,
    const base::Optional<IntSize>& background_size) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DVLOG(2) << __FUNCTION__;
  // Don't process any outstanding frames once the session is ended.
  if (ended_)
    return;

  base_pose_matrix_ = std::move(base_pose_matrix);

  // Don't allow frames to be processed if there's no layers attached to the
  // session. That would allow tracking with no associated visuals.
  if (!base_layer_)
    return;

  XRFrame* presentation_frame = CreatePresentationFrame();

  if (pending_frame_) {
    pending_frame_ = false;

    // Make sure that any frame-bounded changed to the views array take effect.
    if (update_views_next_frame_) {
      views_dirty_ = true;
      update_views_next_frame_ = false;
    }

    // Cache the base layer, since it could change during the frame callback.
    XRLayer* frame_base_layer = base_layer_;
    frame_base_layer->OnFrameStart(output_mailbox_holder);

    // TODO(836349): revisit sending background image data to blink at all.
    if (background_mailbox_holder) {
      // If using a background image, the caller must provide its pixel size
      // also. The source size can differ from the current drawing buffer size.
      DCHECK(background_size);
      frame_base_layer->HandleBackgroundImage(background_mailbox_holder.value(),
                                              background_size.value());
    }

    // Resolve the queued requestAnimationFrame callbacks. All XR rendering will
    // happen within these calls. resolving_frame_ will be true for the duration
    // of the callbacks.
    base::AutoReset<bool> resolving(&resolving_frame_, true);
    callback_collection_->ExecuteCallbacks(this, timestamp, presentation_frame);

    // The session might have ended in the middle of the frame. Only call
    // OnFrameEnd if it's still valid.
    if (!ended_)
      frame_base_layer->OnFrameEnd();
  }
}

void XRSession::LogGetPose() const {
  Document* doc = To<Document>(GetExecutionContext());
  if (!did_log_getDevicePose_ && doc) {
    did_log_getDevicePose_ = true;

    ukm::builders::XR_WebXR(device_->GetSourceId())
        .SetDidRequestPose(1)
        .Record(doc->UkmRecorder());
  }
}

XRFrame* XRSession::CreatePresentationFrame() {
  XRFrame* presentation_frame = new XRFrame(this);
  if (base_pose_matrix_) {
    presentation_frame->SetBasePoseMatrix(*base_pose_matrix_);
  }
  return presentation_frame;
}

// Called when the canvas element for this session's output context is resized.
void XRSession::UpdateCanvasDimensions(Element* element) {
  DCHECK(element);

  double devicePixelRatio = 1.0;
  LocalFrame* frame = device_->xr()->GetFrame();
  if (frame) {
    devicePixelRatio = frame->DevicePixelRatio();
  }

  update_views_next_frame_ = true;
  output_width_ = element->OffsetWidth() * devicePixelRatio;
  output_height_ = element->OffsetHeight() * devicePixelRatio;
  int output_angle = 0;

  // TODO(crbug.com/836948): handle square canvases.
  // TODO(crbug.com/840346): we should not need to use ScreenOrientation here.
  ScreenOrientation* orientation = ScreenOrientation::Create(frame);

  if (orientation) {
    output_angle = orientation->angle();
    DVLOG(2) << __FUNCTION__ << ": got angle=" << output_angle;
  }

  if (device_->xrEnvironmentProviderPtr()) {
    device_->xrEnvironmentProviderPtr()->UpdateSessionGeometry(
        IntSize(output_width_, output_height_),
        display::Display::DegreesToRotation(output_angle));
  }

  if (base_layer_) {
    base_layer_->OnResize();
  }
}

void XRSession::OnInputStateChange(
    int16_t frame_id,
    const WTF::Vector<device::mojom::blink::XRInputSourceStatePtr>&
        input_states) {
  bool devices_changed = false;

  // Update any input sources with new state information. Any updated input
  // sources are marked as active.
  for (const auto& input_state : input_states) {
    XRInputSource* input_source = input_sources_.at(input_state->source_id);
    if (!input_source) {
      input_source = new XRInputSource(this, input_state->source_id);
      input_sources_.Set(input_state->source_id, input_source);
      devices_changed = true;
    }
    input_source->active_frame_id = frame_id;
    UpdateInputSourceState(input_source, input_state);
  }

  // Remove any input sources that are inactive..
  std::vector<uint32_t> inactive_sources;
  for (const auto& input_source : input_sources_.Values()) {
    if (input_source->active_frame_id != frame_id) {
      inactive_sources.push_back(input_source->source_id());
      devices_changed = true;
    }
  }

  if (inactive_sources.size()) {
    for (uint32_t source_id : inactive_sources) {
      input_sources_.erase(source_id);
    }
  }

  if (devices_changed) {
    DispatchEvent(
        *XRSessionEvent::Create(EventTypeNames::inputsourceschange, this));
  }
}

void XRSession::OnSelectStart(XRInputSource* input_source) {
  // Discard duplicate events
  if (input_source->primary_input_pressed)
    return;

  input_source->primary_input_pressed = true;
  input_source->selection_cancelled = false;

  XRInputSourceEvent* event =
      CreateInputSourceEvent(EventTypeNames::selectstart, input_source);
  DispatchEvent(*event);

  if (event->defaultPrevented())
    input_source->selection_cancelled = true;
}

void XRSession::OnSelectEnd(XRInputSource* input_source) {
  // Discard duplicate events
  if (!input_source->primary_input_pressed)
    return;

  input_source->primary_input_pressed = false;

  LocalFrame* frame = device_->xr()->GetFrame();
  if (!frame)
    return;

  std::unique_ptr<UserGestureIndicator> gesture_indicator =
      LocalFrame::NotifyUserActivation(frame);

  XRInputSourceEvent* event =
      CreateInputSourceEvent(EventTypeNames::selectend, input_source);
  DispatchEvent(*event);

  if (event->defaultPrevented())
    input_source->selection_cancelled = true;
}

void XRSession::OnSelect(XRInputSource* input_source) {
  // If a select was fired but we had not previously started the selection it
  // indictes a sub-frame or instantanous select event, and we should fire a
  // selectstart prior to the selectend.
  if (!input_source->primary_input_pressed) {
    OnSelectStart(input_source);
  }

  // Make sure we end the selection prior to firing the select event.
  OnSelectEnd(input_source);

  if (!input_source->selection_cancelled) {
    XRInputSourceEvent* event =
        CreateInputSourceEvent(EventTypeNames::select, input_source);
    DispatchEvent(*event);
  }
}

void XRSession::OnPoseReset() {
  DispatchEvent(*XRSessionEvent::Create(EventTypeNames::resetpose, this));
}

void XRSession::UpdateInputSourceState(
    XRInputSource* input_source,
    const device::mojom::blink::XRInputSourceStatePtr& state) {
  if (!input_source || !state)
    return;

  // Update the input source's description if this state update
  // includes them.
  if (state->description) {
    const device::mojom::blink::XRInputSourceDescriptionPtr& desc =
        state->description;

    input_source->SetTargetRayMode(
        static_cast<XRInputSource::TargetRayMode>(desc->target_ray_mode));

    input_source->SetHandedness(
        static_cast<XRInputSource::Handedness>(desc->handedness));

    input_source->SetEmulatedPosition(desc->emulated_position);

    if (desc->pointer_offset && desc->pointer_offset->matrix.has_value()) {
      const WTF::Vector<float>& m = desc->pointer_offset->matrix.value();
      std::unique_ptr<TransformationMatrix> pointer_matrix =
          TransformationMatrix::Create(m[0], m[1], m[2], m[3], m[4], m[5], m[6],
                                       m[7], m[8], m[9], m[10], m[11], m[12],
                                       m[13], m[14], m[15]);
      input_source->SetPointerTransformMatrix(std::move(pointer_matrix));
    }
  }

  if (state->grip && state->grip->matrix.has_value()) {
    const Vector<float>& m = state->grip->matrix.value();
    std::unique_ptr<TransformationMatrix> grip_matrix =
        TransformationMatrix::Create(m[0], m[1], m[2], m[3], m[4], m[5], m[6],
                                     m[7], m[8], m[9], m[10], m[11], m[12],
                                     m[13], m[14], m[15]);
    input_source->SetBasePoseMatrix(std::move(grip_matrix));
  }

  // Handle state change of the primary input, which may fire events
  if (state->primary_input_clicked)
    OnSelect(input_source);

  if (state->primary_input_pressed) {
    OnSelectStart(input_source);
  } else if (input_source->primary_input_pressed) {
    // May get here if the input source was previously pressed but now isn't,
    // but the input source did not set primary_input_clicked to true. We will
    // treat this as a cancelled selection, firing the selectend event so the
    // page stays in sync with the controller state but won't fire the
    // usual select event.
    OnSelectEnd(input_source);
  }
}

XRInputSourceEvent* XRSession::CreateInputSourceEvent(
    const AtomicString& type,
    XRInputSource* input_source) {
  XRFrame* presentation_frame = CreatePresentationFrame();
  return XRInputSourceEvent::Create(type, presentation_frame, input_source);
}

void XRSession::OnChanged(device::mojom::blink::VRDisplayInfoPtr display_info) {
  DCHECK(display_info);
  SetXRDisplayInfo(std::move(display_info));
}

void XRSession::OnExitPresent() {
  if (immersive_) {
    ForceEnd();
  }
}

void XRSession::SetXRDisplayInfo(
    device::mojom::blink::VRDisplayInfoPtr display_info) {
  display_info_id_++;
  display_info_ = std::move(display_info);
  is_external_ = display_info_->capabilities->hasExternalDisplay;
}

const HeapVector<Member<XRView>>& XRSession::views() {
  // TODO(bajones): For now we assume that immersive sessions render a stereo
  // pair of views and non-immersive sessions render a single view. That doesn't
  // always hold true, however, so the view configuration should ultimately come
  // from the backing service.
  if (views_dirty_) {
    if (immersive_) {
      // If we don't already have the views allocated, do so now.
      if (views_.IsEmpty()) {
        views_.push_back(new XRView(this, XRView::kEyeLeft));
        views_.push_back(new XRView(this, XRView::kEyeRight));
      }
      // In immersive mode the projection and view matrices must be aligned with
      // the device's physical optics.
      UpdateViewFromEyeParameters(views_[XRView::kEyeLeft],
                                  display_info_->leftEye, depth_near_,
                                  depth_far_);
      UpdateViewFromEyeParameters(views_[XRView::kEyeRight],
                                  display_info_->rightEye, depth_near_,
                                  depth_far_);
    } else {
      if (views_.IsEmpty()) {
        views_.push_back(new XRView(this, XRView::kEyeLeft));
        views_[XRView::kEyeLeft]->UpdateOffset(0, 0, 0);
      }

      float aspect = 1.0f;
      if (output_width_ && output_height_) {
        aspect = static_cast<float>(output_width_) /
                 static_cast<float>(output_height_);
      }

      if (non_immersive_projection_matrix_.size() > 0) {
        views_[XRView::kEyeLeft]->UpdateProjectionMatrixFromRawValues(
            non_immersive_projection_matrix_, depth_near_, depth_far_);
      } else {
        // In non-immersive mode, if there is no explicit projection matrix
        // provided, the projection matrix must be aligned with the
        // output canvas dimensions.
        views_[XRView::kEyeLeft]->UpdateProjectionMatrixFromAspect(
            kMagicWindowVerticalFieldOfView, aspect, depth_near_, depth_far_);
      }
    }

    views_dirty_ = false;
  } else {
    // TODO(https://crbug.com/836926): views_dirty_ is not working right for
    // AR mode, we're not picking up the change on the right frame. Remove this
    // fallback once that's sorted out.
    DVLOG(2) << __FUNCTION__ << ": FIXME, fallback proj matrix update";
    if (non_immersive_projection_matrix_.size() > 0) {
      views_[XRView::kEyeLeft]->UpdateProjectionMatrixFromRawValues(
          non_immersive_projection_matrix_, depth_near_, depth_far_);
    }
  }

  return views_;
}

bool XRSession::HasPendingActivity() const {
  return !callback_collection_->IsEmpty() && !ended_;
}

void XRSession::Trace(blink::Visitor* visitor) {
  visitor->Trace(device_);
  visitor->Trace(output_context_);
  visitor->Trace(base_layer_);
  visitor->Trace(views_);
  visitor->Trace(input_sources_);
  visitor->Trace(resize_observer_);
  visitor->Trace(canvas_input_provider_);
  visitor->Trace(callback_collection_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
