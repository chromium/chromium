// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_chromeos.h"

#include <iterator>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"

namespace device {

namespace {

constexpr char kAxes[][3] = {"_x", "_y", "_z"};

}  // namespace

PlatformSensorChromeOS::PlatformSensorChromeOS(
    int32_t iio_device_id,
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    base::WeakPtr<PlatformSensorProvider> provider,
    mojo::ConnectionErrorWithReasonCallback sensor_device_disconnect_callback,
    double scale,
    mojo::Remote<chromeos::sensors::mojom::SensorDevice> sensor_device_remote)
    : PlatformSensor(type, reading_buffer, std::move(provider)),
      iio_device_id_(iio_device_id),
      sensor_device_disconnect_callback_(
          std::move(sensor_device_disconnect_callback)),
      default_configuration_(
          PlatformSensorConfiguration(GetSensorMaxAllowedFrequency(type))),
      scale_(scale),
      sensor_device_remote_(std::move(sensor_device_remote)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sensor_device_remote_.is_bound());
  DCHECK_GT(scale_, 0.0);

  sensor_device_remote_.set_disconnect_with_reason_handler(
      base::BindOnce(&PlatformSensorChromeOS::OnSensorDeviceDisconnect,
                     weak_factory_.GetWeakPtr()));

  sensor_device_remote_->SetTimeout(0);

  sensor_device_remote_->GetAllChannelIds(
      base::BindOnce(&PlatformSensorChromeOS::GetAllChannelIdsCallback,
                     weak_factory_.GetWeakPtr()));
}

PlatformSensorChromeOS::~PlatformSensorChromeOS() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

mojom::ReportingMode PlatformSensorChromeOS::GetReportingMode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetType() == mojom::SensorType::AMBIENT_LIGHT)
    return mojom::ReportingMode::ON_CHANGE;
  return mojom::ReportingMode::CONTINUOUS;
}

void PlatformSensorChromeOS::OnSampleUpdated(
    const base::flat_map<int32_t, int64_t>& sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!channel_indices_.empty());

  if (sample.size() != channel_indices_.size()) {
    LOG(WARNING) << "Invalid sample with size: " << sample.size();
    OnErrorOccurred(chromeos::sensors::mojom::ObserverErrorType::READ_FAILED);
    return;
  }

  for (auto index : channel_indices_) {
    if (!base::Contains(sample, index)) {
      LOG(ERROR) << "Missing channel: " << iio_channel_ids_[index]
                 << " in sample.";
      OnErrorOccurred(chromeos::sensors::mojom::ObserverErrorType::READ_FAILED);
      return;
    }
  }

  if (num_failed_reads_ > 0 && ++num_recovery_reads_ == kNumRecoveryReads) {
    num_recovery_reads_ = 0;
    --num_failed_reads_;
  }

  SensorReading reading;

  switch (GetType()) {
    case mojom::SensorType::AMBIENT_LIGHT:
      DCHECK_EQ(channel_indices_.size(), 2u);
      reading.als.value = GetScaledValue(sample.at(channel_indices_[0]));
      break;

    case mojom::SensorType::ACCELEROMETER:
    case mojom::SensorType::GRAVITY:
      DCHECK_EQ(channel_indices_.size(), 4u);
      reading.accel.x = GetScaledValue(sample.at(channel_indices_[0]));
      reading.accel.y = GetScaledValue(sample.at(channel_indices_[1]));
      reading.accel.z = GetScaledValue(sample.at(channel_indices_[2]));
      break;

    case mojom::SensorType::GYROSCOPE:
      DCHECK_EQ(channel_indices_.size(), 4u);
      reading.gyro.x = GetScaledValue(sample.at(channel_indices_[0]));
      reading.gyro.y = GetScaledValue(sample.at(channel_indices_[1]));
      reading.gyro.z = GetScaledValue(sample.at(channel_indices_[2]));
      break;

    case mojom::SensorType::MAGNETOMETER:
      DCHECK_EQ(channel_indices_.size(), 4u);
      reading.magn.x = GetScaledValue(sample.at(channel_indices_[0]));
      reading.magn.y = GetScaledValue(sample.at(channel_indices_[1]));
      reading.magn.z = GetScaledValue(sample.at(channel_indices_[2]));
      break;

    default:
      break;
  }

  reading.raw.timestamp =
      base::Nanoseconds(sample.at(channel_indices_.back())).InSecondsF();

  UpdateSharedBufferAndNotifyClients(reading);
}

