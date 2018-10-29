// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_prefs.h"

#include <stddef.h>
#include <stdint.h>

#include <iterator>
#include <utility>

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/crx_file/id_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/user_script.h"

namespace extensions {

namespace {

// Additional preferences keys, which are not needed by external clients.

// True if this extension is running. Note this preference stops getting updated
// during Chrome shutdown (and won't be updated on a browser crash) and so can
// be used at startup to determine whether the extension was running when Chrome
// was last terminated.
constexpr const char kPrefRunning[] = "running";

// Whether this extension had windows when it was last running.
constexpr const char kIsActive[] = "is_active";

// Where an extension was installed from. (see Manifest::Location)
constexpr const char kPrefLocation[] = "location";

// Enabled, disabled, killed, etc. (see Extension::State)
constexpr const char kPrefState[] = "state";

// The path to the current version's manifest file.
constexpr const char kPrefPath[] = "path";

// The dictionary containing the extension's manifest.
constexpr const char kPrefManifest[] = "manifest";

// The version number.
constexpr const char kPrefVersion[] = "manifest.version";

// Indicates whether an extension is blacklisted.
constexpr const char kPrefBlacklist[] = "blacklist";

// If extension is greylisted.
constexpr const char kPrefBlacklistState[] = "blacklist_state";

// The count of how many times we prompted the user to acknowledge an
// extension.
constexpr const char kPrefAcknowledgePromptCount[] = "ack_prompt_count";

// Indicates whether the user has acknowledged various types of extensions.
constexpr const char kPrefExternalAcknowledged[] = "ack_external";
constexpr const char kPrefBlacklistAcknowledged[] = "ack_blacklist";

// Indicates whether the external extension was installed during the first
// run of this profile.
constexpr const char kPrefExternalInstallFirstRun[] = "external_first_run";

// A bitmask of all the reasons an extension is disabled.
constexpr const char kPrefDisableReasons[] = "disable_reasons";

// The key for a serialized Time value indicating the start of the day (from the
// server's perspective) an extension last included a "ping" parameter during
// its update check.
constexpr const char kLastPingDay[] = "lastpingday";

// Similar to kLastPingDay, but for "active" instead of "rollcall" pings.
constexpr const char kLastActivePingDay[] = "last_active_pingday";

// A bit we use to keep track of whether we need to do an "active" ping.
constexpr const char kActiveBit[] = "active_bit";

// Path for settings specific to blacklist update.
constexpr const char kExtensionsBlacklistUpdate[] =
    "extensions.blacklistupdate";

// Path for the delayed install info dictionary preference. The actual string
// value is a legacy artifact for when delayed installs only pertained to
// updates that were waiting for idle.
constexpr const char kDelayedInstallInfo[] = "idle_install_info";

// Reason why the extension's install was delayed.
constexpr const char kDelayedInstallReason[] = "delay_install_reason";

// Path for the suggested page ordinal of a delayed extension install.
constexpr const char kPrefSuggestedPageOrdinal[] = "suggested_page_ordinal";

// A preference that, if true, will allow this extension to run in incognito
// mode.
constexpr const char kPrefIncognitoEnabled[] = "incognito";

// A preference to control whether an extension is allowed to inject script in
// pages with file URLs.
constexpr const char kPrefAllowFileAccess[] = "newAllowFileAccess";
// TODO(jstritar): As part of fixing http://crbug.com/91577, we revoked all
// extension file access by renaming the pref. We should eventually clean up
// the old flag and possibly go back to that name.
// constexpr const char kPrefAllowFileAccessOld[] = "allowFileAccess";

// A preference specifying if the user dragged the app on the NTP.
constexpr const char kPrefUserDraggedApp[] = "user_dragged_app_ntp";

// Preferences that hold which permissions the user has granted the extension.
// We explicitly keep track of these so that extensions can contain unknown
// permissions, for backwards compatibility reasons, and we can still prompt
// the user to accept them once recognized. We store the active permission
// permissions because they may differ from those defined in the manifest.
constexpr const char kPrefActivePermissions[] = "active_permissions";
constexpr const char kPrefGrantedPermissions[] = "granted_permissions";

// The set of permissions that were granted at runtime, rather than at install
// time. This includes permissions granted through the permissions API and
// runtime host permissions.
constexpr const char kPrefRuntimeGrantedPermissions[] =
    "runtime_granted_permissions";

// The preference names for PermissionSet values.
constexpr const char kPrefAPIs[] = "api";
constexpr const char kPrefManifestPermissions[] = "manifest_permissions";
constexpr const char kPrefExplicitHosts[] = "explicit_host";
constexpr const char kPrefScriptableHosts[] = "scriptable_host";

// A preference that indicates when an extension was installed.
constexpr const char kPrefInstallTime[] = "install_time";

// A preference which saves the creation flags for extensions.
constexpr const char kPrefCreationFlags[] = "creation_flags";

// A preference that indicates whether the extension was installed from the
// Chrome Web Store.
constexpr const char kPrefFromWebStore[] = "from_webstore";

// A preference that indicates whether the extension was installed from a
// mock App created from a bookmark.
constexpr const char kPrefFromBookmark[] = "from_bookmark";

// A preference that indicates whether the extension was installed as a
// default app.
constexpr const char kPrefWasInstalledByDefault[] = "was_installed_by_default";

// A preference that indicates whether the extension was installed as an
// OEM app.
constexpr const char kPrefWasInstalledByOem[] = "was_installed_by_oem";

// Key for Geometry Cache preference.
constexpr const char kPrefGeometryCache[] = "geometry_cache";

// A preference that indicates when an extension is last launched.
constexpr const char kPrefLastLaunchTime[] = "last_launch_time";

// Am installation parameter bundled with an extension.
constexpr const char kPrefInstallParam[] = "install_parameter";

// A list of installed ids and a signature.
constexpr const char kInstallSignature[] = "extensions.install_signature";

// A boolean preference that indicates whether the extension should not be
// synced. Default value is false.
constexpr const char kPrefDoNotSync[] = "do_not_sync";

constexpr const char kCorruptedDisableCount[] =
    "extensions.corrupted_disable_count";

// A boolean preference that indicates whether the extension has local changes
// that need to be synced. Default value is false.
constexpr const char kPrefNeedsSync[] = "needs_sync";

// The indexed ruleset checksum for the Declarative Net Request API.
constexpr const char kPrefDNRRulesetChecksum[] = "dnr_ruleset_checksum";

// List of match patterns representing the set of allowed pages for an
// extension for the Declarative Net Request API.
constexpr const char kPrefDNRAllowedPages[] = "dnr_whitelisted_pages";

// Provider of write access to a dictionary storing extension prefs.
class ScopedExtensionPrefUpdate : public prefs::ScopedDictionaryPrefUpdate {
 public:
  ScopedExtensionPrefUpdate(PrefService* service,
                            const std::string& extension_id)
      : ScopedDictionaryPrefUpdate(service, pref_names::kExtensions),
        extension_id_(extension_id) {
    DCHECK(crx_file::id_util::IdIsValid(extension_id_));
  }

  ~ScopedExtensionPrefUpdate() override {}

  // ScopedDictionaryPrefUpdate overrides:
  std::unique_ptr<prefs::DictionaryValueUpdate> Get() override {
    std::unique_ptr<prefs::DictionaryValueUpdate> dict =
        ScopedDictionaryPrefUpdate::Get();
    std::unique_ptr<prefs::DictionaryValueUpdate> extension;
    if (!dict->GetDictionary(extension_id_, &extension)) {
      // Extension pref does not exist, create it.
      extension = dict->SetDictionary(
          extension_id_, std::make_unique<base::DictionaryValue>());
    }
    return extension;
  }

