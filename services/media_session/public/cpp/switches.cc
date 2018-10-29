// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/switches.h"

#include "base/command_line.h"
#include "build/build_config.h"

namespace media_session {
namespace switches {

// Enable a internal audio focus management between tabs in such a way that two
// tabs can't  play on top of each other.
// The allowed values are: "" (empty) or |kEnableAudioFocusDuckFlash|
// or |kEnableAudioFocusNoEnforce|.
const char kEnableAudioFocus[] = "enable-audio-focus";

// This value is used as an option for |kEnableAudioFocus|. Flash will
// be ducked when losing audio focus.
const char kEnableAudioFocusDuckFlash[] = "duck-flash";

// This value is used as an option for |kEnableAudioFocus|. If enabled then
// single media session audio focus will not be enforced. This should be used by
// embedders that wish to track audio focus but without the enforcement.
const char kEnableAudioFocusNoEnforce[] = "no-enforce";

#if !defined(OS_ANDROID)
// Turns on the internal media session backend. This should be used by embedders
// that want to control the media playback with the media session interfaces.
const char kEnableInternalMediaSession[] = "enable-internal-media-session";
#endif  // !defined(OS_ANDROID)

}  // namespace switches

bool IsAudioFocusEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableAudioFocus);
}

bool IsAudioFocusDuckFlashEnabled() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             switches::kEnableAudioFocus) ==
         switches::kEnableAudioFocusDuckFlash;
}

bool IsAudioFocusEnforcementEnabled() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             switches::kEnableAudioFocus) !=
         switches::kEnableAudioFocusNoEnforce;
}

bool IsMediaSessionEnabled() {
// Media session is enabled on Android and Chrome OS to allow control of media
// players as needed.
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  return true;
#else
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(
             media_session::switches::kEnableInternalMediaSession) ||
         IsAudioFocusEnabled();
#endif
}

}  // namespace media_session
