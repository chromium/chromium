// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_SWITCHES_H_
#define EXTENSIONS_COMMON_SWITCHES_H_

#include "build/chromeos_buildflags.h"

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
namespace extensions {

namespace switches {

extern const char kAllowHTTPBackgroundPage[];
extern const char kAllowLegacyExtensionManifests[];
extern const char kAllowlistedExtensionID[];
extern const char kDisableAppContentVerification[];
extern const char kDisableExtensionsHttpThrottling[];
extern const char kEmbeddedExtensionOptions[];
extern const char kEnableExperimentalExtensionApis[];
extern const char kEnableBLEAdvertising[];
extern const char kExtensionProcess[];
extern const char kExtensionsOnChromeURLs[];
extern const char kForceDevModeHighlighting[];
extern const char kLoadApps[];
extern const char kLoadExtension[];
#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kLoadSigninProfileTestExtension[];
extern const char kLoadGuestModeTestExtension[];
#endif
extern const char kOffscreenDocumentTesting[];
extern const char kSetExtensionThrottleTestParams[];
extern const char kShowComponentExtensionOptions[];
extern const char kTraceAppSource[];
extern const char kEnableCrxHashCheck[];
extern const char kAllowFutureManifestVersion[];

}  // namespace switches

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_SWITCHES_H_