 private:
  const std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(ScopedExtensionPrefUpdate);
};

std::string JoinPrefs(base::StringPiece parent, base::StringPiece child) {
  return base::JoinString({parent, child}, ".");
}

// Checks if kPrefBlacklist is set to true in the base::DictionaryValue.
// Return false if the value is false or kPrefBlacklist does not exist.
// This is used to decide if an extension is blacklisted.
bool IsBlacklistBitSet(const base::DictionaryValue* ext) {
  bool bool_value;
  return ext->GetBoolean(kPrefBlacklist, &bool_value) && bool_value;
}

// Whether SetAlertSystemFirstRun() should always return true, so that alerts
// are triggered, even in first run.
bool g_run_alerts_in_first_run_for_testing = false;

}  // namespace

//
// ScopedDictionaryUpdate
//
ExtensionPrefs::ScopedDictionaryUpdate::ScopedDictionaryUpdate(
    ExtensionPrefs* prefs,
    const std::string& extension_id,
    const std::string& key)
    : update_(std::make_unique<ScopedExtensionPrefUpdate>(prefs->pref_service(),
                                                          extension_id)),
      key_(key) {}

ExtensionPrefs::ScopedDictionaryUpdate::~ScopedDictionaryUpdate() = default;

std::unique_ptr<prefs::DictionaryValueUpdate>
ExtensionPrefs::ScopedDictionaryUpdate::Get() {
  auto dict = update_->Get();
  std::unique_ptr<prefs::DictionaryValueUpdate> key_value;
  dict->GetDictionary(key_, &key_value);
  return key_value;
}

std::unique_ptr<prefs::DictionaryValueUpdate>
ExtensionPrefs::ScopedDictionaryUpdate::Create() {
  auto dict = update_->Get();
  std::unique_ptr<prefs::DictionaryValueUpdate> key_value;
  if (dict->GetDictionary(key_, &key_value))
    return key_value;

  return dict->SetDictionary(key_, std::make_unique<base::DictionaryValue>());
}

ExtensionPrefs::ScopedListUpdate::ScopedListUpdate(
    ExtensionPrefs* prefs,
    const std::string& extension_id,
    const std::string& key)
    : update_(std::make_unique<ScopedExtensionPrefUpdate>(prefs->pref_service(),
                                                          extension_id)),
      key_(key) {}

ExtensionPrefs::ScopedListUpdate::~ScopedListUpdate() = default;

base::ListValue* ExtensionPrefs::ScopedListUpdate::Get() {
  base::ListValue* key_value = NULL;
  (*update_)->GetList(key_, &key_value);
  return key_value;
}

base::ListValue* ExtensionPrefs::ScopedListUpdate::Create() {
  base::ListValue* key_value = NULL;
  if ((*update_)->GetList(key_, &key_value))
    return key_value;

  auto list_value = std::make_unique<base::ListValue>();
  key_value = list_value.get();
  (*update_)->Set(key_, std::move(list_value));
  return key_value;
}

//
// ExtensionPrefs
//

// static
ExtensionPrefs* ExtensionPrefs::Create(
    content::BrowserContext* browser_context,
    PrefService* prefs,
    const base::FilePath& root_dir,
    ExtensionPrefValueMap* extension_pref_value_map,
    bool extensions_disabled,
    const std::vector<ExtensionPrefsObserver*>& early_observers) {
  return ExtensionPrefs::Create(
      browser_context, prefs, root_dir, extension_pref_value_map,
      extensions_disabled, early_observers, base::DefaultClock::GetInstance());
}

// static
ExtensionPrefs* ExtensionPrefs::Create(
    content::BrowserContext* browser_context,
    PrefService* pref_service,
    const base::FilePath& root_dir,
    ExtensionPrefValueMap* extension_pref_value_map,
    bool extensions_disabled,
    const std::vector<ExtensionPrefsObserver*>& early_observers,
    base::Clock* clock) {
  return new ExtensionPrefs(browser_context, pref_service, root_dir,
                            extension_pref_value_map, clock,
                            extensions_disabled, early_observers);
}

ExtensionPrefs::~ExtensionPrefs() {
}

// static
ExtensionPrefs* ExtensionPrefs::Get(content::BrowserContext* context) {
  return ExtensionPrefsFactory::GetInstance()->GetForBrowserContext(context);
}

static base::FilePath::StringType MakePathRelative(const base::FilePath& parent,
                                             const base::FilePath& child) {
  if (!parent.IsParent(child))
    return child.value();

  base::FilePath::StringType retval = child.value().substr(
      parent.value().length());
  if (base::FilePath::IsSeparator(retval[0]))
    return retval.substr(1);
  else
    return retval;
}

void ExtensionPrefs::MakePathsRelative() {
  const base::DictionaryValue* dict =
      prefs_->GetDictionary(pref_names::kExtensions);
  if (!dict || dict->empty())
    return;

  // Collect all extensions ids with absolute paths in |absolute_keys|.
  std::set<std::string> absolute_keys;
  for (base::DictionaryValue::Iterator i(*dict); !i.IsAtEnd(); i.Advance()) {
    const base::DictionaryValue* extension_dict = NULL;
    if (!i.value().GetAsDictionary(&extension_dict))
      continue;
    int location_value;
    if (extension_dict->GetInteger(kPrefLocation, &location_value) &&
        Manifest::IsUnpackedLocation(
            static_cast<Manifest::Location>(location_value))) {
      // Unpacked extensions can have absolute paths.
      continue;
    }
    base::FilePath::StringType path_string;
    if (!extension_dict->GetString(kPrefPath, &path_string))
      continue;
    base::FilePath path(path_string);
    if (path.IsAbsolute())
      absolute_keys.insert(i.key());
  }
  if (absolute_keys.empty())
    return;

  // Fix these paths.
  prefs::ScopedDictionaryPrefUpdate update(prefs_, pref_names::kExtensions);
  auto update_dict = update.Get();
  for (auto i = absolute_keys.begin(); i != absolute_keys.end(); ++i) {
    std::unique_ptr<prefs::DictionaryValueUpdate> extension_dict;
    if (!update_dict->GetDictionaryWithoutPathExpansion(*i, &extension_dict)) {
      NOTREACHED() << "Control should never reach here for extension " << *i;
      continue;
    }
    base::FilePath::StringType path_string;
    extension_dict->GetString(kPrefPath, &path_string);
    base::FilePath path(path_string);
    extension_dict->SetString(kPrefPath,
        MakePathRelative(install_directory_, path));
  }
}

const base::DictionaryValue* ExtensionPrefs::GetExtensionPref(
    const std::string& extension_id) const {
  const base::DictionaryValue* extensions =
      prefs_->GetDictionary(pref_names::kExtensions);
  const base::DictionaryValue* extension_dict = NULL;
  if (!extensions ||
      !extensions->GetDictionary(extension_id, &extension_dict)) {
    return NULL;
  }
  return extension_dict;
}

void ExtensionPrefs::UpdateExtensionPref(
    const std::string& extension_id,
    base::StringPiece key,
    std::unique_ptr<base::Value> data_value) {
  if (!crx_file::id_util::IdIsValid(extension_id)) {
    NOTREACHED() << "Invalid extension_id " << extension_id;
    return;
  }
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  if (data_value)
    update->Set(key, std::move(data_value));
  else
    update->Remove(key, NULL);
}

void ExtensionPrefs::DeleteExtensionPrefs(const std::string& extension_id) {
  extension_pref_value_map_->UnregisterExtension(extension_id);
  for (auto& observer : observer_list_)
    observer.OnExtensionPrefsDeleted(extension_id);
  prefs::ScopedDictionaryPrefUpdate update(prefs_, pref_names::kExtensions);
  update->Remove(extension_id, NULL);
}

bool ExtensionPrefs::ReadPrefAsBoolean(const std::string& extension_id,
                                       base::StringPiece pref_key,
                                       bool* out_value) const {
  const base::DictionaryValue* ext = GetExtensionPref(extension_id);
  if (!ext || !ext->GetBoolean(pref_key, out_value))
    return false;

  return true;
}

bool ExtensionPrefs::ReadPrefAsInteger(const std::string& extension_id,
                                       base::StringPiece pref_key,
                                       int* out_value) const {
  const base::DictionaryValue* ext = GetExtensionPref(extension_id);
  if (!ext || !ext->GetInteger(pref_key, out_value))
    return false;

  return true;
}

bool ExtensionPrefs::ReadPrefAsString(const std::string& extension_id,
                                      base::StringPiece pref_key,
                                      std::string* out_value) const {
  const base::DictionaryValue* ext = GetExtensionPref(extension_id);
  if (!ext || !ext->GetString(pref_key, out_value))
    return false;

  return true;
}

bool ExtensionPrefs::ReadPrefAsList(const std::string& extension_id,
                                    base::StringPiece pref_key,
                                    const base::ListValue** out_value) const {
  const base::DictionaryValue* ext = GetExtensionPref(extension_id);
  const base::ListValue* out = NULL;
  if (!ext || !ext->GetList(pref_key, &out))
    return false;
  if (out_value)
    *out_value = out;

  return true;
}

bool ExtensionPrefs::ReadPrefAsDictionary(
    const std::string& extension_id,
    base::StringPiece pref_key,
    const base::DictionaryValue** out_value) const {
  const base::DictionaryValue* ext = GetExtensionPref(extension_id);
  const base::DictionaryValue* out = NULL;
  if (!ext || !ext->GetDictionary(pref_key, &out))
    return false;
  if (out_value)
    *out_value = out;

  return true;
}

bool ExtensionPrefs::HasPrefForExtension(
    const std::string& extension_id) const {
  return GetExtensionPref(extension_id) != NULL;
}

bool ExtensionPrefs::ReadPrefAsURLPatternSet(const std::string& extension_id,
                                             base::StringPiece pref_key,
                                             URLPatternSet* result,
                                             int valid_schemes) const {
  const base::ListValue* value = NULL;
  if (!ReadPrefAsList(extension_id, pref_key, &value))
    return false;
  const base::DictionaryValue* extension = GetExtensionPref(extension_id);
  if (!extension)
    return false;
  int location;
  if (extension->GetInteger(kPrefLocation, &location) &&
      static_cast<Manifest::Location>(location) == Manifest::COMPONENT) {
    valid_schemes |= URLPattern::SCHEME_CHROMEUI;
  }

  bool allow_file_access = AllowFileAccess(extension_id);
  return result->Populate(*value, valid_schemes, allow_file_access, NULL);
}

void ExtensionPrefs::SetExtensionPrefURLPatternSet(
    const std::string& extension_id,
    base::StringPiece pref_key,
    const URLPatternSet& set) {
  // Clear the |pref_key| in case |set| is empty.
  std::unique_ptr<base::Value> value = set.is_empty() ? nullptr : set.ToValue();
  UpdateExtensionPref(extension_id, pref_key, std::move(value));
}

bool ExtensionPrefs::ReadPrefAsBooleanAndReturn(
    const std::string& extension_id,
    base::StringPiece pref_key) const {
  bool out_value = false;
  return ReadPrefAsBoolean(extension_id, pref_key, &out_value) && out_value;
}

std::unique_ptr<const PermissionSet> ExtensionPrefs::ReadPrefAsPermissionSet(
    const std::string& extension_id,
    base::StringPiece pref_key) const {
  if (!GetExtensionPref(extension_id))
    return nullptr;

  // Retrieve the API permissions. Please refer SetExtensionPrefPermissionSet()
  // for api_values format.
  APIPermissionSet apis;
  const base::ListValue* api_values = NULL;
  std::string api_pref = JoinPrefs(pref_key, kPrefAPIs);
  if (ReadPrefAsList(extension_id, api_pref, &api_values)) {
    APIPermissionSet::ParseFromJSON(api_values,
                                    APIPermissionSet::kAllowInternalPermissions,
                                    &apis, NULL, NULL);
  }

  // Retrieve the Manifest Keys permissions. Please refer to
  // |SetExtensionPrefPermissionSet| for manifest_permissions_values format.
  ManifestPermissionSet manifest_permissions;
  const base::ListValue* manifest_permissions_values = NULL;
  std::string manifest_permission_pref =
      JoinPrefs(pref_key, kPrefManifestPermissions);
  if (ReadPrefAsList(extension_id, manifest_permission_pref,
                     &manifest_permissions_values)) {
    ManifestPermissionSet::ParseFromJSON(
        manifest_permissions_values, &manifest_permissions, NULL, NULL);
  }

  // Retrieve the explicit host permissions.
  URLPatternSet explicit_hosts;
  ReadPrefAsURLPatternSet(
      extension_id, JoinPrefs(pref_key, kPrefExplicitHosts),
      &explicit_hosts, Extension::kValidHostPermissionSchemes);

  // Retrieve the scriptable host permissions.
  URLPatternSet scriptable_hosts;
  ReadPrefAsURLPatternSet(
      extension_id, JoinPrefs(pref_key, kPrefScriptableHosts),
      &scriptable_hosts, UserScript::ValidUserScriptSchemes());

  return std::make_unique<PermissionSet>(apis, manifest_permissions,
                                         explicit_hosts, scriptable_hosts);
}

// Set the API or Manifest permissions.
// The format of api_values is:
// [ "permission_name1",   // permissions do not support detail.
//   "permission_name2",
//   {"permission_name3": value },
//   // permission supports detail, permission detail will be stored in value.
//   ...
// ]
template <typename T>
static std::unique_ptr<base::ListValue> CreatePermissionList(
    const T& permissions) {
  auto values = std::make_unique<base::ListValue>();
  for (typename T::const_iterator i = permissions.begin();
      i != permissions.end(); ++i) {
    std::unique_ptr<base::Value> detail(i->ToValue());
    if (detail) {
      auto tmp(std::make_unique<base::DictionaryValue>());
      tmp->Set(i->name(), std::move(detail));
      values->Append(std::move(tmp));
    } else {
      values->AppendString(i->name());
    }
  }
  return values;
}

void ExtensionPrefs::SetExtensionPrefPermissionSet(
    const std::string& extension_id,
    base::StringPiece pref_key,
    const PermissionSet& new_value) {
  std::string api_pref = JoinPrefs(pref_key, kPrefAPIs);
  UpdateExtensionPref(extension_id, api_pref,
                      CreatePermissionList(new_value.apis()));

  std::string manifest_permissions_pref =
      JoinPrefs(pref_key, kPrefManifestPermissions);
  UpdateExtensionPref(extension_id, manifest_permissions_pref,
                      CreatePermissionList(new_value.manifest_permissions()));

  // Set the explicit host permissions.
  SetExtensionPrefURLPatternSet(extension_id,
                                JoinPrefs(pref_key, kPrefExplicitHosts),
                                new_value.explicit_hosts());

  // Set the scriptable host permissions.
  SetExtensionPrefURLPatternSet(extension_id,
                                JoinPrefs(pref_key, kPrefScriptableHosts),
                                new_value.scriptable_hosts());
}

void ExtensionPrefs::AddToPrefPermissionSet(const ExtensionId& extension_id,
                                            const PermissionSet& permissions,
                                            const char* pref_name) {
  CHECK(crx_file::id_util::IdIsValid(extension_id));
  std::unique_ptr<const PermissionSet> current =
      ReadPrefAsPermissionSet(extension_id, pref_name);
  std::unique_ptr<const PermissionSet> union_set;
  if (current)
    union_set = PermissionSet::CreateUnion(permissions, *current);
  // The new permissions are the union of the already stored permissions and the
  // newly added permissions.
  SetExtensionPrefPermissionSet(extension_id, pref_name,
                                union_set ? *union_set : permissions);
}

void ExtensionPrefs::RemoveFromPrefPermissionSet(
    const ExtensionId& extension_id,
    const PermissionSet& permissions,
    const char* pref_name) {
  CHECK(crx_file::id_util::IdIsValid(extension_id));

  std::unique_ptr<const PermissionSet> current =
      ReadPrefAsPermissionSet(extension_id, pref_name);

  if (!current)
    return;  // Nothing to remove.

  // The new permissions are the difference of the already stored permissions
  // and the newly removed permissions.
  SetExtensionPrefPermissionSet(
      extension_id, pref_name,
      *PermissionSet::CreateDifference(*current, permissions));
}

int ExtensionPrefs::IncrementAcknowledgePromptCount(
    const std::string& extension_id) {
  int count = 0;
  ReadPrefAsInteger(extension_id, kPrefAcknowledgePromptCount, &count);
  ++count;
  UpdateExtensionPref(extension_id, kPrefAcknowledgePromptCount,
                      std::make_unique<base::Value>(count));
  return count;
}

bool ExtensionPrefs::IsExternalExtensionAcknowledged(
    const std::string& extension_id) const {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefExternalAcknowledged);
}

