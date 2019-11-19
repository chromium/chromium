// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/sensor_impl.h"

#include <utility>

namespace device {

SensorImpl::SensorImpl(scoped_refptr<PlatformSensor> sensor)
    : sensor_(std::move(sensor)),
      reading_notification_enabled_(true),
      suspended_(false) {
  sensor_->AddClient(this);
}

SensorImpl::~SensorImpl() {
  sensor_->RemoveClient(this);
}

mojo::PendingReceiver<mojom::SensorClient> SensorImpl::GetClient() {
  return client_.BindNewPipeAndPassReceiver();
}

void SensorImpl::AddConfiguration(
    const PlatformSensorConfiguration& configuration,
    AddConfigurationCallback callback) {
  // TODO(Mikhail): To avoid overflowing browser by repeated AddConfigs
  // (maybe limit the number of configs per client).
  std::move(callback).Run(sensor_->StartListening(this, configuration));
}

void SensorImpl::GetDefaultConfiguration(
    GetDefaultConfigurationCallback callback) {
  std::move(callback).Run(sensor_->GetDefaultConfiguration());
}

void SensorImpl::RemoveConfiguration(
    const PlatformSensorConfiguration& configuration) {
  sensor_->StopListening(this, configuration);
}

void SensorImpl::Suspend() {
  suspended_ = true;
  sensor_->UpdateSensor();
}

void SensorImpl::Resume() {
  suspended_ = false;
  sensor_->UpdateSensor();
}

void SensorImpl::ConfigureReadingChangeNotifications(bool enabled) {
  reading_notification_enabled_ = enabled;
}

void SensorImpl::OnSensorReadingChanged(mojom::SensorType type) {
  DCHECK(!suspended_);
  if (client_ && reading_notification_enabled_ &&
      sensor_->GetReportingMode() == mojom::ReportingMode::ON_CHANGE) {
    client_->SensorReadingChanged();
  }
}

void SensorImpl::OnSensorError() {
  DCHECK(!suspended_);
  if (client_)
    client_->RaiseError();
}

bool SensorImpl::IsSuspended() {
  return suspended_;
}

}  // namespace device
