// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_APPLICATION_AUDIO_CAPTURE_ID_MAC_H_
#define MEDIA_WEBRTC_APPLICATION_AUDIO_CAPTURE_ID_MAC_H_

#include <sys/types.h>

#include <optional>
#include <string>

#include "base/component_export.h"

namespace media {

struct COMPONENT_EXPORT(MEDIA_WEBRTC) ApplicationAudioCaptureId {
  std::string bundle_id;
  std::optional<pid_t> pid;

  bool operator==(const ApplicationAudioCaptureId& other) const = default;
};

// Retrieves the ApplicationAudioCaptureId for the process identified by `pid`.
//
// For a specific list of Chromium-based browsers and PWAs installed by them,
// this function returns a ApplicationAudioCaptureId that contains both a Bundle
// ID and a PID. The Bundle ID is truncated to its base prefix (removing
// components like development variants or PWA identifiers). For browsers, the
// returned PID is the browser's main process PID. For PWAs, the returned PID is
// the PID of the browser that installed the PWA. For example:
// "com.google.Chrome.beta" will return
// ApplicationAudioCaptureId{"com.google.Chrome", PID_of_Chrome_beta_process}.
// "org.chromium.Chromium.app.a1b2c3" (a PWA installed by Chromium) will return
// ApplicationAudioCaptureId{"org.chromium.Chromium", PID_of_Chromium_process}.
//
// For other non-Chromium applications, it returns the application's unchanged
// Bundle ID and an empty pid.
//
// Returns std::nullopt if the process does not exist or is not a bundled
// application, or if `pid` is a PWA, and there are no, or more than one,
// running apps with the PWA's browser Bundle ID.
// TODO(crbug.com/507803904): Add RTC logs.
COMPONENT_EXPORT(MEDIA_WEBRTC)
std::optional<ApplicationAudioCaptureId> GetApplicationAudioCaptureIdForProcess(
    pid_t pid);

}  // namespace media

#endif  // MEDIA_WEBRTC_APPLICATION_AUDIO_CAPTURE_ID_MAC_H_
