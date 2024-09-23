// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_RENDER_LOOP_H_
#define DEVICE_VR_OPENXR_OPENXR_RENDER_LOOP_H_

#include <stdint.h>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "device/vr/openxr/context_provider_callbacks.h"
#include "device/vr/openxr/exit_xr_present_reason.h"
#include "device/vr/openxr/openxr_anchor_manager.h"
#include "device/vr/openxr/openxr_graphics_binding.h"
#include "device/vr/openxr/openxr_platform_helper.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "device/vr/util/fps_meter.h"
#include "device/vr/util/sliding_average.h"
#include "device/vr/vr_device.h"
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
#include "ui/gfx/geometry/rect_f.h"

#if BUILDFLAG(IS_WIN)
#include "base/threading/thread.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/java_handler_thread.h"
#endif

namespace gpu::gles2 {
class GLES2Interface;
}  // namespace gpu::gles2

namespace gfx {
class GpuFence;
}  // namespace gfx

namespace device {

class OpenXrApiWrapper;

#if BUILDFLAG(IS_ANDROID)
class XRThread : public base::android::JavaHandlerThread {
 public:
  explicit XRThread(const char* name)
      : base::android::JavaHandlerThread(name) {}
  ~XRThread() override = default;
};
#elif BUILDFLAG(IS_WIN)
class XRThread : public base::Thread {
 public:
  explicit XRThread(const char* name) : base::Thread(name) {}
  ~XRThread() override = default;
};
#else
#error "Trying to build OpenXR for an unsupported platform"
#endif

class OpenXrRenderLoop : public XRThread,
                         public mojom::XRPresentationProvider,
                         public mojom::XRFrameDataProvider,
                         public mojom::ImmersiveOverlay,
                         public mojom::XREnvironmentIntegrationProvider,
                         public viz::ContextLostObserver {
 public:
  using RequestSessionCallback =
      base::OnceCallback<void(bool result,
                              mojom::XRSessionPtr,
                              mojo::PendingRemote<mojom::ImmersiveOverlay>)>;

  OpenXrRenderLoop(
      VizContextProviderFactoryAsync context_provider_factory_async,
      XrInstance instance,
      const OpenXrExtensionHelper& extension_helper_,
      OpenXrPlatformHelper* platform_helper);

  OpenXrRenderLoop(const OpenXrRenderLoop&) = delete;
  OpenXrRenderLoop& operator=(const OpenXrRenderLoop&) = delete;

  ~OpenXrRenderLoop() override;

  void ExitPresent(ExitXrPresentReason reason);

  gpu::gles2::GLES2Interface* GetContextGL();

  void GetFrameData(
      mojom::XRFrameDataRequestOptionsPtr options,
      XRFrameDataProvider::GetFrameDataCallback callback) override;

  void RequestSession(base::RepeatingCallback<void(mojom::XRVisibilityState)>
                          on_visibility_state_changed,
                      mojom::XRRuntimeSessionOptionsPtr options,
                      RequestSessionCallback callback);

 private:
  void SetVisibilityState(mojom::XRVisibilityState visibility_state);
  void SetStageParameters(mojom::VRStageParametersPtr stage_parameters);

  // base::Thread overrides:
  void CleanUp() override;

  void ClearPendingFrame();
  void StartPendingFrame();

  void StartRuntimeFinish(
      base::RepeatingCallback<void(mojom::XRVisibilityState)>
          on_visibility_state_changed,
      mojom::XRRuntimeSessionOptionsPtr options,
      bool success);

  // Will Submit if we have textures submitted from the Overlay (if it is
  // visible), and WebXR (if it is visible).  We decide what to wait for during
  // StartPendingFrame, may mark things as ready after SubmitFrameMissing and
  // SubmitFrameWithTextureHandle (for WebXR), or SubmitOverlayTexture (for
  // overlays), or SetOverlayAndWebXRVisibility (for WebXR and overlays).
  // Finally, if we exit presentation while waiting for outstanding submits, we
  // will clean up our pending-frame state.
  void MaybeCompositeAndSubmit();

  // Sets all relevant internal state to mark that we have successfully received
  // a frame. Will return whether or not the given frame index was expected.
  // If not expected, not all state may be successfully cleared.
  bool MarkFrameSubmitted(int16_t frame_index);

  // XRPresentationProvider overrides:
#if BUILDFLAG(IS_WIN)
  void SubmitFrameWithTextureHandle(int16_t frame_index,
                                    mojo::PlatformHandle texture_handle,
                                    const gpu::SyncToken& sync_token) override;
#endif
  void SubmitFrameMissing(int16_t frame_index, const gpu::SyncToken&) override;
  void SubmitFrame(int16_t frame_index,
                   const gpu::MailboxHolder& mailbox,
                   base::TimeDelta time_waited) final;
  void SubmitFrameDrawnIntoTexture(int16_t frame_index,
                                   const gpu::SyncToken&,
                                   base::TimeDelta time_waited) override;
  void UpdateLayerBounds(int16_t frame_id,
                         const gfx::RectF& left_bounds,
                         const gfx::RectF& right_bounds,
                         const gfx::Size& source_size) override;

  // ImmersiveOverlay:
  void SubmitOverlayTexture(int16_t frame_id,
                            gfx::GpuMemoryBufferHandle texture,
                            const gpu::SyncToken& sync_token,
                            const gfx::RectF& left_bounds,
                            const gfx::RectF& right_bounds,
                            SubmitOverlayTextureCallback callback) override;
  void RequestNextOverlayPose(RequestNextOverlayPoseCallback callback) override;
  void SetOverlayAndWebXRVisibility(bool overlay_visible,
                                    bool webxr_visible) override;
  void RequestNotificationOnWebXrSubmitted(
      RequestNotificationOnWebXrSubmittedCallback callback) override;

  void SendFrameData(XRFrameDataProvider::GetFrameDataCallback callback,
                     mojom::XRFrameDataPtr frame_data);

  struct OutstandingFrame {
    OutstandingFrame();
    ~OutstandingFrame();
    bool webxr_has_pose_ = false;
    bool overlay_has_pose_ = false;
    bool webxr_submitted_ = false;
    bool overlay_submitted_ = false;
    bool waiting_for_webxr_ = false;
    bool waiting_for_overlay_ = false;

    mojom::XRFrameDataPtr frame_data_;
    mojom::XRRenderInfoPtr render_info_;

    base::TimeTicks sent_frame_data_time_;
    base::TimeTicks submit_frame_time_;
    base::TimeTicks frame_ready_time_;
  };

  mojom::XRFrameDataPtr GetNextFrameData();

  // TODO(crbug.com/41489956): Investigate removing this callback.
  using ContextProviderAcquiredCallback =
      base::OnceCallback<void(bool success)>;

  void StartRuntime(base::RepeatingCallback<void(mojom::XRVisibilityState)>
                        on_visibility_state_changed,
                    mojom::XRRuntimeSessionOptionsPtr options);
  void StopRuntime();
  void OnSessionStart();
  bool HasSessionEnded();
  bool SubmitCompositedFrame();

  // viz::ContextLostObserver Implementation
  void OnContextLost() override;

  void OnOpenXrSessionStarted(
      base::RepeatingCallback<void(mojom::XRVisibilityState)>
          on_visibility_state_changed,
      mojom::XRRuntimeSessionOptionsPtr options,
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

  void StartContextProviderIfNeeded(ContextProviderAcquiredCallback callback);
  void OnContextProviderCreated(
      ContextProviderAcquiredCallback start_runtime_callback,
      scoped_refptr<viz::ContextProvider> context_provider);
  void OnContextLostCallback(
      scoped_refptr<viz::ContextProvider> context_provider);

  void OnWebXrTokenSignaled(int16_t frame_index,
                            GLuint id,
                            std::unique_ptr<gfx::GpuFence> gpu_fence);

  void MaybeRejectSessionCallback();

  bool IsFeatureEnabled(device::mojom::XRSessionFeature feature) const;
  int16_t next_frame_id_ = 0;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  // Owned by OpenXrStatics
  XrInstance instance_;
  const raw_ref<const OpenXrExtensionHelper> extension_helper_;

  scoped_refptr<viz::ContextProvider> context_provider_;
  VizContextProviderFactoryAsync context_provider_factory_async_;

  FPSMeter fps_meter_;
  SlidingTimeDeltaAverage webxr_js_time_;
  SlidingTimeDeltaAverage webxr_gpu_time_;

  std::optional<OutstandingFrame> pending_frame_;

  bool is_presenting_ = false;  // True if we have a presenting session.
  bool webxr_visible_ = true;   // The browser may hide a presenting session.
  bool overlay_visible_ = false;
  base::OnceCallback<void()> delayed_get_frame_data_callback_;

  gfx::RectF left_webxr_bounds_;
  gfx::RectF right_webxr_bounds_;
  gfx::Size source_size_;

  mojo::Remote<mojom::XRPresentationClient> submit_client_;
  SubmitOverlayTextureCallback overlay_submit_callback_;
  RequestNotificationOnWebXrSubmittedCallback on_webxr_submitted_;
  bool webxr_has_pose_ = false;
  base::RepeatingCallback<void(mojom::XRVisibilityState)>
      on_visibility_state_changed_;
  mojo::Receiver<mojom::XRPresentationProvider> presentation_receiver_{this};
  mojo::Receiver<mojom::XRFrameDataProvider> frame_data_receiver_{this};
  mojo::Receiver<mojom::ImmersiveOverlay> overlay_receiver_{this};
  mojom::XRVisibilityState visibility_state_ =
      mojom::XRVisibilityState::VISIBLE;
  mojom::VRStageParametersPtr current_stage_parameters_;
  uint32_t stage_parameters_id_;

  // Lifetime of the platform helper is guaranteed by the OpenXrDevice.
  raw_ptr<OpenXrPlatformHelper> platform_helper_;
  std::unique_ptr<OpenXrGraphicsBinding> graphics_binding_;
  std::unique_ptr<OpenXrApiWrapper> openxr_;

  mojo::AssociatedReceiver<mojom::XREnvironmentIntegrationProvider>
      environment_receiver_{this};

  RequestSessionCallback request_session_callback_;

  // This must be the last member
  base::WeakPtrFactory<OpenXrRenderLoop> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_RENDER_LOOP_H_
