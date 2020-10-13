// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_RENDER_LOOP_H_
#define DEVICE_VR_OPENXR_OPENXR_RENDER_LOOP_H_

#include <stdint.h>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "device/vr/windows/compositor_base.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

struct XrView;

namespace device {

class OpenXrApiWrapper;
class OpenXRInputHelper;

class OpenXrRenderLoop : public XRCompositorCommon {
 public:
  OpenXrRenderLoop(base::RepeatingCallback<void(mojom::VRDisplayInfoPtr)>
                       on_display_info_changed,
                   XrInstance instance);
  ~OpenXrRenderLoop() override;

 private:
  // XRCompositorCommon:
  void ClearPendingFrameInternal() override;

  // XRDeviceAbstraction:
  mojom::XRFrameDataPtr GetNextFrameData() override;
  bool StartRuntime() override;
  void StopRuntime() override;
  void OnSessionStart() override;
  bool PreComposite() override;
  bool HasSessionEnded() override;
  bool SubmitCompositedFrame() override;

  void InitializeDisplayInfo();
  bool UpdateEyeParameters();
  bool UpdateEye(const XrView& view_head,
                 const gfx::Size& view_size,
                 mojom::VREyeParametersPtr* eye) const;
  bool UpdateStageParameters();

  // Owned by OpenXrStatics
  XrInstance instance_;

  std::unique_ptr<OpenXrApiWrapper> openxr_;
  std::unique_ptr<OpenXRInputHelper> input_helper_;
  XrExtent2Df current_stage_bounds_;

  base::RepeatingCallback<void(mojom::VRDisplayInfoPtr)>
      on_display_info_changed_;
  mojom::VRDisplayInfoPtr current_display_info_;

  // This must be the last member
  base::WeakPtrFactory<OpenXrRenderLoop> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OpenXrRenderLoop);
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_RENDER_LOOP_H_
