// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_VR_VR_DISPLAY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_VR_VR_DISPLAY_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_frame_request_callback.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/pausable_object.h"
#include "third_party/blink/renderer/modules/vr/vr_display_capabilities.h"
#include "third_party/blink/renderer/modules/vr/vr_layer_init.h"
#include "third_party/blink/renderer/platform/graphics/gpu/xr_frame_transport.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {

class PLATFORM_EXPORT Image;
class NavigatorVR;
class VRController;
class VREyeParameters;
class VRFrameData;
class VRStageParameters;
class VRDisplay;

class WebGLRenderingContextBase;

// Wrapper class to allow the VRDisplay to distinguish between immersive and
// non-immersive XRSession events.
class SessionClientBinding
    : public GarbageCollectedFinalized<SessionClientBinding>,
      public device::mojom::blink::XRSessionClient {
 public:
  enum class SessionBindingType {
    kImmersive = 0,
    kNonImmersive = 1,
  };

  SessionClientBinding(VRDisplay* display,
                       SessionBindingType immersive,
                       device::mojom::blink::XRSessionClientRequest request);
  ~SessionClientBinding() override;
  void Close();

  void Trace(blink::Visitor*);

 private:
  void OnChanged(device::mojom::blink::VRDisplayInfoPtr) override;
  void OnExitPresent() override;
  void OnBlur() override;
  void OnFocus() override;

  // VRDisplay keeps all references to SessionClientBinding, so as soon as
  // VRDisplay is destroyed, so is the SessionClientBinding.
  Member<VRDisplay> display_;
  bool is_immersive_;
  mojo::Binding<device::mojom::blink::XRSessionClient> client_binding_;
};

enum VREye { kVREyeNone, kVREyeLeft, kVREyeRight };