void ExtensionPrefs::AcknowledgeExternalExtension(
    const std::string& extension_id) {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  UpdateExtensionPref(extension_id, kPrefExternalAcknowledged,
                      std::make_unique<base::Value>(true));
  UpdateExtensionPref(extension_id, kPrefAcknowledgePromptCount, nullptr);
}

bool ExtensionPrefs::IsBlacklistedExtensionAcknowledged(
    const std::string& extension_id) const {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefBlacklistAcknowledged);
}

void ExtensionPrefs::AcknowledgeBlacklistedExtension(
    const std::string& extension_id) {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  UpdateExtensionPref(extension_id, kPrefBlacklistAcknowledged,
                      std::make_unique<base::Value>(true));
  UpdateExtensionPref(extension_id, kPrefAcknowledgePromptCount, nullptr);
}

bool ExtensionPrefs::IsExternalInstallFirstRun(
    const std::string& extension_id) const {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefExternalInstallFirstRun);
}

void ExtensionPrefs::SetExternalInstallFirstRun(
    const std::string& extension_id) {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  UpdateExtensionPref(extension_id, kPrefExternalInstallFirstRun,
                      std::make_unique<base::Value>(true));
}

bool ExtensionPrefs::SetAlertSystemFirstRun() {
  if (prefs_->GetBoolean(pref_names::kAlertsInitialized)) {
    return true;
  }
  prefs_->SetBoolean(pref_names::kAlertsInitialized, true);
  return g_run_alerts_in_first_run_for_testing;  // Note: normally false.
}

bool ExtensionPrefs::DidExtensionEscalatePermissions(
    const std::string& extension_id) const {
  return HasDisableReason(extension_id,
                          disable_reason::DISABLE_PERMISSIONS_INCREASE) ||
         HasDisableReason(extension_id, disable_reason::DISABLE_REMOTE_INSTALL);
}

int ExtensionPrefs::GetDisableReasons(const std::string& extension_id) const {
  int value = -1;
  if (ReadPrefAsInteger(extension_id, kPrefDisableReasons, &value) &&
      value >= 0) {
    // TODO(crbug.com/860198): After we've gotten rid of the migration code for
    // DEPRECATED_DISABLE_UNKNOWN_FROM_SYNC, we should maybe filter it out here
    // just to be sure:
    // value = value & ~disable_reason::DEPRECATED_DISABLE_UNKNOWN_FROM_SYNC;
    return value;
  }
  return disable_reason::DISABLE_NONE;
}

bool ExtensionPrefs::HasDisableReason(
    const std::string& extension_id,
    disable_reason::DisableReason disable_reason) const {
  return (GetDisableReasons(extension_id) & disable_reason) != 0;
}

void ExtensionPrefs::AddDisableReason(
    const std::string& extension_id,
    disable_reason::DisableReason disable_reason) {
  DCHECK(!DoesExtensionHaveState(extension_id, Extension::ENABLED));
  ModifyDisableReasons(extension_id, disable_reason, DISABLE_REASON_ADD);
}

void ExtensionPrefs::AddDisableReasons(const std::string& extension_id,
                                       int disable_reasons) {
  DCHECK(!DoesExtensionHaveState(extension_id, Extension::ENABLED));
  ModifyDisableReasons(extension_id, disable_reasons, DISABLE_REASON_ADD);
}

void ExtensionPrefs::RemoveDisableReason(
    const std::string& extension_id,
    disable_reason::DisableReason disable_reason) {
  ModifyDisableReasons(extension_id, disable_reason, DISABLE_REASON_REMOVE);
}

void ExtensionPrefs::ReplaceDisableReasons(const std::string& extension_id,
                                           int disable_reasons) {
  ModifyDisableReasons(extension_id, disable_reasons, DISABLE_REASON_REPLACE);
}

void ExtensionPrefs::ClearDisableReasons(const std::string& extension_id) {
  ModifyDisableReasons(extension_id, disable_reason::DISABLE_NONE,
                       DISABLE_REASON_CLEAR);
}

void ExtensionPrefs::ModifyDisableReasons(const std::string& extension_id,
                                          int reasons,
                                          DisableReasonChange change) {
  int old_value = GetDisableReasons(extension_id);
  int new_value = old_value;
  switch (change) {
    case DISABLE_REASON_ADD:
      new_value |= reasons;
      break;
    case DISABLE_REASON_REMOVE:
      new_value &= ~reasons;
      break;
    case DISABLE_REASON_REPLACE:
      new_value = reasons;
      break;
    case DISABLE_REASON_CLEAR:
      new_value = disable_reason::DISABLE_NONE;
      break;
  }

  if (old_value == new_value)  // no change, return.
    return;

  if (new_value == disable_reason::DISABLE_NONE) {
    UpdateExtensionPref(extension_id, kPrefDisableReasons, nullptr);
  } else {
    UpdateExtensionPref(extension_id, kPrefDisableReasons,
                        std::make_unique<base::Value>(new_value));
  }

  for (auto& observer : observer_list_)
    observer.OnExtensionDisableReasonsChanged(extension_id, new_value);
}

