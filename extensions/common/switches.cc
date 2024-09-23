// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/switches.h"

#include "build/chromeos_buildflags.h"

namespace extensions::switches {

const char kAllowHTTPBackgroundPage[] = "allow-http-background-page";
const char kAllowLegacyExtensionManifests[] =
    "allow-legacy-extension-manifests";
const char kAllowlistedExtensionID[] = "allowlisted-extension-id";
const char kEmbeddedExtensionOptions[] = "embedded-extension-options";
const char kEnableBLEAdvertising[] = "enable-ble-advertising-in-apps";
const char kEnableExperimentalExtensionApis[] =
    "enable-experimental-extension-apis";
const char kDisableExtensionsFileAccessCheck[] =
    "disable-extensions-file-access-check";
const char kDisableExtensionsHttpThrottling[] =
    "disable-extensions-http-throttling";
const char kExtensionProcess[] = "extension-process";
const char kExtensionsOnChromeURLs[] = "extensions-on-chrome-urls";
const char kDisableAppContentVerification[] =
    "disable-app-content-verification";
const char kLoadApps[] = "load-apps";
const char kLoadExtension[] = "load-extension";

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kLoadSigninProfileTestExtension[] =
    "load-signin-profile-test-extension";
const char kLoadGuestModeTestExtension[] = "load-guest-mode-test-extension";
#endif

const char kOffscreenDocumentTesting[] = "offscreen-document-testing";
const char kSetExtensionThrottleTestParams[] =
    "set-extension-throttle-test-params";
const char kShowComponentExtensionOptions[] =
    "show-component-extension-options";
const char kTraceAppSource[] = "enable-trace-app-source";
const char kEnableCrxHashCheck[] = "enable-crx-hash-check";
const char kAllowFutureManifestVersion[] = "allow-future-manifest-version";
const char kExtensionTestApiOnWebPages[] = "extension-test-api-on-web-pages";

}  // namespace extensions::switches
