// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_V4L2_WEBCAM_H_
#define EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_V4L2_WEBCAM_H_

#include "extensions/browser/api/webcam_private/webcam.h"

#include <stdint.h>

#include "base/files/scoped_file.h"

namespace extensions {

class V4L2Webcam : public Webcam {
 public:
  V4L2Webcam(const std::string& device_id);

  V4L2Webcam(const V4L2Webcam&) = delete;
  V4L2Webcam& operator=(const V4L2Webcam&) = delete;

  bool Open();

 private:
  ~V4L2Webcam() override;
  bool EnsureLogitechCommandsMapped();
  bool EnsureAverCommandsMapped();
  static bool SetWebcamParameter(int fd, uint32_t control_id, int value);
  static bool GetWebcamParameter(int fd,
                                 uint32_t control_id,
                                 int* value,
                                 int* min_value,
                                 int* max_value);

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
  base::ScopedFD fd_;
};


}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_V4L2_WEBCAM_H_