std::set<std::string> ExtensionPrefs::GetBlacklistedExtensions() const {
  std::set<std::string> ids;

  const base::DictionaryValue* extensions =
      prefs_->GetDictionary(pref_names::kExtensions);
  if (!extensions)
    return ids;

  for (base::DictionaryValue::Iterator it(*extensions);
       !it.IsAtEnd(); it.Advance()) {
    if (!it.value().is_dict()) {
      NOTREACHED() << "Invalid pref for extension " << it.key();
      continue;
    }
    if (IsBlacklistBitSet(
            static_cast<const base::DictionaryValue*>(&it.value()))) {
      ids.insert(it.key());
    }
  }

  return ids;
}

void ExtensionPrefs::SetExtensionBlacklisted(const std::string& extension_id,
                                             bool is_blacklisted) {
  bool currently_blacklisted = IsExtensionBlacklisted(extension_id);
  if (is_blacklisted == currently_blacklisted)
    return;

  // Always make sure the "acknowledged" bit is cleared since the blacklist bit
  // is changing.
  UpdateExtensionPref(extension_id, kPrefBlacklistAcknowledged, nullptr);

  if (is_blacklisted) {
    UpdateExtensionPref(extension_id, kPrefBlacklist,
                        std::make_unique<base::Value>(true));
  } else {
    UpdateExtensionPref(extension_id, kPrefBlacklist, nullptr);
    const base::DictionaryValue* dict = GetExtensionPref(extension_id);
    if (dict && dict->empty())
      DeleteExtensionPrefs(extension_id);
  }
}

bool ExtensionPrefs::IsExtensionBlacklisted(const std::string& id) const {
  const base::DictionaryValue* ext_prefs = GetExtensionPref(id);
  return ext_prefs && IsBlacklistBitSet(ext_prefs);
}

namespace {

// Serializes a 64bit integer as a string value.
void SaveInt64(prefs::DictionaryValueUpdate* dictionary,
               const char* key,
               const int64_t value) {
  if (!dictionary)
    return;

  std::string string_value = base::Int64ToString(value);
  dictionary->SetString(key, string_value);
}

// Deserializes a 64bit integer stored as a string value.
bool ReadInt64(const base::DictionaryValue* dictionary,
               const char* key,
               int64_t* value) {
  if (!dictionary)
    return false;

  std::string string_value;
  if (!dictionary->GetString(key, &string_value))
    return false;

  return base::StringToInt64(string_value, value);
}

// Serializes |time| as a string value mapped to |key| in |dictionary|.
void SaveTime(prefs::DictionaryValueUpdate* dictionary,
              const char* key,
              const base::Time& time) {
  SaveInt64(dictionary, key, time.ToInternalValue());
}

// The opposite of SaveTime. If |key| is not found, this returns an empty Time
// (is_null() will return true).
base::Time ReadTime(const base::DictionaryValue* dictionary, const char* key) {
  int64_t value;
  if (ReadInt64(dictionary, key, &value))
    return base::Time::FromInternalValue(value);

  return base::Time();
}

}  // namespace

base::Time ExtensionPrefs::LastPingDay(const std::string& extension_id) const {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  return ReadTime(GetExtensionPref(extension_id), kLastPingDay);
}

void ExtensionPrefs::SetLastPingDay(const std::string& extension_id,
                                    const base::Time& time) {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  SaveTime(update.Get().get(), kLastPingDay, time);
}

base::Time ExtensionPrefs::BlacklistLastPingDay() const {
  return ReadTime(prefs_->GetDictionary(kExtensionsBlacklistUpdate),
                  kLastPingDay);
}

void ExtensionPrefs::SetBlacklistLastPingDay(const base::Time& time) {
  prefs::ScopedDictionaryPrefUpdate update(prefs_, kExtensionsBlacklistUpdate);
  SaveTime(update.Get().get(), kLastPingDay, time);
}

base::Time ExtensionPrefs::LastActivePingDay(
    const std::string& extension_id) const {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  return ReadTime(GetExtensionPref(extension_id), kLastActivePingDay);
}

void ExtensionPrefs::SetLastActivePingDay(const std::string& extension_id,
                                          const base::Time& time) {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  SaveTime(update.Get().get(), kLastActivePingDay, time);
}

bool ExtensionPrefs::GetActiveBit(const std::string& extension_id) const {
  const base::DictionaryValue* dictionary = GetExtensionPref(extension_id);
  bool result = false;
  if (dictionary && dictionary->GetBoolean(kActiveBit, &result))
    return result;
  return false;
}

void ExtensionPrefs::SetActiveBit(const std::string& extension_id,
                                  bool active) {
  UpdateExtensionPref(extension_id, kActiveBit,
                      std::make_unique<base::Value>(active));
}

std::unique_ptr<const PermissionSet> ExtensionPrefs::GetGrantedPermissions(
    const std::string& extension_id) const {
  CHECK(crx_file::id_util::IdIsValid(extension_id));
  return ReadPrefAsPermissionSet(extension_id, kPrefGrantedPermissions);
}

void ExtensionPrefs::AddGrantedPermissions(const std::string& extension_id,
                                           const PermissionSet& permissions) {
  AddToPrefPermissionSet(extension_id, permissions, kPrefGrantedPermissions);
}

void ExtensionPrefs::RemoveGrantedPermissions(
    const std::string& extension_id,
    const PermissionSet& permissions) {
  RemoveFromPrefPermissionSet(extension_id, permissions,
                              kPrefGrantedPermissions);
}

std::unique_ptr<const PermissionSet> ExtensionPrefs::GetActivePermissions(
    const std::string& extension_id) const {
  CHECK(crx_file::id_util::IdIsValid(extension_id));
  return ReadPrefAsPermissionSet(extension_id, kPrefActivePermissions);
}

void ExtensionPrefs::SetActivePermissions(const std::string& extension_id,
                                          const PermissionSet& permissions) {
  SetExtensionPrefPermissionSet(
      extension_id, kPrefActivePermissions, permissions);
}

std::unique_ptr<const PermissionSet>
ExtensionPrefs::GetRuntimeGrantedPermissions(
    const ExtensionId& extension_id) const {
  CHECK(crx_file::id_util::IdIsValid(extension_id));
  return ReadPrefAsPermissionSet(extension_id, kPrefRuntimeGrantedPermissions);
}

void ExtensionPrefs::AddRuntimeGrantedPermissions(
    const ExtensionId& extension_id,
    const PermissionSet& permissions) {
  AddToPrefPermissionSet(extension_id, permissions,
                         kPrefRuntimeGrantedPermissions);
  for (auto& observer : observer_list_)
    observer.OnExtensionRuntimePermissionsChanged(extension_id);
}

void ExtensionPrefs::RemoveRuntimeGrantedPermissions(
    const ExtensionId& extension_id,
    const PermissionSet& permissions) {
  RemoveFromPrefPermissionSet(extension_id, permissions,
                              kPrefRuntimeGrantedPermissions);
  for (auto& observer : observer_list_)
    observer.OnExtensionRuntimePermissionsChanged(extension_id);
}

void ExtensionPrefs::SetExtensionRunning(const std::string& extension_id,
    bool is_running) {
  UpdateExtensionPref(extension_id, kPrefRunning,
                      std::make_unique<base::Value>(is_running));
}

bool ExtensionPrefs::IsExtensionRunning(const std::string& extension_id) const {
  const base::DictionaryValue* extension = GetExtensionPref(extension_id);
  if (!extension)
    return false;
  bool running = false;
  extension->GetBoolean(kPrefRunning, &running);
  return running;
}

void ExtensionPrefs::SetIsActive(const std::string& extension_id,
                                 bool is_active) {
  UpdateExtensionPref(extension_id, kIsActive,
                      std::make_unique<base::Value>(is_active));
}

bool ExtensionPrefs::IsActive(const std::string& extension_id) const {
  const base::DictionaryValue* extension = GetExtensionPref(extension_id);
  if (!extension)
    return false;
  bool is_active = false;
  extension->GetBoolean(kIsActive, &is_active);
  return is_active;
}

bool ExtensionPrefs::IsIncognitoEnabled(const std::string& extension_id) const {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefIncognitoEnabled);
}

void ExtensionPrefs::SetIsIncognitoEnabled(const std::string& extension_id,
                                           bool enabled) {
  UpdateExtensionPref(extension_id, kPrefIncognitoEnabled,
                      std::make_unique<base::Value>(enabled));
  extension_pref_value_map_->SetExtensionIncognitoState(extension_id, enabled);
}

bool ExtensionPrefs::AllowFileAccess(const std::string& extension_id) const {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefAllowFileAccess);
}

void ExtensionPrefs::SetAllowFileAccess(const std::string& extension_id,
                                        bool allow) {
  UpdateExtensionPref(extension_id, kPrefAllowFileAccess,
                      std::make_unique<base::Value>(allow));
}

bool ExtensionPrefs::HasAllowFileAccessSetting(
    const std::string& extension_id) const {
  const base::DictionaryValue* ext = GetExtensionPref(extension_id);
  return ext && ext->HasKey(kPrefAllowFileAccess);
}

