// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_SWITCHES_H_
#define EXTENSIONS_COMMON_SWITCHES_H_

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
namespace extensions {

namespace switches {

extern const char kAllowHTTPBackgroundPage[];
extern const char kAllowLegacyExtensionManifests[];
extern const char kDisableDesktopCaptureAudio[];
extern const char kDisableExtensionsHttpThrottling[];
extern const char kEmbeddedExtensionOptions[];
extern const char kEnableExperimentalExtensionApis[];
extern const char kEnableOverrideBookmarksUI[];
extern const char kEnableBLEAdvertising[];
extern const char kErrorConsole[];
extern const char kExtensionProcess[];
extern const char kExtensionsOnChromeURLs[];
extern const char kForceDevModeHighlighting[];
extern const char kForceEmptyCorbAllowlist[];
extern const char kLoadApps[];
extern const char kLoadExtension[];
extern const char kSetExtensionThrottleTestParams[];
extern const char kShowComponentExtensionOptions[];
extern const char kTraceAppSource[];
extern const char kWhitelistedExtensionID[];
extern const char kEnableCrxHashCheck[];

}  // namespace switches

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_SWITCHES_H_