class VRDisplay final : public EventTargetWithInlineData,
                        public ActiveScriptWrappable<VRDisplay>,
                        public PausableObject,
                        public device::mojom::blink::VRDisplayClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(VRDisplay);
  USING_PRE_FINALIZER(VRDisplay, Dispose);

 public:
  ~VRDisplay() override;

  // We hand out at most one VRDisplay, so hardcode displayId to 1.
  unsigned displayId() const { return 1; }
  const String& displayName() const { return display_name_; }

  VRDisplayCapabilities* capabilities() const { return capabilities_; }
  VRStageParameters* stageParameters() const { return stage_parameters_; }
  device::mojom::blink::XRDevice* device() { return device_ptr_.get(); }

  bool isPresenting() const { return is_presenting_; }
  bool canPresent() const { return capabilities_->canPresent(); }

  bool getFrameData(VRFrameData*);

  double depthNear() const { return depth_near_; }
  double depthFar() const { return depth_far_; }

  void setDepthNear(double value) { depth_near_ = value; }
  void setDepthFar(double value) { depth_far_ = value; }

  VREyeParameters* getEyeParameters(const String&);

  int requestAnimationFrame(V8FrameRequestCallback*);
  void cancelAnimationFrame(int id);

  ScriptPromise requestPresent(ScriptState*,
                               const HeapVector<VRLayerInit>& layers);
  ScriptPromise exitPresent(ScriptState*);

  HeapVector<VRLayerInit> getLayers();

  void submitFrame();

  Document* GetDocument();
  device::mojom::blink::VRDisplayClientPtr GetDisplayClient();

  // EventTarget overrides:
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // ContextLifecycleObserver implementation.
  void ContextDestroyed(ExecutionContext*) override;

  // ScriptWrappable implementation.
  bool HasPendingActivity() const final;

  // PausableObject:
  void Pause() override;
  void Unpause() override;

  void OnChanged(device::mojom::blink::VRDisplayInfoPtr, bool is_immersive);
  void OnExitPresent(bool is_immersive);
  void OnBlur(bool is_immersive);
  void OnFocus(bool is_immersive);

  void FocusChanged();

  void OnNonImmersiveVSync(TimeTicks timestamp);
  int PendingNonImmersiveVSyncId() { return pending_non_immersive_vsync_id_; }

  void Trace(blink::Visitor*) override;

 protected:
  friend class VRController;

  VRDisplay(NavigatorVR*, device::mojom::blink::XRDevicePtr);

  void Update(const device::mojom::blink::VRDisplayInfoPtr&);

  void UpdatePose();

  void BeginPresent();
  void ForceExitPresent();

  void UpdateLayerBounds();

  VRController* Controller();

 private:
  void OnRequestImmersiveSessionReturned(
      device::mojom::blink::XRSessionPtr session);
  void OnNonImmersiveSessionRequestReturned(
      device::mojom::blink::XRSessionPtr session);

  void OnConnected();
  void OnDisconnected();

  void StopPresenting();

  void OnPresentChange();

  // VRDisplayClient
  void OnActivate(device::mojom::blink::VRDisplayEventReason,
                  OnActivateCallback on_handled) override;
  void OnDeactivate(device::mojom::blink::VRDisplayEventReason) override;

  void OnPresentingVSync(device::mojom::blink::XRFrameDataPtr);
  void OnPresentationProviderConnectionError();

  void OnNonImmersiveFrameData(device::mojom::blink::XRFrameDataPtr);

  bool FocusedOrPresenting();

  ScriptedAnimationController& EnsureScriptedAnimationController(Document*);
  void ProcessScheduledAnimations(TimeTicks timestamp);
  void ProcessScheduledWindowAnimations(TimeTicks timestamp);

  // Request delivery of a VSync event for either magic window mode or
  // presenting mode as applicable. May be called more than once per frame, it
  // ensures that there's at most one VSync request active at a time.
  // Does nothing if the web application hasn't requested a rAF callback.
  void RequestVSync();

  scoped_refptr<Image> GetFrameImage(
      std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback);

  Member<NavigatorVR> navigator_vr_;
  String display_name_;
  bool is_connected_ = false;
  bool is_presenting_ = false;
  bool is_valid_device_for_presenting_ = true;
  Member<VRDisplayCapabilities> capabilities_;
  Member<VRStageParameters> stage_parameters_;
  Member<VREyeParameters> eye_parameters_left_;
  Member<VREyeParameters> eye_parameters_right_;
  device::mojom::blink::VRPosePtr frame_pose_;
  device::mojom::blink::VRPosePtr pending_pose_;

  // Set to true between OnActivate and requestPresent to indicate that we're in
  // a display activation state.
  bool in_display_activate_ = false;

  // This frame ID is vr-specific and is used to track when frames arrive at the
  // VR compositor so that it knows which poses to use, when to apply bounds
  // updates, etc.
  int16_t vr_frame_id_ = -1;
  VRLayerInit layer_;
  double depth_near_ = 0.01;
  double depth_far_ = 10000.0;

  // Current dimensions of the WebVR source canvas. May be different from
  // the recommended renderWidth/Height if the client overrides dimensions.
  int source_width_ = 0;
  int source_height_ = 0;

  void Dispose();

  gpu::gles2::GLES2Interface* context_gl_ = nullptr;
  Member<WebGLRenderingContextBase> rendering_context_;
  Member<XRFrameTransport> frame_transport_;

  TraceWrapperMember<ScriptedAnimationController>
      scripted_animation_controller_;
  bool pending_vrdisplay_raf_ = false;
  bool pending_presenting_vsync_ = false;
  bool pending_non_immersive_vsync_ = false;
  int pending_non_immersive_vsync_id_ = -1;
  base::OnceClosure non_immersive_vsync_waiting_for_pose_;
  WTF::TimeTicks non_immersive_pose_request_time_;
  WTF::TimeTicks non_immersive_pose_received_time_;
  bool in_animation_frame_ = false;
  bool did_submit_this_frame_ = false;
  bool display_blurred_ = false;
  bool pending_present_request_ = false;

  // Metrics data - indicates whether we've already measured this data so we
  // don't do it every frame.
  bool did_log_getFrameData_ = false;
  bool did_log_requestPresent_ = false;

  device::mojom::blink::XRFrameDataProviderPtr non_immersive_provider_;

  device::mojom::blink::XRDevicePtr device_ptr_;

  bool present_image_needs_copy_ = false;

  Member<SessionClientBinding> non_immersive_client_binding_;
  Member<SessionClientBinding> immersive_client_binding_;
  mojo::Binding<device::mojom::blink::VRDisplayClient> display_client_binding_;
  device::mojom::blink::XRFrameDataProviderPtr vr_presentation_data_provider_;
  device::mojom::blink::XRPresentationProviderPtr vr_presentation_provider_;

  HeapDeque<Member<ScriptPromiseResolver>> pending_present_resolvers_;
};

using VRDisplayVector = HeapVector<Member<VRDisplay>>;

// When adding values, insert them before Max and add them to
// VRPresentationResult in enums.xml. Do not reuse values.
// Also, remove kPlaceholderForPreviousHighValue.
// When values become obsolete, comment them out here and mark them deprecated
// in enums.xml.
enum class PresentationResult {
  kRequested = 0,
  kSuccess = 1,
  kSuccessAlreadyPresenting = 2,
  kVRDisplayCannotPresent = 3,
  kPresentationNotSupportedByDisplay = 4,
  // kVRDisplayNotFound = 5,
  kNotInitiatedByUserGesture = 6,
  kInvalidNumberOfLayers = 7,
  kInvalidLayerSource = 8,
  kLayerSourceMissingWebGLContext = 9,
  kInvalidLayerBounds = 10,
  // kServiceInactive = 11,
  // kRequestDenied = 12,
  // kFullscreenNotEnabled = 13,
  // TODO(ddorwin): Remove this placeholder when adding a new value.
  kPlaceholderForPreviousHighValue = 13,
  kPresentationResultMax,  // Must be last member of enum.
};

void ReportPresentationResult(PresentationResult);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_VR_VR_DISPLAY_H_
