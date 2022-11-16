// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_session.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/cxx17_backports.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "base/types/pass_key.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_frame_request_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_hit_test_options_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_image_tracking_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_render_state_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_transient_input_hit_test_options_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/probe/async_task_context.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/xr/type_converters.h"
#include "third_party/blink/renderer/modules/xr/xr_anchor_set.h"
#include "third_party/blink/renderer/modules/xr/xr_bounded_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_camera.h"
#include "third_party/blink/renderer/modules/xr/xr_canvas_input_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_cube_map.h"
#include "third_party/blink/renderer/modules/xr/xr_depth_information.h"
#include "third_party/blink/renderer/modules/xr/xr_depth_manager.h"
#include "third_party/blink/renderer/modules/xr/xr_dom_overlay_state.h"
#include "third_party/blink/renderer/modules/xr/xr_frame.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_hit_test_source.h"
#include "third_party/blink/renderer/modules/xr/xr_image_tracking_result.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source_event.h"
#include "third_party/blink/renderer/modules/xr/xr_input_sources_change_event.h"
#include "third_party/blink/renderer/modules/xr/xr_light_probe.h"
#include "third_party/blink/renderer/modules/xr/xr_plane_manager.h"
#include "third_party/blink/renderer/modules/xr/xr_ray.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_render_state.h"
#include "third_party/blink/renderer/modules/xr/xr_session_event.h"
#include "third_party/blink/renderer/modules/xr/xr_session_viewport_scaler.h"
#include "third_party/blink/renderer/modules/xr/xr_system.h"
#include "third_party/blink/renderer/modules/xr/xr_transient_input_hit_test_source.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_layer.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

namespace {

const char kSessionEnded[] = "XRSession has already ended.";

const char kReferenceSpaceNotSupported[] =
    "This device does not support the requested reference space type.";

const char kIncompatibleLayer[] =
    "XRWebGLLayer was created with a different session.";

const char kInlineVerticalFOVNotSupported[] =
    "This session does not support inlineVerticalFieldOfView";

const char kAnchorsNotSupportedByDevice[] = "Device does not support anchors!";

const char kHitTestNotSupportedByDevice[] = "Device does not support hit test!";

const char kDeviceDisconnected[] = "The XR device has been disconnected.";

const char kUnableToDecomposeMatrix[] =
    "The operation was unable to decompose a matrix and could not be "
    "completed.";

const char kUnableToRetrieveNativeOrigin[] =
    "The operation was unable to retrieve the native origin from XRSpace and "
    "could not be completed.";

const char kHitTestFeatureNotSupported[] =
    "Hit test feature is not supported by the session.";

const char kHitTestSubscriptionFailed[] = "Hit test subscription failed.";

const char kAnchorCreationFailed[] = "Anchor creation failed.";

const char kLightEstimationFeatureNotSupported[] =
    "Light estimation feature is not supported.";

const char kImageTrackingFeatureNotSupported[] =
    "Image tracking feature is not supported.";

const char kEntityTypesNotSpecified[] =
    "No entityTypes specified: the array cannot be empty!";

const float kMinDefaultFramebufferScale = 0.1f;
const float kMaxDefaultFramebufferScale = 1.0f;

// Indices into the views array.
const unsigned int kMonoView = 0;

// Returns the session feature corresponding to the given reference space type.
absl::optional<device::mojom::XRSessionFeature> MapReferenceSpaceTypeToFeature(
    device::mojom::blink::XRReferenceSpaceType type) {
  switch (type) {
    case device::mojom::blink::XRReferenceSpaceType::kViewer:
      return device::mojom::XRSessionFeature::REF_SPACE_VIEWER;
    case device::mojom::blink::XRReferenceSpaceType::kLocal:
      return device::mojom::XRSessionFeature::REF_SPACE_LOCAL;
    case device::mojom::blink::XRReferenceSpaceType::kLocalFloor:
      return device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR;
    case device::mojom::blink::XRReferenceSpaceType::kBoundedFloor:
      return device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR;
    case device::mojom::blink::XRReferenceSpaceType::kUnbounded:
      return device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED;
  }

  NOTREACHED();
  return absl::nullopt;
}

std::unique_ptr<gfx::Transform> getPoseMatrix(
    const device::mojom::blink::VRPosePtr& pose) {
  if (!pose)
    return nullptr;

  device::Pose device_pose =
      device::Pose(pose->position.value_or(gfx::Point3F()),
                   pose->orientation.value_or(gfx::Quaternion()));

  return std::make_unique<gfx::Transform>(device_pose.ToTransform());
}

absl::optional<device::mojom::blink::EntityTypeForHitTest>
EntityTypeForHitTestFromString(const String& string) {
  if (string == "plane")
    return device::mojom::blink::EntityTypeForHitTest::PLANE;

  if (string == "point")
    return device::mojom::blink::EntityTypeForHitTest::POINT;

  NOTREACHED();
  return absl::nullopt;
}

// Returns a vector of entity types from hit test options, without duplicates.
// OptionsType can be either XRHitTestOptionsInit or
// XRTransientInputHitTestOptionsInit.
template <typename OptionsType>
Vector<device::mojom::blink::EntityTypeForHitTest> GetEntityTypesForHitTest(
    OptionsType* options_init) {
  DCHECK(options_init);
  HashSet<device::mojom::blink::EntityTypeForHitTest> result_set;

  if (RuntimeEnabledFeatures::WebXRHitTestEntityTypesEnabled() &&
      options_init->hasEntityTypes()) {
    DVLOG(2) << __func__ << ": options_init->entityTypes().size()="
             << options_init->entityTypes().size();
    for (const auto& entity_type_string : options_init->entityTypes()) {
      auto maybe_entity_type =
          EntityTypeForHitTestFromString(entity_type_string);

      if (maybe_entity_type) {
        result_set.insert(*maybe_entity_type);
      } else {
        DVLOG(1) << __func__ << ": entityTypes entry ignored:"
                 << IDLEnumAsString(entity_type_string);
      }
    }
  } else {
    result_set.insert(device::mojom::blink::EntityTypeForHitTest::PLANE);
  }

  DVLOG(2) << __func__ << ": result_set.size()=" << result_set.size();
  DCHECK(!result_set.empty());

  Vector<device::mojom::blink::EntityTypeForHitTest> result(result_set);

  DVLOG(2) << __func__ << ": result.size()=" << result.size();
  return result;
}

template <typename T>
HashSet<uint64_t> GetIdsOfUnusedHitTestSources(
    const HeapHashMap<uint64_t, WeakMember<T>>& id_to_hit_test_source,
    const HashSet<uint64_t>& all_ids) {
  // Gather all IDs of unused hit test sources:
  HashSet<uint64_t> unused_hit_test_source_ids;
  for (auto& id : all_ids) {
    if (!base::Contains(id_to_hit_test_source, id)) {
      unused_hit_test_source_ids.insert(id);
    }
  }

  return unused_hit_test_source_ids;
}

}  // namespace

#define DCHECK_HIT_TEST_SOURCES()                                         \
  do {                                                                    \
    DCHECK_EQ(hit_test_source_ids_.size(),                                \
              hit_test_source_ids_to_hit_test_sources_.size());           \
    DCHECK_EQ(                                                            \
        hit_test_source_for_transient_input_ids_.size(),                  \
        hit_test_source_ids_to_transient_input_hit_test_sources_.size()); \
  } while (0)

constexpr char XRSession::kNoRigidTransformSpecified[];
constexpr char XRSession::kUnableToRetrieveMatrix[];
constexpr char XRSession::kNoSpaceSpecified[];
constexpr char XRSession::kAnchorsFeatureNotSupported[];
constexpr char XRSession::kPlanesFeatureNotSupported[];
constexpr char XRSession::kDepthSensingFeatureNotSupported[];
constexpr char XRSession::kRawCameraAccessFeatureNotSupported[];
constexpr char XRSession::kCannotCancelHitTestSource[];
constexpr char XRSession::kCannotReportPoses[];

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

  void Trace(Visitor* visitor) const override {
    visitor->Trace(session_);
    ResizeObserver::Delegate::Trace(visitor);
  }

 private:
  Member<XRSession> session_;
};

XRSession::MetricsReporter::MetricsReporter(
    mojo::Remote<device::mojom::blink::XRSessionMetricsRecorder> recorder)
    : recorder_(std::move(recorder)) {}

void XRSession::MetricsReporter::ReportFeatureUsed(
    device::mojom::blink::XRSessionFeature feature) {
  using device::mojom::blink::XRSessionFeature;

  // If we've already reported using this feature, no need to report again.
  if (!reported_features_.insert(feature).is_new_entry) {
    return;
  }

  switch (feature) {
    case XRSessionFeature::REF_SPACE_VIEWER:
      recorder_->ReportFeatureUsed(XRSessionFeature::REF_SPACE_VIEWER);
      break;
    case XRSessionFeature::REF_SPACE_LOCAL:
      recorder_->ReportFeatureUsed(XRSessionFeature::REF_SPACE_LOCAL);
      break;
    case XRSessionFeature::REF_SPACE_LOCAL_FLOOR:
      recorder_->ReportFeatureUsed(XRSessionFeature::REF_SPACE_LOCAL_FLOOR);
      break;
    case XRSessionFeature::REF_SPACE_BOUNDED_FLOOR:
      recorder_->ReportFeatureUsed(XRSessionFeature::REF_SPACE_BOUNDED_FLOOR);
      break;
    case XRSessionFeature::REF_SPACE_UNBOUNDED:
      recorder_->ReportFeatureUsed(XRSessionFeature::REF_SPACE_UNBOUNDED);
      break;
    case XRSessionFeature::DOM_OVERLAY:
    case XRSessionFeature::HIT_TEST:
    case XRSessionFeature::LIGHT_ESTIMATION:
    case XRSessionFeature::ANCHORS:
    case XRSessionFeature::CAMERA_ACCESS:
    case XRSessionFeature::PLANE_DETECTION:
    case XRSessionFeature::DEPTH:
    case XRSessionFeature::IMAGE_TRACKING:
    case XRSessionFeature::HAND_INPUT:
    case XRSessionFeature::SECONDARY_VIEWS:
    case XRSessionFeature::LAYERS:
      // Not recording metrics for these features currently.
      break;
  }
}

