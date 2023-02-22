// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/pref_names.h"

#include "base/notreached.h"
#include "build/build_config.h"

namespace extensions {
namespace pref_names {

bool ScopeToPrefName(ExtensionPrefsScope scope, std::string* result) {
  switch (scope) {
    case kExtensionPrefsScopeRegular:
      *result = kPrefPreferences;
      return true;
    case kExtensionPrefsScopeRegularOnly:
      *result = kPrefRegularOnlyPreferences;
      return true;
    case kExtensionPrefsScopeIncognitoPersistent:
      *result = kPrefIncognitoPreferences;
      return true;
    case kExtensionPrefsScopeIncognitoSessionOnly:
      return false;
  }
  NOTREACHED();
  return false;
}

const char kAlertsInitialized[] = "extensions.alerts.initialized";
const char kAllowedInstallSites[] = "extensions.allowed_install_sites";
const char kAllowedTypes[] = "extensions.allowed_types";
const char kAppFullscreenAllowed[] = "apps.fullscreen.allowed";
const char kBlockExternalExtensions[] = "extensions.block_external_extensions";
const char kDeletedComponentExtensions[] =
    "extensions.deleted_component_extensions";
const char kExtendedBackgroundLifetimeForPortConnectionsToUrls[] =
    "extensions.extended_background_lifetime_urls";
const char kExtensions[] = "extensions.settings";
const char kExtensionManagement[] = "extensions.management";
const char kExtensionUnpublishedAvailability[] =
    "extensions.unpublished_availability";
const char kInstallAllowList[] = "extensions.install.allowlist";
const char kInstallDenyList[] = "extensions.install.denylist";
const char kInstallForceList[] = "extensions.install.forcelist";
const char kLastChromeVersion[] = "extensions.last_chrome_version";
const char kNativeMessagingBlocklist[] = "native_messaging.blocklist";
const char kNativeMessagingAllowlist[] = "native_messaging.allowlist";
const char kNativeMessagingUserLevelHosts[] =
    "native_messaging.user_level_hosts";
const char kManifestV2Availability[] = "extensions.manifest_v2";
const char kPinnedExtensions[] = "extensions.pinned_extensions";
const char kStorageGarbageCollect[] = "extensions.storage.garbagecollect";
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
extern const char kChromeAppsEnabled[] = "extensions.chrome_apps_enabled";
#endif
const char kChromeAppsWebViewPermissiveBehaviorAllowed[] =
    "extensions.webview_permissive_behavior";

const char kPrefPreferences[] = "preferences";
const char kPrefIncognitoPreferences[] = "incognito_preferences";
const char kPrefRegularOnlyPreferences[] = "regular_only_preferences";
const char kPrefContentSettings[] = "content_settings";
const char kPrefIncognitoContentSettings[] = "incognito_content_settings";

}  // namespace pref_names
}  // namespace extensions
