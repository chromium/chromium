// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SESSION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SESSION_H_

#include "base/containers/span.h"
#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_request_callback_collection.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source_array.h"
#include "third_party/blink/renderer/platform/geometry/double_size.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"

namespace blink {

class Element;
class ExceptionState;
class HTMLCanvasElement;
class ResizeObserver;
class ScriptPromiseResolver;
class V8XRFrameRequestCallback;
class XR;
class XRAnchor;
class XRAnchorSet;
class XRCanvasInputProvider;
class XRHitTestOptionsInit;
class XRHitTestSource;
class XRPlane;
class XRRay;
class XRReferenceSpace;
class XRRenderState;
class XRRenderStateInit;
class XRRigidTransform;
class XRSpace;
class XRViewData;
class XRWebGLLayer;
class XRWorldInformation;
class XRWorldTrackingState;
class XRWorldTrackingStateInit;

using XRSessionFeatureSet = WTF::HashSet<device::mojom::XRSessionFeature>;

class XRSession final
    : public EventTargetWithInlineData,
      public device::mojom::blink::XRSessionClient,
      public device::mojom::blink::XRInputSourceButtonListener,
      public ActiveScriptWrappable<XRSession> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(XRSession);

 public:
  enum SessionMode { kModeInline = 0, kModeImmersiveVR, kModeImmersiveAR };

  enum EnvironmentBlendMode {
    kBlendModeOpaque = 0,
    kBlendModeAdditive,
    kBlendModeAlphaBlend
  };

  struct MetricsReporter {
    explicit MetricsReporter(
        mojo::Remote<device::mojom::blink::XRSessionMetricsRecorder> recorder);

    // Reports a use (or attempted use) of the given feature to the underlying
    // metrics recorder.
    void ReportFeatureUsed(device::mojom::blink::XRSessionFeature feature);

   private:
    mojo::Remote<device::mojom::blink::XRSessionMetricsRecorder> recorder_;

    // Keeps track of which features have already been reported, to reduce
    // redundant mojom calls.
    WTF::HashSet<device::mojom::blink::XRSessionFeature> reported_features_;
  };

  XRSession(XR* xr,
            mojo::PendingReceiver<device::mojom::blink::XRSessionClient>
                client_receiver,
            SessionMode mode,
            EnvironmentBlendMode environment_blend_mode,
            bool uses_input_eventing,
            bool sensorless_session,
            XRSessionFeatureSet enabled_features);
  ~XRSession() override = default;

  XR* xr() const { return xr_; }
  const String& environmentBlendMode() const { return blend_mode_string_; }
  const String visibilityState() const;
  XRRenderState* renderState() const { return render_state_; }
  XRWorldTrackingState* worldTrackingState() { return world_tracking_state_; }
  XRSpace* viewerSpace() const;
  XRAnchorSet* trackedAnchors() const;

  bool immersive() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(end, kEnd)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(select, kSelect)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(inputsourceschange, kInputsourceschange)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(selectstart, kSelectstart)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(selectend, kSelectend)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(visibilitychange, kVisibilitychange)

  void updateRenderState(XRRenderStateInit* render_state_init,
                         ExceptionState& exception_state);
  void updateWorldTrackingState(
      XRWorldTrackingStateInit* world_tracking_state_init,
      ExceptionState& exception_state);
  ScriptPromise requestReferenceSpace(ScriptState* script_state,
                                      const String& type,
                                      ExceptionState&);

  // IDL-exposed
  ScriptPromise createAnchor(ScriptState* script_state,
                             XRRigidTransform* initial_pose,
                             XRSpace* space,
                             ExceptionState& exception_state);

  // helper, not IDL-exposed
  ScriptPromise CreateAnchor(ScriptState* script_state,
                             XRRigidTransform* pose,
                             XRSpace* space,
                             XRPlane* plane,
                             ExceptionState& exception_state);

  int requestAnimationFrame(V8XRFrameRequestCallback* callback);
  void cancelAnimationFrame(int id);

  XRInputSourceArray* inputSources() const;

  ScriptPromise requestHitTestSource(ScriptState* script_state,
                                     XRHitTestOptionsInit* options,
                                     ExceptionState& exception_state);

  ScriptPromise requestHitTest(ScriptState* script_state,
                               XRRay* ray,
                               XRSpace* space,
                               ExceptionState&);

  // Called by JavaScript to manually end the session.
  ScriptPromise end(ScriptState* script_state, ExceptionState&);

  bool ended() const { return ended_; }

  // Called when the session is ended, either via calling the "end" function or
  // when the presentation service connection is closed.
  enum class ShutdownPolicy {
    kWaitForResponse,  // expect a future OnExitPresent call
    kImmediate,        // do all cleanup immediately
  };
  void ForceEnd(ShutdownPolicy);

