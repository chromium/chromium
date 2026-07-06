// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/sensor_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "services/device/generic_sensor/sensor_provider_impl.h"

namespace device {

// Receives suspend/resume control messages from the browser process via Mojo
// and forwards them to the associated SensorImpl. This allows the browser
// to supervise the sensor session (e.g. suspending it when the page is
// backgrounded or in BFCache).
class SensorImpl::SensorClientControllerImpl
    : public mojom::SensorClientController {
 public:
  SensorClientControllerImpl(
      SensorImpl* sensor,
      mojo::PendingReceiver<mojom::SensorClientController> receiver)
      : sensor_(sensor), receiver_(this, std::move(receiver)) {
    // A disconnect on the control pipe indicates that the browser/controller is
    // revoking access to the sensor (e.g., when the user revokes sensor
    // permissions in site settings while active, or when the frame is
    // destroyed). Removing the sensor tears down the renderer-facing Sensor and
    // SensorClient pipes.
    receiver_.set_disconnect_handler(base::BindOnce(
        &SensorClientControllerImpl::OnDisconnect, base::Unretained(this)));
  }
  void Suspend() override { sensor_->OnControllerSuspend(); }
  void Resume() override { sensor_->OnControllerResume(); }

 private:
  void OnDisconnect() { sensor_->OnControllerDisconnect(); }
  raw_ptr<SensorImpl> sensor_;
  mojo::Receiver<mojom::SensorClientController> receiver_;
};

SensorImpl::SensorImpl(
    scoped_refptr<PlatformSensor> sensor,
    mojo::PendingReceiver<mojom::SensorClientController> controller,
    bool initially_suspended,
    SensorProviderImpl* provider)
    : sensor_(std::move(sensor)),
      reading_notification_enabled_(true),
      client_suspended_(false),
      controller_suspended_(initially_suspended),
      provider_(provider) {
  sensor_->AddClient(this);
  if (controller.is_valid()) {
    client_controller_ = std::make_unique<SensorClientControllerImpl>(
        this, std::move(controller));
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
  if (client_suspended_) {
    return;
  }
  client_suspended_ = true;
  sensor_->UpdateSensor();
}

void SensorImpl::Resume() {
  if (!client_suspended_) {
    return;
  }
  client_suspended_ = false;
  sensor_->UpdateSensor();
}

void SensorImpl::OnControllerSuspend() {
  if (controller_suspended_) {
    return;
  }
  controller_suspended_ = true;
  sensor_->UpdateSensor();
}

void SensorImpl::OnControllerResume() {
  if (!controller_suspended_) {
    return;
  }
  controller_suspended_ = false;
  sensor_->UpdateSensor();
}

void SensorImpl::OnControllerDisconnect() {
  provider_->RemoveSensor(this);
}

void SensorImpl::ConfigureReadingChangeNotifications(bool enabled) {
  reading_notification_enabled_ = enabled;
}

void SensorImpl::OnSensorReadingChanged(mojom::SensorType type) {
  DCHECK(!IsSuspended());
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
  return client_suspended_ || controller_suspended_;
}

}  // namespace device