XRDepthManager* XRSession::CreateDepthManagerIfEnabled(
    const XRSessionFeatureSet& feature_set,
    const device::mojom::blink::XRSessionDeviceConfig& device_config) {
  DVLOG(2) << __func__;

  if (!base::Contains(feature_set, device::mojom::XRSessionFeature::DEPTH)) {
    return nullptr;
  }

  if (!device_config.depth_configuration) {
    DCHECK(false) << "The session reports that depth sensing is supported but "
                     "did not report depth sensing API configuration!";
    return nullptr;
  }

  return MakeGarbageCollected<XRDepthManager>(
      base::PassKey<XRSession>{}, this, *device_config.depth_configuration);
}

XRSession::XRSession(
    XRSystem* xr,
    mojo::PendingReceiver<device::mojom::blink::XRSessionClient>
        client_receiver,
    device::mojom::blink::XRSessionMode mode,
    device::mojom::blink::XREnvironmentBlendMode environment_blend_mode,
    device::mojom::blink::XRInteractionMode interaction_mode,
    device::mojom::blink::XRSessionDeviceConfigPtr device_config,
    bool sensorless_session,
    XRSessionFeatureSet enabled_features)
    : xr_(xr),
      mode_(mode),
      environment_integration_(
          mode == device::mojom::blink::XRSessionMode::kImmersiveAr),
      enabled_features_(std::move(enabled_features)),
      plane_manager_(
          MakeGarbageCollected<XRPlaneManager>(base::PassKey<XRSession>{},
                                               this)),
      depth_manager_(
          CreateDepthManagerIfEnabled(enabled_features_, *device_config)),
      input_sources_(MakeGarbageCollected<XRInputSourceArray>()),
      client_receiver_(this, xr->GetExecutionContext()),
      input_receiver_(this, xr->GetExecutionContext()),
      callback_collection_(
          MakeGarbageCollected<XRFrameRequestCallbackCollection>(
              xr->GetExecutionContext())),
      uses_input_eventing_(device_config->uses_input_eventing),
      supports_viewport_scaling_(immersive() &&
                                 device_config->supports_viewport_scaling),
      enable_anti_aliasing_(device_config->enable_anti_aliasing),
      sensorless_session_(sensorless_session) {
  client_receiver_.Bind(
      std::move(client_receiver),
      xr->GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI));
  render_state_ = MakeGarbageCollected<XRRenderState>(immersive());
  // Ensure that frame focus is considered in the initial visibilityState.
  UpdateVisibilityState();

  // Clamp to a reasonable min/max size for the default framebuffer scale.
  recommended_framebuffer_scale_ =
      base::clamp(device_config->default_framebuffer_scale,
                  kMinDefaultFramebufferScale, kMaxDefaultFramebufferScale);

  UpdateViews(device_config->views);

  DVLOG(2) << __func__
           << ": supports_viewport_scaling_=" << supports_viewport_scaling_;

  switch (environment_blend_mode) {
    case device::mojom::blink::XREnvironmentBlendMode::kOpaque:
      blend_mode_string_ = "opaque";
      break;
    case device::mojom::blink::XREnvironmentBlendMode::kAdditive:
      blend_mode_string_ = "additive";
      break;
    case device::mojom::blink::XREnvironmentBlendMode::kAlphaBlend:
      blend_mode_string_ = "alpha-blend";
      break;
    default:
      NOTREACHED() << "Unknown environment blend mode: "
                   << environment_blend_mode;
  }

  switch (interaction_mode) {
    case device::mojom::blink::XRInteractionMode::kScreenSpace:
      interaction_mode_string_ = "screen-space";
      break;
    case device::mojom::blink::XRInteractionMode::kWorldSpace:
      interaction_mode_string_ = "world-space";
      break;
  }
}

void XRSession::SetDOMOverlayElement(Element* element) {
  DVLOG(2) << __func__ << ": element=" << element;
  DCHECK(
      enabled_features_.Contains(device::mojom::XRSessionFeature::DOM_OVERLAY));
  DCHECK(element);

  overlay_element_ = element;

  // Set up the domOverlayState attribute. This could be done lazily on first
  // access, but it's a tiny object and it's unclear if the memory that might
  // save during XR sessions is worth the code size increase to do so. This
  // should be revisited if the state gets more complex in the future.
  //
  // At this time, "screen" is the only supported DOM Overlay type.
  dom_overlay_state_ = MakeGarbageCollected<XRDOMOverlayState>(
      XRDOMOverlayState::DOMOverlayType::kScreen);
}

const String XRSession::visibilityState() const {
  switch (visibility_state_) {
    case XRVisibilityState::VISIBLE:
      return "visible";
    case XRVisibilityState::VISIBLE_BLURRED:
      return "visible-blurred";
    case XRVisibilityState::HIDDEN:
      return "hidden";
  }
}

XRAnchorSet* XRSession::TrackedAnchors() const {
  DVLOG(3) << __func__;

  if (!IsFeatureEnabled(device::mojom::XRSessionFeature::ANCHORS)) {
    return MakeGarbageCollected<XRAnchorSet>(HeapHashSet<Member<XRAnchor>>{});
  }

  HeapHashSet<Member<XRAnchor>> result;
  for (auto& anchor_id_and_anchor : anchor_ids_to_anchors_) {
    result.insert(anchor_id_and_anchor.value);
  }

  return MakeGarbageCollected<XRAnchorSet>(result);
}

bool XRSession::immersive() const {
  return mode_ == device::mojom::blink::XRSessionMode::kImmersiveVr ||
         mode_ == device::mojom::blink::XRSessionMode::kImmersiveAr;
}

ExecutionContext* XRSession::GetExecutionContext() const {
  return xr_->GetExecutionContext();
}

const AtomicString& XRSession::InterfaceName() const {
  return event_target_names::kXRSession;
}

mojo::PendingAssociatedRemote<device::mojom::blink::XRInputSourceButtonListener>
XRSession::GetInputClickListener() {
  DCHECK(!input_receiver_.is_bound());
  return input_receiver_.BindNewEndpointAndPassRemote(
      xr_->GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI));
}

void XRSession::updateRenderState(XRRenderStateInit* init,
                                  ExceptionState& exception_state) {
  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return;
  }

  if (immersive() && init->hasInlineVerticalFieldOfView()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInlineVerticalFOVNotSupported);
    return;
  }

  // Validate that any baseLayer provided was created with this session.
  if (init->hasBaseLayer() && init->baseLayer() &&
      init->baseLayer()->session() != this) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kIncompatibleLayer);
    return;
  }

  pending_render_state_.push_back(init);

  // Updating our render state may have caused us to be in a state where we
  // should be requesting frames again. Kick off a new frame request in case
  // there are any pending callbacks to flush them out.
  MaybeRequestFrame();
}

const String& XRSession::depthUsage(ExceptionState& exception_state) {
  if (!depth_manager_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kDepthSensingFeatureNotSupported);
    return g_empty_string;
  }

  return depth_manager_->depthUsage();
}

const String& XRSession::depthDataFormat(ExceptionState& exception_state) {
  if (!depth_manager_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kDepthSensingFeatureNotSupported);
    return g_empty_string;
  }

  return depth_manager_->depthDataFormat();
}

void XRSession::UpdateViews(
    const Vector<device::mojom::blink::XRViewPtr>& views) {
  if (views.empty()) {
    // If there are no views provided for this frame, keep the views we
    // currently have from the previous frame.
    return;
  }

  if (pending_views_.size() != views.size()) {
    pending_views_.resize(views.size());
  }

  for (wtf_size_t i = 0; i < views.size(); i++) {
      pending_views_[i] = views[i].Clone();
  }
}

void XRSession::UpdateStageParameters(
    uint32_t stage_parameters_id,
    const device::mojom::blink::VRStageParametersPtr& stage_parameters) {
  // Only update if the ID is different, indicating a change.
  if (stage_parameters_id_ != stage_parameters_id) {
    stage_parameters_id_ = stage_parameters_id;
    stage_parameters_ = stage_parameters.Clone();
  }
}

ScriptPromise XRSession::requestReferenceSpace(
    ScriptState* script_state,
    const String& type,
    ExceptionState& exception_state) {
  DVLOG(2) << __func__ << ": type=" << type;

  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return ScriptPromise();
  }

  device::mojom::blink::XRReferenceSpaceType requested_type =
      XRReferenceSpace::StringToReferenceSpaceType(type);

  if (sensorless_session_ &&
      requested_type != device::mojom::blink::XRReferenceSpaceType::kViewer) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kReferenceSpaceNotSupported);
    return ScriptPromise();
  }

  // If the session feature required by this reference space type is not
  // enabled, reject the session.
  auto type_as_feature = MapReferenceSpaceTypeToFeature(requested_type);
  if (!type_as_feature) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kReferenceSpaceNotSupported);
    return ScriptPromise();
  }

  // Report attempt to use this feature
  if (metrics_reporter_) {
    metrics_reporter_->ReportFeatureUsed(type_as_feature.value());
  }

  if (!IsFeatureEnabled(type_as_feature.value())) {
    DVLOG(2) << __func__ << ": feature not enabled, type=" << type;
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kReferenceSpaceNotSupported);
    return ScriptPromise();
  }

  XRReferenceSpace* reference_space = nullptr;
  switch (requested_type) {
    case device::mojom::blink::XRReferenceSpaceType::kViewer:
    case device::mojom::blink::XRReferenceSpaceType::kLocal:
    case device::mojom::blink::XRReferenceSpaceType::kLocalFloor:
      reference_space =
          MakeGarbageCollected<XRReferenceSpace>(this, requested_type);
      break;
    case device::mojom::blink::XRReferenceSpaceType::kBoundedFloor: {
      if (immersive()) {
        reference_space = MakeGarbageCollected<XRBoundedReferenceSpace>(this);
      }
      break;
    }
    case device::mojom::blink::XRReferenceSpaceType::kUnbounded:
      if (immersive()) {
        reference_space =
            MakeGarbageCollected<XRReferenceSpace>(this, requested_type);
      }
      break;
  }

  // If the above switch statement failed to assign to reference_space,
  // it's because the reference space wasn't supported by the device.
  if (!reference_space) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kReferenceSpaceNotSupported);
    return ScriptPromise();
  }

  DCHECK(reference_space);
  reference_spaces_.push_back(reference_space);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Resolve(reference_space);

  UMA_HISTOGRAM_ENUMERATION("XR.WebXR.ReferenceSpace.Succeeded",
                            requested_type);

  return promise;
}

