// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_ENVIRONMENT_OPTIONS_H_
#define REMOTING_HOST_DESKTOP_ENVIRONMENT_OPTIONS_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "remoting/base/session_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"

namespace remoting {

// A container of options a DesktopEnvironment or its derived classes need to
// control the behavior.
class DesktopEnvironmentOptions final {
 public:
  // Returns instance of DesktopEnvironmentOptions with default parameters, and
  // initializes DesktopCaptureOptions by using
  // DesktopCaptureOptions::CreateDefault().
  static DesktopEnvironmentOptions CreateDefault();

  DesktopEnvironmentOptions();
  DesktopEnvironmentOptions(DesktopEnvironmentOptions&& other);
  DesktopEnvironmentOptions(const DesktopEnvironmentOptions& other);
  ~DesktopEnvironmentOptions();

  DesktopEnvironmentOptions& operator=(DesktopEnvironmentOptions&& other);
  DesktopEnvironmentOptions& operator=(const DesktopEnvironmentOptions& other);

  bool enable_curtaining() const;
  void set_enable_curtaining(bool enabled);

  bool enable_user_interface() const;
  void set_enable_user_interface(bool enabled);

  bool terminate_upon_input() const;
  void set_terminate_upon_input(bool enabled);

  bool enable_file_transfer() const;
  void set_enable_file_transfer(bool enabled);

  const webrtc::DesktopCaptureOptions* desktop_capture_options() const;
  webrtc::DesktopCaptureOptions* desktop_capture_options();

  // Reads configurations from a SessionOptions instance.
  void ApplySessionOptions(const SessionOptions& options);

 private:
  // Sets default values for default constructor and CreateDefault() function.
  void Initialize();

  // True if the curtain mode should be enabled by the DesktopEnvironment
  // instances. Note, not all DesktopEnvironments support curtain mode.
  bool enable_curtaining_ = false;

  // True if a user-interactive window is showing up in it2me scenario.
  bool enable_user_interface_ = true;

  // True if the session should be terminated when local input is detected.
  bool terminate_upon_input_ = false;

  // True if this host has file transfer enabled.
  bool enable_file_transfer_ = false;

  // The DesktopCaptureOptions to initialize DesktopCapturer.
  webrtc::DesktopCaptureOptions desktop_capture_options_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_ENVIRONMENT_OPTIONS_H_