  // Describes the scalar to be applied to the default framebuffer dimensions
  // which gives 1:1 pixel ratio at the center of the user's view.
  double NativeFramebufferScale() const;

  // Describes the recommended dimensions of layer framebuffers. Should be a
  // value that provides a good balance between quality and performance.
  DoubleSize DefaultFramebufferSize() const;

  // Reports the size of the output canvas, if one is available. If not
  // reports (0, 0);
  DoubleSize OutputCanvasSize() const;
  void DetachOutputCanvas(HTMLCanvasElement* output_canvas);

  void LogGetPose() const;

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  void OnFocusChanged();
  void OnFrame(double timestamp,
               const base::Optional<gpu::MailboxHolder>& output_mailbox_holder);

  // XRInputSourceButtonListener
  void OnButtonEvent(
      device::mojom::blink::XRInputSourceStatePtr input_source) override;

  WTF::Vector<XRViewData>& views();

  void AddTransientInputSource(XRInputSource* input_source);
  void RemoveTransientInputSource(XRInputSource* input_source);

  void OnPoseReset();

  const device::mojom::blink::VRDisplayInfoPtr& GetVRDisplayInfo() const {
    return display_info_;
  }

  mojo::PendingAssociatedRemote<
      device::mojom::blink::XRInputSourceButtonListener>
  GetInputClickListener();

  bool EmulatedPosition() const {
    // If we don't have display info then we should be using the identity
    // reference space, which by definition will be emulating the position.
    if (!display_info_) {
      return true;
    }

    return emulated_position_;
  }

  // Immersive sessions currently use two views for VR, and only a single view
  // for smartphone immersive AR mode. Convention is that we use the left eye
  // if there's only a single view.
  bool StereoscopicViews() { return display_info_ && display_info_->right_eye; }

  void UpdateEyeParameters(
      const device::mojom::blink::VREyeParametersPtr& left_eye,
      const device::mojom::blink::VREyeParametersPtr& right_eye);
  void UpdateStageParameters(
      const device::mojom::blink::VRStageParametersPtr& stage_parameters);
  // Incremented every time display_info_ is changed, so that other objects that
  // depend on it can know when they need to update.
  unsigned int DisplayInfoPtrId() const { return display_info_id_; }
  unsigned int StageParametersId() const { return stage_parameters_id_; }

  // Returns true if the session recognizes passed in hit_test_source as still
  // existing.
  bool ValidateHitTestSourceExists(XRHitTestSource* hit_test_source);

  void SetXRDisplayInfo(device::mojom::blink::VRDisplayInfoPtr display_info);

  bool UsesInputEventing() { return uses_input_eventing_; }

  void Trace(blink::Visitor* visitor) override;

  // ScriptWrappable
  bool HasPendingActivity() const override;

  bool CanReportPoses();

  // Creates presentation frame based on current state of the session.
  // State currently used in XRFrame creation is mojo_from_viewer_ and
  // world_information_. The created XRFrame also stores a reference to this
  // XRSession.
  XRFrame* CreatePresentationFrame();

  // Updates the internal XRSession state that is relevant to creating
  // presentation frames.
  void UpdatePresentationFrameState(
      double timestamp,
      const device::mojom::blink::VRPosePtr& frame_pose,
      const device::mojom::blink::XRFrameDataPtr& frame_data,
      int16_t frame_id,
      bool emulated_position);

  // Notifies immersive session that the environment integration provider has
  // been created by the session's XR instance, |xr_|. Gives a session an
  // opportunity to register its own error handlers on environment integration
  // provider endpoint.
  void OnEnvironmentProviderCreated();

  // Returns whether the given feature is enabled for this session.
  bool IsFeatureEnabled(device::mojom::XRSessionFeature feature) const;

  // Sets the metrics reporter for this session. This should only be done once.
  void SetMetricsReporter(std::unique_ptr<MetricsReporter> reporter);

 private:
  class XRSessionResizeObserverDelegate;

  using XRVisibilityState = device::mojom::blink::XRVisibilityState;

  void UpdateCanvasDimensions(Element*);
  void ApplyPendingRenderState();

  void MaybeRequestFrame();

  void OnInputStateChangeInternal(
      int16_t frame_id,
      base::span<const device::mojom::blink::XRInputSourceStatePtr>
          input_states);
  void ProcessInputSourceEvents(
      base::span<const device::mojom::blink::XRInputSourceStatePtr>
          input_states);
  void UpdateWorldUnderstandingStateForFrame(
      double timestamp,
      const device::mojom::blink::XRFrameDataPtr& frame_data);

  // XRSessionClient
  void OnChanged(device::mojom::blink::VRDisplayInfoPtr display_info) override;
  void OnExitPresent() override;
  void OnVisibilityStateChanged(
      device::mojom::blink::XRVisibilityState visibility_state) override;

  void UpdateVisibilityState();