bool ExtensionPrefs::DoesExtensionHaveState(
    const std::string& id, Extension::State check_state) const {
  const base::DictionaryValue* extension = GetExtensionPref(id);
  int state = -1;
  if (!extension || !extension->GetInteger(kPrefState, &state))
    return false;

  if (state < 0 || state >= Extension::NUM_STATES) {
    LOG(ERROR) << "Bad pref 'state' for extension '" << id << "'";
    return false;
  }

  return state == check_state;
}

bool ExtensionPrefs::IsExternalExtensionUninstalled(
    const std::string& id) const {
  return DoesExtensionHaveState(id, Extension::EXTERNAL_EXTENSION_UNINSTALLED);
}

bool ExtensionPrefs::IsExtensionDisabled(const std::string& id) const {
  return DoesExtensionHaveState(id, Extension::DISABLED);
}

ExtensionIdList ExtensionPrefs::GetToolbarOrder() const {
  ExtensionIdList id_list_out;
  GetUserExtensionPrefIntoContainer(pref_names::kToolbar, &id_list_out);
  return id_list_out;
}

void ExtensionPrefs::SetToolbarOrder(const ExtensionIdList& extension_ids) {
  SetExtensionPrefFromContainer(pref_names::kToolbar, extension_ids);
}

void ExtensionPrefs::OnExtensionInstalled(
    const Extension* extension,
    Extension::State initial_state,
    const syncer::StringOrdinal& page_ordinal,
    int install_flags,
    const std::string& install_parameter,
    const base::Optional<int>& dnr_ruleset_checksum) {
  ScopedExtensionPrefUpdate update(prefs_, extension->id());
  auto extension_dict = update.Get();
  const base::Time install_time = clock_->Now();
  PopulateExtensionInfoPrefs(extension, install_time, initial_state,
                             install_flags, install_parameter,
                             dnr_ruleset_checksum, extension_dict.get());

  FinishExtensionInfoPrefs(extension->id(), install_time,
                           extension->RequiresSortOrdinal(), page_ordinal,
                           extension_dict.get());
}

void ExtensionPrefs::OnExtensionUninstalled(const std::string& extension_id,
                                            const Manifest::Location& location,
                                            bool external_uninstall) {
  app_sorting()->ClearOrdinals(extension_id);

  // For external extensions, we save a preference reminding ourself not to try
  // and install the extension anymore (except when |external_uninstall| is
  // true, which signifies that the registry key was deleted or the pref file
  // no longer lists the extension).
  if (!external_uninstall && Manifest::IsExternalLocation(location)) {
    UpdateExtensionPref(extension_id, kPrefState,
                        std::make_unique<base::Value>(
                            Extension::EXTERNAL_EXTENSION_UNINSTALLED));
    extension_pref_value_map_->SetExtensionState(extension_id, false);
    for (auto& observer : observer_list_)
      observer.OnExtensionStateChanged(extension_id, false);
  } else {
    DeleteExtensionPrefs(extension_id);
  }
}

void ExtensionPrefs::SetExtensionEnabled(const std::string& extension_id) {
  UpdateExtensionPref(extension_id, kPrefState,
                      std::make_unique<base::Value>(Extension::ENABLED));
  extension_pref_value_map_->SetExtensionState(extension_id, true);
  UpdateExtensionPref(extension_id, kPrefDisableReasons, nullptr);
  for (auto& observer : observer_list_)
    observer.OnExtensionStateChanged(extension_id, true);
}

void ExtensionPrefs::SetExtensionDisabled(const std::string& extension_id,
                                          int disable_reasons) {
  if (!IsExternalExtensionUninstalled(extension_id)) {
    UpdateExtensionPref(extension_id, kPrefState,
                        std::make_unique<base::Value>(Extension::DISABLED));
    extension_pref_value_map_->SetExtensionState(extension_id, false);
  }
  UpdateExtensionPref(extension_id, kPrefDisableReasons,
                      std::make_unique<base::Value>(disable_reasons));
  for (auto& observer : observer_list_)
    observer.OnExtensionStateChanged(extension_id, false);
}

void ExtensionPrefs::SetExtensionBlacklistState(const std::string& extension_id,
                                                BlacklistState state) {
  SetExtensionBlacklisted(extension_id, state == BLACKLISTED_MALWARE);
  UpdateExtensionPref(extension_id, kPrefBlacklistState,
                      std::make_unique<base::Value>(state));
}

BlacklistState ExtensionPrefs::GetExtensionBlacklistState(
    const std::string& extension_id) const {
  if (IsExtensionBlacklisted(extension_id))
    return BLACKLISTED_MALWARE;
  const base::DictionaryValue* ext_prefs = GetExtensionPref(extension_id);
  int int_value = 0;
  if (ext_prefs && ext_prefs->GetInteger(kPrefBlacklistState, &int_value))
    return static_cast<BlacklistState>(int_value);

  return NOT_BLACKLISTED;
}

std::string ExtensionPrefs::GetVersionString(
    const std::string& extension_id) const {
  const base::DictionaryValue* extension = GetExtensionPref(extension_id);
  if (!extension)
    return std::string();

  std::string version;
  extension->GetString(kPrefVersion, &version);

  return version;
}

void ExtensionPrefs::UpdateManifest(const Extension* extension) {
  if (!Manifest::IsUnpackedLocation(extension->location())) {
    const base::DictionaryValue* extension_dict =
        GetExtensionPref(extension->id());
    if (!extension_dict)
      return;
    const base::DictionaryValue* old_manifest = NULL;
    bool update_required =
        !extension_dict->GetDictionary(kPrefManifest, &old_manifest) ||
        !extension->manifest()->value()->Equals(old_manifest);
    if (update_required) {
      UpdateExtensionPref(extension->id(), kPrefManifest,
                          extension->manifest()->value()->CreateDeepCopy());
    }
  }
}

std::unique_ptr<ExtensionInfo> ExtensionPrefs::GetInstalledInfoHelper(
    const std::string& extension_id,
    const base::DictionaryValue* extension,
    bool include_component_extensions) const {
  int location_value;
  if (!extension->GetInteger(kPrefLocation, &location_value))
    return std::unique_ptr<ExtensionInfo>();

  Manifest::Location location = static_cast<Manifest::Location>(location_value);
  if (location == Manifest::COMPONENT && !include_component_extensions) {
    // Component extensions are ignored by default. Component extensions may
    // have data saved in preferences, but they are already loaded at this point
    // (by ComponentLoader) and shouldn't be populated into the result of
    // GetInstalledExtensionsInfo, otherwise InstalledLoader would also want to
    // load them.
    return std::unique_ptr<ExtensionInfo>();
  }

  // Only the following extension types have data saved in the preferences.
  if (location != Manifest::INTERNAL && location != Manifest::COMPONENT &&
      !Manifest::IsUnpackedLocation(location) &&
      !Manifest::IsExternalLocation(location)) {
    NOTREACHED();
    return std::unique_ptr<ExtensionInfo>();
  }

  const base::DictionaryValue* manifest = NULL;
  if (!Manifest::IsUnpackedLocation(location) &&
      !extension->GetDictionary(kPrefManifest, &manifest)) {
    LOG(WARNING) << "Missing manifest for extension " << extension_id;
    // Just a warning for now.
  }

  base::FilePath::StringType path;
  if (!extension->GetString(kPrefPath, &path))
    return std::unique_ptr<ExtensionInfo>();

  // Make path absolute. Most (but not all) extension types have relative paths.
  if (!base::FilePath(path).IsAbsolute())
    path = install_directory_.Append(path).value();

  return std::unique_ptr<ExtensionInfo>(new ExtensionInfo(
      manifest, extension_id, base::FilePath(path), location));
}

std::unique_ptr<ExtensionInfo> ExtensionPrefs::GetInstalledExtensionInfo(
    const std::string& extension_id,
    bool include_component_extensions) const {
  const base::DictionaryValue* ext = NULL;
  const base::DictionaryValue* extensions =
      prefs_->GetDictionary(pref_names::kExtensions);
  if (!extensions ||
      !extensions->GetDictionaryWithoutPathExpansion(extension_id, &ext))
    return std::unique_ptr<ExtensionInfo>();
  int state_value;
  if (ext->GetInteger(kPrefState, &state_value) &&
      state_value == Extension::EXTERNAL_EXTENSION_UNINSTALLED) {
    return std::unique_ptr<ExtensionInfo>();
  }

  return GetInstalledInfoHelper(extension_id, ext,
                                include_component_extensions);
}

std::unique_ptr<ExtensionPrefs::ExtensionsInfo>
ExtensionPrefs::GetInstalledExtensionsInfo(
    bool include_component_extensions) const {
  std::unique_ptr<ExtensionsInfo> extensions_info(new ExtensionsInfo);

  const base::DictionaryValue* extensions =
      prefs_->GetDictionary(pref_names::kExtensions);
  for (base::DictionaryValue::Iterator extension_id(*extensions);
       !extension_id.IsAtEnd(); extension_id.Advance()) {
    if (!crx_file::id_util::IdIsValid(extension_id.key()))
      continue;

    std::unique_ptr<ExtensionInfo> info = GetInstalledExtensionInfo(
        extension_id.key(), include_component_extensions);
    if (info)
      extensions_info->push_back(std::move(info));
  }

  return extensions_info;
}

std::unique_ptr<ExtensionPrefs::ExtensionsInfo>
ExtensionPrefs::GetUninstalledExtensionsInfo() const {
  std::unique_ptr<ExtensionsInfo> extensions_info(new ExtensionsInfo);

  const base::DictionaryValue* extensions =
      prefs_->GetDictionary(pref_names::kExtensions);
  for (base::DictionaryValue::Iterator extension_id(*extensions);
       !extension_id.IsAtEnd(); extension_id.Advance()) {
    const base::DictionaryValue* ext = NULL;
    if (!crx_file::id_util::IdIsValid(extension_id.key()) ||
        !IsExternalExtensionUninstalled(extension_id.key()) ||
        !extension_id.value().GetAsDictionary(&ext))
      continue;

    std::unique_ptr<ExtensionInfo> info =
        GetInstalledInfoHelper(extension_id.key(), ext,
                               /*include_component_extensions = */ false);
    if (info)
      extensions_info->push_back(std::move(info));
  }

  return extensions_info;
}

