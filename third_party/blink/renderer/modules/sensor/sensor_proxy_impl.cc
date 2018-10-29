// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/sensor_proxy_impl.h"

#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/modules/sensor/sensor_provider_proxy.h"
#include "third_party/blink/renderer/modules/sensor/sensor_reading_remapper.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"

namespace blink {

using namespace device::mojom::blink;

SensorProxyImpl::SensorProxyImpl(SensorType sensor_type,
                                 SensorProviderProxy* provider,
                                 Page* page)
    : SensorProxy(sensor_type, provider, page),
      client_binding_(this),
      polling_timer_(
          provider->GetSupplementable()->GetTaskRunner(TaskType::kSensor),
          this,
          &SensorProxyImpl::OnPollingTimer) {}

SensorProxyImpl::~SensorProxyImpl() {}

void SensorProxyImpl::Dispose() {
  client_binding_.Close();
}

void SensorProxyImpl::Trace(blink::Visitor* visitor) {
  SensorProxy::Trace(visitor);
}

void SensorProxyImpl::Initialize() {
  if (state_ != kUninitialized)
    return;

  if (!sensor_provider()) {
    HandleSensorError();
    return;
  }

  state_ = kInitializing;
  auto callback =
      WTF::Bind(&SensorProxyImpl::OnSensorCreated, WrapWeakPersistent(this));
  sensor_provider()->GetSensor(type_, std::move(callback));
}

void SensorProxyImpl::AddConfiguration(
    SensorConfigurationPtr configuration,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(IsInitialized());
  AddActiveFrequency(configuration->frequency);
  sensor_->AddConfiguration(std::move(configuration), std::move(callback));
}

void SensorProxyImpl::RemoveConfiguration(
    SensorConfigurationPtr configuration) {
  DCHECK(IsInitialized());
  RemoveActiveFrequency(configuration->frequency);
  sensor_->RemoveConfiguration(std::move(configuration));
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
  if (suspended_)
    return;

  sensor_->Suspend();
  suspended_ = true;
  UpdatePollingStatus();
}

void SensorProxyImpl::Resume() {
  if (!suspended_)
    return;

  sensor_->Resume();
  suspended_ = false;
  UpdatePollingStatus();
}

void SensorProxyImpl::UpdateSensorReading() {
  DCHECK(ShouldProcessReadings());
  DCHECK(shared_buffer_handle_->is_valid());

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
  DCHECK_EQ(ReportingMode::ON_CHANGE, mode_);
  if (ShouldProcessReadings())
    UpdateSensorReading();
}

void SensorProxyImpl::ReportError(DOMExceptionCode code,
                                  const String& message) {
  state_ = kUninitialized;
  active_frequencies_.clear();
  reading_ = device::SensorReading();
  UpdatePollingStatus();

  // The m_sensor.reset() will release all callbacks and its bound parameters,
  // therefore, handleSensorError accepts messages by value.
  sensor_.reset();
  shared_buffer_.reset();
  shared_buffer_handle_.reset();
  default_frequency_ = 0.0;
  frequency_limits_ = {0.0, 0.0};
  client_binding_.Close();

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

void SensorProxyImpl::OnSensorCreated(SensorCreationResult result,
                                      SensorInitParamsPtr params) {
  DCHECK_EQ(kInitializing, state_);
  if (!params) {
    DCHECK_NE(SensorCreationResult::SUCCESS, result);
    HandleSensorError(result);
    return;
  }

  DCHECK_EQ(SensorCreationResult::SUCCESS, result);
  const size_t kReadBufferSize = sizeof(ReadingBuffer);

  DCHECK_EQ(0u, params->buffer_offset % kReadBufferSize);

  mode_ = params->mode;
  if (!params->default_configuration) {
    HandleSensorError();
    return;
  }

  default_frequency_ = params->default_configuration->frequency;
  DCHECK_GT(default_frequency_, 0.0);

  sensor_.Bind(std::move(params->sensor));
  client_binding_.Bind(std::move(params->client_request));

  shared_buffer_handle_ = std::move(params->memory);
  DCHECK(!shared_buffer_);
  shared_buffer_ = shared_buffer_handle_->MapAtOffset(kReadBufferSize,
                                                      params->buffer_offset);

  if (!shared_buffer_) {
    HandleSensorError();
    return;
  }

  const auto* buffer = static_cast<const device::SensorReadingSharedBuffer*>(
      shared_buffer_.get());
  shared_buffer_reader_ =
      std::make_unique<device::SensorReadingSharedBufferReader>(buffer);
  shared_buffer_reader_->GetReading(&reading_);

  frequency_limits_.first = params->minimum_frequency;
  frequency_limits_.second = params->maximum_frequency;

  DCHECK_GT(frequency_limits_.first, 0.0);
  DCHECK_GE(frequency_limits_.second, frequency_limits_.first);
  DCHECK_GE(device::GetSensorMaxAllowedFrequency(type_),
            frequency_limits_.second);

  auto error_callback =
      WTF::Bind(&SensorProxyImpl::HandleSensorError, WrapWeakPersistent(this),
                SensorCreationResult::ERROR_NOT_AVAILABLE);
  sensor_.set_connection_error_handler(std::move(error_callback));

  state_ = kInitialized;

  UpdateSuspendedStatus();

  for (Observer* observer : observers_)
    observer->OnSensorInitialized();
}

void SensorProxyImpl::OnPollingTimer(TimerBase*) {
  UpdateSensorReading();
}

bool SensorProxyImpl::ShouldProcessReadings() const {
  return IsInitialized() && !suspended_ && !active_frequencies_.IsEmpty();
}

void SensorProxyImpl::UpdatePollingStatus() {
  if (mode_ != ReportingMode::CONTINUOUS)
    return;

  if (ShouldProcessReadings()) {
    // TODO(crbug/721297) : We need to find out an algorithm for resulting
    // polling frequency.
    polling_timer_.StartRepeating(
        WTF::TimeDelta::FromSecondsD(1 / active_frequencies_.back()),
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
    NOTREACHED() << "Attempted to remove active frequency which is not present "
                    "in the list";
    return;
  }

  active_frequencies_.erase(it);
  UpdatePollingStatus();

  if (active_frequencies_.IsEmpty())
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