ScriptPromise XRSession::CreateAnchorHelper(
    ScriptState* script_state,
    const gfx::Transform& native_origin_from_anchor,
    const device::mojom::blink::XRNativeOriginInformationPtr&
        native_origin_information,
    absl::optional<uint64_t> maybe_plane_id,
    ExceptionState& exception_state) {
  DVLOG(2) << __func__;

  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return ScriptPromise();
  }

  // Reject the promise if device doesn't support the anchors API.
  if (!xr_->xrEnvironmentProviderRemote()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kAnchorsNotSupportedByDevice);
    return ScriptPromise();
  }

  auto maybe_native_origin_from_anchor_pose =
      CreatePose(native_origin_from_anchor);

  if (!maybe_native_origin_from_anchor_pose) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kUnableToDecomposeMatrix);
    return ScriptPromise();
  }

  DVLOG(3) << __func__
           << ": maybe_native_origin_from_anchor_pose->orientation()= "
           << maybe_native_origin_from_anchor_pose->orientation().ToString()
           << ", maybe_native_origin_from_anchor_pose->position()= "
           << maybe_native_origin_from_anchor_pose->position().ToString();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  if (maybe_plane_id) {
    xr_->xrEnvironmentProviderRemote()->CreatePlaneAnchor(
        native_origin_information->Clone(),
        *maybe_native_origin_from_anchor_pose, *maybe_plane_id,
        resolver->WrapCallbackInScriptScope(WTF::BindOnce(
            &XRSession::OnCreateAnchorResult, WrapPersistent(this))));
  } else {
    xr_->xrEnvironmentProviderRemote()->CreateAnchor(
        native_origin_information->Clone(),
        *maybe_native_origin_from_anchor_pose,
        resolver->WrapCallbackInScriptScope(WTF::BindOnce(
            &XRSession::OnCreateAnchorResult, WrapPersistent(this))));
  }

  create_anchor_promises_.insert(resolver);

  return promise;
}

absl::optional<XRSession::ReferenceSpaceInformation>
XRSession::GetStationaryReferenceSpace() const {
  // For anchor creation, we should first attempt to use the local space as it
  // is supposed to be more stable, but if that is unavailable, we can try using
  // unbounded space. Otherwise, there's not much we can do & we have to return
  // nullopt.

  // Try to get mojo_from_local:
  auto reference_space_type = device::mojom::XRReferenceSpaceType::kLocal;
  auto mojo_from_space = GetMojoFrom(reference_space_type);

  if (!mojo_from_space) {
    // Local space is not available, try to get mojo_from_unbounded:
    reference_space_type = device::mojom::XRReferenceSpaceType::kUnbounded;
    mojo_from_space = GetMojoFrom(reference_space_type);
  }

  if (!mojo_from_space) {
    // Unbounded is also not available.
    return absl::nullopt;
  }

  ReferenceSpaceInformation result;
  result.mojo_from_space = *mojo_from_space;
  result.native_origin =
      device::mojom::blink::XRNativeOriginInformation::NewReferenceSpaceType(
          reference_space_type);
  return result;
}

void XRSession::ScheduleVideoFrameCallbacksExecution(
    ExecuteVfcCallback execute_vfc_callback) {
  vfc_execution_queue_.push_back(std::move(execute_vfc_callback));
  MaybeRequestFrame();
}

void XRSession::ExecuteVideoFrameCallbacks(double timestamp) {
  Vector<ExecuteVfcCallback> execute_vfc_callbacks;
  vfc_execution_queue_.swap(execute_vfc_callbacks);
  for (auto& callback : execute_vfc_callbacks)
    std::move(callback).Run(timestamp);
}

int XRSession::requestAnimationFrame(V8XRFrameRequestCallback* callback) {
  DVLOG(3) << __func__;

  TRACE_EVENT0("gpu", __func__);
  // Don't allow any new frame requests once the session is ended.
  if (ended_)
    return 0;

  int id = callback_collection_->RegisterCallback(callback);
  MaybeRequestFrame();
  return id;
}

void XRSession::cancelAnimationFrame(int id) {
  callback_collection_->CancelCallback(id);
}

XRInputSourceArray* XRSession::inputSources(ScriptState* script_state) const {
  if (!did_log_getInputSources_ && script_state->ContextIsValid()) {
    ukm::builders::XR_WebXR(GetExecutionContext()->UkmSourceID())
        .SetDidGetXRInputSources(1)
        .Record(LocalDOMWindow::From(script_state)->UkmRecorder());
    did_log_getInputSources_ = true;
  }

  return input_sources_;
}

ScriptPromise XRSession::requestHitTestSource(
    ScriptState* script_state,
    XRHitTestOptionsInit* options_init,
    ExceptionState& exception_state) {
  DVLOG(2) << __func__;
  DCHECK(options_init);

  if (!IsFeatureEnabled(device::mojom::XRSessionFeature::HIT_TEST)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kHitTestFeatureNotSupported);
    return {};
  }

  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return {};
  }

  if (!xr_->xrEnvironmentProviderRemote()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kHitTestNotSupportedByDevice);
    return {};
  }

  // 1. Grab the native origin from the passed in XRSpace.
  device::mojom::blink::XRNativeOriginInformationPtr maybe_native_origin =
      options_init && options_init->hasSpace()
          ? options_init->space()->NativeOrigin()
          : nullptr;

  if (!maybe_native_origin) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kUnableToRetrieveNativeOrigin);
    return {};
  }

  // 2. Convert the XRRay to be expressed in terms of passed in XRSpace. This
  // should only matter for spaces whose transforms are not fully known on the
  // device (for example any space containing origin-offset).
  // Null checks not needed since native origin wouldn't be set if options_init
  // or space() were null.
  gfx::Transform native_from_offset =
      options_init->space()->NativeFromOffsetMatrix();

  if (RuntimeEnabledFeatures::WebXRHitTestEntityTypesEnabled() &&
      options_init->hasEntityTypes() && options_init->entityTypes().empty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kEntityTypesNotSpecified);
    return {};
  }

  auto entity_types = GetEntityTypesForHitTest(options_init);

  DVLOG(3) << __func__
           << ": native_from_offset = " << native_from_offset.ToString();

  // Transformation from passed in pose to |space|.

  XRRay* offsetRay = options_init && options_init->hasOffsetRay()
                         ? options_init->offsetRay()
                         : MakeGarbageCollected<XRRay>();
  auto space_from_ray = offsetRay->RawMatrix();
  auto origin_from_ray = native_from_offset * space_from_ray;

  DVLOG(3) << __func__ << ": space_from_ray = " << space_from_ray.ToString();
  DVLOG(3) << __func__ << ": origin_from_ray = " << origin_from_ray.ToString();

  device::mojom::blink::XRRayPtr ray_mojo = device::mojom::blink::XRRay::New();

  ray_mojo->origin = origin_from_ray.MapPoint({0, 0, 0});

  // Zero out the translation of origin_from_ray matrix to correctly map a 3D
  // vector.
  gfx::Vector3dF translation = origin_from_ray.To3dTranslation();
  origin_from_ray.Translate3d(-translation.x(), -translation.y(),
                              -translation.z());

  ray_mojo->direction = origin_from_ray.MapPoint({0, 0, -1}).OffsetFromOrigin();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  xr_->xrEnvironmentProviderRemote()->SubscribeToHitTest(
      maybe_native_origin->Clone(), entity_types, std::move(ray_mojo),
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(
          &XRSession::OnSubscribeToHitTestResult, WrapPersistent(this))));
  request_hit_test_source_promises_.insert(resolver);

  return promise;
}

ScriptPromise XRSession::requestHitTestSourceForTransientInput(
    ScriptState* script_state,
    XRTransientInputHitTestOptionsInit* options_init,
    ExceptionState& exception_state) {
  DVLOG(2) << __func__;
  DCHECK(options_init);

  if (!IsFeatureEnabled(device::mojom::XRSessionFeature::HIT_TEST)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kHitTestFeatureNotSupported);
    return {};
  }

  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return {};
  }

  if (!xr_->xrEnvironmentProviderRemote()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kHitTestNotSupportedByDevice);
    return {};
  }

  if (RuntimeEnabledFeatures::WebXRHitTestEntityTypesEnabled() &&
      options_init->hasEntityTypes() && options_init->entityTypes().empty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kEntityTypesNotSpecified);
    return {};
  }

  auto entity_types = GetEntityTypesForHitTest(options_init);

  XRRay* offsetRay = options_init && options_init->hasOffsetRay()
                         ? options_init->offsetRay()
                         : MakeGarbageCollected<XRRay>();

  device::mojom::blink::XRRayPtr ray_mojo = device::mojom::blink::XRRay::New();
  ray_mojo->origin = {static_cast<float>(offsetRay->origin()->x()),
                      static_cast<float>(offsetRay->origin()->y()),
                      static_cast<float>(offsetRay->origin()->z())};
  ray_mojo->direction = {static_cast<float>(offsetRay->direction()->x()),
                         static_cast<float>(offsetRay->direction()->y()),
                         static_cast<float>(offsetRay->direction()->z())};

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  xr_->xrEnvironmentProviderRemote()->SubscribeToHitTestForTransientInput(
      options_init->profile(), entity_types, std::move(ray_mojo),
      resolver->WrapCallbackInScriptScope(
          WTF::BindOnce(&XRSession::OnSubscribeToHitTestForTransientInputResult,
                        WrapPersistent(this))));
  request_hit_test_source_promises_.insert(resolver);

  return promise;
}

