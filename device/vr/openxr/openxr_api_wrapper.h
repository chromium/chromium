// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_API_WRAPPER_H_
#define DEVICE_VR_OPENXR_OPENXR_API_WRAPPER_H_

#include <d3d11_4.h>
#include <stdint.h>
#include <wrl.h>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"

#include "device/vr/openxr/openxr_anchor_manager.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_export.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "third_party/openxr/src/include/openxr/openxr_platform.h"

namespace gfx {
class Quaternion;
class Point3F;
class Size;
class Transform;
}  // namespace gfx

namespace viz {
class ContextProvider;
}  // namespace viz

namespace device {

class OpenXRInputHelper;
class VRTestHook;
class ServiceTestHook;

using SessionEndedCallback = base::RepeatingCallback<void()>;
using VisibilityChangedCallback =
    base::RepeatingCallback<void(mojom::XRVisibilityState)>;

class OpenXrApiWrapper {
 public:
  OpenXrApiWrapper();
  ~OpenXrApiWrapper();
  bool IsInitialized() const;

  static std::unique_ptr<OpenXrApiWrapper> Create(XrInstance instance);

  static VRTestHook* GetTestHook();

  bool UpdateAndGetSessionEnded();

  XrResult InitSession(
      const Microsoft::WRL::ComPtr<ID3D11Device>& d3d_device,
      const OpenXrExtensionHelper& extension_helper,
      const SessionEndedCallback& on_session_ended_callback,
      const VisibilityChangedCallback& visibility_changed_callback);

  XrSpace GetReferenceSpace(device::mojom::XRReferenceSpaceType type) const;

  XrResult BeginFrame(Microsoft::WRL::ComPtr<ID3D11Texture2D>* texture,
                      gpu::MailboxHolder* mailbox_holder);
  XrResult EndFrame();
  bool HasPendingFrame() const;
  bool HasFrameState() const;

  XrResult GetHeadPose(base::Optional<gfx::Quaternion>* orientation,
                       base::Optional<gfx::Point3F>* position,
                       bool* emulated_position) const;
  void GetHeadFromEyes(XrView* left, XrView* right) const;
  std::vector<mojom::XRInputSourceStatePtr> GetInputState(
      bool hand_input_enabled);

  gfx::Size GetViewSize() const;
  XrTime GetPredictedDisplayTime() const;
  XrResult GetLuid(LUID* luid,
                   const OpenXrExtensionHelper& extension_helper) const;
  bool GetStageParameters(XrExtent2Df* stage_bounds,
                          gfx::Transform* local_from_stage);

  device::mojom::XREnvironmentBlendMode PickEnvironmentBlendModeForSession(
      device::mojom::XRSessionMode session_mode);

  OpenXrAnchorManager* GetOrCreateAnchorManager(
      const OpenXrExtensionHelper& extension_helper);

  void CreateSharedMailboxes(viz::ContextProvider* context_provider);

  bool CanEnableAntiAliasing() const;
  bool IsUsingSharedImages() const;

  static void DEVICE_VR_EXPORT SetTestHook(VRTestHook* hook);
  void StoreFence(Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence,
                  int16_t frame_index);

 private:
  void Reset();
  bool Initialize(XrInstance instance);
  void Uninitialize();

  XrResult InitializeSystem();
  XrResult InitializeEnvironmentBlendMode(XrSystemId system);
  XrResult ProcessEvents();
  void EnsureEventPolling();

  XrResult CreateSession(
      const Microsoft::WRL::ComPtr<ID3D11Device>& d3d_device);
  XrResult CreateSwapchain();
  XrResult CreateSpace(XrReferenceSpaceType type, XrSpace* space);

  XrResult BeginSession();
  XrResult UpdateProjectionLayers();
  XrResult LocateViews(XrReferenceSpaceType type,
                       std::vector<XrView>* views) const;

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

  bool ShouldCreateSharedImages() const;

  // The session is running only after xrBeginSession and before xrEndSession.
  // It is not considered running after creation but before xrBeginSession.
  bool session_running_;
  bool pending_frame_;

  VisibilityChangedCallback visibility_changed_callback_;
  SessionEndedCallback on_session_ended_callback_;

  // Testing objects
  static VRTestHook* test_hook_;
  static ServiceTestHook* service_test_hook_;

  std::unique_ptr<OpenXRInputHelper> input_helper_;

  // OpenXR objects

  // These objects are valid on successful initialization.
  XrInstance instance_;
  XrSystemId system_;
  std::vector<XrViewConfigurationView> view_configs_;
  XrEnvironmentBlendMode blend_mode_;
  XrExtent2Df stage_bounds_;

  // These objects are valid only while a session is active,
  // and stay constant throughout the lifetime of a session.
  XrSession session_;
  XrSwapchain color_swapchain_;

  // When shared images are being used, there is a corresponding MailboxHolder
  // and D3D11Fence for each D3D11 texture in the vector.
  struct SwapChainInfo {
    explicit SwapChainInfo(ID3D11Texture2D*);
    ~SwapChainInfo();
    SwapChainInfo(SwapChainInfo&&);

    ID3D11Texture2D* d3d11_texture = nullptr;
    gpu::MailboxHolder mailbox_holder;
    Microsoft::WRL::ComPtr<ID3D11Fence> d3d11_fence;
  };
  std::vector<SwapChainInfo> color_swapchain_images_;

  XrSpace local_space_;
  XrSpace stage_space_;
  XrSpace view_space_;
  XrSpace unbounded_space_;

  // These objects store information about the current frame. They're
  // valid only while a session is active, and they are updated each frame.
  XrFrameState frame_state_;
  std::vector<XrView> origin_from_eye_views_;
  std::vector<XrView> head_from_eye_views_;
  std::vector<XrCompositionLayerProjectionView> layer_projection_views_;

  std::unique_ptr<OpenXrAnchorManager> anchor_manager_;

  base::WeakPtrFactory<OpenXrApiWrapper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OpenXrApiWrapper);
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_API_WRAPPER_H_
