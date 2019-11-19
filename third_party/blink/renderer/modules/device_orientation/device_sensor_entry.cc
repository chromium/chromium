// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_orientation/device_sensor_entry.h"

#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer_reader.h"
#include "third_party/blink/renderer/modules/device_orientation/device_sensor_event_pump.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

DeviceSensorEntry::DeviceSensorEntry(DeviceSensorEventPump* event_pump,
                                     device::mojom::blink::SensorType type)
    : event_pump_(event_pump), type_(type) {}

void DeviceSensorEntry::Dispose() {
  client_receiver_.reset();
}

DeviceSensorEntry::~DeviceSensorEntry() = default;

void DeviceSensorEntry::Start(
    device::mojom::blink::SensorProvider* sensor_provider) {
  if (state_ == State::NOT_INITIALIZED) {
    state_ = State::INITIALIZING;
    sensor_provider->GetSensor(type_,
                               WTF::Bind(&DeviceSensorEntry::OnSensorCreated,
                                         WrapWeakPersistent(this)));
  } else if (state_ == State::SUSPENDED) {
    sensor_remote_->Resume();
    state_ = State::ACTIVE;
    event_pump_->DidStartIfPossible();
  } else if (state_ == State::SHOULD_SUSPEND) {
    // This can happen when calling Start(), Stop(), Start() in a sequence:
    // After the first Start() call, the sensor state is
    // State::INITIALIZING. Then after the Stop() call, the sensor
    // state is State::SHOULD_SUSPEND, and the next Start() call needs
    // to set the sensor state to be State::INITIALIZING again.
    state_ = State::INITIALIZING;
  } else {
    NOTREACHED();
  }
}

void DeviceSensorEntry::Stop() {
  if (sensor_remote_) {
    sensor_remote_->Suspend();
    state_ = State::SUSPENDED;
  } else if (state_ == State::INITIALIZING) {
    // When the sensor needs to be suspended, and it is still in the
    // State::INITIALIZING state, the sensor creation is not affected
    // (the DeviceSensorEntry::OnSensorCreated() callback will run as usual),
    // but the sensor is marked as State::SHOULD_SUSPEND, and when the sensor is
    // created successfully, it will be suspended and its state will be marked
    // as State::SUSPENDED in the DeviceSensorEntry::OnSensorAddConfiguration().
    state_ = State::SHOULD_SUSPEND;
  }
}

bool DeviceSensorEntry::IsConnected() const {
  return sensor_remote_.is_bound();
}

bool DeviceSensorEntry::ReadyOrErrored() const {
  // When some sensors are not available, the pump still needs to fire
  // events which set the unavailable sensor data fields to null.
  return state_ == State::ACTIVE || state_ == State::NOT_INITIALIZED;
}

bool DeviceSensorEntry::GetReading(device::SensorReading* reading) {
  if (!sensor_remote_)
    return false;

  DCHECK(shared_buffer_reader_);

  if (!shared_buffer_reader_->GetReading(reading)) {
    HandleSensorError();
    return false;
  }

  return true;
}

void DeviceSensorEntry::Trace(Visitor* visitor) {
  visitor->Trace(event_pump_);
}

void DeviceSensorEntry::RaiseError() {
  HandleSensorError();
}

void DeviceSensorEntry::SensorReadingChanged() {
  // Since DeviceSensorEventPump::FireEvent is called in a fixed
  // frequency, the |shared_buffer| is read frequently, and
  // Sensor::ConfigureReadingChangeNotifications() is set to false,
  // so this method is not called and doesn't need to be implemented.
  NOTREACHED();
}

void DeviceSensorEntry::OnSensorCreated(
    device::mojom::blink::SensorCreationResult result,
    device::mojom::blink::SensorInitParamsPtr params) {
  // |state_| can be State::SHOULD_SUSPEND if Stop() is called
  // before OnSensorCreated() is called.
  DCHECK(state_ == State::INITIALIZING || state_ == State::SHOULD_SUSPEND);

  if (!params) {
    HandleSensorError();
    event_pump_->DidStartIfPossible();
    return;
  }
  DCHECK_EQ(device::mojom::SensorCreationResult::SUCCESS, result);

  constexpr size_t kReadBufferSize = sizeof(device::SensorReadingSharedBuffer);

  DCHECK_EQ(0u, params->buffer_offset % kReadBufferSize);

  sensor_remote_.Bind(std::move(params->sensor));
  client_receiver_.Bind(std::move(params->client_receiver));

  shared_buffer_reader_ = device::SensorReadingSharedBufferReader::Create(
      std::move(params->memory), params->buffer_offset);
  if (!shared_buffer_reader_) {
    HandleSensorError();
    event_pump_->DidStartIfPossible();
    return;
  }

  device::mojom::blink::SensorConfigurationPtr config =
      std::move(params->default_configuration);
  config->frequency = std::min(
      static_cast<double>(DeviceSensorEventPump::kDefaultPumpFrequencyHz),
      params->maximum_frequency);

  sensor_remote_.set_disconnect_handler(WTF::Bind(
      &DeviceSensorEntry::HandleSensorError, WrapWeakPersistent(this)));
  sensor_remote_->ConfigureReadingChangeNotifications(/*enabled=*/false);
  sensor_remote_->AddConfiguration(
      std::move(config), WTF::Bind(&DeviceSensorEntry::OnSensorAddConfiguration,
                                   WrapWeakPersistent(this)));
}

void DeviceSensorEntry::OnSensorAddConfiguration(bool success) {
  if (!success)
    HandleSensorError();

  if (state_ == State::INITIALIZING) {
    state_ = State::ACTIVE;
    event_pump_->DidStartIfPossible();
  } else if (state_ == State::SHOULD_SUSPEND) {
    sensor_remote_->Suspend();
    state_ = State::SUSPENDED;
  }
}

void DeviceSensorEntry::HandleSensorError() {
  sensor_remote_.reset();
  state_ = State::NOT_INITIALIZED;
  shared_buffer_reader_.reset();
  client_receiver_.reset();
}

}  // namespace blink
