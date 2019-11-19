// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_orientation/device_orientation_event_pump.h"

#include <cmath>

#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/mojom/sensor.mojom-blink.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/platform_event_controller.h"
#include "third_party/blink/renderer/modules/device_orientation/device_orientation_data.h"
#include "third_party/blink/renderer/modules/device_orientation/device_sensor_entry.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace {

bool IsAngleDifferentThreshold(double angle1, double angle2) {
  return (std::fabs(angle1 - angle2) >=
          blink::DeviceOrientationEventPump::kOrientationThreshold);
}

bool IsSignificantlyDifferent(const blink::DeviceOrientationData* data1,
                              const blink::DeviceOrientationData* data2) {
  if (data1->CanProvideAlpha() != data2->CanProvideAlpha() ||
      data1->CanProvideBeta() != data2->CanProvideBeta() ||
      data1->CanProvideGamma() != data2->CanProvideGamma())
    return true;
  return (data1->CanProvideAlpha() &&
          IsAngleDifferentThreshold(data1->Alpha(), data2->Alpha())) ||
         (data1->CanProvideBeta() &&
          IsAngleDifferentThreshold(data1->Beta(), data2->Beta())) ||
         (data1->CanProvideGamma() &&
          IsAngleDifferentThreshold(data1->Gamma(), data2->Gamma()));
}

}  // namespace

