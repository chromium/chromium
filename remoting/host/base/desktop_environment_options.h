// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BASE_DESKTOP_ENVIRONMENT_OPTIONS_H_
#define REMOTING_HOST_BASE_DESKTOP_ENVIRONMENT_OPTIONS_H_

#include <optional>

#include "base/time/time.h"
#include "remoting/base/session_options.h"
#include "remoting/base/session_policies.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"

namespace remoting {

// Where to route audio from the local host during a CRD session, where a remote
// client is viewing the local host's screen.
// The enum below is used in histograms, do not remove/renumber entries. If
// you're adding to this enum, update the corresponding enum listing in
// tools/metrics/histograms/metadata/remoting/enums.xml.
enum class AudioPlaybackMode {
  kUnknown,
  // Audio is played on the local (host) machine.
  kLocalOnly,
  // Audio is played on the remote (client) machine and the local (host)
  // machine.
  kRemoteAndLocal,
  // Audio is played on the remote (client) machine.
  kRemoteOnly,
  kMaxValue = kRemoteOnly,
};

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

  bool enable_notifications() const;
  void set_enable_notifications(bool enabled);

  bool terminate_upon_input() const;
  void set_terminate_upon_input(bool enabled);

  bool enable_remote_webauthn() const;
  void set_enable_remote_webauthn(bool enabled);

  const webrtc::DesktopCaptureOptions* desktop_capture_options() const;
  webrtc::DesktopCaptureOptions* desktop_capture_options();

  bool capture_video_on_dedicated_thread() const;
  void set_capture_video_on_dedicated_thread(bool use_dedicated_thread);

  base::TimeDelta maximum_session_duration() const;
  void set_maximum_session_duration(base::TimeDelta duration);

  AudioPlaybackMode audio_playback_mode() const;
  void set_audio_playback_mode(AudioPlaybackMode);

  // Reads configurations from a SessionOptions instance.
  void ApplySessionOptions(const SessionOptions& options);

 private:
  // Sets default values for default constructor and CreateDefault() function.
  void Initialize();

  // True if the curtain mode should be enabled by the DesktopEnvironment
  // instances. Note, not all DesktopEnvironments support curtain mode.
  bool enable_curtaining_ = false;

  // True if user-interactive windows should be displayed on the desktop.
  bool enable_user_interface_ = true;

  // True if a notification should be shown when a remote user is connected.
  bool enable_notifications_ = true;

  // True if the session should be terminated when local input is detected.
  bool terminate_upon_input_ = false;

  // True if this host has the remote WebAuthn feature enabled.
  bool enable_remote_webauthn_ = false;

  // True if the video capturer should be run on a dedicated thread.
  bool capture_video_on_dedicated_thread_ = false;

  // Maximum session duration after which session will be terminated.
  base::TimeDelta maximum_session_duration_;

  // Audio playback mode for the session.
  AudioPlaybackMode audio_playback_mode_ = AudioPlaybackMode::kRemoteAndLocal;

  // The DesktopCaptureOptions to initialize DesktopCapturer.
  webrtc::DesktopCaptureOptions desktop_capture_options_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_BASE_DESKTOP_ENVIRONMENT_OPTIONS_H_