void XRSession::OnSubscribeToHitTestResult(
    ScriptPromiseResolver* resolver,
    device::mojom::SubscribeToHitTestResult result,
    uint64_t subscription_id) {
  DVLOG(2) << __func__ << ": result=" << result
           << ", subscription_id=" << subscription_id;

  DCHECK(request_hit_test_source_promises_.Contains(resolver));
  request_hit_test_source_promises_.erase(resolver);

  if (result != device::mojom::SubscribeToHitTestResult::SUCCESS) {
    resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                     kHitTestSubscriptionFailed);
    return;
  }

  XRHitTestSource* hit_test_source =
      MakeGarbageCollected<XRHitTestSource>(subscription_id, this);

  hit_test_source_ids_to_hit_test_sources_.insert(subscription_id,
                                                  hit_test_source);
  hit_test_source_ids_.insert(subscription_id);

  resolver->Resolve(hit_test_source);
}

void XRSession::OnSubscribeToHitTestForTransientInputResult(
    ScriptPromiseResolver* resolver,
    device::mojom::SubscribeToHitTestResult result,
    uint64_t subscription_id) {
  DVLOG(2) << __func__ << ": result=" << result
           << ", subscription_id=" << subscription_id;

  DCHECK(request_hit_test_source_promises_.Contains(resolver));
  request_hit_test_source_promises_.erase(resolver);

  if (result != device::mojom::SubscribeToHitTestResult::SUCCESS) {
    resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                     kHitTestSubscriptionFailed);
    return;
  }

  XRTransientInputHitTestSource* hit_test_source =
      MakeGarbageCollected<XRTransientInputHitTestSource>(subscription_id,
                                                          this);

  hit_test_source_ids_to_transient_input_hit_test_sources_.insert(
      subscription_id, hit_test_source);
  hit_test_source_for_transient_input_ids_.insert(subscription_id);

  resolver->Resolve(hit_test_source);
}

void XRSession::OnCreateAnchorResult(ScriptPromiseResolver* resolver,
                                     device::mojom::CreateAnchorResult result,
                                     uint64_t id) {
  DVLOG(2) << __func__ << ": result=" << result << ", id=" << id;

  DCHECK(create_anchor_promises_.Contains(resolver));
  create_anchor_promises_.erase(resolver);

  if (result == device::mojom::CreateAnchorResult::SUCCESS) {
    // Anchor was created successfully on the device. Subsequent frame update
    // must contain newly created anchor data.
    anchor_ids_to_pending_anchor_promises_.insert(id, resolver);
  } else {
    resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                     kAnchorCreationFailed);
  }
}

void XRSession::OnEnvironmentProviderCreated() {
  EnsureEnvironmentErrorHandler();
}

void XRSession::EnsureEnvironmentErrorHandler() {
  // Install error handler on environment provider to ensure that we get
  // notified so that we can clean up all relevant pending promises.
  if (!environment_error_handler_subscribed_ &&
      xr_->xrEnvironmentProviderRemote()) {
    environment_error_handler_subscribed_ = true;
    xr_->AddEnvironmentProviderErrorHandler(WTF::BindOnce(
        &XRSession::OnEnvironmentProviderError, WrapWeakPersistent(this)));
  }
}

void XRSession::OnEnvironmentProviderError() {
  HeapHashSet<Member<ScriptPromiseResolver>> create_anchor_promises;
  create_anchor_promises_.swap(create_anchor_promises);
  for (ScriptPromiseResolver* resolver : create_anchor_promises) {
    ScriptState* resolver_script_state = resolver->GetScriptState();
    if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                       resolver_script_state)) {
      continue;
    }
    ScriptState::Scope script_state_scope(resolver_script_state);
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     kDeviceDisconnected);
  }

  HeapHashSet<Member<ScriptPromiseResolver>> request_hit_test_source_promises;
  request_hit_test_source_promises_.swap(request_hit_test_source_promises);
  for (ScriptPromiseResolver* resolver : request_hit_test_source_promises) {
    ScriptState* resolver_script_state = resolver->GetScriptState();
    if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                       resolver_script_state)) {
      continue;
    }
    ScriptState::Scope script_state_scope(resolver_script_state);
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     kDeviceDisconnected);
  }

  HeapVector<Member<ScriptPromiseResolver>> image_score_promises;
  image_scores_resolvers_.swap(image_score_promises);
  for (ScriptPromiseResolver* resolver : image_score_promises) {
    ScriptState* resolver_script_state = resolver->GetScriptState();
    if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                       resolver_script_state)) {
      continue;
    }
    ScriptState::Scope script_state_scope(resolver_script_state);
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     kDeviceDisconnected);
  }
}

void XRSession::ProcessAnchorsData(
    const device::mojom::blink::XRAnchorsData* tracked_anchors_data,
    double timestamp) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("xr.debug"), __func__);

  if (!tracked_anchors_data) {
    DVLOG(3) << __func__ << ": tracked_anchors_data is null";

    // We have received a nullptr. Clear stored anchors.
    // The device can send either null or empty data - in both cases, it means
    // that there are no anchors available.
    anchor_ids_to_anchors_.clear();
    return;
  }

  TRACE_COUNTER2("xr", "Anchor statistics", "All anchors",
                 tracked_anchors_data->all_anchors_ids.size(),
                 "Updated anchors",
                 tracked_anchors_data->updated_anchors_data.size());

  DVLOG(3) << __func__ << ": updated anchors size="
           << tracked_anchors_data->updated_anchors_data.size()
           << ", all anchors size="
           << tracked_anchors_data->all_anchors_ids.size();

  HeapHashMap<uint64_t, Member<XRAnchor>> updated_anchors;

  // First, process all anchors that had their information updated (new anchors
  // are also processed here).
  for (const auto& anchor : tracked_anchors_data->updated_anchors_data) {
    DCHECK(anchor);

    auto it = anchor_ids_to_anchors_.find(anchor->id);
    if (it != anchor_ids_to_anchors_.end()) {
      updated_anchors.insert(anchor->id, it->value);
      it->value->Update(*anchor);
    } else {
      DVLOG(3) << __func__ << ": processing newly created anchor, anchor->id="
               << anchor->id;

      auto resolver_it =
          anchor_ids_to_pending_anchor_promises_.find(anchor->id);
      if (resolver_it == anchor_ids_to_pending_anchor_promises_.end()) {
        DCHECK(false)
            << "Newly created anchor must have a corresponding resolver!";
        continue;
      }

      XRAnchor* xr_anchor =
          MakeGarbageCollected<XRAnchor>(anchor->id, this, *anchor);
      resolver_it->value->Resolve(xr_anchor);
      anchor_ids_to_pending_anchor_promises_.erase(resolver_it);

      updated_anchors.insert(anchor->id, xr_anchor);
    }
  }

  // Then, copy over the anchors that were not updated but are still present.
  for (const auto& anchor_id : tracked_anchors_data->all_anchors_ids) {
    auto it_updated = updated_anchors.find(anchor_id);

    // If the anchor was already updated, there is nothing to do as it was
    // already moved to |updated_anchors|. Otherwise just copy it over as-is.
    if (it_updated == updated_anchors.end()) {
      auto it = anchor_ids_to_anchors_.find(anchor_id);
      DCHECK(it != anchor_ids_to_anchors_.end());
      updated_anchors.insert(anchor_id, it->value);
    }
  }

  DVLOG(3) << __func__
           << ": anchor count before update=" << anchor_ids_to_anchors_.size()
           << ", after update=" << updated_anchors.size();

  anchor_ids_to_anchors_.swap(updated_anchors);

  DCHECK(anchor_ids_to_pending_anchor_promises_.empty())
      << "All anchors should be updated in the frame in which they were "
         "created, got "
      << anchor_ids_to_pending_anchor_promises_.size()
      << " anchors that have not been updated";
}

XRPlaneSet* XRSession::GetDetectedPlanes() const {
  return plane_manager_->GetDetectedPlanes();
}

void XRSession::CleanUpUnusedHitTestSources() {
  auto unused_hit_test_source_ids = GetIdsOfUnusedHitTestSources(
      hit_test_source_ids_to_hit_test_sources_, hit_test_source_ids_);
  for (auto id : unused_hit_test_source_ids) {
    xr_->xrEnvironmentProviderRemote()->UnsubscribeFromHitTest(id);
  }

  hit_test_source_ids_.RemoveAll(unused_hit_test_source_ids);

  auto unused_transient_hit_source_ids = GetIdsOfUnusedHitTestSources(
      hit_test_source_ids_to_transient_input_hit_test_sources_,
      hit_test_source_for_transient_input_ids_);
  for (auto id : unused_transient_hit_source_ids) {
    xr_->xrEnvironmentProviderRemote()->UnsubscribeFromHitTest(id);
  }

  hit_test_source_for_transient_input_ids_.RemoveAll(
      unused_transient_hit_source_ids);

  DCHECK_HIT_TEST_SOURCES();

  DVLOG(3) << __func__ << ": Number of active hit test sources: "
           << hit_test_source_ids_.size()
           << ", number of active hit test sources for transient input: "
           << hit_test_source_for_transient_input_ids_.size();
}

