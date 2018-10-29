// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_SWITCHES_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_SWITCHES_H_

#include "base/component_export.h"
#include "build/build_config.h"

namespace media_session {
namespace switches {

COMPONENT_EXPORT(MEDIA_SESSION_CPP) extern const char kEnableAudioFocus[];
COMPONENT_EXPORT(MEDIA_SESSION_CPP)
extern const char kEnableAudioFocusDuckFlash[];
COMPONENT_EXPORT(MEDIA_SESSION_CPP)
extern const char kEnableAudioFocusNoEnforce[];

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(MEDIA_SESSION_CPP)
extern const char kEnableInternalMediaSession[];
#endif  // !defined(OS_ANDROID)

}  // namespace switches

COMPONENT_EXPORT(MEDIA_SESSION_CPP) bool IsAudioFocusEnabled();

// Based on the command line of the current process, determine if
// audio focus duck flash should be enabled.
COMPONENT_EXPORT(MEDIA_SESSION_CPP) bool IsAudioFocusDuckFlashEnabled();

// Based on the command line of the current process, determine if
// audio focus enforcement should be enabled.
COMPONENT_EXPORT(MEDIA_SESSION_CPP) bool IsAudioFocusEnforcementEnabled();

COMPONENT_EXPORT(MEDIA_SESSION_CPP) bool IsMediaSessionEnabled();

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_SWITCHES_H_