void PlatformSensorChromeOS::OnErrorOccurred(
    chromeos::sensors::mojom::ObserverErrorType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (type) {
    case chromeos::sensors::mojom::ObserverErrorType::ALREADY_STARTED:
      LOG(ERROR) << "Sensor " << iio_device_id_
                 << ": Another observer has already started to read samples";
      ResetOnError();
      break;

    case chromeos::sensors::mojom::ObserverErrorType::FREQUENCY_INVALID:
      LOG(ERROR) << "Sensor " << iio_device_id_
                 << ": Observer started with an invalid frequency";
      ResetOnError();
      break;

    case chromeos::sensors::mojom::ObserverErrorType::NO_ENABLED_CHANNELS:
      LOG(ERROR) << "Sensor " << iio_device_id_
                 << ": Observer started with no channels enabled";
      SetChannelsEnabled();
      break;

    case chromeos::sensors::mojom::ObserverErrorType::SET_FREQUENCY_IO_FAILED:
      LOG(ERROR) << "Sensor " << iio_device_id_
                 << ": Failed to set frequency to the physical device";
      break;

    case chromeos::sensors::mojom::ObserverErrorType::GET_FD_FAILED:
      LOG(ERROR) << "Sensor " << iio_device_id_
                 << ": Failed to get the device's fd to poll on";
      break;

    case chromeos::sensors::mojom::ObserverErrorType::READ_FAILED:
      LOG(ERROR) << "Sensor " << iio_device_id_ << ": Failed to read a sample";
      OnReadFailure();
      break;

    case chromeos::sensors::mojom::ObserverErrorType::READ_TIMEOUT:
      LOG(ERROR) << "Sensor " << iio_device_id_ << ": A read timed out";
      break;

    default:
      LOG(ERROR) << "Sensor " << iio_device_id_ << ": error "
                 << static_cast<int>(type);
      break;
  }
}

bool PlatformSensorChromeOS::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!sensor_device_remote_.is_bound()) {
    LOG(WARNING) << "Unbound sensor_device_remote_, skipping StartSensor.";
    return false;
  }

  if (configuration.frequency() <= 0.0) {
    LOG(ERROR) << "Invalid frequency: " << configuration.frequency()
               << " in sensor with id: " << iio_device_id_;
    return false;
  }

  if (receiver_.is_bound() &&
      configuration.frequency() == current_configuration_.frequency()) {
    // Nothing to do.
    return true;
  }

  current_configuration_ = configuration;
  StartReadingIfReady();
  return true;
}

void PlatformSensorChromeOS::StopSensor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receiver_.reset();
}

bool PlatformSensorChromeOS::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return configuration.frequency() > 0 &&
         configuration.frequency() <= default_configuration_.frequency();
}

PlatformSensorConfiguration PlatformSensorChromeOS::GetDefaultConfiguration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return default_configuration_;
}

void PlatformSensorChromeOS::SensorReplaced() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "SensorReplaced with id: " << iio_device_id_;
  ResetReadingBuffer();
  ResetOnError();
}

void PlatformSensorChromeOS::ResetOnError() {
  LOG(ERROR) << "ResetOnError of sensor with id: " << iio_device_id_;
  sensor_device_remote_.reset();
  receiver_.reset();
  NotifySensorError();
}

void PlatformSensorChromeOS::OnSensorDeviceDisconnect(
    uint32_t custom_reason_code,
    const std::string& description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto reason =
      static_cast<chromeos::sensors::mojom::SensorDeviceDisconnectReason>(
          custom_reason_code);
  LOG(ERROR) << "OnSensorDeviceDisconnect, reason: " << reason
             << ", description: " << description;

  std::move(sensor_device_disconnect_callback_)
      .Run(custom_reason_code, description);

  ResetOnError();
}

void PlatformSensorChromeOS::StartReadingIfReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sensor_device_remote_.is_bound());

  if (required_channel_ids_.empty() ||
      current_configuration_.frequency() <= 0.0) {
    // Not ready yet.
    return;
  }

  UpdateSensorDeviceFrequency();

  if (receiver_.is_bound())
    return;

  sensor_device_remote_->StartReadingSamples(BindNewPipeAndPassRemote());
}

mojo::PendingRemote<chromeos::sensors::mojom::SensorDeviceSamplesObserver>
PlatformSensorChromeOS::BindNewPipeAndPassRemote() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!receiver_.is_bound());
  auto pending_remote = receiver_.BindNewPipeAndPassRemote(main_task_runner());

  receiver_.set_disconnect_with_reason_handler(
      base::BindOnce(&PlatformSensorChromeOS::OnObserverDisconnect,
                     weak_factory_.GetWeakPtr()));
  return pending_remote;
}

