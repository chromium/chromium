// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_SETTING_KEYS_H_
#define REMOTING_HOST_HOST_SETTING_KEYS_H_

#include "remoting/base/host_settings.h"

namespace remoting {

// If setting is provided, the Mac host will capture audio from the audio device
// specified by the UID and stream it to the client. See AudioCapturerMac for
// more information.
constexpr HostSettingKey kMacAudioCaptureDeviceUid = "audio_capture_device_uid";

constexpr HostSettingKey kLinuxPreviousDefaultWebBrowserXfce =
    "previous_default_browser_xfce";

constexpr HostSettingKey kLinuxPreviousDefaultWebBrowserCinnamon =
    "previous_default_browser_cinnamon";

constexpr HostSettingKey kLinuxPreviousDefaultWebBrowserGnome =
    "previous_default_browser_gnome";

constexpr HostSettingKey kLinuxPreviousDefaultWebBrowserGeneric =
    "previous_default_browser_generic";

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_SETTING_KEYS_H_