void XRSession::ProcessHitTestData(
    const device::mojom::blink::XRHitTestSubscriptionResultsData*
        hit_test_subscriptions_data) {
  DVLOG(2) << __func__;

  // Application's code can just drop references to hit test sources w/o first
  // canceling them - ensure that we communicate that the subscriptions are no
  // longer present to the device.
  CleanUpUnusedHitTestSources();

  if (hit_test_subscriptions_data) {
    // We have received hit test results for hit test subscriptions - process
    // each result and notify its corresponding hit test source about new
    // results for the current frame.
    DVLOG(3) << __func__ << ": hit_test_subscriptions_data->results.size()="
             << hit_test_subscriptions_data->results.size() << ", "
             << "hit_test_subscriptions_data->transient_input_results.size()="
             << hit_test_subscriptions_data->transient_input_results.size();

    for (auto& hit_test_subscription_data :
         hit_test_subscriptions_data->results) {
      auto it = hit_test_source_ids_to_hit_test_sources_.find(
          hit_test_subscription_data->subscription_id);
      if (it != hit_test_source_ids_to_hit_test_sources_.end()) {
        it->value->Update(hit_test_subscription_data->hit_test_results);
      }
    }

    for (auto& transient_input_hit_test_subscription_data :
         hit_test_subscriptions_data->transient_input_results) {
      auto it = hit_test_source_ids_to_transient_input_hit_test_sources_.find(
          transient_input_hit_test_subscription_data->subscription_id);
      if (it !=
          hit_test_source_ids_to_transient_input_hit_test_sources_.end()) {
        it->value->Update(transient_input_hit_test_subscription_data
                              ->input_source_id_to_hit_test_results,
                          input_sources_);
      }
    }
  } else {
    DVLOG(3) << __func__ << ": hit_test_subscriptions_data unavailable";

    // We have not received hit test results for any of the hit test
    // subscriptions in the current frame - clean up the results on all hit test
    // source objects.
    for (auto& subscription_id_and_hit_test_source :
         hit_test_source_ids_to_hit_test_sources_) {
      subscription_id_and_hit_test_source.value->Update({});
    }

    for (auto& subscription_id_and_transient_input_hit_test_source :
         hit_test_source_ids_to_transient_input_hit_test_sources_) {
      subscription_id_and_transient_input_hit_test_source.value->Update(
          {}, nullptr);
    }
  }
}

XRCPUDepthInformation* XRSession::GetCpuDepthInformation(
    const XRFrame* xr_frame,
    ExceptionState& exception_state) const {
  if (!depth_manager_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kDepthSensingFeatureNotSupported);
    return nullptr;
  }

  return depth_manager_->GetCpuDepthInformation(xr_frame, exception_state);
}

XRWebGLDepthInformation* XRSession::GetWebGLDepthInformation(
    const XRFrame* xr_frame,
    ExceptionState& exception_state) const {
  if (!depth_manager_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kDepthSensingFeatureNotSupported);
    return nullptr;
  }

  return depth_manager_->GetWebGLDepthInformation(xr_frame, exception_state);
}

ScriptPromise XRSession::requestLightProbe(ScriptState* script_state,
                                           XRLightProbeInit* light_probe_init,
                                           ExceptionState& exception_state) {
  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return ScriptPromise();
  }

  if (!IsFeatureEnabled(device::mojom::XRSessionFeature::LIGHT_ESTIMATION)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kLightEstimationFeatureNotSupported);
    return ScriptPromise();
  }

  if (light_probe_init->reflectionFormat() != "srgba8" &&
      light_probe_init->reflectionFormat() != "rgba16f") {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Reflection format \"" +
            IDLEnumAsString(light_probe_init->reflectionFormat()) +
            "\" not supported.");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!world_light_probe_) {
    // TODO(https://crbug.com/1147569): This is problematic because it means the
    // first reflection format that gets requested is the only one that can be
    // returned.
    world_light_probe_ =
        MakeGarbageCollected<XRLightProbe>(this, light_probe_init);
  }

  resolver->Resolve(world_light_probe_);

  return promise;
}

ScriptPromise XRSession::end(ScriptState* script_state,
                             ExceptionState& exception_state) {
  DVLOG(2) << __func__;
  // Don't allow a session to end twice.
  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return ScriptPromise();
  }

  ForceEnd(ShutdownPolicy::kWaitForResponse);

  end_session_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = end_session_resolver_->Promise();

  DVLOG(1) << __func__ << ": returning promise";
  return promise;
}

void XRSession::ForceEnd(ShutdownPolicy shutdown_policy) {
  bool wait_for_response;
  switch (shutdown_policy) {
    case ShutdownPolicy::kWaitForResponse:
      wait_for_response = true;
      break;
    case ShutdownPolicy::kImmediate:
      wait_for_response = false;
      break;
  }

  DVLOG(3) << __func__ << ": wait_for_response=" << wait_for_response
           << " ended_=" << ended_
           << " waiting_for_shutdown_=" << waiting_for_shutdown_;

  // If we've already ended, then just abort.  Since this is called only by C++
  // code, and predominantly just to ensure that the session is shut down, this
  // is fine.
  if (ended_) {
    // If we're currently waiting for an OnExitPresent, but are told not
    // to expect that anymore (i.e. due to a connection error), proceed
    // to full shutdown now.
    if (!wait_for_response && waiting_for_shutdown_) {
      HandleShutdown();
    }
    return;
  }

  // Detach this session from the XR system.
  ended_ = true;
  pending_frame_ = false;

  for (unsigned i = 0; i < input_sources_->length(); i++) {
    auto* input_source = (*input_sources_)[i];
    input_source->OnRemoved();
  }

  input_sources_ = nullptr;

  if (canvas_input_provider_) {
    canvas_input_provider_->Stop();
    canvas_input_provider_ = nullptr;
  }

  xr_->ExitPresent(
      WTF::BindOnce(&XRSession::OnExitPresent, WrapWeakPersistent(this)));

  if (wait_for_response) {
    waiting_for_shutdown_ = true;
  } else {
    HandleShutdown();
  }
}

void XRSession::HandleShutdown() {
  DVLOG(2) << __func__;
  DCHECK(ended_);
  waiting_for_shutdown_ = false;

  if (xr_->IsContextDestroyed()) {
    // If this is being called due to the context being destroyed,
    // it's illegal to run JavaScript code, so we cannot emit an
    // end event or resolve the stored promise. Don't bother calling
    // the frame provider's OnSessionEnded, that's being disposed of
    // also.
    DVLOG(3) << __func__ << ": Context destroyed";
    if (end_session_resolver_) {
      end_session_resolver_->Detach();
      end_session_resolver_ = nullptr;
    }
    return;
  }

  // Notify the frame provider that we've ended. Do this before notifying the
  // page, so that if the page tries (and is able to) create a session within
  // either the promise or the event callback, it's not blocked by the frame
  // provider thinking there's still an active immersive session.
  xr_->frameProvider()->OnSessionEnded(this);

  if (end_session_resolver_) {
    DVLOG(3) << __func__ << ": Resolving end_session_resolver_";
    end_session_resolver_->Resolve();
    end_session_resolver_ = nullptr;
  }

  DispatchEvent(*XRSessionEvent::Create(event_type_names::kEnd, this));
  DVLOG(3) << __func__ << ": session end event dispatched";

  // Now that we've notified the page that we've ended, try to restart the non-
  // immersive frame loop. Note that if the page was able to request a new
  // session in the end event, this may be a no-op.
  xr_->frameProvider()->RestartNonImmersiveFrameLoop();
}

double XRSession::NativeFramebufferScale() const {
  if (immersive()) {
    DCHECK(recommended_framebuffer_scale_);

    // Return the inverse of the recommended scale, since that's what we'll need
    // to multiply the recommended size by to get back to the native size.
    return 1.0 / recommended_framebuffer_scale_;
  }
  return 1.0;
}

double XRSession::RecommendedFramebufferScale() const {
  return recommended_framebuffer_scale_;
}

gfx::SizeF XRSession::RecommendedFramebufferSize() const {
  if (!immersive()) {
    return gfx::SizeF(OutputCanvasSize());
  }

  float scale = recommended_framebuffer_scale_;
  float width = 0;
  float height = 0;

  // For the moment, concatenate all the views into a big strip.
  // Won't scale well for displays that use more than a stereo pair.
  for (const auto& view : pending_views_) {
    width += view->viewport.width();
    height = std::max<float>(height, view->viewport.height());
  }

  return gfx::SizeF(width * scale, height * scale);
}

gfx::Size XRSession::OutputCanvasSize() const {
  if (!render_state_->output_canvas()) {
    return gfx::Size();
  }

  return gfx::Size(output_width_, output_height_);
}

void XRSession::OnFocusChanged() {
  UpdateVisibilityState();
}

void XRSession::OnVisibilityStateChanged(XRVisibilityState visibility_state) {
  // TODO(crbug.com/1002742): Until some ambiguities in the spec are cleared up,
  // force "visible-blurred" states from the device to report as "hidden"
  if (visibility_state == XRVisibilityState::VISIBLE_BLURRED) {
    visibility_state = XRVisibilityState::HIDDEN;
  }

  if (device_visibility_state_ != visibility_state) {
    device_visibility_state_ = visibility_state;
    UpdateVisibilityState();
  }
}

// The ultimate visibility state of the session is a combination of the devices
// reported visibility state and, for inline sessions, the frame focus, which
// will override the device visibility to "hidden" if the frame is not currently
// focused.
void XRSession::UpdateVisibilityState() {
  // Don't need to track the visibility state if the session has ended.
  if (ended_) {
    return;
  }

  XRVisibilityState state = device_visibility_state_;

  // The WebXR spec requires that if our document is not focused, that we don't
  // hand out real poses. For immersive sessions, we have to rely on the device
  // to tell us it's visibility state, as some runtimes (WMR) put focus in the
  // headset, and thus we cannot rely on Document Focus state. This is fine
  // because while the runtime reports us as focused the content owned by the
  // session should be focued, which is owned by the document. For inline, we
  // can and must rely on frame focus.
  if (!immersive() && !xr_->IsFrameFocused()) {
    state = XRVisibilityState::HIDDEN;
  }

  if (visibility_state_ != state) {
    visibility_state_ = state;

    // If the visibility state was changed to something other than hidden, we
    // may be able to restart the frame loop.
    MaybeRequestFrame();

    DispatchEvent(
        *XRSessionEvent::Create(event_type_names::kVisibilitychange, this));
  }
}

