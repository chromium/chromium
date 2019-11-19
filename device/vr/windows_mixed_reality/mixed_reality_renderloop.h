// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_RENDERLOOP_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_RENDERLOOP_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/win/scoped_winrt_initializer.h"
#include "build/build_config.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "device/vr/windows/compositor_base.h"
#include "device/vr/windows/d3d11_texture_helper.h"
#include "device/vr/windows_mixed_reality/mixed_reality_input_helper.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/win/window_impl.h"

namespace device {

class MixedRealityWindow;
class WMRAttachedOrigin;
class WMRCamera;
class WMRCameraPose;
class WMRCoordinateSystem;
class WMRHolographicFrame;
class WMRHolographicSpace;
class WMRRenderingParameters;
class WMRStageOrigin;
class WMRStageStatics;
class WMRTimestamp;

class MixedRealityRenderLoop : public XRCompositorCommon {
 public:
  explicit MixedRealityRenderLoop(
      base::RepeatingCallback<void(mojom::VRDisplayInfoPtr)>
          on_display_info_changed);
  ~MixedRealityRenderLoop() override;

  const WMRCoordinateSystem* GetOrigin();
  void OnInputSourceEvent(mojom::XRInputSourceStatePtr input_state);

 private:
  // XRCompositorCommon:
  bool StartRuntime() override;
  void StopRuntime() override;
  void OnSessionStart() override;
  bool UsesInputEventing() override;

  // XRDeviceAbstraction:
  mojom::XRFrameDataPtr GetNextFrameData() override;
  bool PreComposite() override;
  bool SubmitCompositedFrame() override;

  // Helpers to implement XRDeviceAbstraction.
  void InitializeOrigin();
  void InitializeSpace();
  void StartPresenting();
  void UpdateWMRDataForNextFrame();

  // Returns true if display info has changed. Does not update stage parameters.
  bool UpdateDisplayInfo();
  // Returns true if stage parameters have changed.
  bool UpdateStageParameters();

  // Helper methods for the stage.
  void ClearStageOrigin();
  void InitializeStageOrigin();
  bool EnsureStageStatics();
  void ClearStageStatics();
  void OnCurrentStageChanged();

  void OnUserPresenceChanged();
  void UpdateVisibilityState();

  // Will try to update the stage bounds if the following are true:
  // 1) We have a spatial_stage.
  // 2) That spatial stage supports bounded movement.
  // 3) The current bounds array is empty.
  void EnsureStageBounds();

  void OnWindowDestroyed();

  std::unique_ptr<base::win::ScopedWinrtInitializer> initializer_;

  std::unique_ptr<WMRHolographicSpace> holographic_space_;
  std::unique_ptr<WMRStageOrigin> spatial_stage_;
  std::unique_ptr<WMRCoordinateSystem> stationary_origin_;
  std::unique_ptr<WMRCoordinateSystem> stage_origin_;
  std::unique_ptr<WMRCoordinateSystem> anchor_origin_;
  bool stage_transform_needs_updating_ = false;
  std::unique_ptr<WMRAttachedOrigin> attached_;
  bool emulated_position_ = false;
  base::Optional<gfx::Transform> last_origin_from_attached_;

  std::unique_ptr<MixedRealityWindow> window_;
  mojom::VRDisplayInfoPtr current_display_info_;
  base::RepeatingCallback<void(mojom::VRDisplayInfoPtr)>
      on_display_info_changed_;

  // Per frame data:
  std::unique_ptr<WMRHolographicFrame> holographic_frame_;
  std::unique_ptr<WMRTimestamp> timestamp_;

  // We only support one headset at a time - this is the one pose.
  std::unique_ptr<WMRCameraPose> pose_;
  std::unique_ptr<WMRRenderingParameters> rendering_params_;
  std::unique_ptr<WMRCamera> camera_;

  std::unique_ptr<MixedRealityInputHelper> input_helper_;

  std::unique_ptr<WMRStageStatics> stage_statics_;
  std::unique_ptr<base::CallbackList<void()>::Subscription>
      stage_changed_subscription_;

  std::unique_ptr<base::CallbackList<void()>::Subscription>
      user_presence_changed_subscription_;

  std::vector<gfx::Point3F> bounds_;
  bool bounds_updated_ = false;

  // This must be the last member
  base::WeakPtrFactory<MixedRealityRenderLoop> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MixedRealityRenderLoop);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_RENDERLOOP_H_
