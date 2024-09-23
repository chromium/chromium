// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_pump.h"

#include <cmath>

#include "base/numerics/angle_conversions.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/mojom/sensor.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/platform_event_controller.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_data.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_acceleration.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_pump.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_rotation_rate.h"
#include "third_party/blink/renderer/modules/device_orientation/device_sensor_entry.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace {

constexpr double kDefaultPumpDelayMilliseconds =
    blink::DeviceMotionEventPump::kDefaultPumpDelayMicroseconds / 1000;

}  // namespace

namespace blink {

DeviceMotionEventPump::DeviceMotionEventPump(LocalFrame& frame)
    : DeviceSensorEventPump(frame) {
  accelerometer_ = MakeGarbageCollected<DeviceSensorEntry>(
      this, frame.DomWindow(), device::mojom::blink::SensorType::ACCELEROMETER);
  linear_acceleration_sensor_ = MakeGarbageCollected<DeviceSensorEntry>(
      this, frame.DomWindow(),
      device::mojom::blink::SensorType::LINEAR_ACCELERATION);
  gyroscope_ = MakeGarbageCollected<DeviceSensorEntry>(
      this, frame.DomWindow(), device::mojom::blink::SensorType::GYROSCOPE);
}

DeviceMotionEventPump::~DeviceMotionEventPump() = default;

void DeviceMotionEventPump::SetController(PlatformEventController* controller) {
  DCHECK(controller);
  DCHECK(!controller_);

  controller_ = controller;
  StartListening(*controller_->GetWindow().GetFrame());
}

void DeviceMotionEventPump::RemoveController() {
  controller_ = nullptr;
  StopListening();
}

DeviceMotionData* DeviceMotionEventPump::LatestDeviceMotionData() {
  return data_.Get();
}

void DeviceMotionEventPump::Trace(Visitor* visitor) const {
  visitor->Trace(accelerometer_);
  visitor->Trace(linear_acceleration_sensor_);
  visitor->Trace(gyroscope_);
  visitor->Trace(data_);
  visitor->Trace(controller_);
  DeviceSensorEventPump::Trace(visitor);
}

void DeviceMotionEventPump::StartListening(LocalFrame& frame) {
  Start(frame);
}

void DeviceMotionEventPump::SendStartMessage(LocalFrame& frame) {
  if (!sensor_provider_.is_bound()) {
    frame.GetBrowserInterfaceBroker().GetInterface(
        sensor_provider_.BindNewPipeAndPassReceiver(
            frame.GetTaskRunner(TaskType::kSensor)));
    sensor_provider_.set_disconnect_handler(
        WTF::BindOnce(&DeviceSensorEventPump::HandleSensorProviderError,
                      WrapWeakPersistent(this)));
  }

  accelerometer_->Start(sensor_provider_.get());
  linear_acceleration_sensor_->Start(sensor_provider_.get());
  gyroscope_->Start(sensor_provider_.get());
}

void DeviceMotionEventPump::StopListening() {
  Stop();
  data_.Clear();
}

void DeviceMotionEventPump::SendStopMessage() {
  // SendStopMessage() gets called both when the page visibility changes and if
  // all device motion event listeners are unregistered. Since removing the
  // event listener is more rare than the page visibility changing,
  // Sensor::Suspend() is used to optimize this case for not doing extra work.

  accelerometer_->Stop();
  linear_acceleration_sensor_->Stop();
  gyroscope_->Stop();
}

void DeviceMotionEventPump::NotifyController() {
  DCHECK(controller_);
  controller_->DidUpdateData();
}

void DeviceMotionEventPump::FireEvent(TimerBase*) {
  DeviceMotionData* data = GetDataFromSharedMemory();

  // data is null if not all sensors are active
  if (data) {
    data_ = data;
    NotifyController();
  }
}

bool DeviceMotionEventPump::SensorsReadyOrErrored() const {
  return accelerometer_->ReadyOrErrored() &&
         linear_acceleration_sensor_->ReadyOrErrored() &&
         gyroscope_->ReadyOrErrored();
}

DeviceMotionData* DeviceMotionEventPump::GetDataFromSharedMemory() {
  DeviceMotionEventAcceleration* acceleration = nullptr;
  DeviceMotionEventAcceleration* acceleration_including_gravity = nullptr;
  DeviceMotionEventRotationRate* rotation_rate = nullptr;

  device::SensorReading accelerometer_reading;
  if (accelerometer_->GetReading(&accelerometer_reading)) {
    if (accelerometer_reading.timestamp() == 0.0)
      return nullptr;

    acceleration_including_gravity = DeviceMotionEventAcceleration::Create(
        accelerometer_reading.accel.x, accelerometer_reading.accel.y,
        accelerometer_reading.accel.z);
  } else {
    acceleration_including_gravity =
        DeviceMotionEventAcceleration::Create(NAN, NAN, NAN);
  }

  device::SensorReading linear_acceleration_sensor_reading;
  if (linear_acceleration_sensor_->GetReading(
          &linear_acceleration_sensor_reading)) {
    if (linear_acceleration_sensor_reading.timestamp() == 0.0)
      return nullptr;

    acceleration = DeviceMotionEventAcceleration::Create(
        linear_acceleration_sensor_reading.accel.x,
        linear_acceleration_sensor_reading.accel.y,
        linear_acceleration_sensor_reading.accel.z);
  } else {
    acceleration = DeviceMotionEventAcceleration::Create(NAN, NAN, NAN);
  }

  device::SensorReading gyroscope_reading;
  if (gyroscope_->GetReading(&gyroscope_reading)) {
    if (gyroscope_reading.timestamp() == 0.0)
      return nullptr;

    rotation_rate = DeviceMotionEventRotationRate::Create(
        base::RadToDeg(gyroscope_reading.gyro.x.value()),
        base::RadToDeg(gyroscope_reading.gyro.y.value()),
        base::RadToDeg(gyroscope_reading.gyro.z.value()));
  } else {
    rotation_rate = DeviceMotionEventRotationRate::Create(NAN, NAN, NAN);
  }

  // The device orientation spec states that interval should be in
  // milliseconds.
  // https://w3c.github.io/deviceorientation/spec-source-orientation.html#devicemotion
  return DeviceMotionData::Create(acceleration, acceleration_including_gravity,
                                  rotation_rate, kDefaultPumpDelayMilliseconds);
}

}  // namespace blink