void XRSession::MaybeRequestFrame() {
  bool will_have_base_layer = !!render_state_->baseLayer();
  for (const auto& init : pending_render_state_) {
    if (init->hasBaseLayer()) {
      will_have_base_layer = !!init->baseLayer();
    }
  }

  // A page will not be allowed to get frames if its visibility state is hidden.
  bool page_allowed_frames = visibility_state_ != XRVisibilityState::HIDDEN;

  // A page is configured properly if it will have a base layer when the frame
  // callback gets resolved.
  bool page_configured_properly = will_have_base_layer;

  // If we have an outstanding callback registered, then we know that the page
  // actually wants frames.
  bool page_wants_frame =
      !callback_collection_->IsEmpty() || !vfc_execution_queue_.empty();

  // A page can process frames if it has its appropriate base layer set and has
  // indicated that it actually wants frames.
  bool page_can_process_frames = page_configured_properly && page_wants_frame;

  // We consider frames to be throttled if the page is not allowed frames, but
  // otherwise would be able to receive them. Therefore, if the page isn't in a
  // state to process frames, it doesn't matter if we are throttling it, any
  // "stalls" should be attributed to the page being poorly behaved.
  bool frames_throttled = page_can_process_frames && !page_allowed_frames;

  // If our throttled state has changed, notify anyone who may care
  if (frames_throttled_ != frames_throttled) {
    frames_throttled_ = frames_throttled;
    xr_->SetFramesThrottled(this, frames_throttled_);
  }

  // We can request a frame if we don't have one already pending, the page is
  // allowed to request frames, and the page is set up to properly handle frames
  // and wants one.
  bool request_frame =
      !pending_frame_ && page_allowed_frames && page_can_process_frames;
  if (request_frame) {
    xr_->frameProvider()->RequestFrame(this);
    pending_frame_ = true;
  }
}

void XRSession::DetachOutputCanvas(HTMLCanvasElement* canvas) {
  if (!canvas)
    return;

  // Remove anything in this session observing the given output canvas.
  if (resize_observer_) {
    resize_observer_->unobserve(canvas);
  }

  if (canvas_input_provider_ && canvas_input_provider_->canvas() == canvas) {
    canvas_input_provider_->Stop();
    canvas_input_provider_ = nullptr;
  }
}

void XRSession::ApplyPendingRenderState() {
  DCHECK(!prev_base_layer_);
  if (pending_render_state_.size() > 0) {
    prev_base_layer_ = render_state_->baseLayer();
    HTMLCanvasElement* prev_ouput_canvas = render_state_->output_canvas();

    // Loop through each pending render state and apply it to the active one.
    for (auto& init : pending_render_state_) {
      render_state_->Update(init);
    }
    pending_render_state_.clear();

    // If this is an inline session and the base layer has changed, give it an
    // opportunity to update it's drawing buffer size.
    if (!immersive() && render_state_->baseLayer() &&
        render_state_->baseLayer() != prev_base_layer_) {
      render_state_->baseLayer()->OnResize();
    }

    // If the output canvas changed, remove listeners from the old one and add
    // listeners to the new one as appropriate.
    if (prev_ouput_canvas != render_state_->output_canvas()) {
      // Remove anything observing the previous canvas.
      if (prev_ouput_canvas) {
        DetachOutputCanvas(prev_ouput_canvas);
      }

      // Monitor the new canvas for resize/input events.
      HTMLCanvasElement* canvas = render_state_->output_canvas();
      if (canvas) {
        if (!resize_observer_) {
          resize_observer_ = ResizeObserver::Create(
              canvas->GetDocument().domWindow(),
              MakeGarbageCollected<XRSessionResizeObserverDelegate>(this));
        }
        resize_observer_->observe(canvas);

        // Begin processing input events on the output context's canvas.
        if (!immersive()) {
          canvas_input_provider_ =
              MakeGarbageCollected<XRCanvasInputProvider>(this, canvas);
        }

        // Get the new canvas dimensions
        UpdateCanvasDimensions(canvas);
      }
    }
  }
}

void XRSession::UpdatePresentationFrameState(
    double timestamp,
    const device::mojom::blink::VRPosePtr& mojo_from_viewer_pose,
    const device::mojom::blink::XRFrameDataPtr& frame_data,
    int16_t frame_id,
    bool emulated_position) {
  TRACE_EVENT0("gpu", __func__);
  DVLOG(2) << __func__ << " : frame_data valid? " << (frame_data ? true : false)
           << ", emulated_position=" << emulated_position;
  // Don't process any outstanding frames once the session is ended.
  if (ended_)
    return;

  if (frame_data) {
    // Views need to be updated first, since views() creates a new set of views.
    UpdateViews(frame_data->views);

    // Apply dynamic viewport scaling if available.
    if (supports_viewport_scaling_) {
      float gpu_load = frame_data->rendering_time_ratio;
      absl::optional<double> scale = absl::nullopt;
      if (gpu_load > 0.0f) {
        if (!viewport_scaler_) {
          // Lazily create an instance of the viewport scaler on first use.
          viewport_scaler_ = std::make_unique<XRSessionViewportScaler>();
        }

        viewport_scaler_->UpdateRenderingTimeRatio(gpu_load);
        scale = viewport_scaler_->Scale();
        DVLOG(3) << __func__ << ": gpu_load=" << gpu_load
                 << " scale=" << *scale;
      }
      for (XRViewData* view : views()) {
        view->SetRecommendedViewportScale(scale);
      }
    }
  }

  mojo_from_viewer_ = getPoseMatrix(mojo_from_viewer_pose);
  DVLOG(2) << __func__ << " : mojo_from_viewer_ valid? "
           << (mojo_from_viewer_ ? true : false);

  emulated_position_ = emulated_position;

  // Process XR input sources
  if (frame_data) {
    base::span<const device::mojom::blink::XRInputSourceStatePtr> input_states;
    if (frame_data->input_state.has_value())
      input_states = frame_data->input_state.value();

    OnInputStateChangeInternal(frame_id, input_states);

    // World understanding includes hit testing for transient input sources, and
    // these sources may have been hidden when touching DOM Overlay content
    // that's inside cross-origin iframes. Since hit test subscriptions only
    // happen for existing input_sources_ entries, these touches will not
    // generate hit test results. For this to work, this step must happen
    // after OnInputStateChangeInternal which updated input sources.
    UpdateWorldUnderstandingStateForFrame(timestamp, frame_data);

    // If this session uses input eventing, XR select events are handled via
    // OnButtonEvent, so they need to be ignored here to avoid duplicate events.
    if (!uses_input_eventing_) {
      ProcessInputSourceEvents(input_states);
    }
  } else {
    UpdateWorldUnderstandingStateForFrame(timestamp, frame_data);
  }
}

ScriptPromise XRSession::getTrackedImageScores(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DVLOG(3) << __func__;
  if (ended_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionEnded);
    return ScriptPromise();
  }

  if (!IsFeatureEnabled(device::mojom::XRSessionFeature::IMAGE_TRACKING)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kImageTrackingFeatureNotSupported);
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  if (tracked_image_scores_available_) {
    DVLOG(3) << __func__ << ": returning existing results";
    resolver->Resolve(tracked_image_scores_);
  } else {
    DVLOG(3) << __func__ << ": storing promise";
    image_scores_resolvers_.push_back(resolver);
  }

  return promise;
}

void XRSession::ProcessTrackedImagesData(
    const device::mojom::blink::XRTrackedImagesData* images_data) {
  DVLOG(3) << __func__;
  frame_tracked_images_.clear();

  if (!images_data) {
    return;
  }

  for (const auto& image : images_data->images_data) {
    DVLOG(3) << __func__ << ": image index=" << image->index;
    XRImageTrackingResult* result =
        MakeGarbageCollected<XRImageTrackingResult>(this, *image);
    frame_tracked_images_.push_back(result);
  }

  if (images_data->image_trackable_scores) {
    DVLOG(3) << ": got image_trackable_scores";
    DCHECK(!tracked_image_scores_available_);
    auto& scores = images_data->image_trackable_scores.value();
    for (WTF::wtf_size_t index = 0; index < scores.size(); ++index) {
      tracked_image_scores_.push_back(scores[index] ? "trackable"
                                                    : "untrackable");
      DVLOG(3) << __func__ << ": score[" << index
               << "]=" << tracked_image_scores_[index];
    }
    HeapVector<Member<ScriptPromiseResolver>> image_score_promises;
    image_scores_resolvers_.swap(image_score_promises);
    for (ScriptPromiseResolver* resolver : image_score_promises) {
      DVLOG(3) << __func__ << ": resolving promise";
      resolver->Resolve(tracked_image_scores_);
    }
    tracked_image_scores_available_ = true;
  }
}

HeapVector<Member<XRImageTrackingResult>> XRSession::ImageTrackingResults(
    ExceptionState& exception_state) {
  if (!IsFeatureEnabled(device::mojom::XRSessionFeature::IMAGE_TRACKING)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      kImageTrackingFeatureNotSupported);
    return {};
  }

  return frame_tracked_images_;
}

void XRSession::UpdateWorldUnderstandingStateForFrame(
    double timestamp,
    const device::mojom::blink::XRFrameDataPtr& frame_data) {
  // Update objects that might change on per-frame basis.
  if (frame_data) {
    plane_manager_->ProcessPlaneInformation(
        frame_data->detected_planes_data.get(), timestamp);
    ProcessAnchorsData(frame_data->anchors_data.get(), timestamp);
    ProcessHitTestData(frame_data->hit_test_subscription_results.get());

    if (depth_manager_) {
      depth_manager_->ProcessDepthInformation(
          std::move(frame_data->depth_data));
    }

    ProcessTrackedImagesData(frame_data->tracked_images.get());

    const device::mojom::blink::XRLightEstimationData* light_data =
        frame_data->light_estimation_data.get();
    if (world_light_probe_ && light_data) {
      world_light_probe_->ProcessLightEstimationData(light_data, timestamp);
    }

    camera_image_size_ = absl::nullopt;
    if (frame_data->camera_image_size.has_value()) {
      // Let's store the camera image size. The texture ID will be filled out on
      // the XRWebGLLayer by the session once the frame starts
      // (in XRSession::OnFrame()).
      camera_image_size_ = frame_data->camera_image_size;
    }
  } else {
    plane_manager_->ProcessPlaneInformation(nullptr, timestamp);
    ProcessAnchorsData(nullptr, timestamp);
    ProcessHitTestData(nullptr);

    if (depth_manager_) {
      depth_manager_->ProcessDepthInformation(nullptr);
    }

    ProcessTrackedImagesData(nullptr);

    if (world_light_probe_) {
      world_light_probe_->ProcessLightEstimationData(nullptr, timestamp);
    }

    camera_image_size_ = absl::nullopt;
  }
}

