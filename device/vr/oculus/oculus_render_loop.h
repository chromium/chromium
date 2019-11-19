// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OCULUS_RENDER_LOOP_H
#define DEVICE_VR_OCULUS_RENDER_LOOP_H

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "device/vr/windows/compositor_base.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/libovr/src/Include/OVR_CAPI.h"
#include "ui/gfx/geometry/rect_f.h"

#if defined(OS_WIN)
#include "device/vr/windows/d3d11_texture_helper.h"
#endif

namespace device {

const int kMaxOculusRenderLoopInputId = (ovrControllerType_Remote + 1);

class OculusRenderLoop : public XRCompositorCommon {
 public:
  OculusRenderLoop();
  ~OculusRenderLoop() override;

 private:
  // XRDeviceAbstraction:
  mojom::XRFrameDataPtr GetNextFrameData() override;
  bool StartRuntime() override;
  void StopRuntime() override;
  void OnSessionStart() override;
  bool PreComposite() override;
  bool SubmitCompositedFrame() override;
  void OnLayerBoundsChanged() override;

  // Helpers to implement XRDeviceAbstraction:
  void CreateOvrSwapChain();
  void DestroyOvrSwapChain();

  std::vector<mojom::XRInputSourceStatePtr> GetInputState(
      const ovrTrackingState& tracking_state);

  device::mojom::XRInputSourceStatePtr GetTouchData(
      ovrControllerType type,
      const ovrPoseStatef& pose,
      const ovrInputState& input_state,
      ovrHandType hand);

  long long ovr_frame_index_ = 0;
  ovrSession session_ = nullptr;
  ovrGraphicsLuid luid_ = {};
  ovrPosef last_render_pose_;
  ovrTextureSwapChain texture_swap_chain_ = 0;
  gfx::Size swap_chain_size_;
  double sensor_time_;
  bool primary_input_pressed[kMaxOculusRenderLoopInputId];

  DISALLOW_COPY_AND_ASSIGN(OculusRenderLoop);
};

}  // namespace device

#endif  // DEVICE_VR_OCULUS_RENDER_LOOP_H
