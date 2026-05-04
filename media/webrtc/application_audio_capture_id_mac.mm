// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/application_audio_capture_id_mac.h"

#import <AppKit/AppKit.h>
#include <libproc.h>

#include <string_view>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"

namespace media {
namespace {

// Checks if the browser_bundle_id is a PWA installed by a browser and if so,
// returns the bundle_id of the browser that installed it.
// PWAs use the parent browser's Bundle ID as a prefix, followed by the
// '.app.' suffix and a unique hex code.
// Example: 'org.chromium.Chromium.app.a1b2c3'
std::optional<std::string> MaybeGetPwaInstallerBundleId(
    std::string_view browser_bundle_id) {
  constexpr std::string_view kPwaSuffix = ".app.";
  size_t pwa_suffix_position = browser_bundle_id.find(kPwaSuffix);
  if (pwa_suffix_position == std::string_view::npos) {
    return std::nullopt;
  }
  return std::make_optional<std::string>(
      browser_bundle_id.substr(0, pwa_suffix_position));
}

// Truncate a Chromium browser's bundle id to its prefix if it's a variant
// or PWA. If it's not a Chromium browser or variant or PWA, return
// std::nullopt.
std::optional<std::string> MaybeGetTruncatedChromiumBundleId(
    std::string_view bundle_id) {
  constexpr std::string_view kBrowserPrefixes[] = {
      "com.google.Chrome", "org.chromium.Chromium", "com.microsoft.edgemac",
      "com.operasoftware.Opera"};
  for (std::string_view prefix : kBrowserPrefixes) {
    if (base::StartsWith(bundle_id, prefix, base::CompareCase::SENSITIVE)) {
      return std::make_optional<std::string>(prefix);
    }
  }
  return std::nullopt;
}

// Attempts to retrieve the macOS Bundle identifier for the process identified
// by `pid`. Returns std::nullopt if the process does not exist or is not a
// bundled application.
std::optional<std::string> GetBundleIdForProcess(pid_t pid) {
  NSRunningApplication* app =
      [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
  if (!app || !app.bundleIdentifier) {
    return std::nullopt;
  }
  return std::make_optional<std::string>(
      base::SysNSStringToUTF8(app.bundleIdentifier));
}

}  // namespace

std::optional<ApplicationAudioCaptureId> GetApplicationAudioCaptureIdForProcess(
    pid_t pid) {
  std::optional<std::string> bundle_id = GetBundleIdForProcess(pid);

  if (!bundle_id) {
    return std::nullopt;
  }

  std::optional<std::string> truncated_chromium_bundle_id =
      MaybeGetTruncatedChromiumBundleId(*bundle_id);

  if (!truncated_chromium_bundle_id) {
    // Capturing non-Chromium application window.
    return std::make_optional<ApplicationAudioCaptureId>(*bundle_id,
                                                         std::nullopt);
  }

  std::optional<std::string> pwa_installer_bundle_id =
      MaybeGetPwaInstallerBundleId(*bundle_id);
  if (!pwa_installer_bundle_id) {
    // Capturing Chromium browser window. Window PID is the main PID of the
    // browser.
    return std::make_optional<ApplicationAudioCaptureId>(
        *truncated_chromium_bundle_id, std::make_optional(pid));
  }

  // For PWAs we return the base bundle_id and the PID of the browser that
  // installed the PWA. If the browser PID can't be uniquely determined
  // (e.g. multiple instances running) we return nullopt.
  NSArray<NSRunningApplication*>* browser_apps = [NSRunningApplication
      runningApplicationsWithBundleIdentifier:base::SysUTF8ToNSString(
                                                  *pwa_installer_bundle_id)];
  if (browser_apps.count != 1) {
    return std::nullopt;
  }
  return std::make_optional<ApplicationAudioCaptureId>(
      *truncated_chromium_bundle_id,
      std::make_optional(browser_apps[0].processIdentifier));
}

}  // namespace media
