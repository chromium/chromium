// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/switches.h"

#include "build/chromeos_buildflags.h"

namespace extensions {

namespace switches {

// Allows non-https URL for background_page for hosted apps.
const char kAllowHTTPBackgroundPage[] = "allow-http-background-page";

// Allows the browser to load extensions that lack a modern manifest when that
// would otherwise be forbidden.
const char kAllowLegacyExtensionManifests[] =
    "allow-legacy-extension-manifests";

// Adds the given extension ID to all the permission allowlists.
const char kAllowlistedExtensionID[] = "allowlisted-extension-id";

// Enables extension options to be embedded in chrome://extensions rather than
// a new tab.
const char kEmbeddedExtensionOptions[] = "embedded-extension-options";

// Enable BLE Advertising in apps.
const char kEnableBLEAdvertising[] = "enable-ble-advertising-in-apps";

// Enables extension APIs that are in development.
const char kEnableExperimentalExtensionApis[] =
    "enable-experimental-extension-apis";

// Disable checking for user opt-in for extensions that want to inject script
// into file URLs (ie, always allow it). This is used during automated testing.
const char kDisableExtensionsFileAccessCheck[] =
    "disable-extensions-file-access-check";

// Disable the net::URLRequestThrottlerManager functionality for
// requests originating from extensions.
const char kDisableExtensionsHttpThrottling[] =
    "disable-extensions-http-throttling";

// Marks a renderer as extension process.
const char kExtensionProcess[] = "extension-process";

// Enables extensions running scripts on chrome:// URLs.
// Extensions still need to explicitly request access to chrome:// URLs in the
// manifest.
const char kExtensionsOnChromeURLs[] = "extensions-on-chrome-urls";

// Whether to force developer mode extensions highlighting.
const char kForceDevModeHighlighting[] = "force-dev-mode-highlighting";

// Whether to disable app content verification when testing changes locally on
// Chromebox for Meetings hardware.
const char kDisableAppContentVerification[] =
    "disable-app-content-verification";

// Comma-separated list of paths to apps to load at startup. The first app in
// the list will be launched.
const char kLoadApps[] = "load-apps";

// Comma-separated list of paths to extensions to load at startup.
const char kLoadExtension[] = "load-extension";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Path to the unpacked test extension to load into the signin profile. The ID
// extension loaded must match kTestSigninProfileExtensionId.
const char kLoadSigninProfileTestExtension[] =
    "load-signin-profile-test-extension";

// Path to the unpacked test extension to load into guest mode. The extension ID
// must match kGuestModeTestExtensionId.
const char kLoadGuestModeTestExtension[] = "load-guest-mode-test-extension";
#endif

// Allows the use of the `testing` reason in offscreen documents.
const char kOffscreenDocumentTesting[] = "offscreen-document-testing";

// Set the parameters for ExtensionURLLoaderThrottleBrowserTest.
const char kSetExtensionThrottleTestParams[] =
    "set-extension-throttle-test-params";

// Makes component extensions appear in chrome://settings/extensions.
const char kShowComponentExtensionOptions[] =
    "show-component-extension-options";

// Pass launch source to platform apps.
const char kTraceAppSource[] = "enable-trace-app-source";

// Enable package hash check: the .crx file sha256 hash sum should be equal to
// the one received from update manifest.
const char kEnableCrxHashCheck[] = "enable-crx-hash-check";

// Mute extension errors while working with new manifest version.
const char kAllowFutureManifestVersion[] = "allow-future-manifest-version";

}  // namespace switches

}  // namespace extensions
