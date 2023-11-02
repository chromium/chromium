// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_IP_WEBCAM_H_
#define EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_IP_WEBCAM_H_

#include <string>

#include "extensions/browser/api/webcam_private/webcam.h"

namespace extensions {

class IpWebcam : public Webcam {
 public:
  explicit IpWebcam(const std::string& device_id);

  IpWebcam(const IpWebcam&) = delete;
  IpWebcam& operator=(const IpWebcam&) = delete;

 private:
  ~IpWebcam() override;

  // Webcam:
  void GetPan(const GetPTZCompleteCallback& callback) override;
  void GetTilt(const GetPTZCompleteCallback& callback) override;
  void GetZoom(const GetPTZCompleteCallback& callback) override;
  void GetFocus(const GetPTZCompleteCallback& callback) override;
  void SetPan(int value,
              int pan_speed,
              const SetPTZCompleteCallback& callback) override;
  void SetTilt(int value,
               int tilt_speed,
               const SetPTZCompleteCallback& callback) override;
  void SetZoom(int value, const SetPTZCompleteCallback& callback) override;
  void SetPanDirection(PanDirection direction,
                       int pan_speed,
                       const SetPTZCompleteCallback& callback) override;
  void SetTiltDirection(TiltDirection direction,
                        int tilt_speed,
                        const SetPTZCompleteCallback& callback) override;
  void Reset(bool pan,
             bool tilt,
             bool zoom,
             const SetPTZCompleteCallback& callback) override;

  void SetHome(const SetPTZCompleteCallback& callback) override;
  void SetFocus(int value, const SetPTZCompleteCallback& callback) override;
  void SetAutofocusState(AutofocusState state,
                         const SetPTZCompleteCallback& callback) override;
  void RestoreCameraPreset(int preset_number,
                           const SetPTZCompleteCallback& callback) override;
  void SetCameraPreset(int preset_number,
                       const SetPTZCompleteCallback& callback) override;

  const std::string device_id_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_IP_WEBCAM_H_