namespace blink {

const double DeviceOrientationEventPump::kOrientationThreshold = 0.1;

DeviceOrientationEventPump::DeviceOrientationEventPump(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    bool absolute)
    : DeviceSensorEventPump(std::move(task_runner)),
      absolute_(absolute),
      fall_back_to_absolute_orientation_sensor_(!absolute) {
  relative_orientation_sensor_ = MakeGarbageCollected<DeviceSensorEntry>(
      this, device::mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES);
  absolute_orientation_sensor_ = MakeGarbageCollected<DeviceSensorEntry>(
      this, device::mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES);
}

DeviceOrientationEventPump::~DeviceOrientationEventPump() = default;

void DeviceOrientationEventPump::SetController(
    PlatformEventController* controller) {
  DCHECK(controller);
  DCHECK(!controller_);

  controller_ = controller;
  StartListening(controller_->GetDocument()
                     ? controller_->GetDocument()->GetFrame()
                     : nullptr);
}

void DeviceOrientationEventPump::RemoveController() {
  controller_ = nullptr;
  StopListening();
}

DeviceOrientationData*
DeviceOrientationEventPump::LatestDeviceOrientationData() {
  return data_.Get();
}

void DeviceOrientationEventPump::Trace(blink::Visitor* visitor) {
  visitor->Trace(relative_orientation_sensor_);
  visitor->Trace(absolute_orientation_sensor_);
  visitor->Trace(data_);
  visitor->Trace(controller_);
}

void DeviceOrientationEventPump::StartListening(LocalFrame* frame) {
  // TODO(crbug.com/850619): ensure a valid frame is passed
  if (!frame)
    return;
  Start(frame);
}

void DeviceOrientationEventPump::SendStartMessage(LocalFrame* frame) {
  if (!sensor_provider_) {
    DCHECK(frame);

    frame->GetBrowserInterfaceBroker().GetInterface(
        sensor_provider_.BindNewPipeAndPassReceiver());
    sensor_provider_.set_disconnect_handler(
        WTF::Bind(&DeviceSensorEventPump::HandleSensorProviderError,
                  WrapWeakPersistent(this)));
  }

  if (absolute_) {
    absolute_orientation_sensor_->Start(sensor_provider_.get());
  } else {
    fall_back_to_absolute_orientation_sensor_ = true;
    should_suspend_absolute_orientation_sensor_ = false;
    relative_orientation_sensor_->Start(sensor_provider_.get());
  }
}

void DeviceOrientationEventPump::StopListening() {
  Stop();
  data_.Clear();
}

void DeviceOrientationEventPump::SendStopMessage() {
  // SendStopMessage() gets called both when the page visibility changes and if
  // all device orientation event listeners are unregistered. Since removing
  // the event listener is more rare than the page visibility changing,
  // Sensor::Suspend() is used to optimize this case for not doing extra work.

  relative_orientation_sensor_->Stop();
  // This is needed in case we fallback to using the absolute orientation
  // sensor. In this case, the relative orientation sensor is marked as
  // SensorState::SHOULD_SUSPEND, and if the relative orientation sensor
  // is not available, the absolute orientation sensor should also be marked as
  // SensorState::SHOULD_SUSPEND, but only after the
  // absolute_orientation_sensor_.Start() is called for initializing
  // the absolute orientation sensor in
  // DeviceOrientationEventPump::DidStartIfPossible().
  if (relative_orientation_sensor_->state() ==
          DeviceSensorEntry::State::SHOULD_SUSPEND &&
      fall_back_to_absolute_orientation_sensor_) {
    should_suspend_absolute_orientation_sensor_ = true;
  }

  absolute_orientation_sensor_->Stop();

  // Reset the cached data because DeviceOrientationDispatcher resets its
  // data when stopping. If we don't reset here as well, then when starting back
  // up we won't notify DeviceOrientationDispatcher of the orientation, since
  // we think it hasn't changed.
  data_ = nullptr;
}

void DeviceOrientationEventPump::NotifyController() {
  DCHECK(controller_);
  controller_->DidUpdateData();
}

void DeviceOrientationEventPump::FireEvent(TimerBase*) {
  DeviceOrientationData* data = GetDataFromSharedMemory();

  if (ShouldFireEvent(data)) {
    data_ = data;
    NotifyController();
  }
}

void DeviceOrientationEventPump::DidStartIfPossible() {
  if (!absolute_ && !relative_orientation_sensor_->IsConnected() &&
      fall_back_to_absolute_orientation_sensor_ && sensor_provider_) {
    // When relative orientation sensor is not available fall back to using
    // the absolute orientation sensor but only on the first failure.
    fall_back_to_absolute_orientation_sensor_ = false;
    absolute_orientation_sensor_->Start(sensor_provider_.get());
    if (should_suspend_absolute_orientation_sensor_) {
      // The absolute orientation sensor needs to be marked as
      // SensorState::SUSPENDED when it is successfully initialized.
      absolute_orientation_sensor_->Stop();
      should_suspend_absolute_orientation_sensor_ = false;
    }
    return;
  }
  DeviceSensorEventPump::DidStartIfPossible();
}

bool DeviceOrientationEventPump::SensorsReadyOrErrored() const {
  if (!relative_orientation_sensor_->ReadyOrErrored() ||
      !absolute_orientation_sensor_->ReadyOrErrored()) {
    return false;
  }

  // At most one sensor can be successfully initialized.
  DCHECK(!relative_orientation_sensor_->IsConnected() ||
         !absolute_orientation_sensor_->IsConnected());

  return true;
}

DeviceOrientationData* DeviceOrientationEventPump::GetDataFromSharedMemory() {
  base::Optional<double> alpha;
  base::Optional<double> beta;
  base::Optional<double> gamma;
  bool absolute = false;
  bool got_reading = false;
  device::SensorReading reading;

  if (!absolute_ && relative_orientation_sensor_->GetReading(&reading)) {
    got_reading = true;
  } else if (absolute_orientation_sensor_->GetReading(&reading)) {
    got_reading = true;
    absolute = true;
  } else {
    absolute = absolute_;
  }

  if (got_reading) {
    // For DeviceOrientation Event, this provides relative orientation data.
    if (reading.timestamp() == 0.0)
      return nullptr;

    if (!std::isnan(reading.orientation_euler.z.value()))
      alpha = reading.orientation_euler.z;

    if (!std::isnan(reading.orientation_euler.x.value()))
      beta = reading.orientation_euler.x;

    if (!std::isnan(reading.orientation_euler.y.value()))
      gamma = reading.orientation_euler.y;
  }

  return DeviceOrientationData::Create(alpha, beta, gamma, absolute);
}

bool DeviceOrientationEventPump::ShouldFireEvent(
    const DeviceOrientationData* data) const {
  // |data| is null if not all sensors are active
  if (!data)
    return false;

  // when the state changes from not having data to having data,
  // the event should be fired
  if (!data_)
    return true;

  return IsSignificantlyDifferent(data_, data);
}

}  // namespace blink
