// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/switches.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/chromeos_buildflags.h"
#include "extensions/common/extension_features.h"

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
const char kExtensionsOnExtensionURLs[] = "extensions-on-extension-urls";

const char kDisableAppContentVerification[] =
    "disable-app-content-verification";
const char kLoadApps[] = "load-apps";
const char kLoadExtension[] = "load-extension";

#if BUILDFLAG(IS_CHROMEOS)
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

const char kZeroStatePromoIphVariantParamName[] =
    "extension-zero-state-iph-variant";
const char kZeroStatePromoCustomActionIph[] = "custom-action-iph";
const char kZeroStatePromoCustomUiChipIphV1[] = "custom-ui-chip-iph";
const char kZeroStatePromoCustomUiChipIphV2[] = "custom-ui-chip-iph-v2";
const char kZeroStatePromoCustomUiChipIphV3[] = "custom-ui-chip-iph-v3";
const char kZeroStatePromoCustomUiPlainLinkIph[] = "custom-ui-plain-link-iph";

const char kExtensionsInstallVerification[] = "extensions-install-verification";
const char kExtensionsNotWebstore[] = "extensions-not-webstore";

bool AreExtensionsOnChromeURLsAllowed() {
  if (base::FeatureList::IsEnabled(
          extensions_features::kDisableExtensionsOnChromeUrlsSwitch)) {
    // Switch is never allowed with the feature enabled.
    return false;
  }

  // Otherwise, all extensions on chrome:-scheme URLs if the relevant
  // commandline switch is present.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kExtensionsOnChromeURLs);
}

bool AreExtensionsOnExtensionURLsAllowed() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExtensionsOnExtensionURLs)) {
    // Extensions are allowed to run on other extensions with the
    // appropriate commandline switch.
    return true;
  }

  // Otherwise, allow extensions on other extension URLs with the
  // --extensions-on-chrome-urls flag. This is for backwards compatibility
  // only.
  // TODO(crbug.com/419530940): Remove extension URLs check on
  // `--extensions-on-chrome-urls` switch once fully launched.
  return !base::FeatureList::IsEnabled(
             extensions_features::kDisableExtensionsOnChromeUrlsSwitch) &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kExtensionsOnChromeURLs);
}

}  // namespace extensions::switches
