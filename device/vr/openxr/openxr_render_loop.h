// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_RENDER_LOOP_H_
#define DEVICE_VR_OPENXR_OPENXR_RENDER_LOOP_H_

#include <stdint.h>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "device/vr/openxr/context_provider_callbacks.h"
#include "device/vr/openxr/openxr_anchor_manager.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/windows/compositor_base.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace gfx {
class GpuFence;
}  // namespace gfx

namespace device {

class OpenXrApiWrapper;

class OpenXrRenderLoop : public XRCompositorCommon,
                         public mojom::XREnvironmentIntegrationProvider,
                         public viz::ContextLostObserver {
 public:
  OpenXrRenderLoop(
      VizContextProviderFactoryAsync context_provider_factory_async,
      XrInstance instance,
      const OpenXrExtensionHelper& extension_helper_);

  OpenXrRenderLoop(const OpenXrRenderLoop&) = delete;
  OpenXrRenderLoop& operator=(const OpenXrRenderLoop&) = delete;

  ~OpenXrRenderLoop() override;

 private:
  // XRCompositorCommon:
  gpu::gles2::GLES2Interface* GetContextGL() override;
  void ClearPendingFrameInternal() override;
  bool IsUsingSharedImages() const override;
  void SubmitFrameDrawnIntoTexture(int16_t frame_index,
                                   const gpu::SyncToken&,
                                   base::TimeDelta time_waited) override;

  // XRDeviceAbstraction:
  mojom::XRFrameDataPtr GetNextFrameData() override;
  void StartRuntime(StartRuntimeCallback start_runtime_callback) override;
  void StopRuntime() override;
  void OnSessionStart() override;
  bool HasSessionEnded() override;
  bool SubmitCompositedFrame() override;
  void EnableSupportedFeatures(
      const std::vector<device::mojom::XRSessionFeature>& requiredFeatures,
      const std::vector<device::mojom::XRSessionFeature>& optionalFeatures)
      override;
  device::mojom::XREnvironmentBlendMode GetEnvironmentBlendMode(
      device::mojom::XRSessionMode session_mode) override;
  device::mojom::XRInteractionMode GetInteractionMode(
      device::mojom::XRSessionMode session_mode) override;
  bool CanEnableAntiAliasing() const override;
  std::vector<mojom::XRViewPtr> GetDefaultViews() const override;

  // viz::ContextLostObserver Implementation
  void OnContextLost() override;

  void OnOpenXrSessionStarted(StartRuntimeCallback start_runtime_callback,
                              XrResult result);
  bool UpdateViews();
  bool UpdateView(const XrView& view_head,
                  int width,
                  int height,
                  mojom::XRViewPtr* view) const;
  void UpdateStageParameters();

  // XREnvironmentIntegrationProvider
  void GetEnvironmentIntegrationProvider(
      mojo::PendingAssociatedReceiver<
          device::mojom::XREnvironmentIntegrationProvider> environment_provider)
      override;
  void SubscribeToHitTest(
      mojom::XRNativeOriginInformationPtr native_origin_information,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray,
      mojom::XREnvironmentIntegrationProvider::SubscribeToHitTestCallback
          callback) override;
  void SubscribeToHitTestForTransientInput(
      const std::string& profile_name,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray,
      mojom::XREnvironmentIntegrationProvider::
          SubscribeToHitTestForTransientInputCallback callback) override;
  void UnsubscribeFromHitTest(uint64_t subscription_id) override;
  void CreateAnchor(
      mojom::XRNativeOriginInformationPtr native_origin_information,
      const device::Pose& native_origin_from_anchor,
      CreateAnchorCallback callback) override;
  void CreatePlaneAnchor(
      mojom::XRNativeOriginInformationPtr native_origin_information,
      const device::Pose& native_origin_from_anchor,
      uint64_t plane_id,
      CreatePlaneAnchorCallback callback) override;
  void DetachAnchor(uint64_t anchor_id) override;

  void ProcessCreateAnchorRequests(
      OpenXrAnchorManager* anchor_manager,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state);

  void StartContextProviderIfNeeded(
      StartRuntimeCallback start_runtime_callback);
  void OnContextProviderCreated(
      StartRuntimeCallback start_runtime_callback,
      scoped_refptr<viz::ContextProvider> context_provider);
  void OnContextLostCallback(
      scoped_refptr<viz::ContextProvider> context_provider);

  void OnWebXrTokenSignaled(int16_t frame_index,
                            GLuint id,
                            std::unique_ptr<gfx::GpuFence> gpu_fence);

  bool IsFeatureEnabled(device::mojom::XRSessionFeature feature) const;

  // Owned by OpenXrStatics
  XrInstance instance_;
  const raw_ref<const OpenXrExtensionHelper> extension_helper_;

  std::unique_ptr<OpenXrApiWrapper> openxr_;

  mojo::AssociatedReceiver<mojom::XREnvironmentIntegrationProvider>
      environment_receiver_{this};

  scoped_refptr<viz::ContextProvider> context_provider_;
  VizContextProviderFactoryAsync context_provider_factory_async_;

  // This must be the last member
  base::WeakPtrFactory<OpenXrRenderLoop> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_RENDER_LOOP_H_
