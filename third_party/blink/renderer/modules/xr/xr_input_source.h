// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_INPUT_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_INPUT_SOURCE_H_

#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad.h"
#include "third_party/blink/renderer/modules/xr/xr_native_origin_information.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace device {
class Gamepad;
}

namespace blink {

class XRGripSpace;
class XRInputSourceEvent;
class XRSession;
class XRSpace;
class XRTargetRaySpace;

class XRInputSource : public ScriptWrappable, public Gamepad::Client {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(XRInputSource);

 public:
  static XRInputSource* CreateOrUpdateFrom(
      XRInputSource* other /* may be null, input */,
      XRSession* session,
      const device::mojom::blink::XRInputSourceStatePtr& state);

  XRInputSource(XRSession*,
                uint32_t source_id,
                device::mojom::XRTargetRayMode target_ray_mode =
                    device::mojom::XRTargetRayMode::GAZING);
  XRInputSource(const XRInputSource& other);
  ~XRInputSource() override = default;

  int16_t activeFrameId() const { return state_.active_frame_id; }
  void setActiveFrameId(int16_t id) { state_.active_frame_id = id; }

  XRSession* session() const { return session_; }

  const String handedness() const;
  const String targetRayMode() const;
  bool emulatedPosition() const { return state_.emulated_position; }
  XRSpace* targetRaySpace() const;
  XRSpace* gripSpace() const;
  Gamepad* gamepad() const { return gamepad_; }
  Vector<String> profiles() const { return state_.profiles; }

  uint32_t source_id() const { return state_.source_id; }

  void SetInputFromPointer(const TransformationMatrix*);
  void SetGamepadConnected(bool state);

  // Gamepad::Client
  GamepadHapticActuator* GetVibrationActuatorForGamepad(
      const Gamepad&) override {
    // TODO(https://crbug.com/955097): XRInputSource implementation of
    // Gamepad::Client must manage vibration actuator state in a similar way to
    // NavigatorGamepad.
    return nullptr;
  }

  device::mojom::XRTargetRayMode TargetRayMode() const {
    return state_.target_ray_mode;
  }
  const TransformationMatrix* MojoFromInput() const {
    return mojo_from_input_.get();
  }
  const TransformationMatrix* InputFromPointer() const {
    return input_from_pointer_.get();
  }

  base::Optional<XRNativeOriginInformation> nativeOrigin() const;

  void OnSelectStart();
  void OnSelectEnd();
  void OnSelect();
  void UpdateSelectState(
      const device::mojom::blink::XRInputSourceStatePtr& state);
  void OnRemoved();

  void Trace(blink::Visitor*) override;

 private:
  // In order to ease copying, any new member variables that can be trivially
  // copied (except for Member<T> variables), should go here
  struct InternalState {
    int16_t active_frame_id = -1;
    bool primary_input_pressed = false;
    bool selection_cancelled = false;
    const uint32_t source_id;
    device::mojom::XRHandedness handedness = device::mojom::XRHandedness::NONE;
    device::mojom::XRTargetRayMode target_ray_mode;
    bool emulated_position = false;
    base::TimeTicks base_timestamp;
    Vector<String> profiles;

    InternalState(uint32_t source_id,
                  device::mojom::XRTargetRayMode,
                  base::TimeTicks base_timestamp);
    InternalState(const InternalState& other);
    ~InternalState();
  };

  // Use to check if the updates that would/should be made by a given
  // XRInputSourceState would invalidate any SameObject properties guaranteed
  // by the idl, and thus require the xr_input_source to be recreated.
  bool InvalidatesSameObject(
      const device::mojom::blink::XRInputSourceStatePtr& state);

  // Note that UpdateGamepad should only be called after a check/recreation
  // from InvalidatesSameObject
  void UpdateGamepad(const base::Optional<device::Gamepad>& gamepad);

  XRInputSourceEvent* CreateInputSourceEvent(const AtomicString& type);

  // These member variables all require special behavior when being copied or
  // are Member<T> type variables.  When adding another one, be sure to keep the
  // deep-copy constructor updated, when adding any new variable.
  InternalState state_;
  const Member<XRSession> session_;
  Member<XRTargetRaySpace> target_ray_space_;
  Member<XRGripSpace> grip_space_;
  Member<Gamepad> gamepad_;

  // Input device pose in mojo space. This is the grip pose for
  // tracked controllers, and the viewer pose for screen input.
  std::unique_ptr<TransformationMatrix> mojo_from_input_;

  // Pointer pose in input device space, this is the transform to apply to
  // mojo_from_input_ to get the pointer matrix. In most cases it should be
  // static.
  std::unique_ptr<TransformationMatrix> input_from_pointer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_INPUT_SOURCE_H_