void ExtensionPrefs::SetDelayedInstallInfo(
    const Extension* extension,
    Extension::State initial_state,
    int install_flags,
    DelayReason delay_reason,
    const syncer::StringOrdinal& page_ordinal,
    const std::string& install_parameter,
    const base::Optional<int>& dnr_ruleset_checksum) {
  ScopedDictionaryUpdate update(this, extension->id(), kDelayedInstallInfo);
  auto extension_dict = update.Create();
  PopulateExtensionInfoPrefs(extension, clock_->Now(), initial_state,
                             install_flags, install_parameter,
                             dnr_ruleset_checksum, extension_dict.get());

  // Add transient data that is needed by FinishDelayedInstallInfo(), but
  // should not be in the final extension prefs. All entries here should have
  // a corresponding Remove() call in FinishDelayedInstallInfo().
  if (extension->RequiresSortOrdinal()) {
    extension_dict->SetString(
        kPrefSuggestedPageOrdinal,
        page_ordinal.IsValid() ? page_ordinal.ToInternalValue()
                               : std::string());
  }
  extension_dict->SetInteger(kDelayedInstallReason,
                             static_cast<int>(delay_reason));
}

bool ExtensionPrefs::RemoveDelayedInstallInfo(
    const std::string& extension_id) {
  if (!GetExtensionPref(extension_id))
    return false;
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  bool result = update->Remove(kDelayedInstallInfo, NULL);
  return result;
}

bool ExtensionPrefs::FinishDelayedInstallInfo(
    const std::string& extension_id) {
  CHECK(crx_file::id_util::IdIsValid(extension_id));
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  auto extension_dict = update.Get();
  std::unique_ptr<prefs::DictionaryValueUpdate> pending_install_dict;
  if (!extension_dict->GetDictionary(kDelayedInstallInfo,
                                     &pending_install_dict)) {
    return false;
  }

  // Retrieve and clear transient values populated by SetDelayedInstallInfo().
  // Also do any other data cleanup that makes sense.
  std::string serialized_ordinal;
  syncer::StringOrdinal suggested_page_ordinal;
  bool needs_sort_ordinal = false;
  if (pending_install_dict->GetString(kPrefSuggestedPageOrdinal,
                                      &serialized_ordinal)) {
    suggested_page_ordinal = syncer::StringOrdinal(serialized_ordinal);
    needs_sort_ordinal = true;
    pending_install_dict->Remove(kPrefSuggestedPageOrdinal, NULL);
  }
  pending_install_dict->Remove(kDelayedInstallReason, NULL);

  const base::Time install_time = clock_->Now();
  pending_install_dict->SetString(
      kPrefInstallTime, base::Int64ToString(install_time.ToInternalValue()));

  // Commit the delayed install data.
  for (base::DictionaryValue::Iterator it(
           *pending_install_dict->AsConstDictionary());
       !it.IsAtEnd(); it.Advance()) {
    extension_dict->Set(it.key(),
                        std::make_unique<base::Value>(it.value().Clone()));
  }
  FinishExtensionInfoPrefs(extension_id, install_time, needs_sort_ordinal,
                           suggested_page_ordinal, extension_dict.get());
  return true;
}

std::unique_ptr<ExtensionInfo> ExtensionPrefs::GetDelayedInstallInfo(
    const std::string& extension_id) const {
  const base::DictionaryValue* extension_prefs =
      GetExtensionPref(extension_id);
  if (!extension_prefs)
    return std::unique_ptr<ExtensionInfo>();

  const base::DictionaryValue* ext = NULL;
  if (!extension_prefs->GetDictionary(kDelayedInstallInfo, &ext))
    return std::unique_ptr<ExtensionInfo>();

  return GetInstalledInfoHelper(extension_id, ext,
                                /*include_component_extensions = */ false);
}

ExtensionPrefs::DelayReason ExtensionPrefs::GetDelayedInstallReason(
    const std::string& extension_id) const {
  const base::DictionaryValue* extension_prefs =
      GetExtensionPref(extension_id);
  if (!extension_prefs)
    return DELAY_REASON_NONE;

  const base::DictionaryValue* ext = NULL;
  if (!extension_prefs->GetDictionary(kDelayedInstallInfo, &ext))
    return DELAY_REASON_NONE;

  int delay_reason;
  if (!ext->GetInteger(kDelayedInstallReason, &delay_reason))
    return DELAY_REASON_NONE;

  return static_cast<DelayReason>(delay_reason);
}

std::unique_ptr<ExtensionPrefs::ExtensionsInfo>
ExtensionPrefs::GetAllDelayedInstallInfo() const {
  std::unique_ptr<ExtensionsInfo> extensions_info(new ExtensionsInfo);

  const base::DictionaryValue* extensions =
      prefs_->GetDictionary(pref_names::kExtensions);
  for (base::DictionaryValue::Iterator extension_id(*extensions);
       !extension_id.IsAtEnd(); extension_id.Advance()) {
    if (!crx_file::id_util::IdIsValid(extension_id.key()))
      continue;

    std::unique_ptr<ExtensionInfo> info =
        GetDelayedInstallInfo(extension_id.key());
    if (info)
      extensions_info->push_back(std::move(info));
  }

  return extensions_info;
}

bool ExtensionPrefs::WasAppDraggedByUser(
    const std::string& extension_id) const {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefUserDraggedApp);
}

void ExtensionPrefs::SetAppDraggedByUser(const std::string& extension_id) {
  UpdateExtensionPref(extension_id, kPrefUserDraggedApp,
                      std::make_unique<base::Value>(true));
}

bool ExtensionPrefs::IsFromWebStore(
    const std::string& extension_id) const {
  const base::DictionaryValue* dictionary = GetExtensionPref(extension_id);
  bool result = false;
  if (dictionary && dictionary->GetBoolean(kPrefFromWebStore, &result))
    return result;
  return false;
}

bool ExtensionPrefs::IsFromBookmark(
    const std::string& extension_id) const {
  const base::DictionaryValue* dictionary = GetExtensionPref(extension_id);
  bool result = false;
  if (dictionary && dictionary->GetBoolean(kPrefFromBookmark, &result))
    return result;
  return false;
}

int ExtensionPrefs::GetCreationFlags(const std::string& extension_id) const {
  int creation_flags = Extension::NO_FLAGS;
  if (!ReadPrefAsInteger(extension_id, kPrefCreationFlags, &creation_flags)) {
    // Since kPrefCreationFlags was added later, it will be missing for
    // previously installed extensions.
    if (IsFromBookmark(extension_id))
      creation_flags |= Extension::FROM_BOOKMARK;
    if (IsFromWebStore(extension_id))
      creation_flags |= Extension::FROM_WEBSTORE;
    if (WasInstalledByDefault(extension_id))
      creation_flags |= Extension::WAS_INSTALLED_BY_DEFAULT;
    if (WasInstalledByOem(extension_id))
      creation_flags |= Extension::WAS_INSTALLED_BY_OEM;
  }
  return creation_flags;
}

int ExtensionPrefs::GetDelayedInstallCreationFlags(
    const std::string& extension_id) const {
  int creation_flags = Extension::NO_FLAGS;
  const base::DictionaryValue* delayed_info = NULL;
  if (ReadPrefAsDictionary(extension_id, kDelayedInstallInfo, &delayed_info)) {
    delayed_info->GetInteger(kPrefCreationFlags, &creation_flags);
  }
  return creation_flags;
}

bool ExtensionPrefs::WasInstalledByDefault(
    const std::string& extension_id) const {
  const base::DictionaryValue* dictionary = GetExtensionPref(extension_id);
  bool result = false;
  if (dictionary &&
      dictionary->GetBoolean(kPrefWasInstalledByDefault, &result))
    return result;
  return false;
}

bool ExtensionPrefs::WasInstalledByOem(const std::string& extension_id) const {
  const base::DictionaryValue* dictionary = GetExtensionPref(extension_id);
  bool result = false;
  if (dictionary && dictionary->GetBoolean(kPrefWasInstalledByOem, &result))
    return result;
  return false;
}

base::Time ExtensionPrefs::GetInstallTime(
    const std::string& extension_id) const {
  const base::DictionaryValue* extension = GetExtensionPref(extension_id);
  if (!extension) {
    NOTREACHED();
    return base::Time();
  }
  std::string install_time_str;
  if (!extension->GetString(kPrefInstallTime, &install_time_str))
    return base::Time();
  int64_t install_time_i64 = 0;
  if (!base::StringToInt64(install_time_str, &install_time_i64))
    return base::Time();
  return base::Time::FromInternalValue(install_time_i64);
}

bool ExtensionPrefs::DoNotSync(const std::string& extension_id) const {
  bool do_not_sync;
  if (!ReadPrefAsBoolean(extension_id, kPrefDoNotSync, &do_not_sync))
    return false;

  return do_not_sync;
}

base::Time ExtensionPrefs::GetLastLaunchTime(
    const std::string& extension_id) const {
  const base::DictionaryValue* extension = GetExtensionPref(extension_id);
  if (!extension)
    return base::Time();

  std::string launch_time_str;
  if (!extension->GetString(kPrefLastLaunchTime, &launch_time_str))
    return base::Time();
  int64_t launch_time_i64 = 0;
  if (!base::StringToInt64(launch_time_str, &launch_time_i64))
    return base::Time();
  return base::Time::FromInternalValue(launch_time_i64);
}

