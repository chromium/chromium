// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_APPLICATION_LOOPBACK_DEVICE_HELPER_H_
#define MEDIA_AUDIO_APPLICATION_LOOPBACK_DEVICE_HELPER_H_

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "media/base/media_export.h"

// Application Loopback Device IDs are of the form
// "<base_id>:<application_id>" on Windows OS and
// "<base_id>:<bundle_id>[:<application_id>]" on macOS.
// <application_id> is the target PID for the application that will be
// captured.
// For macOS: <bundle_id> is the Bundle identifier of the application that will
// be captured. After the Bundle ID, one <application_id> may be included. If
// <application_id> is not present, audio will be captured from any process
// whose Bundle ID matches <bundle_id>. If <application_id> is present, audio
// will be captured from any process that both matches the <bundle_id> AND has
// a PID or parent PID that matches the <application_id>.
// <base_id> is either "kApplicationLoopbackDeviceId" or
// "kRestrictOwnAudioBrowserLoopbackDeviceId".
// If <base_id> is "kRestrictOwnAudioBrowserLoopbackDeviceId", the browser
// should capture itself, meaning <bundle_id> should be the bundle_id of the
// browser and <application_id> should be the pid of the main process for the
// browser.
// For example:
// On Windows OS: "kRestrictOwnAudioBrowserLoopbackDeviceId:9876"
// On macOS: "kRestrictOwnAudioBrowserLoopbackDeviceId:org.chromium.Chrome:9876"

namespace media {

#if BUILDFLAG(IS_MAC)
// Creates a Application Loopback Device ID for a given Bundle ID and
// Application ID. Audio will be captured from process audio objects that
// match the Bundle ID, and have a PID or parent PID that matches the
// Application ID. If `application_id` is empty, audio will be captured from
// any process whose Bundle ID matches `bundle_id`.
std::string MEDIA_EXPORT
CreateApplicationLoopbackDeviceId(std::string_view bundle_id,
                                  std::optional<pid_t> application_id);

// A Restrict Own Audio device ID is used in the cases where the browser
// is capturing its own audio, but is not allowed to capture audio from the tab
// that performs the capture. `bundle_id` should be the bundle identifier
// of the browser. `application_id` should be the pid of the main process for
// the browser.
std::string MEDIA_EXPORT
CreateRestrictOwnAudioBrowserLoopbackDeviceId(std::string_view bundle_id,
                                              pid_t application_id);

// Extracts the Bundle ID and optional Application ID from a Application
// Loopback Device ID.
// A CHECK() will be triggered if the device_id is not a Application loopback
// device ID, or it does not contain a bundle ID.
std::pair<std::string, std::optional<pid_t>> MEDIA_EXPORT
ParseApplicationLoopbackDeviceId(std::string_view device_id);

#elif BUILDFLAG(IS_WIN)

std::string MEDIA_EXPORT
CreateApplicationLoopbackDeviceId(const uint32_t application_id);

// A Restrict Own Audio Device ID is used in the cases where the browser
// is capturing its own audio, but is not allowed to capture audio from the tab
// that performs the capture. The Device ID still contain the Application ID of
// this browser.
std::string MEDIA_EXPORT CreateRestrictOwnAudioBrowserLoopbackDeviceId();

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
