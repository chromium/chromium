// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_API_WRAPPER_H_
#define DEVICE_VR_OPENXR_OPENXR_API_WRAPPER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "device/vr/openxr/exit_xr_present_reason.h"
#include "device/vr/openxr/openxr_anchor_manager.h"
#include "device/vr/openxr/openxr_depth_sensor.h"
#include "device/vr/openxr/openxr_graphics_binding.h"
#include "device/vr/openxr/openxr_light_estimator.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/openxr/openxr_scene_understanding_manager.h"
#include "device/vr/openxr/openxr_stage_bounds_provider.h"
#include "device/vr/openxr/openxr_unbounded_space_provider.h"
#include "device/vr/openxr/openxr_view_configuration.h"
#include "device/vr/openxr/openxr_visibility_mask_handler.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "device/vr/vr_export.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

#if BUILDFLAG(IS_WIN)
#include <d3d11_4.h>
#include <wrl.h>
#endif

namespace gfx {
class Transform;
}  // namespace gfx

namespace viz {
class ContextProvider;
}  // namespace viz

namespace device {

class OpenXrExtensionHelper;
class OpenXRInputHelper;
class VRTestHook;
class ServiceTestHook;

using SessionStartedCallback =
    base::OnceCallback<void(mojom::XRRuntimeSessionOptionsPtr options,
                            XrResult result)>;
using SessionEndedCallback = base::RepeatingCallback<void(ExitXrPresentReason)>;
using VisibilityChangedCallback =
    base::RepeatingCallback<void(mojom::XRVisibilityState)>;

// An XrPosef with the space it is relative to.
struct XrLocation {
  XrPosef pose;
  XrSpace space;
};

class OpenXrApiWrapper {
 public:
  using XrFutureReadyCallback = base::OnceCallback<void(XrFutureEXT)>;
  OpenXrApiWrapper();

  OpenXrApiWrapper(const OpenXrApiWrapper&) = delete;
  OpenXrApiWrapper& operator=(const OpenXrApiWrapper&) = delete;

  ~OpenXrApiWrapper();
  bool IsInitialized() const;

  static std::unique_ptr<OpenXrApiWrapper> Create(
      XrInstance instance,
      OpenXrGraphicsBinding* graphics_binding);

  static XrResult GetSystem(XrInstance instance, XrSystemId* system);

  static std::vector<XrEnvironmentBlendMode> GetSupportedBlendModes(
      XrInstance instance,
      XrSystemId system);

  static VRTestHook* GetTestHook();

  static bool NeedsSeparateActivity();

  bool UpdateAndGetSessionEnded();

  // The supplied graphics_binding is guaranteed by the caller to exist until
  // this object is destroyed.
  XrResult InitSession(mojom::XRRuntimeSessionOptionsPtr options,
                       const OpenXrExtensionHelper& extension_helper,
                       SessionStartedCallback on_session_started_callback,
                       SessionEndedCallback on_session_ended_callback,
                       VisibilityChangedCallback visibility_changed_callback);

  XrSpace GetReferenceSpace(mojom::XRReferenceSpaceType type) const;
  std::optional<XrLocation> GetXrLocationFromNativeOriginInformation(
      const mojom::XRNativeOriginInformation& native_origin,
      const gfx::Transform& native_origin_from_object);

  XrInstance instance() const { return instance_; }
  XrSession session() const { return session_; }
  XrSystemId system() const { return system_; }

  XrResult BeginFrame();
  XrResult EndFrame();
  bool HasPendingFrame() const;
  bool HasFrameState() const;
  bool IsFeatureEnabled(device::mojom::XRSessionFeature feature) const;

  const std::unordered_set<mojom::XRSessionFeature>& GetEnabledFeatures() const;
  std::vector<mojom::XRViewPtr> GetViews() const;
  mojom::VRPosePtr GetViewerPose() const;
  std::vector<mojom::XRInputSourceStatePtr> GetInputState();
  void OnHideInputSources();

  std::vector<mojom::XRViewPtr> GetDefaultViews() const;
  float RecommendedViewportScale() const;
  XrTime GetPredictedDisplayTime() const;
  bool GetStageParameters(std::vector<gfx::Point3F>& stage_bounds,
                          gfx::Transform& local_from_stage);
  std::optional<gfx::Transform> GetLocalFromFloor();

  device::mojom::XREnvironmentBlendMode PickEnvironmentBlendModeForSession(
      device::mojom::XRSessionMode session_mode);

  // Various manager getters if they exist.
  OpenXrPlaneManager* GetPlaneManager();
  OpenXrAnchorManager* GetAnchorManager();
  OpenXrHitTestManager* GetHitTestManager();
  OpenXrLightEstimator* GetLightEstimator();
  OpenXrDepthSensor* GetDepthSensor();

  void OnContextProviderCreated(
      scoped_refptr<viz::ContextProvider> context_provider);
  void OnContextProviderLost();

  bool CanEnableAntiAliasing() const;

  // Polls the given future until it is ready. Once ready, the provided
  // callback will be invoked with the future. If polling fails, the callback
  // will be invoked with XR_NULL_FUTURE_EXT.
  void PollFuture(XrFutureEXT future,
                  base::OnceCallback<void(XrFutureEXT)> on_ready_callback);

  uint32_t GetRecommendedSwapchainSampleCount() const;

  static void DEVICE_VR_EXPORT SetTestHook(VRTestHook* hook);