void ExtensionPrefs::SetLastLaunchTime(const std::string& extension_id,
                                       const base::Time& time) {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  SaveTime(update.Get().get(), kPrefLastLaunchTime, time);
}

void ExtensionPrefs::ClearLastLaunchTimes() {
  const base::DictionaryValue* dict =
      prefs_->GetDictionary(pref_names::kExtensions);
  if (!dict || dict->empty())
    return;

  // Collect all the keys to remove the last launched preference from.
  prefs::ScopedDictionaryPrefUpdate update(prefs_, pref_names::kExtensions);
  auto update_dict = update.Get();
  for (base::DictionaryValue::Iterator i(*update_dict->AsConstDictionary());
       !i.IsAtEnd(); i.Advance()) {
    std::unique_ptr<prefs::DictionaryValueUpdate> extension_dict;
    if (!update_dict->GetDictionary(i.key(), &extension_dict))
      continue;

    if (extension_dict->HasKey(kPrefLastLaunchTime))
      extension_dict->Remove(kPrefLastLaunchTime, NULL);
  }
}

void ExtensionPrefs::GetExtensions(ExtensionIdList* out) const {
  CHECK(out);

  std::unique_ptr<ExtensionsInfo> extensions_info(GetInstalledExtensionsInfo());

  for (size_t i = 0; i < extensions_info->size(); ++i) {
    ExtensionInfo* info = extensions_info->at(i).get();
    out->push_back(info->extension_id);
  }
}

