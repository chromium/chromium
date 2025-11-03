// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_SWITCHES_H_
#define EXTENSIONS_COMMON_SWITCHES_H_

#include "build/build_config.h"

namespace extensions::switches {

// Allows non-https URL for background_page for hosted apps.
extern const char kAllowHTTPBackgroundPage[];

// Allows the browser to load extensions that lack a modern manifest when that
// would otherwise be forbidden.
extern const char kAllowLegacyExtensionManifests[];

// Adds the given extension ID to all the permission allowlists.
extern const char kAllowlistedExtensionID[];

// Whether to disable app content verification when testing changes locally on
// Chromebox for Meetings hardware.
extern const char kDisableAppContentVerification[];

// Disable checking for user opt-in for extensions that want to inject script
// into file URLs (ie, always allow it). This is used during automated testing.
extern const char kDisableExtensionsFileAccessCheck[];

// Disables extensions.
inline constexpr char kDisableExtensions[] = "disable-extensions";

// Disable extensions except those specified in a comma-separated list.
inline constexpr char kDisableExtensionsExcept[] = "disable-extensions-except";

// Disable the net::URLRequestThrottlerManager functionality for
// requests originating from extensions.
extern const char kDisableExtensionsHttpThrottling[];

// Enables extension options to be embedded in chrome://extensions rather than
// a new tab.
extern const char kEmbeddedExtensionOptions[];

// Enables extension APIs that are in development.
extern const char kEnableExperimentalExtensionApis[];

// Enable BLE Advertising in apps.
extern const char kEnableBLEAdvertising[];

// Marks a renderer as extension process.
extern const char kExtensionProcess[];

// Enables extensions running scripts on chrome:// URLs.
// Extensions still need to explicitly request access to chrome:// URLs in the
// manifest.
extern const char kExtensionsOnChromeURLs[];

// Enables extensions running scripts on chrome-extension:// URLs.
// Extensions still need to explicitly request access to chrome-extension://
// URLs in the manifest.
extern const char kExtensionsOnExtensionURLs[];

// Comma-separated list of paths to apps to load at startup. The first app in
// the list will be launched.
extern const char kLoadApps[];

// Comma-separated list of paths to extensions to load at startup.
extern const char kLoadExtension[];

#if BUILDFLAG(IS_CHROMEOS)
// Path to the unpacked test extension to load into the signin profile. The ID
// extension loaded must match kTestSigninProfileExtensionId.
extern const char kLoadSigninProfileTestExtension[];

// Path to the unpacked test extension to load into guest mode. The extension ID
// must match kGuestModeTestExtensionId.
extern const char kLoadGuestModeTestExtension[];
#endif

// Allows the use of the `testing` reason in offscreen documents.
extern const char kOffscreenDocumentTesting[];

// Set the parameters for ExtensionURLLoaderThrottleBrowserTest.
extern const char kSetExtensionThrottleTestParams[];

// Makes component extensions appear in chrome://settings/extensions.
extern const char kShowComponentExtensionOptions[];

// Pass launch source to platform apps.
extern const char kTraceAppSource[];

// Enable package hash check: the .crx file sha256 hash sum should be equal to
// the one received from update manifest.
extern const char kEnableCrxHashCheck[];

// Mute extension errors while working with new manifest version.
extern const char kAllowFutureManifestVersion[];

// Allow the chrome.test API to be exposed on web page contexts for testing.
// TODO(tjudkins): This will need to be added to the list of flags that get
// copied from the browser to the renderer in ChromeContentBrowserClient to
// actually use it in browser tests.
extern const char kExtensionTestApiOnWebPages[];

// The feature parameter name that controls the variant of IPH shown when the
// user has no extensions installed.
extern const char kZeroStatePromoIphVariantParamName[];

// When the user has no extensions installed, display a custom action IPH
// that upon triggering, opens a new tab to the Chrome Web Store.
extern const char kZeroStatePromoCustomActionIph[];

// When the user has no extensions installed, display a custom UI IPH that
// presents the user with different collections of extensions to explore,
// each in a cr-chip button.
extern const char kZeroStatePromoCustomUiChipIphV1[];

// When the user has no extensions installed, display a custom UI IPH that
// presents the user with different collections of extensions to explore,
// each in a cr-chip button. This variation has a different color scheme
// to highlight the chips.
extern const char kZeroStatePromoCustomUiChipIphV2[];

// When the user has no extensions installed, display a custom UI IPH that
// presents the user with different collections of extensions to explore,
// each in a cr-chip button. This variation has a different color scheme
// to highlight the chips. The ordering and selection of the chips are
// slightly different than variation 2.
extern const char kZeroStatePromoCustomUiChipIphV3[];

// When the user has no extensions installed, display a custom UI IPH that
// presents the user with different collections of extensions to explore,
// each in a plain text link.
extern const char kZeroStatePromoCustomUiPlainLinkIph[];

// Turns on extension install verification if it would not otherwise have been
// turned on.
extern const char kExtensionsInstallVerification[];

// Specifies a comma-separated list of extension ids that should be forced to
// be treated as not from the webstore when doing install verification.
extern const char kExtensionsNotWebstore[];

// Checks if extensions are allowed to run on chrome:// URLs.
bool AreExtensionsOnChromeURLsAllowed();

// Checks if extensions are allowed to run on chrome-extension:// URLs.
bool AreExtensionsOnExtensionURLsAllowed();

}  // namespace extensions::switches

#endif  // EXTENSIONS_COMMON_SWITCHES_H_
