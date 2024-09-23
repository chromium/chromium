/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/device_orientation/device_motion_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_device_motion_event_init.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_controller.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_data.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_acceleration.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_rotation_rate.h"

namespace blink {

DeviceMotionEvent::~DeviceMotionEvent() = default;

DeviceMotionEvent::DeviceMotionEvent()
    : device_motion_data_(DeviceMotionData::Create()) {}

DeviceMotionEvent::DeviceMotionEvent(const AtomicString& event_type,
                                     const DeviceMotionEventInit* initializer)
    : Event(event_type, initializer),
      device_motion_data_(DeviceMotionData::Create(initializer)) {}

DeviceMotionEvent::DeviceMotionEvent(const AtomicString& event_type,
                                     const DeviceMotionData* device_motion_data)
    : Event(event_type, Bubbles::kNo, Cancelable::kNo),
      device_motion_data_(device_motion_data) {}

DeviceMotionEventAcceleration* DeviceMotionEvent::acceleration() {
  return device_motion_data_->GetAcceleration();
}

DeviceMotionEventAcceleration*
DeviceMotionEvent::accelerationIncludingGravity() {
  return device_motion_data_->GetAccelerationIncludingGravity();
}

DeviceMotionEventRotationRate* DeviceMotionEvent::rotationRate() {
  return device_motion_data_->GetRotationRate();
}

double DeviceMotionEvent::interval() const {
  return device_motion_data_->Interval();
}

// static
ScriptPromise<V8DeviceOrientationPermissionState>
DeviceMotionEvent::requestPermission(ScriptState* script_state) {
  if (!script_state->ContextIsValid())
    return EmptyPromise();

  auto* window = To<LocalDOMWindow>(ExecutionContext::From(script_state));
  if (!window) {
    NOTREACHED_IN_MIGRATION();
    return EmptyPromise();
  }

  return DeviceMotionController::From(*window).RequestPermission(script_state);
}
const AtomicString& DeviceMotionEvent::InterfaceName() const {
  return event_interface_names::kDeviceMotionEvent;
}

void DeviceMotionEvent::Trace(Visitor* visitor) const {
  visitor->Trace(device_motion_data_);
  Event::Trace(visitor);
}

}  // namespace blink
