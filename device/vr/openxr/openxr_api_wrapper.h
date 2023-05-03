// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_API_WRAPPER_H_
#define DEVICE_VR_OPENXR_OPENXR_API_WRAPPER_H_

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "device/vr/openxr/openxr_anchor_manager.h"
#include "device/vr/openxr/openxr_graphics_binding.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/openxr/openxr_scene_understanding_manager.h"
#include "device/vr/openxr/openxr_view_configuration.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "device/vr/vr_export.h"
#include "device/vr/windows/compositor_base.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

#if BUILDFLAG(IS_WIN)
#include <d3d11_4.h>
#include <wrl.h>
#endif

namespace gfx {
class Size;
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

using SessionStartedCallback = base::OnceCallback<void(XrResult result)>;
using SessionEndedCallback = base::RepeatingCallback<void(ExitXrPresentReason)>;
using VisibilityChangedCallback =
    base::RepeatingCallback<void(mojom::XRVisibilityState)>;

// TODO(https://crbug.com/1441072): Refactor this class.
struct SwapChainInfo {
#if BUILDFLAG(IS_WIN)
  explicit SwapChainInfo(ID3D11Texture2D*);
#else
  SwapChainInfo();
#endif
  ~SwapChainInfo();
  SwapChainInfo(SwapChainInfo&&);
  SwapChainInfo& operator=(SwapChainInfo&&);

  void Clear();

#if BUILDFLAG(IS_WIN)
  // When shared images are being used, there is a corresponding MailboxHolder
  // and D3D11Fence for each D3D11 texture in the vector.
  raw_ptr<ID3D11Texture2D> d3d11_texture = nullptr;
  Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence;
#endif
  gpu::MailboxHolder mailbox_holder;
};

class OpenXrApiWrapper {
 public:
  OpenXrApiWrapper();

  OpenXrApiWrapper(const OpenXrApiWrapper&) = delete;
  OpenXrApiWrapper& operator=(const OpenXrApiWrapper&) = delete;

  ~OpenXrApiWrapper();
  bool IsInitialized() const;

  static std::unique_ptr<OpenXrApiWrapper> Create(XrInstance instance);

  static XrResult GetSystem(XrInstance instance, XrSystemId* system);

  static std::vector<XrEnvironmentBlendMode> GetSupportedBlendModes(
      XrInstance instance,
      XrSystemId system);

  static VRTestHook* GetTestHook();

  bool UpdateAndGetSessionEnded();

  // The supplied graphics_binding is guaranteed by the caller to exist until
  // this object is destroyed.
  XrResult InitSession(
      const std::unordered_set<mojom::XRSessionFeature>& enabled_features,
      OpenXrGraphicsBinding* graphics_binding,
      const OpenXrExtensionHelper& extension_helper,
      SessionStartedCallback on_session_started_callback,
      SessionEndedCallback on_session_ended_callback,
      VisibilityChangedCallback visibility_changed_callback);

  XrSpace GetReferenceSpace(device::mojom::XRReferenceSpaceType type) const;

  XrResult BeginFrame(SwapChainInfo** frame_info);
  XrResult EndFrame();
  bool HasPendingFrame() const;
  bool HasFrameState() const;

  std::vector<mojom::XRViewPtr> GetViews() const;
  mojom::VRPosePtr GetViewerPose() const;
  std::vector<mojom::XRInputSourceStatePtr> GetInputState(
      bool hand_input_enabled);

  std::vector<mojom::XRViewPtr> GetDefaultViews() const;
  gfx::Size GetSwapchainSize() const;
  XrTime GetPredictedDisplayTime() const;
  bool GetStageParameters(XrExtent2Df& stage_bounds,
                          gfx::Transform& local_from_stage);
  bool StageParametersEnabled() const;

  device::mojom::XREnvironmentBlendMode PickEnvironmentBlendModeForSession(
      device::mojom::XRSessionMode session_mode);

  OpenXrAnchorManager* GetOrCreateAnchorManager(
      const OpenXrExtensionHelper& extension_helper);
  OpenXRSceneUnderstandingManager* GetOrCreateSceneUnderstandingManager(
      const OpenXrExtensionHelper& extension_helper);

