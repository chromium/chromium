// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_CHROMEOS_ENTERPRISE_PARAMS_H_
#define REMOTING_HOST_CHROMEOS_CHROMEOS_ENTERPRISE_PARAMS_H_

#include <string>

#include "base/time/time.h"
#include "base/values.h"

namespace remoting {

// The caller who initiated the request.
enum class ChromeOsEnterpriseRequestOrigin {
  kUnknown,
  kEnterpriseAdmin,
  kClassManagement,
};

// Where to route audio from the local host during the CRD session, where a
// remote client is viewing the local host's screen.
enum class ChromeOsEnterpriseAudioPlayback {
  kUnknown,
  kLocalOnly,
  kRemoteAndLocal,
  kRemoteOnly,
};

// Converts a `ChromeOsEnterpriseAudioPlayback` to a string, for logging.
std::string ConvertChromeOsEnterpriseAudioPlaybackToString(
    ChromeOsEnterpriseAudioPlayback audio_playback);

// ChromeOS enterprise specific parameters.
// These parameters are not exposed through the public Mojom APIs, for security
// reasons.
struct ChromeOsEnterpriseParams {
  ChromeOsEnterpriseParams();

  ChromeOsEnterpriseParams(const ChromeOsEnterpriseParams& other);
  ChromeOsEnterpriseParams& operator=(const ChromeOsEnterpriseParams& other);

  ~ChromeOsEnterpriseParams();

  bool operator==(const ChromeOsEnterpriseParams& other) const;

  // Helpers used to serialize/deserialize enterprise params.
  static ChromeOsEnterpriseParams FromDict(const base::Value::Dict& dict);
  base::Value::Dict ToDict() const;

  // Local machine configuration.
  bool suppress_user_dialogs = false;
  bool suppress_notifications = false;
  bool terminate_upon_input = false;
  bool curtain_local_user_session = false;
  base::TimeDelta maximum_session_duration;
  bool allow_remote_input = true;
  bool allow_clipboard_sync = true;

  // Remote machine configuration.
  bool show_troubleshooting_tools = false;
  bool allow_troubleshooting_tools = false;
  bool allow_reconnections = false;
  bool allow_file_transfer = false;
  bool connection_dialog_required = false;
  ChromeOsEnterpriseRequestOrigin request_origin =
      ChromeOsEnterpriseRequestOrigin::kUnknown;
  ChromeOsEnterpriseAudioPlayback audio_playback =
      ChromeOsEnterpriseAudioPlayback::kUnknown;

  // Both local and remote machine configuration.
  base::TimeDelta connection_auto_accept_timeout;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_CHROMEOS_ENTERPRISE_PARAMS_H_
