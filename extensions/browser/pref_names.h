// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PREF_NAMES_H_
#define EXTENSIONS_BROWSER_PREF_NAMES_H_

#include <string>

#include "build/build_config.h"
#include "extensions/common/api/types.h"

// Preference keys which are needed by both the ExtensionPrefs and by external
// clients, such as APIs.

namespace extensions {
namespace pref_names {

// If the given |scope| is persisted, return true and populate |result| with the
// appropriate property (i.e. one of kPref*) within a kExtensions dictionary. If
// |scope| is not persisted, return false, and leave |result| unchanged.
bool ScopeToPrefName(extensions::api::types::ChromeSettingScope scope,
                     std::string* result);

// Browser-level preferences ---------------------------------------------------

// Whether we have run the extension-alert system (see ExtensionGlobalError)
// at least once for this profile.
inline constexpr char kAlertsInitialized[] = "extensions.alerts.initialized";

// The sites that are allowed to install extensions. These sites should be
// allowed to install extensions without the scary dangerous downloads bar.
// Also, when off-store-extension installs are disabled, these sites are exempt.
inline constexpr char kAllowedInstallSites[] =
    "extensions.allowed_install_sites";

// A list of allowed extension types. Extensions can only be installed if their
// type is on this allowlist or alternatively on kInstallAllowList or
// kInstallForceList.
inline constexpr char kAllowedTypes[] = "extensions.allowed_types";

// A boolean that tracks whether apps are allowed to enter fullscreen mode.
inline constexpr char kAppFullscreenAllowed[] = "apps.fullscreen.allowed";

// A boolean indicating if external extensions are blocked from installing.
inline constexpr char kBlockExternalExtensions[] =
    "extensions.block_external_extensions";

// A preference for a list of Component extensions that have been
// uninstalled/removed and should not be reloaded.
inline constexpr char kDeletedComponentExtensions[] =
    "extensions.deleted_component_extensions";

// A list of app origins that will grant a long-lived background lifetime to
// the connecting extension, if connected to them via persistent messaging
// ports. The value is controlled by the
// `ExtensionExtendedBackgroundLifetimeForPortConnectionsToUrls` policy.
inline constexpr char kExtendedBackgroundLifetimeForPortConnectionsToUrls[] =
    "extensions.extended_background_lifetime_urls";

// Dictionary pref that keeps track of per-extension settings. The keys are
// extension ids.
inline constexpr char kExtensions[] = "extensions.settings";

// Dictionary pref that manages extensions, controlled by policy.
// Values are expected to conform to the schema of the ExtensionManagement
// policy.
inline constexpr char kExtensionManagement[] = "extensions.management";

// An integer that indicates the availability of extensions that are unpublished
// on the Chrome Web Store. More details can be found at
// ExtensionUnpublishedAvailability.yaml
inline constexpr char kExtensionUnpublishedAvailability[] =
    "extensions.unpublished_availability";

// A allowlist of extension ids the user can install: exceptions from the
// following denylist.
inline constexpr char kInstallAllowList[] = "extensions.install.allowlist";

// A denylist, containing extensions the user cannot install. This list can
// contain "*" meaning all extensions. This list should not be confused with the
// extension blocklist, which is Google controlled.
inline constexpr char kInstallDenyList[] = "extensions.install.denylist";

// A list containing extensions that Chrome will silently install
// at startup time. It is a list of strings, each string contains
// an extension ID and an update URL, delimited by a semicolon.
// This preference is set by an admin policy, and meant to be only
// accessed through extensions::ExternalPolicyProvider.
inline constexpr char kInstallForceList[] = "extensions.install.forcelist";

// A dictionary containing, for each extension id, additional
// OAuth redirect URLs that will be allowed in chrome.identity API.
inline constexpr char kOAuthRedirectUrls[] = "extensions.oauth_redirect_urls";

// String pref for what version chrome was last time the extension prefs were
// loaded.
inline constexpr char kLastChromeVersion[] = "extensions.last_chrome_version";

// Blocklist and allowlist for Native Messaging Hosts.
inline constexpr char kNativeMessagingBlocklist[] =
    "native_messaging.blocklist";
inline constexpr char kNativeMessagingAllowlist[] =
    "native_messaging.allowlist";

// Flag allowing usage of Native Messaging hosts installed on user level.
inline constexpr char kNativeMessagingUserLevelHosts[] =
    "native_messaging.user_level_hosts";

// An integer indicates the availability of manifest v2 extensions. The value is
// controlled by the ExtensionManifestV2Availability policy. More details can
// be found at ExtensionManifestV2Availability.yaml.
inline constexpr char kManifestV2Availability[] = "extensions.manifest_v2";

// A preference that tracks extensions pinned to the toolbar. This is a list
// object stored in the Preferences file. The extensions are stored by ID.
inline constexpr char kPinnedExtensions[] = "extensions.pinned_extensions";

// Indicates on-disk data might have skeletal data that needs to be cleaned
// on the next start of the browser.
// TODO(crbug.com/40922689): Delete ExtensionsPref::kStorageGarbageCollect.
inline constexpr char kStorageGarbageCollect[] =
    "extensions.storage.garbagecollect";

// Pref for policy to enable/disable loading extension from command line
inline constexpr char kExtensionInstallTypeBlocklist[] =
    "extensions.extension_install_type_blocklist";

// Properties in kExtensions dictionaries --------------------------------------

// Extension-controlled preferences.
extern const char kPrefPreferences[];

// Extension-controlled incognito preferences.
extern const char kPrefIncognitoPreferences[];

// Extension-controlled regular-only preferences.
extern const char kPrefRegularOnlyPreferences[];

// Extension-set content settings.
extern const char kPrefContentSettings[];

// Extension-set incognito content settings.
extern const char kPrefIncognitoContentSettings[];

}  // namespace pref_names
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PREF_NAMES_H_
