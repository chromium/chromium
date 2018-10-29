// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_H_
#define EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/api_resource.h"

namespace extensions {

class Webcam : public base::RefCounted<Webcam> {
 public:
  enum PanDirection {
    PAN_LEFT,
    PAN_RIGHT,
    PAN_STOP,
  };

  enum TiltDirection {
    TILT_UP,
    TILT_DOWN,
    TILT_STOP,
  };

  enum AutofocusState {
    AUTOFOCUS_ON,
    AUTOFOCUS_OFF,
  };

  Webcam();

  using GetPTZCompleteCallback = base::Callback<
      void(bool success, int value, int min_value, int max_value)>;
  using SetPTZCompleteCallback = base::Callback<void(bool success)>;

  virtual void GetPan(const GetPTZCompleteCallback& callback) = 0;
  virtual void GetTilt(const GetPTZCompleteCallback& callback) = 0;
  virtual void GetZoom(const GetPTZCompleteCallback& callback) = 0;
  virtual void GetFocus(const GetPTZCompleteCallback& callback) = 0;
  virtual void SetPan(int value,
                      int pan_speed,
                      const SetPTZCompleteCallback& callback) = 0;
  virtual void SetTilt(int value,
                       int tilt_speed,
                       const SetPTZCompleteCallback& callback) = 0;
  virtual void SetZoom(int value, const SetPTZCompleteCallback& callback) = 0;
  virtual void SetPanDirection(PanDirection direction,
                               int pan_speed,
                               const SetPTZCompleteCallback& callback) = 0;
  virtual void SetTiltDirection(TiltDirection direction,
                                int tilt_speed,
                                const SetPTZCompleteCallback& callback) = 0;
  virtual void SetFocus(int value, const SetPTZCompleteCallback& callback) = 0;
  virtual void SetAutofocusState(AutofocusState state,
                                 const SetPTZCompleteCallback& callback) = 0;
  virtual void Reset(bool pan,
                     bool tilt,
                     bool zoom,
                     const SetPTZCompleteCallback& callback) = 0;

 protected:
  friend class base::RefCounted<Webcam>;
  virtual ~Webcam();

 private:
  DISALLOW_COPY_AND_ASSIGN(Webcam);
};

class WebcamResource : public ApiResource {
 public:
  WebcamResource(const std::string& owner_extension_id,
                 Webcam* webcam,
                 const std::string& webcam_id);
  ~WebcamResource() override;

  static const content::BrowserThread::ID kThreadId =
      content::BrowserThread::UI;

  Webcam* GetWebcam() const;
  const std::string GetWebcamId() const;

  // ApiResource overrides.
  bool IsPersistent() const override;

 private:
  scoped_refptr<Webcam> webcam_;
  std::string webcam_id_;

  DISALLOW_COPY_AND_ASSIGN(WebcamResource);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_H_
