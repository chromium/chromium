// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENVR_RENDER_LOOP_H
#define DEVICE_VR_OPENVR_RENDER_LOOP_H

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "device/vr/windows/compositor_base.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/openvr/src/headers/openvr.h"
#include "ui/gfx/geometry/rect_f.h"

#if defined(OS_WIN)
#include "device/vr/windows/d3d11_texture_helper.h"
#endif

namespace device {

class OpenVRWrapper;

class OpenVRRenderLoop : public XRCompositorCommon {
 public:
  OpenVRRenderLoop();
  ~OpenVRRenderLoop() override;

 private:
  // XRDeviceAbstraction:
  mojom::XRFrameDataPtr GetNextFrameData() override;
  bool StartRuntime() override;
  void StopRuntime() override;
  void OnSessionStart() override;
  bool PreComposite() override;
  bool SubmitCompositedFrame() override;

  // Helpers to implement XRDeviceAbstraction.
  mojom::VRPosePtr GetPose();
  std::vector<mojom::XRInputSourceStatePtr> GetInputState(
      vr::TrackedDevicePose_t* poses,
      uint32_t count);

  struct InputActiveState {
    bool active;
    bool primary_input_pressed;
    vr::ETrackedDeviceClass device_class;
    vr::ETrackedControllerRole controller_role;

    std::vector<std::string> profiles;

    InputActiveState();
    ~InputActiveState();
    void MarkAsInactive();

    DISALLOW_COPY_AND_ASSIGN(InputActiveState);
  };

  InputActiveState input_active_states_[vr::k_unMaxTrackedDeviceCount];
  std::unique_ptr<OpenVRWrapper> openvr_;

  DISALLOW_COPY_AND_ASSIGN(OpenVRRenderLoop);
};

}  // namespace device

#endif  // DEVICE_VR_OPENVR_RENDER_LOOP_H
