/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/device_orientation/device_motion_data.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_device_motion_event_init.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_acceleration.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_rotation_rate.h"

namespace blink {

DeviceMotionData* DeviceMotionData::Create(
    DeviceMotionEventAcceleration* acceleration,
    DeviceMotionEventAcceleration* acceleration_including_gravity,
    DeviceMotionEventRotationRate* rotation_rate,
    double interval) {
  return MakeGarbageCollected<DeviceMotionData>(
      acceleration, acceleration_including_gravity, rotation_rate, interval);
}

DeviceMotionData* DeviceMotionData::Create(const DeviceMotionEventInit* init) {
  return DeviceMotionData::Create(
      init->hasAcceleration()
          ? DeviceMotionEventAcceleration::Create(init->acceleration())
          : nullptr,
      init->hasAccelerationIncludingGravity()
          ? DeviceMotionEventAcceleration::Create(
                init->accelerationIncludingGravity())
          : nullptr,
      init->hasRotationRate()
          ? DeviceMotionEventRotationRate::Create(init->rotationRate())
          : nullptr,
      init->interval());
}

DeviceMotionData* DeviceMotionData::Create() {
  return MakeGarbageCollected<DeviceMotionData>();
}

DeviceMotionData::DeviceMotionData() : interval_(0) {}

DeviceMotionData::DeviceMotionData(
    DeviceMotionEventAcceleration* acceleration,
    DeviceMotionEventAcceleration* acceleration_including_gravity,
    DeviceMotionEventRotationRate* rotation_rate,
    double interval)
    : acceleration_(acceleration),
      acceleration_including_gravity_(acceleration_including_gravity),
      rotation_rate_(rotation_rate),
      interval_(interval) {}

void DeviceMotionData::Trace(Visitor* visitor) const {
  visitor->Trace(acceleration_);
  visitor->Trace(acceleration_including_gravity_);
  visitor->Trace(rotation_rate_);
}

bool DeviceMotionData::CanProvideEventData() const {
  const bool has_acceleration =
      acceleration_ && acceleration_->HasAccelerationData();
  const bool has_acceleration_including_gravity =
      acceleration_including_gravity_ &&
      acceleration_including_gravity_->HasAccelerationData();
  const bool has_rotation_rate =
      rotation_rate_ && rotation_rate_->HasRotationData();

  return has_acceleration || has_acceleration_including_gravity ||
         has_rotation_rate;
}

}  // namespace blink