void ExtensionPrefs::AddObserver(ExtensionPrefsObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ExtensionPrefs::RemoveObserver(ExtensionPrefsObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void ExtensionPrefs::InitPrefStore() {
  TRACE_EVENT0("browser,startup", "ExtensionPrefs::InitPrefStore")
  SCOPED_UMA_HISTOGRAM_TIMER("Extensions.InitPrefStoreTime");

  // When this is called, the PrefService is initialized and provides access
  // to the user preferences stored in a JSON file.
  std::unique_ptr<ExtensionsInfo> extensions_info;
  {
    SCOPED_UMA_HISTOGRAM_TIMER("Extensions.InitPrefGetExtensionsTime");
    extensions_info =
        GetInstalledExtensionsInfo(/*include_component_extensions = */ true);
  }

  if (extensions_disabled_) {
    // Normally, if extensions are disabled, we don't want to load the
    // controlled prefs from that extension. However, some extensions are
    // *always* loaded, even with e.g. --disable-extensions. For these, we
    // need to load the extension-controlled preferences.
    // See https://crbug.com/828295.
    auto predicate = [](const auto& info) {
      // HACK(devlin): Unpacked extensions stored in preferences do not have a
      // manifest, only a path (from which the manifest is later loaded). This
      // means that we don't know what type the extension is just from the
      // preferences (and, indeed, it may change types, if the file on disk has
      // changed).
      // Because of this, we may be passing |is_theme| incorrectly for unpacked
      // extensions below. This is okay in this instance, since if the extension
      // is a theme, initializing the controlled prefs shouldn't matter.
      // However, this is a pretty hacky solution. It would likely be better if
      // we could instead initialize the controlled preferences when the
      // extension is more finalized, but this also needs to happen sufficiently
      // before other subsystems are notified about the extension being loaded.
      Manifest::Type type =
          info->extension_manifest
              ? Manifest::GetTypeFromManifestValue(*info->extension_manifest)
              : Manifest::TYPE_UNKNOWN;
      bool is_theme = type == Manifest::TYPE_THEME;
      // Erase the entry if the extension won't be loaded.
      return !Manifest::ShouldAlwaysLoadExtension(info->extension_location,
                                                  is_theme);
    };
    base::EraseIf(*extensions_info, predicate);
  }

  InitExtensionControlledPrefs(*extensions_info);

  extension_pref_value_map_->NotifyInitializationCompleted();
}

bool ExtensionPrefs::HasIncognitoPrefValue(const std::string& pref_key) const {
  bool has_incognito_pref_value = false;
  extension_pref_value_map_->GetEffectivePrefValue(pref_key,
                                                   true,
                                                   &has_incognito_pref_value);
  return has_incognito_pref_value;
}

const base::DictionaryValue* ExtensionPrefs::GetGeometryCache(
    const std::string& extension_id) const {
  const base::DictionaryValue* extension_prefs = GetExtensionPref(extension_id);
  if (!extension_prefs)
    return NULL;

  const base::DictionaryValue* ext = NULL;
  if (!extension_prefs->GetDictionary(kPrefGeometryCache, &ext))
    return NULL;

  return ext;
}

void ExtensionPrefs::SetGeometryCache(
    const std::string& extension_id,
    std::unique_ptr<base::DictionaryValue> cache) {
  UpdateExtensionPref(extension_id, kPrefGeometryCache, std::move(cache));
}

const base::DictionaryValue* ExtensionPrefs::GetInstallSignature() const {
  return prefs_->GetDictionary(kInstallSignature);
}

void ExtensionPrefs::SetInstallSignature(
    const base::DictionaryValue* signature) {
  if (signature) {
    prefs_->Set(kInstallSignature, *signature);
    DVLOG(1) << "SetInstallSignature - saving";
  } else {
    DVLOG(1) << "SetInstallSignature - clearing";
    prefs_->ClearPref(kInstallSignature);
  }
}

std::string ExtensionPrefs::GetInstallParam(
    const std::string& extension_id) const {
  const base::DictionaryValue* extension = GetExtensionPref(extension_id);
  if (!extension)  // Expected during unit testing.
    return std::string();
  std::string install_parameter;
  if (!extension->GetString(kPrefInstallParam, &install_parameter))
    return std::string();
  return install_parameter;
}

void ExtensionPrefs::SetInstallParam(const std::string& extension_id,
                                     const std::string& install_parameter) {
  UpdateExtensionPref(extension_id, kPrefInstallParam,
                      std::make_unique<base::Value>(install_parameter));
}

int ExtensionPrefs::GetCorruptedDisableCount() const {
  return prefs_->GetInteger(kCorruptedDisableCount);
}

void ExtensionPrefs::IncrementCorruptedDisableCount() {
  int count = prefs_->GetInteger(kCorruptedDisableCount);
  prefs_->SetInteger(kCorruptedDisableCount, count + 1);
}

bool ExtensionPrefs::NeedsSync(const std::string& extension_id) const {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefNeedsSync);
}

void ExtensionPrefs::SetNeedsSync(const std::string& extension_id,
                                  bool needs_sync) {
  UpdateExtensionPref(
      extension_id, kPrefNeedsSync,
      needs_sync ? std::make_unique<base::Value>(true) : nullptr);
}

bool ExtensionPrefs::GetDNRRulesetChecksum(const ExtensionId& extension_id,
                                           int* dnr_ruleset_checksum) const {
  return ReadPrefAsInteger(extension_id, kPrefDNRRulesetChecksum,
                           dnr_ruleset_checksum);
}

void ExtensionPrefs::SetDNRRulesetChecksum(const ExtensionId& extension_id,
                                           int dnr_ruleset_checksum) {
  UpdateExtensionPref(extension_id, kPrefDNRRulesetChecksum,
                      std::make_unique<base::Value>(dnr_ruleset_checksum));
}

void ExtensionPrefs::SetDNRAllowedPages(const ExtensionId& extension_id,
                                        URLPatternSet set) {
  SetExtensionPrefURLPatternSet(extension_id, kPrefDNRAllowedPages, set);
}

URLPatternSet ExtensionPrefs::GetDNRAllowedPages(
    const ExtensionId& extension_id) const {
  URLPatternSet result;
  ReadPrefAsURLPatternSet(extension_id, kPrefDNRAllowedPages, &result,
                          URLPattern::SCHEME_ALL);
  return result;
}

// static
void ExtensionPrefs::SetRunAlertsInFirstRunForTest() {
  g_run_alerts_in_first_run_for_testing = true;
}

void ExtensionPrefs::ClearExternalUninstallForTesting(const ExtensionId& id) {
  DeleteExtensionPrefs(id);
}

ExtensionPrefs::ExtensionPrefs(
    content::BrowserContext* browser_context,
    PrefService* prefs,
    const base::FilePath& root_dir,
    ExtensionPrefValueMap* extension_pref_value_map,
    base::Clock* clock,
    bool extensions_disabled,
    const std::vector<ExtensionPrefsObserver*>& early_observers)
    : browser_context_(browser_context),
      prefs_(prefs),
      install_directory_(root_dir),
      extension_pref_value_map_(extension_pref_value_map),
      clock_(clock),
      extensions_disabled_(extensions_disabled) {
  MakePathsRelative();

  // Ensure that any early observers are watching before prefs are initialized.
  for (auto iter = early_observers.cbegin(); iter != early_observers.cend();
       ++iter) {
    AddObserver(*iter);
  }

  InitPrefStore();
}

AppSorting* ExtensionPrefs::app_sorting() const {
  return ExtensionSystem::Get(browser_context_)->app_sorting();
}

void ExtensionPrefs::SetNeedsStorageGarbageCollection(bool value) {
  prefs_->SetBoolean(pref_names::kStorageGarbageCollect, value);
}

bool ExtensionPrefs::NeedsStorageGarbageCollection() const {
  return prefs_->GetBoolean(pref_names::kStorageGarbageCollect);
}

// static
void ExtensionPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(pref_names::kExtensions);
  registry->RegisterListPref(pref_names::kToolbar,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(pref_names::kToolbarSize, -1);
  registry->RegisterDictionaryPref(kExtensionsBlacklistUpdate);
  registry->RegisterListPref(pref_names::kInstallAllowList);
  registry->RegisterListPref(pref_names::kInstallDenyList);
  registry->RegisterDictionaryPref(pref_names::kInstallForceList);
  registry->RegisterDictionaryPref(pref_names::kInstallLoginScreenAppList);
  registry->RegisterListPref(pref_names::kAllowedTypes);
  registry->RegisterBooleanPref(pref_names::kStorageGarbageCollect, false);
  registry->RegisterInt64Pref(pref_names::kLastUpdateCheck, 0);
  registry->RegisterInt64Pref(pref_names::kNextUpdateCheck, 0);
  registry->RegisterListPref(pref_names::kAllowedInstallSites);
  registry->RegisterStringPref(pref_names::kLastChromeVersion, std::string());
  registry->RegisterDictionaryPref(kInstallSignature);

  registry->RegisterListPref(pref_names::kNativeMessagingBlacklist);
  registry->RegisterListPref(pref_names::kNativeMessagingWhitelist);
  registry->RegisterBooleanPref(pref_names::kNativeMessagingUserLevelHosts,
                                true);
  registry->RegisterIntegerPref(kCorruptedDisableCount, 0);

#if !defined(OS_MACOSX)
  registry->RegisterBooleanPref(pref_names::kAppFullscreenAllowed, true);
#endif
}

template <class ExtensionIdContainer>
bool ExtensionPrefs::GetUserExtensionPrefIntoContainer(
    const char* pref,
    ExtensionIdContainer* id_container_out) const {
  DCHECK(id_container_out->empty());

  const base::Value* user_pref_value = prefs_->GetUserPrefValue(pref);
  const base::ListValue* user_pref_as_list;
  if (!user_pref_value || !user_pref_value->GetAsList(&user_pref_as_list))
    return false;

  std::insert_iterator<ExtensionIdContainer> insert_iterator(
      *id_container_out, id_container_out->end());
  std::string extension_id;
  for (auto value_it = user_pref_as_list->begin();
       value_it != user_pref_as_list->end(); ++value_it) {
    if (!value_it->GetAsString(&extension_id)) {
      NOTREACHED();
      continue;
    }
    insert_iterator = extension_id;
  }
  return true;
}

template <class ExtensionIdContainer>
void ExtensionPrefs::SetExtensionPrefFromContainer(
    const char* pref,
    const ExtensionIdContainer& strings) {
  ListPrefUpdate update(prefs_, pref);
  base::ListValue* list_of_values = update.Get();
  list_of_values->Clear();
  for (auto iter = strings.cbegin(); iter != strings.cend(); ++iter) {
    list_of_values->AppendString(*iter);
  }
}

void ExtensionPrefs::PopulateExtensionInfoPrefs(
    const Extension* extension,
    const base::Time install_time,
    Extension::State initial_state,
    int install_flags,
    const std::string& install_parameter,
    const base::Optional<int>& dnr_ruleset_checksum,
    prefs::DictionaryValueUpdate* extension_dict) const {
  extension_dict->SetInteger(kPrefState, initial_state);
  extension_dict->SetInteger(kPrefLocation, extension->location());
  extension_dict->SetInteger(kPrefCreationFlags, extension->creation_flags());
  extension_dict->SetBoolean(kPrefFromWebStore, extension->from_webstore());
  extension_dict->SetBoolean(kPrefFromBookmark, extension->from_bookmark());
  extension_dict->SetBoolean(kPrefWasInstalledByDefault,
                             extension->was_installed_by_default());
  extension_dict->SetBoolean(kPrefWasInstalledByOem,
                             extension->was_installed_by_oem());
  extension_dict->SetString(
      kPrefInstallTime, base::Int64ToString(install_time.ToInternalValue()));
  if (install_flags & kInstallFlagIsBlacklistedForMalware)
    extension_dict->SetBoolean(kPrefBlacklist, true);
  if (dnr_ruleset_checksum)
    extension_dict->SetInteger(kPrefDNRRulesetChecksum, *dnr_ruleset_checksum);

  base::FilePath::StringType path = MakePathRelative(install_directory_,
                                                     extension->path());
  extension_dict->SetString(kPrefPath, path);
  if (!install_parameter.empty()) {
    extension_dict->SetString(kPrefInstallParam, install_parameter);
  }
  // We store prefs about LOAD extensions, but don't cache their manifest
  // since it may change on disk.
  if (!Manifest::IsUnpackedLocation(extension->location())) {
    extension_dict->SetKey(kPrefManifest,
                           extension->manifest()->value()->Clone());
  }

  // Only writes kPrefDoNotSync when it is not the default.
  if (install_flags & kInstallFlagDoNotSync)
    extension_dict->SetBoolean(kPrefDoNotSync, true);
  else
    extension_dict->Remove(kPrefDoNotSync, NULL);
}

void ExtensionPrefs::InitExtensionControlledPrefs(
    const ExtensionsInfo& extensions_info) {
  TRACE_EVENT0("browser,startup",
               "ExtensionPrefs::InitExtensionControlledPrefs")
  SCOPED_UMA_HISTOGRAM_TIMER("Extensions.InitExtensionControlledPrefsTime");

  for (const auto& info : extensions_info) {
    const ExtensionId& extension_id = info->extension_id;

    base::Time install_time = GetInstallTime(extension_id);
    bool is_enabled = !IsExtensionDisabled(extension_id);
    bool is_incognito_enabled = IsIncognitoEnabled(extension_id);
    extension_pref_value_map_->RegisterExtension(
        extension_id, install_time, is_enabled, is_incognito_enabled);

    for (auto& observer : observer_list_)
      observer.OnExtensionRegistered(extension_id, install_time, is_enabled);

    // Set regular extension controlled prefs.
    LoadExtensionControlledPrefs(extension_id, kExtensionPrefsScopeRegular);
    // Set incognito extension controlled prefs.
    LoadExtensionControlledPrefs(extension_id,
                                 kExtensionPrefsScopeIncognitoPersistent);
    // Set regular-only extension controlled prefs.
    LoadExtensionControlledPrefs(extension_id, kExtensionPrefsScopeRegularOnly);

    for (auto& observer : observer_list_)
      observer.OnExtensionPrefsLoaded(extension_id, this);
  }
}

void ExtensionPrefs::LoadExtensionControlledPrefs(
    const ExtensionId& extension_id,
    ExtensionPrefsScope scope) {
  std::string scope_string;
  if (!pref_names::ScopeToPrefName(scope, &scope_string))
    return;
  std::string key = extension_id + "." + scope_string;

  const base::DictionaryValue* source_dict =
      pref_service()->GetDictionary(pref_names::kExtensions);
  const base::DictionaryValue* preferences = NULL;
  if (!source_dict->GetDictionary(key, &preferences))
    return;

  for (base::DictionaryValue::Iterator iter(*preferences); !iter.IsAtEnd();
       iter.Advance()) {
    extension_pref_value_map_->SetExtensionPref(extension_id, iter.key(), scope,
                                                iter.value().DeepCopy());
  }
}

void ExtensionPrefs::FinishExtensionInfoPrefs(
    const std::string& extension_id,
    const base::Time install_time,
    bool needs_sort_ordinal,
    const syncer::StringOrdinal& suggested_page_ordinal,
    prefs::DictionaryValueUpdate* extension_dict) {
  // Reinitializes various preferences with empty dictionaries.
  if (!extension_dict->HasKey(pref_names::kPrefPreferences)) {
    extension_dict->Set(pref_names::kPrefPreferences,
                        std::make_unique<base::DictionaryValue>());
  }

  if (!extension_dict->HasKey(pref_names::kPrefIncognitoPreferences)) {
    extension_dict->Set(pref_names::kPrefIncognitoPreferences,
                        std::make_unique<base::DictionaryValue>());
  }

  if (!extension_dict->HasKey(pref_names::kPrefRegularOnlyPreferences)) {
    extension_dict->Set(pref_names::kPrefRegularOnlyPreferences,
                        std::make_unique<base::DictionaryValue>());
  }

  if (!extension_dict->HasKey(pref_names::kPrefContentSettings))
    extension_dict->Set(pref_names::kPrefContentSettings,
                        std::make_unique<base::ListValue>());

  if (!extension_dict->HasKey(pref_names::kPrefIncognitoContentSettings)) {
    extension_dict->Set(pref_names::kPrefIncognitoContentSettings,
                        std::make_unique<base::ListValue>());
  }

  // If this point has been reached, any pending installs should be considered
  // out of date.
  extension_dict->Remove(kDelayedInstallInfo, NULL);

  // Clear state that may be registered from a previous install.
  extension_dict->Remove(EventRouter::kRegisteredLazyEvents, nullptr);
  extension_dict->Remove(EventRouter::kRegisteredServiceWorkerEvents, nullptr);

  // FYI, all code below here races on sudden shutdown because |extension_dict|,
  // |app_sorting|, |extension_pref_value_map_|, and (potentially) observers
  // are updated non-transactionally. This is probably not fixable without
  // nested transactional updates to pref dictionaries.
  if (needs_sort_ordinal)
    app_sorting()->EnsureValidOrdinals(extension_id, suggested_page_ordinal);

  bool is_enabled = false;
  int initial_state;
  if (extension_dict->GetInteger(kPrefState, &initial_state)) {
    is_enabled = initial_state == Extension::ENABLED;
  }
  bool is_incognito_enabled = IsIncognitoEnabled(extension_id);

  extension_pref_value_map_->RegisterExtension(
      extension_id, install_time, is_enabled, is_incognito_enabled);

  for (auto& observer : observer_list_)
    observer.OnExtensionRegistered(extension_id, install_time, is_enabled);
}

}  // namespace extensions
