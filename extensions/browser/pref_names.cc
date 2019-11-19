// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/pref_names.h"

#include "base/logging.h"

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
const char kExtensions[] = "extensions.settings";
const char kExtensionCheckupOnStartup[] = "extensions.checkup_on_startup";
const char kExtensionManagement[] = "extensions.management";
const char kInstallAllowList[] = "extensions.install.allowlist";
const char kInstallDenyList[] = "extensions.install.denylist";
const char kInstallForceList[] = "extensions.install.forcelist";
const char kLoginScreenExtensions[] =
    "extensions.install.login_screen_extensions";
const char kLastChromeVersion[] = "extensions.last_chrome_version";
const char kNativeMessagingBlacklist[] = "native_messaging.blacklist";
const char kNativeMessagingWhitelist[] = "native_messaging.whitelist";
const char kNativeMessagingUserLevelHosts[] =
    "native_messaging.user_level_hosts";
const char kPinnedExtensions[] = "extensions.pinned_extensions";
const char kStorageGarbageCollect[] = "extensions.storage.garbagecollect";
const char kToolbar[] = "extensions.toolbar";
const char kToolbarSize[] = "extensions.toolbarsize";

const char kPrefPreferences[] = "preferences";
const char kPrefIncognitoPreferences[] = "incognito_preferences";
const char kPrefRegularOnlyPreferences[] = "regular_only_preferences";
const char kPrefContentSettings[] = "content_settings";
const char kPrefIncognitoContentSettings[] = "incognito_content_settings";

}  // namespace pref_names
}  // namespace extensions
