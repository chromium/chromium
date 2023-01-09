// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_WINDOWS_COMPOSITOR_BASE_H_
#define DEVICE_VR_WINDOWS_COMPOSITOR_BASE_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/util/fps_meter.h"
#include "device/vr/util/sliding_average.h"
#include "device/vr/vr_device.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "ui/gfx/geometry/rect_f.h"

#if BUILDFLAG(IS_WIN)
#include "device/vr/windows/d3d11_texture_helper.h"
#endif

namespace gpu::gles2 {
class GLES2Interface;
}  // namespace gpu::gles2

namespace device {

enum class ExitXrPresentReason : int32_t {
  kUnknown = 0,
  kMojoConnectionError = 1,
  kOpenXrUninitialize = 2,
  kStartRuntimeFailed = 3,
  kOpenXrStartFailed = 4,
  kXrEndFrameFailed = 5,
  kGetFrameAfterSessionEnded = 6,
  kSubmitFrameFailed = 7,
  kBrowserShutdown = 8,
};

class XRDeviceAbstraction {
 public:
  virtual mojom::XRFrameDataPtr GetNextFrameData();

  using StartRuntimeCallback = base::OnceCallback<void(bool success)>;
  virtual void StartRuntime(StartRuntimeCallback start_runtime_callback) = 0;
  virtual void StopRuntime() = 0;
  virtual void OnSessionStart();
  virtual bool HasSessionEnded();
  virtual bool SubmitCompositedFrame() = 0;
  virtual void HandleDeviceLost();
  virtual void OnLayerBoundsChanged();
  // Sets enabled_features_ based on what features are supported
  virtual void EnableSupportedFeatures(
      const std::vector<device::mojom::XRSessionFeature>& requiredFeatures,
      const std::vector<device::mojom::XRSessionFeature>& optionalFeatures) = 0;
  virtual device::mojom::XREnvironmentBlendMode GetEnvironmentBlendMode(
      device::mojom::XRSessionMode session_mode);
  virtual device::mojom::XRInteractionMode GetInteractionMode(
      device::mojom::XRSessionMode session_mode);
  virtual bool CanEnableAntiAliasing() const;
  virtual std::vector<mojom::XRViewPtr> GetDefaultViews() const = 0;
};

class XRCompositorCommon : public base::Thread,
                           public XRDeviceAbstraction,
                           public mojom::XRPresentationProvider,
                           public mojom::XRFrameDataProvider,
                           public mojom::ImmersiveOverlay {
 public:
  using RequestSessionCallback =
      base::OnceCallback<void(bool result, mojom::XRSessionPtr)>;

  XRCompositorCommon();

  XRCompositorCommon(const XRCompositorCommon&) = delete;
  XRCompositorCommon& operator=(const XRCompositorCommon&) = delete;

  ~XRCompositorCommon() override;

  void RequestSession(base::RepeatingCallback<void(mojom::XRVisibilityState)>
                          on_visibility_state_changed,
                      mojom::XRRuntimeSessionOptionsPtr options,
                      RequestSessionCallback callback);
  void ExitPresent(ExitXrPresentReason reason);

  void GetFrameData(mojom::XRFrameDataRequestOptionsPtr options,
                    XRFrameDataProvider::GetFrameDataCallback callback) final;
  void SetInputSourceButtonListener(
      mojo::PendingAssociatedRemote<device::mojom::XRInputSourceButtonListener>
          input_listener_remote) override;

  void GetEnvironmentIntegrationProvider(
      mojo::PendingAssociatedReceiver<
          device::mojom::XREnvironmentIntegrationProvider> environment_provider)
      override;

  void RequestOverlay(mojo::PendingReceiver<mojom::ImmersiveOverlay> receiver);

  virtual gpu::gles2::GLES2Interface* GetContextGL() = 0;

 protected:
  virtual bool UsesInputEventing();
  void SetVisibilityState(mojom::XRVisibilityState visibility_state);
  const mojom::VRStageParametersPtr& GetCurrentStageParameters() const;
  void SetStageParameters(mojom::VRStageParametersPtr stage_parameters);
#if BUILDFLAG(IS_WIN)
  D3D11TextureHelper texture_helper_{this};
#endif
  int16_t next_frame_id_ = 0;

  // Allow derived classes to call methods on the main thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  mojo::AssociatedRemote<mojom::XRInputSourceButtonListener>
      input_event_listener_;

  // Derived classes override this to be notified to clear its pending frame.
  virtual void ClearPendingFrameInternal() {}

  std::unordered_set<device::mojom::XRSessionFeature> enabled_features_;

  // Override the default of false if you wish to use shared buffers across
  // processes
  virtual bool IsUsingSharedImages() const;

#if BUILDFLAG(IS_WIN)
  void SubmitFrameWithTextureHandle(int16_t frame_index,
                                    mojo::PlatformHandle texture_handle,
                                    const gpu::SyncToken& sync_token) final;
#endif

 private:
  // base::Thread overrides:
  void Init() final;
  void CleanUp() final;

  void ClearPendingFrame();
  void StartPendingFrame();

  void StartRuntimeFinish(
      base::RepeatingCallback<void(mojom::XRVisibilityState)>
          on_visibility_state_changed,
      mojom::XRRuntimeSessionOptionsPtr options,
      RequestSessionCallback callback,
      bool success);

  // Will Submit if we have textures submitted from the Overlay (if it is
  // visible), and WebXR (if it is visible).  We decide what to wait for during
  // StartPendingFrame, may mark things as ready after SubmitFrameMissing and
  // SubmitFrameWithTextureHandle (for WebXR), or SubmitOverlayTexture (for
  // overlays), or SetOverlayAndWebXRVisibility (for WebXR and overlays).
  // Finally, if we exit presentation while waiting for outstanding submits, we
  // will clean up our pending-frame state.
  void MaybeCompositeAndSubmit();

  // XRPresentationProvider overrides:
  void SubmitFrameMissing(int16_t frame_index, const gpu::SyncToken&) final;
  void SubmitFrame(int16_t frame_index,
                   const gpu::MailboxHolder& mailbox,
                   base::TimeDelta time_waited) final;
  void SubmitFrameDrawnIntoTexture(int16_t frame_index,
                                   const gpu::SyncToken&,
                                   base::TimeDelta time_waited) override;
  void UpdateLayerBounds(int16_t frame_id,
                         const gfx::RectF& left_bounds,
                         const gfx::RectF& right_bounds,
                         const gfx::Size& source_size) final;

  // ImmersiveOverlay:
  void SubmitOverlayTexture(int16_t frame_id,
                            mojo::PlatformHandle texture,
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

  FPSMeter fps_meter_;
  SlidingTimeDeltaAverage webxr_js_time_;
  SlidingTimeDeltaAverage webxr_gpu_time_;

  absl::optional<OutstandingFrame> pending_frame_;

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
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_COMPOSITOR_BASE_H_
