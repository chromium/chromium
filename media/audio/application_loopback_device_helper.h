// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_APPLICATION_LOOPBACK_DEVICE_HELPER_H_
#define MEDIA_AUDIO_APPLICATION_LOOPBACK_DEVICE_HELPER_H_

#include <cstdint>
#include <string>

#include "build/build_config.h"
#include "media/base/media_export.h"

// Application Loopback Device IDs are of the form
// "<base_id>:<application_id>" on Windows OS and "<base_id>:<bundle_id>"
// on macOS. <application_id> (Windows OS only) is the PID of the topmost
// process in the application that will be captured. <bundle_id> (macOS only)
// is the Bundle ID of the application that will be captured. <base_id> is
// either "kApplicationLoopbackDeviceId" or
// "kRestrictOwnAudioBrowserLoopbackDeviceId".
namespace media {

// TODO(crbug.com/502159773): Hide CreateApplicationLoopbackDeviceId() and
// CreateRestrictOwnAudioBrowserLoopbackDeviceId() to only exist on Windows.
std::string MEDIA_EXPORT
CreateApplicationLoopbackDeviceId(const uint32_t application_id);

// A Restrict Own Audio Device ID is used in the cases where the browser
// is capturing its own audio, but is not allowed to capture audio from the tab
// that performs the capture. The Device ID still contain the Application ID of
// this browser.
std::string MEDIA_EXPORT CreateRestrictOwnAudioBrowserLoopbackDeviceId();

#if BUILDFLAG(IS_MAC)
std::string MEDIA_EXPORT
CreateApplicationLoopbackDeviceId(std::string_view bundle_id);

// A Restrict Own Audio device ID is used in the cases where the browser
// is capturing its own audio, but is not allowed to capture audio from the tab
// that performs the capture. The Device ID should also contain the Bundle ID of
// this browser.
std::string MEDIA_EXPORT
CreateRestrictOwnAudioBrowserLoopbackDeviceId(std::string_view bundle_id);

// Extracts the Bundle ID from a Application Loopback Device ID.
// A CHECK() will be triggered if the device_id is not a Application loopback
// device ID, or it does not contain a bundle ID.
std::string MEDIA_EXPORT
GetBundleIdFromApplicationLoopbackDeviceId(std::string_view device_id);

#elif BUILDFLAG(IS_WIN)

// Extracts the Application ID from a Application Loopback Device ID.
// A CHECK() will be triggered if the device_id is not a Application loopback
// device ID, or it does not contain a valid application ID.
uint32_t MEDIA_EXPORT
GetApplicationIdFromApplicationLoopbackDeviceId(std::string_view device_id);
#endif  // BUILDFLAG(IS_MAC)

bool MEDIA_EXPORT
IsRestrictOwnAudioBrowserLoopbackDeviceId(std::string_view device_id);

}  // namespace media

#endif  // MEDIA_AUDIO_APPLICATION_LOOPBACK_DEVICE_HELPER_H_