 private:
  void Reset();
  bool Initialize(XrInstance instance, OpenXrGraphicsBinding* graphics_binding);
  void Uninitialize();
  XrResult ShutdownSession();
  XrResult EnableSupportedFeatures(
      const OpenXrExtensionHelper& extension_helper);

  XrResult InitializeSystem();
  XrResult InitializeViewConfig(XrViewConfigurationType type,
                                OpenXrViewConfiguration& view_config);
  XrResult GetPropertiesForViewConfig(
      XrViewConfigurationType type,
      std::vector<XrViewConfigurationView>& view_properties) const;
  XrResult InitializeEnvironmentBlendMode(XrSystemId system);
  XrResult ProcessEvents();
  void ProcessPendingFutures();
  void EnsureEventPolling();

  XrResult CreateSession();

  XrResult CreateSwapchain();
  bool RecomputeSwapchainSizeAndViewports();
  XrResult CreateSpace(XrReferenceSpaceType type, XrSpace* space);

  XrResult BeginSession();
  XrResult UpdateSecondaryViewConfigStates(
      const std::vector<XrSecondaryViewConfigurationStateMSFT>& states);
  XrResult UpdateViewConfigurations();
  XrResult LocateViews(XrReferenceSpaceType space_type,
                       OpenXrViewConfiguration& view_config);

  bool HasInstance() const;
  bool HasSystem() const;
  bool HasBlendMode() const;
  bool HasSession() const;
  bool HasSpace(XrReferenceSpaceType type) const;

  void UpdateStageBounds();
  std::optional<gfx::Transform> GetLocalFromStage();
  std::optional<gfx::Transform> GetBaseSpaceFromSpace(
      mojom::XRReferenceSpaceType base_space,
      mojom::XRReferenceSpaceType space);
  XrResult CreateEmulatedLocalFloorSpace(XrSpace* space);
  XrResult UpdateLocalFloorSpace();

  device::mojom::XREnvironmentBlendMode GetMojoBlendMode(
      XrEnvironmentBlendMode xr_blend_mode);

  mojom::XRViewPtr CreateView(const OpenXrViewConfiguration& view_config,
                              uint32_t view_index,
                              mojom::XREye eye,
                              uint32_t x_offset) const;

  bool ShouldCreateSharedImages() const;
  void CreateSharedMailboxes();

  void SetXrSessionState(XrSessionState new_state);

  // The session is running only after xrBeginSession and before xrEndSession.
  // It is not considered running after creation but before xrBeginSession.
  bool session_running_;
  bool pending_frame_;

  SessionStartedCallback on_session_started_callback_;
  SessionEndedCallback on_session_ended_callback_;
  VisibilityChangedCallback visibility_changed_callback_;
  mojom::XRRuntimeSessionOptionsPtr session_options_;

  // Testing objects
  static VRTestHook* test_hook_;
  static ServiceTestHook* service_test_hook_;

  std::unique_ptr<OpenXRInputHelper> input_helper_;

  // OpenXR objects

  // Tracks the session state throughout the lifetime of the Wrapper.
  XrSessionState session_state_ = XR_SESSION_STATE_UNKNOWN;

  // These objects are initialized on successful initialization.
  XrInstance instance_;
  XrSystemId system_;
  XrEnvironmentBlendMode blend_mode_;
  std::vector<gfx::Point3F> stage_bounds_;

  // These objects are initialized when a session begins and stay constant
  // throughout the lifetime of the session.
  XrSession session_ = XR_NULL_HANDLE;
  XrSpace local_space_ = XR_NULL_HANDLE;
  XrSpace local_floor_space_ = XR_NULL_HANDLE;
  XrSpace stage_space_ = XR_NULL_HANDLE;
  XrSpace view_space_ = XR_NULL_HANDLE;
  XrSpace unbounded_space_ = XR_NULL_HANDLE;
  bool emulated_local_floor_ = false;
  bool try_recreate_local_floor_ = false;
  std::unordered_set<mojom::XRSessionFeature> enabled_features_;
  raw_ptr<OpenXrGraphicsBinding> graphics_binding_ = nullptr;

  bool received_initial_valid_primary_views_ = false;
  uint64_t frames_before_initial_valid_primary_views_ = 0;

  // The rest of these objects store information about the current frame and are
  // updated each frame.
  XrFrameState frame_state_;

  OpenXrViewConfiguration primary_view_config_;
  std::unordered_map<XrViewConfigurationType, OpenXrViewConfiguration>
      secondary_view_configs_;

  absl::flat_hash_map<XrFutureEXT, XrFutureReadyCallback> pending_futures_;

  std::unique_ptr<OpenXrDepthSensor> depth_sensor_;
  std::unique_ptr<OpenXrLightEstimator> light_estimator_;
  std::unique_ptr<OpenXrStageBoundsProvider> bounds_provider_;
  std::unique_ptr<OpenXRSceneUnderstandingManager> scene_understanding_manager_;
  std::unique_ptr<OpenXrUnboundedSpaceProvider> unbounded_space_provider_;
  std::unique_ptr<OpenXrVisibilityMaskHandler> visibility_mask_handler_;

  // The context provider is owned by the OpenXrRenderLoop, and may change when
  // there is a context lost.
  scoped_refptr<viz::ContextProvider> context_provider_;

  raw_ptr<const OpenXrExtensionHelper> extension_helper_ = nullptr;

  base::WeakPtrFactory<OpenXrApiWrapper> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_API_WRAPPER_H_