bool XRSession::IsFeatureEnabled(
    device::mojom::XRSessionFeature feature) const {
  return enabled_features_.Contains(feature);
}

void XRSession::SetMetricsReporter(std::unique_ptr<MetricsReporter> reporter) {
  DCHECK(!metrics_reporter_);
  metrics_reporter_ = std::move(reporter);
}

void XRSession::OnFrame(
    double timestamp,
    const absl::optional<gpu::MailboxHolder>& output_mailbox_holder,
    const absl::optional<gpu::MailboxHolder>& camera_image_mailbox_holder) {
  TRACE_EVENT0("gpu", __func__);
  DVLOG(2) << __func__ << ": ended_=" << ended_
           << ", pending_frame_=" << pending_frame_;
  // Don't process any outstanding frames once the session is ended.
  if (ended_)
    return;

  // If there are pending render state changes, apply them now.
  prev_base_layer_ = nullptr;
  ApplyPendingRenderState();

  if (pending_frame_) {
    pending_frame_ = false;

    // Don't allow frames to be processed if there's no layers attached to the
    // session. That would allow tracking with no associated visuals.
    XRWebGLLayer* frame_base_layer = render_state_->baseLayer();
    if (!frame_base_layer) {
      DVLOG(2) << __func__ << ": frame_base_layer not present";

      // If we previously had a frame base layer, we need to still attempt to
      // submit a frame back to the runtime, as all "GetFrameData" calls need a
      // matching submit.
      if (prev_base_layer_) {
        DVLOG(2) << __func__
                 << ": prev_base_layer_ is valid, submitting frame to it";
        prev_base_layer_->OnFrameStart(output_mailbox_holder,
                                       camera_image_mailbox_holder);
        prev_base_layer_->OnFrameEnd();
        prev_base_layer_ = nullptr;
      }
      return;
    }

    // Don't allow frames to be processed if an inline session doesn't have an
    // output canvas.
    if (!immersive() && !render_state_->output_canvas()) {
      DVLOG(2) << __func__
               << ": frames are not to be processed if an inline session "
                  "doesn't have an output canvas";
      return;
    }

    frame_base_layer->OnFrameStart(output_mailbox_holder,
                                   camera_image_mailbox_holder);

    // Don't allow frames to be processed if the session's visibility state is
    // "hidden".
    if (visibility_state_ == XRVisibilityState::HIDDEN) {
      DVLOG(2) << __func__
               << ": frames to be processed if the session's visibility state "
                  "is \"hidden\"";
      // If the frame is skipped because of the visibility state,
      // make sure we end the frame anyway.
      frame_base_layer->OnFrameEnd();
      return;
    }

    XRFrame* presentation_frame = CreatePresentationFrame(true);

    views_updated_this_frame_ = false;

    // If the device has opted in, mark the viewports as modifiable
    // at the start of an animation frame:
    // https://immersive-web.github.io/webxr/#ref-for-view-viewport-modifiable
    if (supports_viewport_scaling_) {
      for (XRViewData* view : views()) {
        view->SetViewportModifiable(true);
      }
    }

    // Resolve the queued requestAnimationFrame callbacks. All XR rendering will
    // happen within these calls. resolving_frame_ will be true for the duration
    // of the callbacks.
    base::AutoReset<bool> resolving(&resolving_frame_, true);
    ExecuteVideoFrameCallbacks(timestamp);
    callback_collection_->ExecuteCallbacks(this, timestamp, presentation_frame);

    // The session might have ended in the middle of the frame. Only call
    // OnFrameEnd if it's still valid.
    if (!ended_)
      frame_base_layer->OnFrameEnd();

    // Ensure the XRFrame cannot be used outside the callbacks.
    presentation_frame->Deactivate();
  }
}

void XRSession::LogGetPose() const {
  if (!did_log_getViewerPose_ && GetExecutionContext()) {
    did_log_getViewerPose_ = true;

    ukm::builders::XR_WebXR(GetExecutionContext()->UkmSourceID())
        .SetDidRequestPose(1)
        .Record(GetExecutionContext()->UkmRecorder());
  }
}

bool XRSession::CanReportPoses() const {
  // The spec has a few requirements for if poses can be reported.
  // If we have a session, then user intent is understood. Therefore, (due to
  // the way visibility state is updatd), the rest of the steps really just
  // boil down to whether or not the XRVisibilityState is Visible.
  return visibility_state_ == XRVisibilityState::VISIBLE;
}

bool XRSession::CanEnableAntiAliasing() const {
  return enable_anti_aliasing_;
}

absl::optional<gfx::Transform> XRSession::GetMojoFrom(
    device::mojom::blink::XRReferenceSpaceType space_type) const {
  if (!CanReportPoses()) {
    DVLOG(2) << __func__ << ": cannot report poses, returning nullopt";
    return absl::nullopt;
  }

  switch (space_type) {
    case device::mojom::blink::XRReferenceSpaceType::kViewer:
      if (!mojo_from_viewer_) {
        if (sensorless_session_) {
          return gfx::Transform();
        }

        return absl::nullopt;
      }

      return *mojo_from_viewer_;
    case device::mojom::blink::XRReferenceSpaceType::kLocal:
      // TODO(https://crbug.com/1070380): This assumes that local space is
      // equivalent to mojo space! Remove the assumption once the bug is fixed.
      return gfx::Transform();
    case device::mojom::blink::XRReferenceSpaceType::kUnbounded:
      // TODO(https://crbug.com/1070380): This assumes that unbounded space is
      // equivalent to mojo space! Remove the assumption once the bug is fixed.
      return gfx::Transform();
    case device::mojom::blink::XRReferenceSpaceType::kLocalFloor:
    case device::mojom::blink::XRReferenceSpaceType::kBoundedFloor:
      // Information about -floor spaces is currently stored elsewhere (in
      // stage_parameters_). It probably should eventually move here.
      return absl::nullopt;
  }
}

XRFrame* XRSession::CreatePresentationFrame(bool is_animation_frame) {
  DVLOG(2) << __func__ << ": is_animation_frame=" << is_animation_frame;

  XRFrame* presentation_frame =
      MakeGarbageCollected<XRFrame>(this, is_animation_frame);
  return presentation_frame;
}

// Called when the canvas element for this session's output context is resized.
void XRSession::UpdateCanvasDimensions(Element* element) {
  DCHECK(element);

  double devicePixelRatio = 1.0;
  LocalDOMWindow* window = To<LocalDOMWindow>(xr_->GetExecutionContext());
  if (window) {
    devicePixelRatio = window->GetFrame()->DevicePixelRatio();
  }

  output_width_ = element->OffsetWidth() * devicePixelRatio;
  output_height_ = element->OffsetHeight() * devicePixelRatio;

  if (render_state_->baseLayer()) {
    render_state_->baseLayer()->OnResize();
  }

  canvas_was_resized_ = true;
}

void XRSession::OnButtonEvent(
    device::mojom::blink::XRInputSourceStatePtr input_state) {
  DCHECK(uses_input_eventing_);
  auto input_states = base::make_span(&input_state, 1);
  OnInputStateChangeInternal(last_frame_id_, input_states);
  ProcessInputSourceEvents(input_states);
}

void XRSession::OnInputStateChangeInternal(
    int16_t frame_id,
    base::span<const device::mojom::blink::XRInputSourceStatePtr>
        input_states) {
  // If we're in any state other than visible, input should not be processed
  if (visibility_state_ != XRVisibilityState::VISIBLE) {
    return;
  }

  HeapVector<Member<XRInputSource>> added;
  HeapVector<Member<XRInputSource>> removed;
  last_frame_id_ = frame_id;

  DVLOG(2) << __func__ << ": frame_id=" << frame_id
           << " input_states.size()=" << input_states.size();
  // Build up our added array, and update the frame id of any active input
  // sources so we can flag the ones that are no longer active.
  for (const auto& input_state : input_states) {
    DVLOG(2) << __func__
             << ": input_state->source_id=" << input_state->source_id
             << " input_state->primary_input_pressed="
             << input_state->primary_input_pressed
             << " clicked=" << input_state->primary_input_clicked;

    XRInputSource* stored_input_source =
        input_sources_->GetWithSourceId(input_state->source_id);
    DVLOG(2) << __func__ << ": stored_input_source=" << stored_input_source;
    XRInputSource* input_source = XRInputSource::CreateOrUpdateFrom(
        stored_input_source, this, input_state);

    // Input sources should use DOM overlay hit test to check if they intersect
    // cross-origin content. If that's the case, the input source is set as
    // invisible, and must not return poses or hit test results.
    bool hide_input_source = false;
    if (IsFeatureEnabled(device::mojom::XRSessionFeature::DOM_OVERLAY) &&
        overlay_element_ && input_state->overlay_pointer_position) {
      input_source->ProcessOverlayHitTest(overlay_element_, input_state);
      if (!stored_input_source && !input_source->IsVisible()) {
        DVLOG(2) << __func__ << ": (new) hidden_input_source";
        hide_input_source = true;
      }
    }

    // Using pointer equality to determine if the pointer needs to be set.
    if (stored_input_source != input_source) {
      DVLOG(2) << __func__ << ": stored_input_source != input_source";
      if (!hide_input_source) {
        input_sources_->SetWithSourceId(input_state->source_id, input_source);
        added.push_back(input_source);
        DVLOG(2) << __func__ << ": ADDED input_source "
                 << input_state->source_id;
      }

      // If we previously had a stored_input_source, disconnect its gamepad
      // and mark that it was removed.
      if (stored_input_source) {
        stored_input_source->SetGamepadConnected(false);
        DVLOG(2) << __func__ << ": REMOVED stored_input_source";
        removed.push_back(stored_input_source);
      }
    }

    input_source->setActiveFrameId(frame_id);
  }

  // Remove any input sources that are inactive, and disconnect their gamepad.
  // Note that this is done in two passes because HeapHashMap makes no
  // guarantees about iterators on removal.
  // We use a separate array of inactive sources here rather than just
  // processing removed, because if we replaced any input sources, they would
  // also be in removed, and we'd remove our newly added source.
  Vector<uint32_t> inactive_sources;
  for (unsigned i = 0; i < input_sources_->length(); i++) {
    auto* input_source = (*input_sources_)[i];
    if (input_source->activeFrameId() != frame_id) {
      inactive_sources.push_back(input_source->source_id());
      input_source->OnRemoved();
      removed.push_back(input_source);
    }
  }

  for (uint32_t source_id : inactive_sources) {
    input_sources_->RemoveWithSourceId(source_id);
  }

  // If there have been any changes, fire the input sources change event.
  if (!added.empty() || !removed.empty()) {
    DispatchEvent(*XRInputSourcesChangeEvent::Create(
        event_type_names::kInputsourceschange, this, added, removed));
  }
}