  void OnContextProviderCreated(
      scoped_refptr<viz::ContextProvider> context_provider);
  void OnContextProviderLost();

  bool CanEnableAntiAliasing() const;
  bool IsUsingSharedImages() const;

  static void DEVICE_VR_EXPORT SetTestHook(VRTestHook* hook);
#if BUILDFLAG(IS_WIN)
  void StoreFence(Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence,
                  int16_t frame_index);
#endif

 private:
  void Reset();
  bool Initialize(XrInstance instance);
  void Uninitialize();

  XrResult InitializeSystem();
  XrResult InitializeViewConfig(XrViewConfigurationType type,
                                OpenXrViewConfiguration& view_config);
  XrResult GetPropertiesForViewConfig(
      XrViewConfigurationType type,
      std::vector<XrViewConfigurationView>& view_properties) const;
  XrResult InitializeEnvironmentBlendMode(XrSystemId system);
  XrResult ProcessEvents();
  void EnsureEventPolling();

  XrResult CreateSession();

  XrResult CreateSwapchain();
  bool RecomputeSwapchainSizeAndViewports();
  XrResult CreateSpace(XrReferenceSpaceType type, XrSpace* space);

  XrResult BeginSession();
  XrResult UpdateSecondaryViewConfigStates(
      const std::vector<XrSecondaryViewConfigurationStateMSFT>& states);
  XrResult UpdateViewConfigurations();
  XrResult PrepareViewConfigForRender(OpenXrViewConfiguration& view_config);
  XrResult LocateViews(XrReferenceSpaceType space_type,
                       OpenXrViewConfiguration& view_config) const;

  bool HasInstance() const;
  bool HasSystem() const;
  bool HasBlendMode() const;
  bool HasSession() const;
  bool HasColorSwapChain() const;
  bool HasSpace(XrReferenceSpaceType type) const;

  uint32_t GetRecommendedSwapchainSampleCount() const;
  XrResult UpdateStageBounds();

  device::mojom::XREnvironmentBlendMode GetMojoBlendMode(
      XrEnvironmentBlendMode xr_blend_mode);

  mojom::XRViewPtr CreateView(const OpenXrViewConfiguration& view_config,
                              uint32_t view_index,
                              mojom::XREye eye,
                              uint32_t x_offset) const;

  bool ShouldCreateSharedImages() const;
  void CreateSharedMailboxes();
  void ReleaseColorSwapchainImages();

  void SetXrSessionState(XrSessionState new_state);

  // The session is running only after xrBeginSession and before xrEndSession.
  // It is not considered running after creation but before xrBeginSession.
  bool session_running_;
  bool pending_frame_;

  SessionStartedCallback on_session_started_callback_;
  SessionEndedCallback on_session_ended_callback_;
  VisibilityChangedCallback visibility_changed_callback_;

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
  XrExtent2Df stage_bounds_;

  // These objects are initialized when a session begins and stay constant
  // throughout the lifetime of the session.
  XrSession session_;
  XrSpace local_space_;
  XrSpace stage_space_;
  XrSpace view_space_;
  XrSpace unbounded_space_;
  bool stage_parameters_enabled_;
  std::unordered_set<mojom::XRSessionFeature> enabled_features_;
  raw_ptr<OpenXrGraphicsBinding> graphics_binding_;

  // The swapchain is initializd when a session begins and is re-created when
  // the state of a secondary view configuration changes.
  XrSwapchain color_swapchain_;
  gfx::Size swapchain_size_;
  std::vector<SwapChainInfo> color_swapchain_images_;

  // The rest of these objects store information about the current frame and are
  // updated each frame.
  XrFrameState frame_state_;

  OpenXrViewConfiguration primary_view_config_;
  std::unordered_map<XrViewConfigurationType, OpenXrViewConfiguration>
      secondary_view_configs_;

  std::unique_ptr<OpenXrAnchorManager> anchor_manager_;
  std::unique_ptr<OpenXRSceneUnderstandingManager> scene_understanding_manager_;

  // The context provider is owned by the OpenXrRenderLoop, and may change when
  // there is a context lost.
  scoped_refptr<viz::ContextProvider> context_provider_;

  base::WeakPtrFactory<OpenXrApiWrapper> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_API_WRAPPER_H_