  void OnHitTestResults(
      ScriptPromiseResolver* resolver,
      base::Optional<WTF::Vector<device::mojom::blink::XRHitResultPtr>>
          results);

  void OnSubscribeToHitTestResult(
      ScriptPromiseResolver* resolver,
      device::mojom::SubscribeToHitTestResult result,
      uint64_t subscription_id);

  void OnCreateAnchorResult(ScriptPromiseResolver* resolver,
                            device::mojom::CreateAnchorResult result,
                            uint64_t id);

  void EnsureEnvironmentErrorHandler();
  void OnEnvironmentProviderError();

  void ProcessAnchorsData(
      const device::mojom::blink::XRAnchorsDataPtr& tracked_anchors_data,
      double timestamp);

  void CleanUpUnusedHitTestSources();

  void ProcessHitTestData(
      const device::mojom::blink::XRHitTestSubscriptionResultsDataPtr&
          hit_test_data);

  void HandleShutdown();

  const Member<XR> xr_;
  const SessionMode mode_;
  const bool environment_integration_;
  String blend_mode_string_;
  XRVisibilityState device_visibility_state_ = XRVisibilityState::VISIBLE;
  XRVisibilityState visibility_state_ = XRVisibilityState::VISIBLE;
  String visibility_state_string_;
  Member<XRRenderState> render_state_;
  Member<XRWorldTrackingState> world_tracking_state_;
  Member<XRWorldInformation> world_information_;
  HeapVector<Member<XRRenderStateInit>> pending_render_state_;

  // Handle delayed events and promises for session shutdown. A JS-initiated
  // end() call call marks the session as ended, but doesn't resolve the end
  // promise or trigger the 'end' event until the device side reports
  // OnExitPresent is complete. If the session end was initiated from the device
  // side, or in case of connection errors, proceed to shutdown_complete_ state
  // immediately.
  Member<ScriptPromiseResolver> end_session_resolver_;
  // "ended_" becomes true as soon as session shutdown is initiated.
  bool ended_ = false;
  bool waiting_for_shutdown_ = false;

  XRSessionFeatureSet enabled_features_;
  std::unique_ptr<MetricsReporter> metrics_reporter_;

  bool is_tracked_anchors_null_ = true;
  HeapHashMap<uint64_t, Member<XRAnchor>> anchor_ids_to_anchors_;

  // Mapping of hit test source ids (aka hit test subscription ids) to hit test
  // sources. Hit test source has to be stored via weak member - JavaScript side
  // will communicate that it's no longer interested in the subscription by
  // dropping all its references to the hit test source & we need to make sure
  // that we don't keep the XRHitTestSources alive.
  HeapHashMap<uint64_t, WeakMember<XRHitTestSource>>
      hit_test_source_ids_to_hit_test_sources_;

  WTF::Vector<XRViewData> views_;

  Member<XRInputSourceArray> input_sources_;
  Member<XRWebGLLayer> prev_base_layer_;
  Member<ResizeObserver> resize_observer_;
  Member<XRCanvasInputProvider> canvas_input_provider_;
  bool environment_error_handler_subscribed_ = false;
  HeapHashSet<Member<ScriptPromiseResolver>> hit_test_promises_;
  // Set of promises returned from CreateAnchor that are still in-flight.
  HeapHashSet<Member<ScriptPromiseResolver>> create_anchor_promises_;
  // Set of promises returned from requestHitTestSource that are still
  // in-flight.
  HeapHashSet<Member<ScriptPromiseResolver>> request_hit_test_source_promises_;
  HeapVector<Member<XRReferenceSpace>> reference_spaces_;

  unsigned int display_info_id_ = 0;
  unsigned int stage_parameters_id_ = 0;
  device::mojom::blink::VRDisplayInfoPtr display_info_;

  mojo::Receiver<device::mojom::blink::XRSessionClient> client_receiver_;
  mojo::AssociatedReceiver<device::mojom::blink::XRInputSourceButtonListener>
      input_receiver_{this};

  Member<XRFrameRequestCallbackCollection> callback_collection_;
  // Viewer pose in mojo space.
  std::unique_ptr<TransformationMatrix> mojo_from_viewer_;

  bool pending_frame_ = false;
  bool resolving_frame_ = false;
  bool update_views_next_frame_ = false;
  bool views_dirty_ = true;
  bool frames_throttled_ = false;

  // Indicates that we've already logged a metric, so don't need to log it
  // again.
  mutable bool did_log_getInputSources_ = false;
  mutable bool did_log_getViewerPose_ = false;

  // Dimensions of the output canvas.
  int output_width_ = 1;
  int output_height_ = 1;

  bool uses_input_eventing_ = false;

  // Indicates that this is a sensorless session which should only support the
  // identity reference space.
  bool sensorless_session_ = false;

  int16_t last_frame_id_ = -1;

  bool emulated_position_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SESSION_H_
