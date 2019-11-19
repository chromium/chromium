// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_INPUT_HELPER_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_INPUT_HELPER_H_

#include <windows.perception.spatial.h>
#include <windows.ui.input.spatial.h>
#include <wrl.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/callback_list.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

namespace device {

class WMRCoordinateSystem;
class WMRInputManager;
class WMRInputSourceState;
class WMRInputSourceEventArgs;
class WMRTimestamp;
class MixedRealityRenderLoop;
class MixedRealityInputHelper {
 public:
  // Note that the WeakPtr should be resolvable on the thread this is created
  // on, (which should be the render loop thread).
  MixedRealityInputHelper(
      HWND hwnd,
      const base::WeakPtr<MixedRealityRenderLoop>& weak_render_loop);
  virtual ~MixedRealityInputHelper();
  std::vector<mojom::XRInputSourceStatePtr> GetInputState(
      const WMRCoordinateSystem* origin,
      const WMRTimestamp* timestamp);

  void Dispose();

 private:
  bool EnsureSpatialInteractionManager();

  mojom::XRInputSourceStatePtr ParseWindowsSourceState(
      const WMRInputSourceState* state,
      const WMRCoordinateSystem* origin);

  // These event subscriptions can come back on a different thread, while
  // everything else is expected to come back on the same thread.
  void OnSourcePressed(const WMRInputSourceEventArgs& args);
  void OnSourceReleased(const WMRInputSourceEventArgs& args);

  void ProcessSourceEvent(
      ABI::Windows::UI::Input::Spatial::SpatialInteractionPressKind press_kind,
      std::unique_ptr<WMRInputSourceState> state,
      bool is_pressed);

  void SubscribeEvents();
  void UnsubscribeEvents();

  std::unique_ptr<WMRInputManager> input_manager_;
  std::unique_ptr<
      base::CallbackList<void(const WMRInputSourceEventArgs&)>::Subscription>
      pressed_subscription_;
  std::unique_ptr<
      base::CallbackList<void(const WMRInputSourceEventArgs&)>::Subscription>
      released_subscription_;

  struct ControllerState {
    bool pressed = false;
    bool clicked = false;
    base::Optional<gfx::Transform> grip_from_pointer = base::nullopt;
    ControllerState();
    virtual ~ControllerState();
  };
  std::unordered_map<uint32_t, ControllerState> controller_states_;
  HWND hwnd_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Must be resolved on the task_runner_ thread, which is the thread we were
  // created on (and should correspond to the render loop thread)
  base::WeakPtr<MixedRealityRenderLoop> weak_render_loop_;

  base::WeakPtrFactory<MixedRealityInputHelper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MixedRealityInputHelper);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_MIXED_REALITY_INPUT_HELPER_H_
