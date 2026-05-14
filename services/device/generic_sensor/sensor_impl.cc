// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/sensor_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "services/device/generic_sensor/sensor_provider_impl.h"

namespace device {

SensorImpl::SensorImpl(
    scoped_refptr<PlatformSensor> sensor,
    mojo::PendingRemote<mojom::SensorConnectionWatcher> watcher,
    SensorProviderImpl* provider)
    : sensor_(std::move(sensor)),
      reading_notification_enabled_(true),
      suspended_(false),
      provider_(provider) {
  sensor_->AddClient(this);
  if (watcher.is_valid()) {
    watcher_.Bind(std::move(watcher));
    watcher_.set_disconnect_handler(base::BindOnce(
        &SensorProviderImpl::RemoveSensor, base::Unretained(provider_), this));
  }
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
  if (client_)
    client_->RaiseError();
}

bool SensorImpl::IsSuspended() {
  return suspended_;
}

}  // namespace device
