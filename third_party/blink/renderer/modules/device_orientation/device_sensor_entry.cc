// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_orientation/device_sensor_entry.h"

#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer_reader.h"
#include "services/device/public/mojom/sensor_provider.mojom-blink.h"
#include "third_party/blink/public/mojom/sensor/web_sensor_provider.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/device_orientation/device_sensor_event_pump.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

DeviceSensorEntry::DeviceSensorEntry(DeviceSensorEventPump* event_pump,
                                     ExecutionContext* context,
                                     device::mojom::blink::SensorType type)
    : event_pump_(event_pump),
      sensor_remote_(context),
      client_receiver_(this, context),
      type_(type) {}

DeviceSensorEntry::~DeviceSensorEntry() = default;

void DeviceSensorEntry::Start(
    mojom::blink::WebSensorProvider* sensor_provider) {
  // If sensor remote is not bound, reset to |kNotInitialized| state (in case
  // we're in some other state), unless we're currently being initialized (which
  // is indicated by either |kInitializing| or |kShouldSuspend| state).
  if (!sensor_remote_.is_bound() && state_ != State::kInitializing &&
      state_ != State::kShouldSuspend) {
    state_ = State::kNotInitialized;
  }

  if (state_ == State::kNotInitialized) {
    state_ = State::kInitializing;
    sensor_provider->GetSensor(
        type_, WTF::BindOnce(&DeviceSensorEntry::OnSensorCreated,
                             WrapWeakPersistent(this)));
  } else if (state_ == State::kSuspended) {
    sensor_remote_->Resume();
    state_ = State::kActive;
    event_pump_->DidStartIfPossible();
  } else if (state_ == State::kShouldSuspend) {
    // This can happen when calling Start(), Stop(), Start() in a sequence:
    // After the first Start() call, the sensor state is
    // State::INITIALIZING. Then after the Stop() call, the sensor
    // state is State::SHOULD_SUSPEND, and the next Start() call needs
    // to set the sensor state to be State::INITIALIZING again.
    state_ = State::kInitializing;
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void DeviceSensorEntry::Stop() {
  if (sensor_remote_.is_bound()) {
    sensor_remote_->Suspend();
    state_ = State::kSuspended;
  } else if (state_ == State::kInitializing) {
    // When the sensor needs to be suspended, and it is still in the
    // State::INITIALIZING state, the sensor creation is not affected
    // (the DeviceSensorEntry::OnSensorCreated() callback will run as usual),
    // but the sensor is marked as State::SHOULD_SUSPEND, and when the sensor is
    // created successfully, it will be suspended and its state will be marked
    // as State::SUSPENDED in the DeviceSensorEntry::OnSensorAddConfiguration().
    state_ = State::kShouldSuspend;
  }
}

bool DeviceSensorEntry::IsConnected() const {
  return sensor_remote_.is_bound();
}

bool DeviceSensorEntry::ReadyOrErrored() const {
  // When some sensors are not available, the pump still needs to fire
  // events which set the unavailable sensor data fields to null.
  return state_ == State::kActive || state_ == State::kNotInitialized;
}

bool DeviceSensorEntry::GetReading(device::SensorReading* reading) {
  if (!sensor_remote_.is_bound())
    return false;

  DCHECK(shared_buffer_reader_);

  if (!shared_buffer_reader_->GetReading(reading)) {
    HandleSensorError();
    return false;
  }

  return true;
}

void DeviceSensorEntry::Trace(Visitor* visitor) const {
  visitor->Trace(event_pump_);
  visitor->Trace(sensor_remote_);
  visitor->Trace(client_receiver_);
}

void DeviceSensorEntry::RaiseError() {
  HandleSensorError();
}

void DeviceSensorEntry::SensorReadingChanged() {
  // Since DeviceSensorEventPump::FireEvent is called in a fixed
  // frequency, the |shared_buffer| is read frequently, and
  // Sensor::ConfigureReadingChangeNotifications() is set to false,
  // so this method is not called and doesn't need to be implemented.
  LOG(ERROR) << "SensorReadingChanged";
}

void DeviceSensorEntry::OnSensorCreated(
    device::mojom::blink::SensorCreationResult result,
    device::mojom::blink::SensorInitParamsPtr params) {
  // |state_| can be State::SHOULD_SUSPEND if Stop() is called
  // before OnSensorCreated() is called.
  DCHECK(state_ == State::kInitializing || state_ == State::kShouldSuspend);

  if (!params) {
    HandleSensorError();
    event_pump_->DidStartIfPossible();
    return;
  }
  DCHECK_EQ(device::mojom::SensorCreationResult::SUCCESS, result);

  constexpr size_t kReadBufferSize = sizeof(device::SensorReadingSharedBuffer);

  DCHECK_EQ(0u, params->buffer_offset % kReadBufferSize);

  sensor_remote_.Bind(std::move(params->sensor), event_pump_->task_runner_);
  client_receiver_.Bind(std::move(params->client_receiver),
                        event_pump_->task_runner_);

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

  sensor_remote_.set_disconnect_handler(WTF::BindOnce(
      &DeviceSensorEntry::HandleSensorError, WrapWeakPersistent(this)));
  sensor_remote_->ConfigureReadingChangeNotifications(/*enabled=*/false);
  sensor_remote_->AddConfiguration(
      std::move(config),
      WTF::BindOnce(&DeviceSensorEntry::OnSensorAddConfiguration,
                    WrapWeakPersistent(this)));
}

void DeviceSensorEntry::OnSensorAddConfiguration(bool success) {
  if (!success)
    HandleSensorError();

  if (state_ == State::kInitializing) {
    state_ = State::kActive;
    event_pump_->DidStartIfPossible();
  } else if (state_ == State::kShouldSuspend) {
    sensor_remote_->Suspend();
    state_ = State::kSuspended;
  }
}

void DeviceSensorEntry::HandleSensorError() {
  sensor_remote_.reset();
  state_ = State::kNotInitialized;
  shared_buffer_reader_.reset();
  client_receiver_.reset();
}

}  // namespace blink