void PlatformSensorChromeOS::OnObserverDisconnect(
    uint32_t custom_reason_code,
    const std::string& description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(receiver_.is_bound());

  auto reason =
      static_cast<chromeos::sensors::mojom::SensorDeviceDisconnectReason>(
          custom_reason_code);
  LOG(ERROR) << "OnObserverDisconnect, reason: " << reason
             << ", description: " << description;

  std::move(sensor_device_disconnect_callback_)
      .Run(custom_reason_code, description);

  ResetOnError();
}

void PlatformSensorChromeOS::SetRequiredChannels() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(required_channel_ids_.empty());  // Should only be called once.

  std::optional<std::string> axes_prefix = std::nullopt;
  switch (GetType()) {
    case mojom::SensorType::AMBIENT_LIGHT:
      required_channel_ids_.push_back(chromeos::sensors::mojom::kLightChannel);
      break;

    case mojom::SensorType::ACCELEROMETER:
      axes_prefix = chromeos::sensors::mojom::kAccelerometerChannel;
      break;

    case mojom::SensorType::GYROSCOPE:
      axes_prefix = chromeos::sensors::mojom::kGyroscopeChannel;
      break;

    case mojom::SensorType::MAGNETOMETER:
      axes_prefix = chromeos::sensors::mojom::kMagnetometerChannel;
      break;

    case mojom::SensorType::GRAVITY:
      axes_prefix = chromeos::sensors::mojom::kGravityChannel;
      break;

    default:
      break;
  }

  if (axes_prefix.has_value()) {
    for (const auto* axis : kAxes)
      required_channel_ids_.push_back(axes_prefix.value() + std::string(axis));
  }

  required_channel_ids_.push_back(chromeos::sensors::mojom::kTimestampChannel);
}

void PlatformSensorChromeOS::GetAllChannelIdsCallback(
    const std::vector<std::string>& iio_channel_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetRequiredChannels();
  DCHECK(!required_channel_ids_.empty());

  iio_channel_ids_ = iio_channel_ids;

  for (const std::string& channel : required_channel_ids_) {
    auto it = base::ranges::find(iio_channel_ids_, channel);
    if (it == iio_channel_ids_.end()) {
      LOG(ERROR) << "Missing channel: " << channel;
      ResetOnError();
      return;
    }
    channel_indices_.push_back(std::distance(iio_channel_ids_.begin(), it));
  }

  DCHECK_EQ(channel_indices_.size(), required_channel_ids_.size());

  SetChannelsEnabled();

  StartReadingIfReady();
}

void PlatformSensorChromeOS::UpdateSensorDeviceFrequency() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sensor_device_remote_.is_bound());

  sensor_device_remote_->SetFrequency(
      current_configuration_.frequency(),
      base::BindOnce(&PlatformSensorChromeOS::SetFrequencyCallback,
                     weak_factory_.GetWeakPtr(),
                     current_configuration_.frequency()));
}

void PlatformSensorChromeOS::SetFrequencyCallback(double target_frequency,
                                                  double result_frequency) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if ((target_frequency <= 0.0 && result_frequency <= 0.0) ||
      (target_frequency > 0.0 && result_frequency > 0.0)) {
    return;
  }

  LOG(ERROR) << "SetFrequency failed. Target frequency: " << target_frequency
             << ", result requency: " << result_frequency;
  ResetOnError();
}

void PlatformSensorChromeOS::SetChannelsEnabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!sensor_device_remote_.is_bound()) {
    LOG(WARNING)
        << "Unbound sensor_device_remote_, skipping SetChannelEnabled.";
    return;
  }

  sensor_device_remote_->SetChannelsEnabled(
      channel_indices_, true,
      base::BindOnce(&PlatformSensorChromeOS::SetChannelsEnabledCallback,
                     weak_factory_.GetWeakPtr()));
}

void PlatformSensorChromeOS::SetChannelsEnabledCallback(
    const std::vector<int32_t>& failed_indices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (failed_indices.empty())
    return;

  for (int32_t index : failed_indices) {
    LOG(ERROR) << "Failed to enable channel: " << iio_channel_ids_[index]
               << " in sensor with id: " << iio_device_id_;
  }

  ResetOnError();
}

double PlatformSensorChromeOS::GetScaledValue(int64_t value) const {
  return value * scale_;
}

void PlatformSensorChromeOS::OnReadFailure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (++num_failed_reads_ < kNumFailedReadsBeforeGivingUp) {
    LOG(ERROR) << "ReadSamples error #" << num_failed_reads_ << " occurred";
    return;
  }

  num_failed_reads_ = num_recovery_reads_ = 0;

  LOG(ERROR) << "Too many failed reads";
  ResetOnError();
}

}  // namespace device
