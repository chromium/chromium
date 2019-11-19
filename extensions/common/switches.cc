// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/switches.h"

namespace extensions {

namespace switches {

// Allows non-https URL for background_page for hosted apps.
const char kAllowHTTPBackgroundPage[] = "allow-http-background-page";

// Allows the browser to load extensions that lack a modern manifest when that
// would otherwise be forbidden.
const char kAllowLegacyExtensionManifests[] =
    "allow-legacy-extension-manifests";

// Enables extension options to be embedded in chrome://extensions rather than
// a new tab.
const char kEmbeddedExtensionOptions[] = "embedded-extension-options";

// Enable BLE Advertisiing in apps.
const char kEnableBLEAdvertising[] = "enable-ble-advertising-in-apps";

const char kDisableDesktopCaptureAudio[] =
    "disable-audio-support-for-desktop-share";

// Enables extension APIs that are in development.
const char kEnableExperimentalExtensionApis[] =
    "enable-experimental-extension-apis";

// Enables extensions to hide bookmarks UI elements.
const char kEnableOverrideBookmarksUI[] = "enable-override-bookmarks-ui";

// Disable the net::URLRequestThrottlerManager functionality for
// requests originating from extensions.
const char kDisableExtensionsHttpThrottling[] =
    "disable-extensions-http-throttling";

// Allows the ErrorConsole to collect runtime and manifest errors, and display
// them in the chrome:extensions page.
const char kErrorConsole[] = "error-console";

// Marks a renderer as extension process.
const char kExtensionProcess[] = "extension-process";

// Enables extensions running scripts on chrome:// URLs.
// Extensions still need to explicitly request access to chrome:// URLs in the
// manifest.
const char kExtensionsOnChromeURLs[] = "extensions-on-chrome-urls";

// Whether to force developer mode extensions highlighting.
const char kForceDevModeHighlighting[] = "force-dev-mode-highlighting";

// Whether |extensions_features::kBypassCorbAllowlistParamName| should always be
// empty (i.e. ignoring hardcoded allowlist and the field trial param).  This
// switch is useful for manually verifying if an extension would continue to
// work fine after removing it from the allowlist.
const char kForceEmptyCorbAllowlist[] = "force-empty-corb-allowlist";

// Comma-separated list of paths to apps to load at startup. The first app in
// the list will be launched.
const char kLoadApps[] = "load-apps";

// Comma-separated list of paths to extensions to load at startup.
const char kLoadExtension[] = "load-extension";

// Set the parameters for ExtensionURLLoaderThrottleBrowserTest.
const char kSetExtensionThrottleTestParams[] =
    "set-extension-throttle-test-params";

// Makes component extensions appear in chrome://settings/extensions.
const char kShowComponentExtensionOptions[] =
    "show-component-extension-options";

// Adds the given extension ID to all the permission whitelists.
const char kWhitelistedExtensionID[] = "whitelisted-extension-id";

// Pass launch source to platform apps.
const char kTraceAppSource[] = "enable-trace-app-source";

// Enable package hash check: the .crx file sha256 hash sum should be equal to
// the one received from update manifest.
const char kEnableCrxHashCheck[] = "enable-crx-hash-check";

}  // namespace switches

}  // namespace extensions
