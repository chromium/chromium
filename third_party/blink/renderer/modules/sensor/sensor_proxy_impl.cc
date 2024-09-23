// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/sensor_proxy_impl.h"

#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/modules/sensor/sensor_provider_proxy.h"
#include "third_party/blink/renderer/modules/sensor/sensor_reading_remapper.h"

using device::mojom::blink::SensorCreationResult;

namespace blink {

SensorProxyImpl::SensorProxyImpl(device::mojom::blink::SensorType sensor_type,
                                 SensorProviderProxy* provider,
                                 Page* page)
    : SensorProxy(sensor_type, provider, page),
      sensor_remote_(provider->GetSupplementable()),
      client_receiver_(this, provider->GetSupplementable()),
      task_runner_(
          provider->GetSupplementable()->GetTaskRunner(TaskType::kSensor)),
      polling_timer_(
          provider->GetSupplementable()->GetTaskRunner(TaskType::kSensor),
          this,
          &SensorProxyImpl::OnPollingTimer) {}

SensorProxyImpl::~SensorProxyImpl() {}

void SensorProxyImpl::Trace(Visitor* visitor) const {
  visitor->Trace(sensor_remote_);
  visitor->Trace(client_receiver_);
  visitor->Trace(polling_timer_);
  SensorProxy::Trace(visitor);
}

void SensorProxyImpl::Initialize() {
  if (state_ != kUninitialized)
    return;

  if (!sensor_provider_proxy()) {
    HandleSensorError();
    return;
  }

  state_ = kInitializing;
  sensor_provider_proxy()->GetSensor(
      type_, WTF::BindOnce(&SensorProxyImpl::OnSensorCreated,
                           WrapWeakPersistent(this)));
}

void SensorProxyImpl::AddConfiguration(
    device::mojom::blink::SensorConfigurationPtr configuration,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(IsInitialized());
  AddActiveFrequency(configuration->frequency);
  sensor_remote_->AddConfiguration(std::move(configuration),
                                   std::move(callback));
}

void SensorProxyImpl::RemoveConfiguration(
    device::mojom::blink::SensorConfigurationPtr configuration) {
  DCHECK(IsInitialized());
  RemoveActiveFrequency(configuration->frequency);
  if (sensor_remote_.is_bound())
    sensor_remote_->RemoveConfiguration(std::move(configuration));
}

double SensorProxyImpl::GetDefaultFrequency() const {
  DCHECK(IsInitialized());
  return default_frequency_;
}

std::pair<double, double> SensorProxyImpl::GetFrequencyLimits() const {
  DCHECK(IsInitialized());
  return frequency_limits_;
}

void SensorProxyImpl::Suspend() {
  if (suspended_ || !sensor_remote_.is_bound())
    return;

  sensor_remote_->Suspend();
  suspended_ = true;
  UpdatePollingStatus();
}

void SensorProxyImpl::Resume() {
  if (!suspended_ || !sensor_remote_.is_bound())
    return;

  sensor_remote_->Resume();
  suspended_ = false;
  UpdatePollingStatus();
}

void SensorProxyImpl::UpdateSensorReading() {
  DCHECK(ShouldProcessReadings());
  DCHECK(shared_buffer_reader_);

  // Try to read the latest value from shared memory. Failure should not be
  // fatal because we only retry a finite number of times.
  device::SensorReading reading_data;
  if (!shared_buffer_reader_->GetReading(&reading_data))
    return;

  double latest_timestamp = reading_data.timestamp();
  if (reading_.timestamp() != latest_timestamp &&
      latest_timestamp != 0.0)  // The shared buffer is zeroed when
                                // sensor is stopped, we skip this
                                // reading.
  {
    DCHECK_GT(latest_timestamp, reading_.timestamp())
        << "Timestamps must increase monotonically";
    reading_ = reading_data;
    for (Observer* observer : observers_)
      observer->OnSensorReadingChanged();
  }
}

void SensorProxyImpl::RaiseError() {
  HandleSensorError();
}

void SensorProxyImpl::SensorReadingChanged() {
  DCHECK_EQ(device::mojom::blink::ReportingMode::ON_CHANGE, mode_);
  if (ShouldProcessReadings())
    UpdateSensorReading();
}

void SensorProxyImpl::ReportError(DOMExceptionCode code,
                                  const String& message) {
  state_ = kUninitialized;
  active_frequencies_.clear();
  reading_ = device::SensorReading();
  UpdatePollingStatus();

  sensor_remote_.reset();
  shared_buffer_reader_.reset();
  default_frequency_ = 0.0;
  frequency_limits_ = {0.0, 0.0};
  client_receiver_.reset();

  SensorProxy::ReportError(code, message);
}

void SensorProxyImpl::HandleSensorError(SensorCreationResult error) {
  if (error == SensorCreationResult::ERROR_NOT_ALLOWED) {
    String description = "Permissions to access sensor are not granted";
    ReportError(DOMExceptionCode::kNotAllowedError, std::move(description));
  } else {
    ReportError(DOMExceptionCode::kNotReadableError, kDefaultErrorDescription);
  }
}

void SensorProxyImpl::OnSensorCreated(
    SensorCreationResult result,
    device::mojom::blink::SensorInitParamsPtr params) {
  DCHECK_EQ(kInitializing, state_);
  if (!params) {
    DCHECK_NE(SensorCreationResult::SUCCESS, result);
    HandleSensorError(result);
    return;
  }

  DCHECK_EQ(SensorCreationResult::SUCCESS, result);

  mode_ = params->mode;
  if (!params->default_configuration) {
    HandleSensorError();
    return;
  }

  default_frequency_ = params->default_configuration->frequency;
  DCHECK_GT(default_frequency_, 0.0);

  sensor_remote_.Bind(std::move(params->sensor), task_runner_);
  client_receiver_.Bind(std::move(params->client_receiver), task_runner_);

  shared_buffer_reader_ = device::SensorReadingSharedBufferReader::Create(
      std::move(params->memory), params->buffer_offset);
  if (!shared_buffer_reader_) {
    HandleSensorError();
    return;
  }

  device::SensorReading reading;
  if (!shared_buffer_reader_->GetReading(&reading)) {
    HandleSensorError();
    return;
  }
  reading_ = std::move(reading);

  frequency_limits_.first = params->minimum_frequency;
  frequency_limits_.second = params->maximum_frequency;

  DCHECK_GT(frequency_limits_.first, 0.0);
  DCHECK_GE(frequency_limits_.second, frequency_limits_.first);
  DCHECK_GE(device::GetSensorMaxAllowedFrequency(type_),
            frequency_limits_.second);

  auto error_callback = WTF::BindOnce(
      &SensorProxyImpl::HandleSensorError, WrapWeakPersistent(this),
      SensorCreationResult::ERROR_NOT_AVAILABLE);
  sensor_remote_.set_disconnect_handler(std::move(error_callback));

  state_ = kInitialized;

  UpdateSuspendedStatus();

  for (Observer* observer : observers_)
    observer->OnSensorInitialized();
}

void SensorProxyImpl::OnPollingTimer(TimerBase*) {
  UpdateSensorReading();
}

bool SensorProxyImpl::ShouldProcessReadings() const {
  return IsInitialized() && !suspended_ && !active_frequencies_.empty();
}

void SensorProxyImpl::UpdatePollingStatus() {
  if (mode_ != device::mojom::blink::ReportingMode::CONTINUOUS)
    return;

  if (ShouldProcessReadings()) {
    // TODO(crbug/721297) : We need to find out an algorithm for resulting
    // polling frequency.
    polling_timer_.StartRepeating(base::Seconds(1 / active_frequencies_.back()),
                                  FROM_HERE);
  } else {
    polling_timer_.Stop();
  }
}

void SensorProxyImpl::RemoveActiveFrequency(double frequency) {
  // Can use binary search as active_frequencies_ is sorted.
  Vector<double>::iterator it = std::lower_bound(
      active_frequencies_.begin(), active_frequencies_.end(), frequency);
  if (it == active_frequencies_.end() || *it != frequency) {
    NOTREACHED_IN_MIGRATION()
        << "Attempted to remove active frequency which is not present "
           "in the list";
    return;
  }

  active_frequencies_.erase(it);
  UpdatePollingStatus();

  if (active_frequencies_.empty())
    reading_ = device::SensorReading();
}

void SensorProxyImpl::AddActiveFrequency(double frequency) {
  Vector<double>::iterator it = std::lower_bound(
      active_frequencies_.begin(), active_frequencies_.end(), frequency);
  if (it == active_frequencies_.end()) {
    active_frequencies_.push_back(frequency);
  } else {
    active_frequencies_.insert(
        static_cast<wtf_size_t>(std::distance(active_frequencies_.begin(), it)),
        frequency);
  }
  UpdatePollingStatus();
}

}  // namespace blink