void XRSession::ProcessInputSourceEvents(
    base::span<const device::mojom::blink::XRInputSourceStatePtr>
        input_states) {
  for (const auto& input_state : input_states) {
    // If anything during the process of updating the select state caused us
    // to end our session, we should stop processing select state updates.
    if (ended_)
      break;

    XRInputSource* input_source =
        input_sources_->GetWithSourceId(input_state->source_id);
    // The input source might not be in input_sources_ if it was created hidden.
    if (input_source) {
      input_source->UpdateButtonStates(input_state);
    }
  }
}

void XRSession::AddTransientInputSource(XRInputSource* input_source) {
  if (ended_)
    return;

  // Ensure we're not overriding an input source that's already present.
  DCHECK(!input_sources_->GetWithSourceId(input_source->source_id()));
  input_sources_->SetWithSourceId(input_source->source_id(), input_source);

  DispatchEvent(*XRInputSourcesChangeEvent::Create(
      event_type_names::kInputsourceschange, this, {input_source}, {}));
}

void XRSession::RemoveTransientInputSource(XRInputSource* input_source) {
  if (ended_)
    return;

  input_sources_->RemoveWithSourceId(input_source->source_id());

  DispatchEvent(*XRInputSourcesChangeEvent::Create(
      event_type_names::kInputsourceschange, this, {}, {input_source}));
}

void XRSession::OnMojoSpaceReset() {
  // Since this eventually dispatches an event to the page, the page could
  // create a new reference space which would invalidate our iterators; so
  // iterate over a copy of the reference space list.
  HeapVector<Member<XRReferenceSpace>> ref_spaces_copy = reference_spaces_;
  for (const auto& reference_space : ref_spaces_copy) {
    reference_space->OnReset();
  }
}

void XRSession::OnExitPresent() {
  DVLOG(2) << __func__ << ": immersive()=" << immersive()
           << " waiting_for_shutdown_=" << waiting_for_shutdown_;
  if (immersive()) {
    ForceEnd(ShutdownPolicy::kImmediate);
  } else if (waiting_for_shutdown_) {
    HandleShutdown();
  }
}

bool XRSession::ValidateHitTestSourceExists(
    XRHitTestSource* hit_test_source) const {
  DCHECK(hit_test_source);
  return base::Contains(hit_test_source_ids_, hit_test_source->id());
}

bool XRSession::ValidateHitTestSourceExists(
    XRTransientInputHitTestSource* hit_test_source) const {
  DCHECK(hit_test_source);
  return base::Contains(hit_test_source_for_transient_input_ids_,
                        hit_test_source->id());
}

bool XRSession::RemoveHitTestSource(XRHitTestSource* hit_test_source) {
  DVLOG(2) << __func__;

  DCHECK(hit_test_source);

  if (!base::Contains(hit_test_source_ids_, hit_test_source->id())) {
    DVLOG(2) << __func__
             << ": hit test source was already removed, hit_test_source->id()="
             << hit_test_source->id();
    return false;
  }

  if (ended_) {
    DVLOG(1) << __func__
             << ": attempted to remove a hit test source on a session that has "
                "already ended.";
    // Since the session has ended, we won't be able to reach out to the device
    // to remove a hit test source subscription. Just notify the caller that the
    // removal was successful.
    return true;
  }

  DCHECK_HIT_TEST_SOURCES();

  hit_test_source_ids_to_hit_test_sources_.erase(hit_test_source->id());
  hit_test_source_ids_.erase(hit_test_source->id());

  DCHECK(xr_->xrEnvironmentProviderRemote());

  xr_->xrEnvironmentProviderRemote()->UnsubscribeFromHitTest(
      hit_test_source->id());

  DCHECK_HIT_TEST_SOURCES();

  return true;
}

bool XRSession::RemoveHitTestSource(
    XRTransientInputHitTestSource* hit_test_source) {
  DVLOG(2) << __func__;

  DCHECK(hit_test_source);

  if (!base::Contains(hit_test_source_for_transient_input_ids_,
                      hit_test_source->id())) {
    DVLOG(2) << __func__
             << ": hit test source was already removed, hit_test_source->id()="
             << hit_test_source->id();
    return false;
  }

  if (ended_) {
    DVLOG(1) << __func__
             << ": attempted to remove a hit test source on a session that has "
                "already ended.";
    // Since the session has ended, we won't be able to reach out to the device
    // to remove a hit test source subscription. Just notify the caller that the
    // removal was successful.
    return true;
  }

  DCHECK_HIT_TEST_SOURCES();

  hit_test_source_ids_to_transient_input_hit_test_sources_.erase(
      hit_test_source->id());
  hit_test_source_for_transient_input_ids_.erase(hit_test_source->id());

  DCHECK(xr_->xrEnvironmentProviderRemote());

  xr_->xrEnvironmentProviderRemote()->UnsubscribeFromHitTest(
      hit_test_source->id());

  DCHECK_HIT_TEST_SOURCES();

  return true;
}

const HeapVector<Member<XRViewData>>& XRSession::views() {
  // TODO(bajones): For now we assume that immersive sessions render a stereo
  // pair of views and non-immersive sessions render a single view. That doesn't
  // always hold true, however, so the view configuration should ultimately come
  // from the backing service. See also XRWebGLLayer::UpdateViewports() which
  // assumes that the views are arranged as follows.
  if (!views_updated_this_frame_) {
    if (immersive()) {
      // In immersive mode the projection and view matrices must be aligned with
      // the device's physical optics.

      // Views shouldn't be re-created on each frame because they contain
      // viewport scaling information, such as requested viewport scales.
      // However, if the number of views changed or if the order of the views
      // changed, we should recreate the views since we aren't able to match
      // the old views to the new views.
      bool create_views = false;
      if (views_.size() != pending_views_.size()) {
        views_.clear();
        views_.resize(pending_views_.size());
        create_views = true;

        if (render_state_->baseLayer()) {
          render_state_->baseLayer()->OnResize();
        }
      }

      for (wtf_size_t i = 0; !create_views && i < pending_views_.size(); ++i) {
        if (views_[i]->Eye() == pending_views_[i]->eye) {
          views_[i]->UpdateView(pending_views_[i], render_state_->depthNear(),
                                render_state_->depthFar());
        } else {
          create_views = true;
        }
      }

      if (create_views) {
        for (wtf_size_t i = 0; i < pending_views_.size(); ++i) {
          views_[i] = MakeGarbageCollected<XRViewData>(
              pending_views_[i], render_state_->depthNear(),
              render_state_->depthFar());
        }
      }
    } else {
      if (canvas_was_resized_) {
        views_.clear();
        canvas_was_resized_ = false;
      }
      if (views_.empty()) {
        views_.emplace_back(MakeGarbageCollected<XRViewData>(
            device::mojom::blink::XREye::kNone,
            gfx::Rect(0, 0, output_width_, output_height_)));
      }

      float aspect = 1.0f;
      if (output_width_ && output_height_) {
        aspect = static_cast<float>(output_width_) /
                 static_cast<float>(output_height_);
      }

      // In non-immersive mode, if there is no explicit projection matrix
      // provided, the projection matrix must be aligned with the
      // output canvas dimensions.
      absl::optional<double> inline_vertical_fov =
          render_state_->inlineVerticalFieldOfView();

      // inlineVerticalFieldOfView should only be null in immersive mode.
      DCHECK(inline_vertical_fov.has_value());
      views_[kMonoView]->UpdateProjectionMatrixFromAspect(
          inline_vertical_fov.value(), aspect, render_state_->depthNear(),
          render_state_->depthFar());
    }

    views_updated_this_frame_ = true;
  }

  return views_;
}

bool XRSession::HasPendingActivity() const {
  return (!callback_collection_->IsEmpty() || !vfc_execution_queue_.empty()) &&
         !ended_;
}

void XRSession::Trace(Visitor* visitor) const {
  visitor->Trace(xr_);
  visitor->Trace(render_state_);
  visitor->Trace(world_light_probe_);
  visitor->Trace(pending_render_state_);
  visitor->Trace(end_session_resolver_);
  visitor->Trace(input_sources_);
  visitor->Trace(resize_observer_);
  visitor->Trace(canvas_input_provider_);
  visitor->Trace(overlay_element_);
  visitor->Trace(dom_overlay_state_);
  visitor->Trace(client_receiver_);
  visitor->Trace(input_receiver_);
  visitor->Trace(callback_collection_);
  visitor->Trace(create_anchor_promises_);
  visitor->Trace(request_hit_test_source_promises_);
  visitor->Trace(reference_spaces_);
  visitor->Trace(plane_manager_);
  visitor->Trace(depth_manager_);
  visitor->Trace(anchor_ids_to_anchors_);
  visitor->Trace(anchor_ids_to_pending_anchor_promises_);
  visitor->Trace(prev_base_layer_);
  visitor->Trace(hit_test_source_ids_to_hit_test_sources_);
  visitor->Trace(hit_test_source_ids_to_transient_input_hit_test_sources_);
  visitor->Trace(views_);
  visitor->Trace(frame_tracked_images_);
  visitor->Trace(image_scores_resolvers_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
