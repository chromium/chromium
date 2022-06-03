// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/webcam_private/ip_webcam.h"

#include <utility>

#include "chromeos/dbus/ip_peripheral/ip_peripheral_service_client.h"

namespace extensions {

IpWebcam::IpWebcam(const std::string& device_id) : device_id_(device_id) {}

IpWebcam::~IpWebcam() {}

void IpWebcam::GetPan(const GetPTZCompleteCallback& callback) {
  chromeos::IpPeripheralServiceClient::Get()->GetPan(device_id_,
                                                     std::move(callback));
}

void IpWebcam::GetTilt(const GetPTZCompleteCallback& callback) {
  chromeos::IpPeripheralServiceClient::Get()->GetTilt(device_id_,
                                                      std::move(callback));
}

void IpWebcam::GetZoom(const GetPTZCompleteCallback& callback) {
  chromeos::IpPeripheralServiceClient::Get()->GetZoom(device_id_,
                                                      std::move(callback));
}

void IpWebcam::GetFocus(const GetPTZCompleteCallback& callback) {
  int value = 0;
  int min_value = 0;
  int max_value = 0;
  bool success = false;

  std::move(callback).Run(success, value, min_value, max_value);
}

void IpWebcam::SetPan(int value,
                      int pan_speed,
                      const SetPTZCompleteCallback& callback) {
  chromeos::IpPeripheralServiceClient::Get()->SetPan(device_id_, value,
                                                     std::move(callback));
}

void IpWebcam::SetTilt(int value,
                       int tilt_speed,
                       const SetPTZCompleteCallback& callback) {
  chromeos::IpPeripheralServiceClient::Get()->SetTilt(device_id_, value,
                                                      std::move(callback));
}

void IpWebcam::SetZoom(int value, const SetPTZCompleteCallback& callback) {
  chromeos::IpPeripheralServiceClient::Get()->SetZoom(device_id_, value,
                                                      std::move(callback));
}

void IpWebcam::SetFocus(int value, const SetPTZCompleteCallback& callback) {
  std::move(callback).Run(false);
}

void IpWebcam::SetAutofocusState(AutofocusState state,
                                 const SetPTZCompleteCallback& callback) {
  std::move(callback).Run(false);
}

void IpWebcam::SetPanDirection(PanDirection direction,
                               int pan_speed,
                               const SetPTZCompleteCallback& callback) {
  std::move(callback).Run(false);
}

void IpWebcam::SetTiltDirection(TiltDirection direction,
                                int tilt_speed,
                                const SetPTZCompleteCallback& callback) {
  std::move(callback).Run(false);
}

void IpWebcam::SetHome(const SetPTZCompleteCallback& callback) {
  std::move(callback).Run(false);
}

void IpWebcam::RestoreCameraPreset(int preset_number,
                                   const SetPTZCompleteCallback& callback) {
  std::move(callback).Run(false);
}

void IpWebcam::SetCameraPreset(int preset_number,
                               const SetPTZCompleteCallback& callback) {
  std::move(callback).Run(false);
}

void IpWebcam::Reset(bool pan,
                     bool tilt,
                     bool zoom,
                     const SetPTZCompleteCallback& callback) {
  std::move(callback).Run(false);
}

}  // namespace extensions
