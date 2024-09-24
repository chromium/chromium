// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_prefs.h"

#include <stddef.h>
#include <stdint.h>

#include <iterator>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/json/values_util.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/crx_file/id_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/api/types.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/app_display_info.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/user_script.h"

using extensions::mojom::ManifestLocation;

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

// Where an extension was installed from. (see mojom::ManifestLocation)
constexpr const char kPrefLocation[] = "location";

// Enabled, disabled, killed, etc. (see Extension::State)
constexpr const char kPrefState[] = "state";

// The path to the current version's manifest file.
constexpr const char kPrefPath[] = "path";

// The dictionary containing the extension's manifest.
constexpr const char kPrefManifest[] = "manifest";

// The version number.
constexpr const char kPrefManifestVersion[] = "manifest.version";

// The count of how many times we prompted the user to acknowledge an
// extension.
constexpr const char kPrefAcknowledgePromptCount[] = "ack_prompt_count";

// Indicates whether the user has acknowledged various types of extensions.
constexpr const char kPrefExternalAcknowledged[] = "ack_external";

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

// Path for settings specific to blocklist update.
constexpr const char kExtensionsBlocklistUpdate[] =
    "extensions.blacklistupdate";

// Path for the delayed install info dictionary preference. The actual string
// value is a legacy artifact for when delayed installs only pertained to
// updates that were waiting for idle.
constexpr const char kDelayedInstallInfo[] = "idle_install_info";

// Path for pref keys marked for deletion in extension prefs while populating
// the delayed install info. These keys are deleted from extension prefs when
// the prefs inside delayed install info are applied to the extension.
constexpr const char kDelayedInstallInfoDeletedPrefKeys[] =
    "delay_install_info_deleted_pref_keys";

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

// The set of permissions the extension desires to have active. This may include
// more than the required permissions from the manifest if the extension has
// optional permissions.
constexpr const char kPrefDesiredActivePermissions[] = "active_permissions";

// The set of permissions that the user has approved for the extension either at
// install time or through an optional permissions request. We track this in
// order to alert the user of permissions escalation.
// This also works with not-yet-recognized permissions (such as if an extension
// installed on stable channel uses a new permission that's only available in
// canary): the recorded granted permissions are determined from the recognized
// set of permissions, so when the new requested permission is later recognized
// (when it's available on stable), the requested set of permissions will
// differ from the stored granted set, and Chrome will notify the user of a
// permissions increase.
constexpr const char kPrefGrantedPermissions[] = "granted_permissions";

// Pref that was previously used to indicate if host permissions should be
// withheld. Due to the confusing name and the need to logically invert it when
// being used, we transitioned to use kPrefWithholdingPermissions
// instead.
const char kGrantExtensionAllHostPermissions[] =
    "extension_can_script_all_urls";

// A preference indicating if requested host permissions are being withheld from
// the extension, requiring them to be granted through the permissions API or
// runtime host permissions.
const char kPrefWithholdingPermissions[] = "withholding_permissions";

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

// A preference that indicates when an extension was first installed.
// This preference is created when an extension is installed and deleted when
// it is removed. It is NOT updated when the extension is updated.
constexpr const char kPrefFirstInstallTime[] = "first_install_time";
// A preference that indicates when an extension was last installed/updated.
constexpr const char kPrefLastUpdateTime[] = "last_update_time";
// A preference that indicates when an extension was installed/updated.
// TODO(anunoy): DEPRECATED! Remove after M113.
// Use kPrefLastUpdateTime instead.
constexpr const char kPrefDeprecatedInstallTime[] = "install_time";

// A preference which saves the creation flags for extensions.
constexpr const char kPrefCreationFlags[] = "creation_flags";

// A preference that indicates whether the extension was installed from the
// Chrome Web Store.
constexpr const char kPrefFromWebStore[] = "from_webstore";

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

// An installation parameter bundled with an extension.
constexpr const char kPrefInstallParam[] = "install_parameter";

// A list of installed ids and a signature.
constexpr const char kInstallSignature[] = "extensions.install_signature";

// A list of IDs of external extensions that the user has chosen to uninstall;
// saved as an indication to not re-install that extension.
constexpr const char kExternalUninstalls[] = "extensions.external_uninstalls";

// A boolean preference that indicates whether the extension should not be
// synced. Default value is false.
constexpr const char kPrefDoNotSync[] = "do_not_sync";

// A boolean preference that indicates whether the extension has local changes
// that need to be synced. Default value is false.
constexpr const char kPrefNeedsSync[] = "needs_sync";

// Key corresponding to the list of enabled static ruleset IDs for an extension.
// Used for the Declarative Net Request API.
constexpr const char kDNREnabledStaticRulesetIDs[] = "dnr_enabled_ruleset_ids";

// The default value to use for permission withholding when setting the pref on
// installation or for extensions where the pref has not been set.
constexpr bool kDefaultWithholdingBehavior = false;

// Checks whether the value passed in is consistent with the expected PrefType.
bool CheckPrefType(PrefType pref_type, const base::Value* value) {
  switch (pref_type) {
    case kBool:
      return value->is_bool();
    case kGURL:
    case kTime:
    case kString:
      return value->is_string();
    case kInteger:
      return value->is_int();
    case kDictionary:
      return value->is_dict();
    case kList:
      return value->is_list();
  }
}

// Serializes |time| as a string value mapped to |key| in |dictionary|.
void SaveTime(prefs::DictionaryValueUpdate* dictionary,
              const char* key,
              const base::Time& time) {
  if (!dictionary)
    return;

  dictionary->Set(key, base::TimeToValue(time));
}

// The opposite of SaveTime. If |key| is not found, this returns an empty Time
// (is_null() will return true).
base::Time ReadTime(const base::Value::Dict* dictionary, const char* key) {
  if (!dictionary)
    return base::Time();

  if (const base::Value* time_value = dictionary->FindByDottedPath(key))
    return base::ValueToTime(time_value).value_or(base::Time());

  return base::Time();
}

// Provider of write access to a dictionary storing extension prefs.
class ScopedExtensionPrefUpdate : public prefs::ScopedDictionaryPrefUpdate {
 public:
  ScopedExtensionPrefUpdate(PrefService* service,
                            const ExtensionId& extension_id)
      : ScopedDictionaryPrefUpdate(service, pref_names::kExtensions),
        extension_id_(extension_id) {
    DCHECK(crx_file::id_util::IdIsValid(extension_id_));
  }

  ScopedExtensionPrefUpdate(const ScopedExtensionPrefUpdate&) = delete;
  ScopedExtensionPrefUpdate& operator=(const ScopedExtensionPrefUpdate&) =
      delete;

  ~ScopedExtensionPrefUpdate() override = default;

  // ScopedDictionaryPrefUpdate overrides:
  std::unique_ptr<prefs::DictionaryValueUpdate> Get() override {
    std::unique_ptr<prefs::DictionaryValueUpdate> dict =
        ScopedDictionaryPrefUpdate::Get();
    std::unique_ptr<prefs::DictionaryValueUpdate> extension;
    if (!dict->GetDictionary(extension_id_, &extension)) {
      // Extension pref does not exist, create it.
      extension = dict->SetDictionary(extension_id_, base::Value::Dict());
    }
    return extension;
  }

 private:
  const ExtensionId extension_id_;
};

// Whether SetAlertSystemFirstRun() should always return true, so that alerts
// are triggered, even in first run.
bool g_run_alerts_in_first_run_for_testing = false;

}  // namespace

//
// ScopedDictionaryUpdate
//
ExtensionPrefs::ScopedDictionaryUpdate::ScopedDictionaryUpdate(
    ExtensionPrefs* prefs,
    const ExtensionId& extension_id,
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

  return dict->SetDictionary(key_, base::Value::Dict());
}

ExtensionPrefs::ScopedListUpdate::ScopedListUpdate(
    ExtensionPrefs* prefs,
    const ExtensionId& extension_id,
    const std::string& key)
    : update_(std::make_unique<ScopedExtensionPrefUpdate>(prefs->pref_service(),
                                                          extension_id)),
      key_(key) {}

ExtensionPrefs::ScopedListUpdate::~ScopedListUpdate() = default;

base::Value::List* ExtensionPrefs::ScopedListUpdate::Get() {
  base::Value::List* key_value = nullptr;
  (*update_)->GetListWithoutPathExpansion(key_, &key_value);
  return key_value;
}

base::Value::List* ExtensionPrefs::ScopedListUpdate::Ensure() {
  if (base::Value::List* existing = Get())
    return existing;
  return &(*update_)->SetKey(key_, base::Value(base::Value::List()))->GetList();
}

//
// ExtensionPrefs
//

// static
std::unique_ptr<ExtensionPrefs> ExtensionPrefs::Create(
    content::BrowserContext* browser_context,
    PrefService* prefs,
    const base::FilePath& root_dir,
    ExtensionPrefValueMap* extension_pref_value_map,
    bool extensions_disabled,
    const std::vector<EarlyExtensionPrefsObserver*>& early_observers) {
  return ExtensionPrefs::Create(
      browser_context, prefs, root_dir, extension_pref_value_map,
      extensions_disabled, early_observers, base::DefaultClock::GetInstance());
}

// static
std::unique_ptr<ExtensionPrefs> ExtensionPrefs::Create(
    content::BrowserContext* browser_context,
    PrefService* pref_service,
    const base::FilePath& root_dir,
    ExtensionPrefValueMap* extension_pref_value_map,
    bool extensions_disabled,
    const std::vector<EarlyExtensionPrefsObserver*>& early_observers,
    base::Clock* clock) {
  return std::make_unique<ExtensionPrefs>(
      browser_context, pref_service, root_dir, extension_pref_value_map, clock,
      extensions_disabled, early_observers);
}

ExtensionPrefs::~ExtensionPrefs() {
  for (auto& observer : observer_list_)
    observer.OnExtensionPrefsWillBeDestroyed(this);
  DCHECK(observer_list_.begin() == observer_list_.end());
}

// static
ExtensionPrefs* ExtensionPrefs::Get(content::BrowserContext* context) {
  return ExtensionPrefsFactory::GetInstance()->GetForBrowserContext(context);
}

static std::string MakePathRelative(const base::FilePath& parent,
                                    const base::FilePath& child) {
  if (!parent.IsParent(child))
    return child.AsUTF8Unsafe();

  base::FilePath::StringType retval =
      child.value().substr(parent.value().length());
  if (base::FilePath::IsSeparator(retval[0]))
    retval = retval.substr(1);
#if BUILDFLAG(IS_WIN)
  return base::WideToUTF8(retval);
#else
  return retval;
#endif
}

void ExtensionPrefs::MakePathsRelative() {
  const base::Value::Dict& dict = prefs_->GetDict(pref_names::kExtensions);
  if (dict.empty())
    return;

  // Collect all extensions ids with absolute paths in |absolute_keys|.
  std::set<std::string> absolute_keys;
  for (const auto [extension_id, extension_item] : dict) {
    if (!extension_item.is_dict())
      continue;
    const base::Value::Dict& extension_dict = extension_item.GetDict();
    std::optional<int> location_value = extension_dict.FindInt(kPrefLocation);
    if (location_value && Manifest::IsUnpackedLocation(
                              static_cast<ManifestLocation>(*location_value))) {
      // Unpacked extensions can have absolute paths.
      continue;
    }
    const std::string* path_string = extension_dict.FindString(kPrefPath);
    if (!path_string)
      continue;
    base::FilePath path = base::FilePath::FromUTF8Unsafe(*path_string);
    if (path.IsAbsolute())
      absolute_keys.insert(extension_id);
  }
  if (absolute_keys.empty())
    return;

  // Fix these paths.
  prefs::ScopedDictionaryPrefUpdate update(prefs_, pref_names::kExtensions);
  auto update_dict = update.Get();
  for (const std::string& key : absolute_keys) {
    std::unique_ptr<prefs::DictionaryValueUpdate> extension_dict;
    if (!update_dict->GetDictionaryWithoutPathExpansion(key, &extension_dict)) {
      NOTREACHED_IN_MIGRATION()
          << "Control should never reach here for extension " << key;
      continue;
    }
    std::string path_string;
    extension_dict->GetString(kPrefPath, &path_string);
    base::FilePath path = base::FilePath::FromUTF8Unsafe(path_string);
    extension_dict->SetString(kPrefPath,
                              MakePathRelative(install_directory_, path));
  }
}

const base::Value::Dict* ExtensionPrefs::GetExtensionPref(
    const ExtensionId& extension_id) const {
  // TODO(https://1297144): Should callers of this method proactively filter out
  // extension IDs? Previously, this function would (potentially surprisingly)
  // return `extensions` below if supplied with an empty `extension_id` due to
  // the legacy behavior of `base::Value::FindDictPath()`.
  if (extension_id.empty()) {
    return nullptr;
  }

  return prefs_->GetDict(pref_names::kExtensions)
      .FindDictByDottedPath(extension_id);
}

void ExtensionPrefs::SetIntegerPref(const ExtensionId& id,
                                    const PrefMap& pref,
                                    int value) {
  DCHECK_EQ(pref.type, PrefType::kInteger);
  UpdateExtensionPrefInternal(id, pref, base::Value(value));
}

void ExtensionPrefs::SetBooleanPref(const ExtensionId& id,
                                    const PrefMap& pref,
                                    bool value) {
  DCHECK_EQ(pref.type, PrefType::kBool);
  UpdateExtensionPrefInternal(id, pref, base::Value(value));
}

void ExtensionPrefs::SetStringPref(const ExtensionId& id,
                                   const PrefMap& pref,
                                   std::string value) {
  DCHECK_EQ(pref.type, PrefType::kString);
  UpdateExtensionPrefInternal(id, pref, base::Value(std::move(value)));
}

void ExtensionPrefs::SetListPref(const ExtensionId& id,
                                 const PrefMap& pref,
                                 base::Value::List value) {
  DCHECK_EQ(pref.type, PrefType::kList);
  UpdateExtensionPrefInternal(id, pref, base::Value(std::move(value)));
}

void ExtensionPrefs::SetDictionaryPref(const ExtensionId& id,
                                       const PrefMap& pref,
                                       base::Value::Dict value) {
  DCHECK_EQ(pref.type, PrefType::kDictionary);
  UpdateExtensionPrefInternal(id, pref, base::Value(std::move(value)));
}

void ExtensionPrefs::SetTimePref(const ExtensionId& id,
                                 const PrefMap& pref,
                                 base::Time value) {
  DCHECK_EQ(pref.type, PrefType::kTime);
  UpdateExtensionPrefInternal(id, pref, ::base::TimeToValue(value));
}

void ExtensionPrefs::UpdateExtensionPrefInternal(
    const ExtensionId& extension_id,
    const PrefMap& pref,
    base::Value data_value) {
  DCHECK_EQ(PrefScope::kExtensionSpecific, pref.scope);
  DCHECK(CheckPrefType(pref.type, &data_value));
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  update->Set(pref.name, std::move(data_value));
  for (auto& observer : observer_list_) {
    observer.OnExtensionPrefsUpdated(extension_id);
  }
}

void ExtensionPrefs::UpdateExtensionPref(
    const ExtensionId& extension_id,
    std::string_view key,
    std::optional<base::Value> data_value) {
  if (!crx_file::id_util::IdIsValid(extension_id)) {
    NOTREACHED_IN_MIGRATION() << "Invalid extension_id " << extension_id;
    return;
  }
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  if (data_value) {
    update->Set(key, *std::move(data_value));
  } else {
    update->Remove(key);
  }
  for (auto& observer : observer_list_) {
    observer.OnExtensionPrefsUpdated(extension_id);
  }
}

void ExtensionPrefs::DeleteExtensionPrefs(const ExtensionId& extension_id) {
  extension_pref_value_map_->UnregisterExtension(extension_id);
  for (auto& observer : observer_list_)
    observer.OnExtensionPrefsDeleted(extension_id);
  prefs::ScopedDictionaryPrefUpdate update(prefs_, pref_names::kExtensions);
  update->Remove(extension_id);
}

void ExtensionPrefs::DeleteExtensionPrefsIfPrefEmpty(
    const ExtensionId& extension_id) {
  const base::Value::Dict* dict = GetExtensionPref(extension_id);
  if (dict && dict->empty())
    DeleteExtensionPrefs(extension_id);
}

bool ExtensionPrefs::ReadPrefAsBoolean(const ExtensionId& extension_id,
                                       const PrefMap& pref,
                                       bool* out_value) const {
  DCHECK_EQ(PrefScope::kExtensionSpecific, pref.scope);
  DCHECK_EQ(PrefType::kBool, pref.type);
  DCHECK(out_value);

  const base::Value::Dict* ext = GetExtensionPref(extension_id);
  if (!ext)
    return false;

  std::optional<bool> value = ext->FindBoolByDottedPath(pref.name);
  if (!value)
    return false;

  *out_value = *value;
  return true;
}

bool ExtensionPrefs::ReadPrefAsInteger(const ExtensionId& extension_id,
                                       const PrefMap& pref,
                                       int* out_value) const {
  DCHECK_EQ(PrefScope::kExtensionSpecific, pref.scope);
  DCHECK_EQ(PrefType::kInteger, pref.type);
  DCHECK(out_value);
  const base::Value::Dict* ext = GetExtensionPref(extension_id);
  if (!ext)
    return false;
  std::optional<int> value = ext->FindIntByDottedPath(pref.name);
  if (!value)
    return false;
  *out_value = *value;
  return true;
}

bool ExtensionPrefs::ReadPrefAsString(const ExtensionId& extension_id,
                                      const PrefMap& pref,
                                      std::string* out_value) const {
  DCHECK_EQ(PrefScope::kExtensionSpecific, pref.scope);
  DCHECK_EQ(PrefType::kString, pref.type);
  DCHECK(out_value);
  const base::Value::Dict* ext = GetExtensionPref(extension_id);
  if (!ext)
    return false;

  const std::string* str = ext->FindStringByDottedPath(pref.name);
  if (!str)
    return false;

  *out_value = *str;
  return true;
}

const base::Value::List* ExtensionPrefs::ReadPrefAsList(
    const ExtensionId& extension_id,
    const PrefMap& pref) const {
  DCHECK_EQ(PrefScope::kExtensionSpecific, pref.scope);
  DCHECK_EQ(PrefType::kList, pref.type);
  const base::Value::Dict* ext = GetExtensionPref(extension_id);
  if (!ext)
    return nullptr;
  return ext->FindListByDottedPath(pref.name);
}

const base::Value::Dict* ExtensionPrefs::ReadPrefAsDictionary(
    const ExtensionId& extension_id,
    const PrefMap& pref) const {
  DCHECK_EQ(PrefScope::kExtensionSpecific, pref.scope);
  DCHECK_EQ(PrefType::kDictionary, pref.type);
  const base::Value::Dict* ext = GetExtensionPref(extension_id);
  if (!ext)
    return nullptr;
  return ext->FindDictByDottedPath(pref.name);
}

base::Time ExtensionPrefs::ReadPrefAsTime(const ExtensionId& extension_id,
                                          const PrefMap& pref) const {
  DCHECK_EQ(PrefScope::kExtensionSpecific, pref.scope);
  DCHECK_EQ(PrefType::kTime, pref.type);
  return ReadTime(GetExtensionPref(extension_id), pref.name);
}

bool ExtensionPrefs::ReadPrefAsBoolean(const ExtensionId& extension_id,
                                       std::string_view pref_key,
                                       bool* out_value) const {
  const base::Value::Dict* ext = GetExtensionPref(extension_id);
  if (!ext)
    return false;

  std::optional<bool> value = ext->FindBoolByDottedPath(pref_key);
  if (!value)
    return false;

  *out_value = *value;
  return true;
}

bool ExtensionPrefs::ReadPrefAsInteger(const ExtensionId& extension_id,
                                       std::string_view pref_key,
                                       int* out_value) const {
  const base::Value::Dict* ext = GetExtensionPref(extension_id);
  if (!ext)
    return false;

  std::optional<int> value = ext->FindIntByDottedPath(pref_key);
  if (!value)
    return false;

  *out_value = *value;
  return true;
}

bool ExtensionPrefs::ReadPrefAsString(const ExtensionId& extension_id,
                                      std::string_view pref_key,
                                      std::string* out_value) const {
  DCHECK(out_value);
  const base::Value::Dict* ext = GetExtensionPref(extension_id);
  if (!ext)
    return false;

  const std::string* str = ext->FindStringByDottedPath(pref_key);
  if (!str)
    return false;

  *out_value = *str;
  return true;
}

const base::Value::List* ExtensionPrefs::ReadPrefAsList(
    const ExtensionId& extension_id,
    std::string_view pref_key) const {
  const base::Value::Dict* ext = GetExtensionPref(extension_id);
  if (!ext)
    return nullptr;
  return ext->FindListByDottedPath(pref_key);
}

const base::Value* ExtensionPrefs::GetPrefAsValue(
    const ExtensionId& extension_id,
    std::string_view pref_key) const {
  const base::Value::Dict* ext = GetExtensionPref(extension_id);
  if (!ext)
    return nullptr;
  const base::Value* value = ext->FindByDottedPath(pref_key);
  return value && value->is_dict() ? value : nullptr;
}

const base::Value::Dict* ExtensionPrefs::ReadPrefAsDict(
    const ExtensionId& extension_id,
    std::string_view pref_key) const {
  const base::Value* out = GetPrefAsValue(extension_id, pref_key);
  return out ? &out->GetDict() : nullptr;
}

bool ExtensionPrefs::HasPrefForExtension(
    const ExtensionId& extension_id) const {
  return GetExtensionPref(extension_id) != nullptr;
}

bool ExtensionPrefs::ReadPrefAsURLPatternSet(const ExtensionId& extension_id,
                                             std::string_view pref_key,
                                             URLPatternSet* result,
                                             int valid_schemes) const {
  const base::Value::List* value = ReadPrefAsList(extension_id, pref_key);
  if (!value)
    return false;
  const base::Value::Dict* extension = GetExtensionPref(extension_id);
  if (!extension)
    return false;
  std::optional<int> location = extension->FindInt(kPrefLocation);
  if (location && static_cast<ManifestLocation>(*location) ==
                      ManifestLocation::kComponent) {
    valid_schemes |= URLPattern::SCHEME_CHROMEUI;
  }

  bool allow_file_access = AllowFileAccess(extension_id);
  return result->Populate(*value, valid_schemes, allow_file_access, nullptr);
}

void ExtensionPrefs::SetExtensionPrefURLPatternSet(
    const ExtensionId& extension_id,
    std::string_view pref_key,
    const URLPatternSet& set) {
  UpdateExtensionPref(extension_id, pref_key, base::Value(set.ToValue()));
}

bool ExtensionPrefs::ReadPrefAsBooleanAndReturn(
    const ExtensionId& extension_id,
    std::string_view pref_key) const {
  bool out_value = false;
  return ReadPrefAsBoolean(extension_id, pref_key, &out_value) && out_value;
}

std::unique_ptr<PermissionSet> ExtensionPrefs::ReadPrefAsPermissionSet(
    const ExtensionId& extension_id,
    std::string_view pref_key) const {
  if (!GetExtensionPref(extension_id))
    return nullptr;

  // Retrieve the API permissions. Please refer SetExtensionPrefPermissionSet()
  // for api_values format.
  APIPermissionSet apis;
  std::string api_pref = JoinPrefs({pref_key, kPrefAPIs});
  const base::Value::List* api_values = ReadPrefAsList(extension_id, api_pref);
  if (api_values) {
    APIPermissionSet::ParseFromJSON(*api_values,
                                    APIPermissionSet::kAllowInternalPermissions,
                                    &apis, nullptr, nullptr);
  }

  // Retrieve the Manifest Keys permissions. Please refer to
  // |SetExtensionPrefPermissionSet| for manifest_permissions_values format.
  ManifestPermissionSet manifest_permissions;
  std::string manifest_permission_pref =
      JoinPrefs({pref_key, kPrefManifestPermissions});
  const base::Value::List* manifest_permissions_values =
      ReadPrefAsList(extension_id, manifest_permission_pref);
  if (manifest_permissions_values) {
    ManifestPermissionSet::ParseFromJSON(
        *manifest_permissions_values, &manifest_permissions, nullptr, nullptr);
  }

  // Retrieve the explicit host permissions.
  URLPatternSet explicit_hosts;
  ReadPrefAsURLPatternSet(
      extension_id, JoinPrefs({pref_key, kPrefExplicitHosts}), &explicit_hosts,
      Extension::kValidHostPermissionSchemes);

  // Retrieve the scriptable host permissions.
  URLPatternSet scriptable_hosts;
  ReadPrefAsURLPatternSet(
      extension_id, JoinPrefs({pref_key, kPrefScriptableHosts}),
      &scriptable_hosts, UserScript::ValidUserScriptSchemes());

  return std::make_unique<PermissionSet>(
      std::move(apis), std::move(manifest_permissions),
      std::move(explicit_hosts), std::move(scriptable_hosts));
}

namespace {

// Set the API or Manifest permissions.
// The format of api_values is:
// [ "permission_name1",   // permissions do not support detail.
//   "permission_name2",
//   {"permission_name3": value },
//   // permission supports detail, permission detail will be stored in value.
//   ...
// ]
template <typename T>
base::Value CreatePermissionList(const T& permissions) {
  base::Value::List values;
  for (const auto* permission : permissions) {
    std::unique_ptr<base::Value> detail(permission->ToValue());
    if (detail) {
      base::Value::Dict tmp;
      tmp.Set(permission->name(),
              base::Value::FromUniquePtrValue(std::move(detail)));
      values.Append(std::move(tmp));
    } else {
      values.Append(permission->name());
    }
  }
  return base::Value(std::move(values));
}

}  // anonymous namespace

void ExtensionPrefs::SetExtensionPrefPermissionSet(
    const ExtensionId& extension_id,
    std::string_view pref_key,
    const PermissionSet& new_value) {
  std::string api_pref = JoinPrefs({pref_key, kPrefAPIs});
  UpdateExtensionPref(extension_id, api_pref,
                      CreatePermissionList(new_value.apis()));

  std::string manifest_permissions_pref =
      JoinPrefs({pref_key, kPrefManifestPermissions});
  UpdateExtensionPref(extension_id, manifest_permissions_pref,
                      CreatePermissionList(new_value.manifest_permissions()));

  // Set the explicit host permissions.
  SetExtensionPrefURLPatternSet(extension_id,
                                JoinPrefs({pref_key, kPrefExplicitHosts}),
                                new_value.explicit_hosts());

  // Set the scriptable host permissions.
  SetExtensionPrefURLPatternSet(extension_id,
                                JoinPrefs({pref_key, kPrefScriptableHosts}),
                                new_value.scriptable_hosts());
}

void ExtensionPrefs::AddToPrefPermissionSet(const ExtensionId& extension_id,
                                            const PermissionSet& permissions,
                                            const char* pref_name) {
  CHECK(crx_file::id_util::IdIsValid(extension_id));
  std::unique_ptr<PermissionSet> current =
      ReadPrefAsPermissionSet(extension_id, pref_name);
  std::unique_ptr<PermissionSet> union_set;
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

  std::unique_ptr<PermissionSet> current =
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
    const ExtensionId& extension_id) {
  int count = 0;
  ReadPrefAsInteger(extension_id, kPrefAcknowledgePromptCount, &count);
  ++count;
  UpdateExtensionPref(extension_id, kPrefAcknowledgePromptCount,
                      base::Value(count));
  return count;
}

bool ExtensionPrefs::IsExternalExtensionAcknowledged(
    const ExtensionId& extension_id) const {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefExternalAcknowledged);
}

void ExtensionPrefs::AcknowledgeExternalExtension(
    const ExtensionId& extension_id) {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  UpdateExtensionPref(extension_id, kPrefExternalAcknowledged,
                      base::Value(true));
  UpdateExtensionPref(extension_id, kPrefAcknowledgePromptCount, std::nullopt);
}

bool ExtensionPrefs::IsBlocklistedExtensionAcknowledged(
    const ExtensionId& extension_id) const {
  return blocklist_prefs::HasAcknowledgedBlocklistState(
      extension_id, BitMapBlocklistState::BLOCKLISTED_MALWARE, this);
}

void ExtensionPrefs::AcknowledgeBlocklistedExtension(
    const ExtensionId& extension_id) {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  blocklist_prefs::AddAcknowledgedBlocklistState(
      extension_id, BitMapBlocklistState::BLOCKLISTED_MALWARE, this);
  UpdateExtensionPref(extension_id, kPrefAcknowledgePromptCount, std::nullopt);
}

bool ExtensionPrefs::IsExternalInstallFirstRun(
    const ExtensionId& extension_id) const {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefExternalInstallFirstRun);
}

void ExtensionPrefs::SetExternalInstallFirstRun(
    const ExtensionId& extension_id) {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  UpdateExtensionPref(extension_id, kPrefExternalInstallFirstRun,
                      base::Value(true));
}

bool ExtensionPrefs::SetAlertSystemFirstRun() {
  if (prefs_->GetBoolean(pref_names::kAlertsInitialized)) {
    return true;
  }
  prefs_->SetBoolean(pref_names::kAlertsInitialized, true);
  return g_run_alerts_in_first_run_for_testing;  // Note: normally false.
}

bool ExtensionPrefs::DidExtensionEscalatePermissions(
    const ExtensionId& extension_id) const {
  return HasDisableReason(extension_id,
                          disable_reason::DISABLE_PERMISSIONS_INCREASE) ||
         HasDisableReason(extension_id, disable_reason::DISABLE_REMOTE_INSTALL);
}

int ExtensionPrefs::GetDisableReasons(const ExtensionId& extension_id) const {
  return GetBitMapPrefBits(extension_id, kPrefDisableReasons,
                           disable_reason::DISABLE_NONE);
}

int ExtensionPrefs::GetBitMapPrefBits(const ExtensionId& extension_id,
                                      std::string_view pref_key,
                                      int default_bit) const {
  int value = -1;
  if (ReadPrefAsInteger(extension_id, pref_key, &value) && value >= 0) {
    return value;
  }
  return default_bit;
}

bool ExtensionPrefs::HasDisableReason(
    const ExtensionId& extension_id,
    disable_reason::DisableReason disable_reason) const {
  return (GetDisableReasons(extension_id) & disable_reason) != 0;
}

void ExtensionPrefs::AddDisableReason(
    const ExtensionId& extension_id,
    disable_reason::DisableReason disable_reason) {
  AddDisableReasons(extension_id, disable_reason);
}

void ExtensionPrefs::AddDisableReasons(const ExtensionId& extension_id,
                                       int disable_reasons) {
  DCHECK(!DoesExtensionHaveState(extension_id, Extension::ENABLED) ||
         blocklist_prefs::IsExtensionBlocklisted(extension_id, this));
  ModifyDisableReasons(extension_id, disable_reasons,
                       BitMapPrefOperation::kAdd);
}

void ExtensionPrefs::RemoveDisableReason(
    const ExtensionId& extension_id,
    disable_reason::DisableReason disable_reason) {
  ModifyDisableReasons(extension_id, disable_reason,
                       BitMapPrefOperation::kRemove);
}

void ExtensionPrefs::ReplaceDisableReasons(const ExtensionId& extension_id,
                                           int disable_reasons) {
  ModifyDisableReasons(extension_id, disable_reasons,
                       BitMapPrefOperation::kReplace);
}

void ExtensionPrefs::ClearDisableReasons(const ExtensionId& extension_id) {
  ModifyDisableReasons(extension_id, disable_reason::DISABLE_NONE,
                       BitMapPrefOperation::kClear);
}

void ExtensionPrefs::ClearInapplicableDisableReasonsForComponentExtension(
    const ExtensionId& component_extension_id) {
  static constexpr int kAllowDisableReasons =
      disable_reason::DISABLE_RELOAD |
      disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT |
      disable_reason::DISABLE_CORRUPTED | disable_reason::DISABLE_REINSTALL;
  int allowed_disable_reasons = kAllowDisableReasons;

  // Some disable reasons incorrectly cause component extensions to never
  // activate on load. See https://crbug.com/946839 for more details on why we
  // do this.
  ModifyDisableReasons(
      component_extension_id,
      allowed_disable_reasons & GetDisableReasons(component_extension_id),
      BitMapPrefOperation::kReplace);
}

void ExtensionPrefs::ModifyDisableReasons(const ExtensionId& extension_id,
                                          int reasons,
                                          BitMapPrefOperation operation) {
  int old_value = GetBitMapPrefBits(extension_id, kPrefDisableReasons,
                                    disable_reason::DISABLE_NONE);
  ModifyBitMapPrefBits(extension_id, reasons, operation, kPrefDisableReasons,
                       disable_reason::DISABLE_NONE);
  int new_value = GetBitMapPrefBits(extension_id, kPrefDisableReasons,
                                    disable_reason::DISABLE_NONE);

  if (old_value == new_value)  // no change, do not notify observers.
    return;

  for (auto& observer : observer_list_)
    observer.OnExtensionDisableReasonsChanged(extension_id, new_value);
}

void ExtensionPrefs::ModifyBitMapPrefBits(const ExtensionId& extension_id,
                                          int pending_bits,
                                          BitMapPrefOperation operation,
                                          std::string_view pref_key,
                                          int default_bit) {
  int old_value = GetBitMapPrefBits(extension_id, pref_key, default_bit);
  int new_value = old_value;
  switch (operation) {
    case BitMapPrefOperation::kAdd:
      new_value |= pending_bits;
      break;
    case BitMapPrefOperation::kRemove:
      new_value &= ~pending_bits;
      break;
    case BitMapPrefOperation::kReplace:
      new_value = pending_bits;
      break;
    case BitMapPrefOperation::kClear:
      new_value = pending_bits;
      break;
  }

  if (old_value == new_value)  // no change, return.
    return;

  if (new_value == default_bit) {
    UpdateExtensionPref(extension_id, pref_key, std::nullopt);
  } else {
    UpdateExtensionPref(extension_id, pref_key, base::Value(new_value));
  }
}

base::Time ExtensionPrefs::LastPingDay(const ExtensionId& extension_id) const {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  static constexpr PrefMap kMap = {kLastPingDay, PrefType::kTime,
                                   PrefScope::kExtensionSpecific};

  return ReadPrefAsTime(extension_id, kMap);
}

void ExtensionPrefs::SetLastPingDay(const ExtensionId& extension_id,
                                    const base::Time& time) {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  SaveTime(update.Get().get(), kLastPingDay, time);
}

base::Time ExtensionPrefs::BlocklistLastPingDay() const {
  return ReadTime(&prefs_->GetDict(kExtensionsBlocklistUpdate), kLastPingDay);
}

void ExtensionPrefs::SetBlocklistLastPingDay(const base::Time& time) {
  prefs::ScopedDictionaryPrefUpdate update(prefs_, kExtensionsBlocklistUpdate);
  SaveTime(update.Get().get(), kLastPingDay, time);
}

base::Time ExtensionPrefs::LastActivePingDay(
    const ExtensionId& extension_id) const {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  static constexpr PrefMap kMap = {kLastActivePingDay, PrefType::kTime,
                                   PrefScope::kExtensionSpecific};

  return ReadPrefAsTime(extension_id, kMap);
}

void ExtensionPrefs::SetLastActivePingDay(const ExtensionId& extension_id,
                                          const base::Time& time) {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  SaveTime(update.Get().get(), kLastActivePingDay, time);
}

bool ExtensionPrefs::GetActiveBit(const ExtensionId& extension_id) const {
  const base::Value::Dict* dictionary = GetExtensionPref(extension_id);
  if (dictionary)
    return dictionary->FindBool(kActiveBit).value_or(false);
  return false;
}

void ExtensionPrefs::SetActiveBit(const ExtensionId& extension_id,
                                  bool active) {
  UpdateExtensionPref(extension_id, kActiveBit, base::Value(active));
}

std::unique_ptr<PermissionSet> ExtensionPrefs::GetGrantedPermissions(
    const ExtensionId& extension_id) const {
  CHECK(crx_file::id_util::IdIsValid(extension_id));
  return ReadPrefAsPermissionSet(extension_id, kPrefGrantedPermissions);
}

void ExtensionPrefs::AddGrantedPermissions(const ExtensionId& extension_id,
                                           const PermissionSet& permissions) {
  AddToPrefPermissionSet(extension_id, permissions, kPrefGrantedPermissions);
}

void ExtensionPrefs::RemoveGrantedPermissions(
    const ExtensionId& extension_id,
    const PermissionSet& permissions) {
  RemoveFromPrefPermissionSet(extension_id, permissions,
                              kPrefGrantedPermissions);
}

std::unique_ptr<PermissionSet> ExtensionPrefs::GetDesiredActivePermissions(
    const ExtensionId& extension_id) const {
  CHECK(crx_file::id_util::IdIsValid(extension_id));
  return ReadPrefAsPermissionSet(extension_id, kPrefDesiredActivePermissions);
}

void ExtensionPrefs::SetDesiredActivePermissions(
    const ExtensionId& extension_id,
    const PermissionSet& permissions) {
  SetExtensionPrefPermissionSet(extension_id, kPrefDesiredActivePermissions,
                                permissions);
}

void ExtensionPrefs::AddDesiredActivePermissions(
    const ExtensionId& extension_id,
    const PermissionSet& permissions) {
  AddToPrefPermissionSet(extension_id, permissions,
                         kPrefDesiredActivePermissions);
}

void ExtensionPrefs::RemoveDesiredActivePermissions(
    const ExtensionId& extension_id,
    const PermissionSet& permissions) {
  RemoveFromPrefPermissionSet(extension_id, permissions,
                              kPrefDesiredActivePermissions);
}

void ExtensionPrefs::SetWithholdingPermissions(const ExtensionId& extension_id,
                                               bool should_withhold) {
  UpdateExtensionPref(extension_id, kPrefWithholdingPermissions,
                      base::Value(should_withhold));
}

bool ExtensionPrefs::GetWithholdingPermissions(
    const ExtensionId& extension_id) const {
  bool permissions_allowed = false;
  if (ReadPrefAsBoolean(extension_id, kPrefWithholdingPermissions,
                        &permissions_allowed)) {
    return permissions_allowed;
  }

  // If no pref was found, we use the default.
  return kDefaultWithholdingBehavior;
}

bool ExtensionPrefs::HasWithholdingPermissionsSetting(
    const ExtensionId& extension_id) const {
  const base::Value::Dict* ext = GetExtensionPref(extension_id);
  return ext && ext->Find(kPrefWithholdingPermissions);
}

std::unique_ptr<PermissionSet> ExtensionPrefs::GetRuntimeGrantedPermissions(
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

void ExtensionPrefs::SetExtensionRunning(const ExtensionId& extension_id,
                                         bool is_running) {
  UpdateExtensionPref(extension_id, kPrefRunning, base::Value(is_running));
}

bool ExtensionPrefs::IsExtensionRunning(const ExtensionId& extension_id) const {
  const base::Value::Dict* extension = GetExtensionPref(extension_id);
  if (extension)
    return extension->FindBool(kPrefRunning).value_or(false);
  return false;
}

void ExtensionPrefs::SetIsActive(const ExtensionId& extension_id,
                                 bool is_active) {
  UpdateExtensionPref(extension_id, kIsActive, base::Value(is_active));
}

bool ExtensionPrefs::IsActive(const ExtensionId& extension_id) const {
  const base::Value::Dict* extension = GetExtensionPref(extension_id);
  if (extension)
    return extension->FindBool(kIsActive).value_or(false);
  return false;
}

bool ExtensionPrefs::IsIncognitoEnabled(const ExtensionId& extension_id) const {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefIncognitoEnabled);
}

void ExtensionPrefs::SetIsIncognitoEnabled(const ExtensionId& extension_id,
                                           bool enabled) {
  UpdateExtensionPref(extension_id, kPrefIncognitoEnabled,
                      base::Value(enabled));
  extension_pref_value_map_->SetExtensionIncognitoState(extension_id, enabled);
}

bool ExtensionPrefs::AllowFileAccess(const ExtensionId& extension_id) const {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefAllowFileAccess);
}

void ExtensionPrefs::SetAllowFileAccess(const ExtensionId& extension_id,
                                        bool allow) {
  UpdateExtensionPref(extension_id, kPrefAllowFileAccess, base::Value(allow));
}

bool ExtensionPrefs::HasAllowFileAccessSetting(
    const ExtensionId& extension_id) const {
  const base::Value::Dict* ext = GetExtensionPref(extension_id);
  return ext && ext->Find(kPrefAllowFileAccess);
}

bool ExtensionPrefs::DoesExtensionHaveState(
    const ExtensionId& id,
    Extension::State check_state) const {
  const base::Value::Dict* extension = GetExtensionPref(id);
  if (!extension)
    return false;

  std::optional<int> state = extension->FindInt(kPrefState);
  if (!state)
    return false;

  if (*state < 0 || *state >= Extension::NUM_STATES) {
    LOG(ERROR) << "Bad pref 'state' for extension '" << id << "'";
    return false;
  }

  return *state == check_state;
}

bool ExtensionPrefs::IsExternalExtensionUninstalled(
    const ExtensionId& id) const {
  ExtensionIdList uninstalled_ids;
  GetUserExtensionPrefIntoContainer(kExternalUninstalls, &uninstalled_ids);
  return base::Contains(uninstalled_ids, id);
}

bool ExtensionPrefs::ClearExternalExtensionUninstalled(const ExtensionId& id) {
  ScopedListPrefUpdate update(prefs_, kExternalUninstalls);
  size_t num_removed = update->EraseIf([&id](const base::Value& value) {
    return value.is_string() && value.GetString() == id;
  });
  return num_removed > 0;
}

bool ExtensionPrefs::IsExtensionDisabled(const ExtensionId& id) const {
  return DoesExtensionHaveState(id, Extension::DISABLED);
}

ExtensionIdList ExtensionPrefs::GetPinnedExtensions() const {
  ExtensionIdList id_list_out;
  GetUserExtensionPrefIntoContainer(pref_names::kPinnedExtensions,
                                    &id_list_out);
  return id_list_out;
}

void ExtensionPrefs::SetPinnedExtensions(const ExtensionIdList& extension_ids) {
  SetExtensionPrefFromContainer(pref_names::kPinnedExtensions, extension_ids);
}

void ExtensionPrefs::OnExtensionInstalled(
    const Extension* extension,
    Extension::State initial_state,
    const syncer::StringOrdinal& page_ordinal,
    int install_flags,
    const std::string& install_parameter,
    base::Value::Dict ruleset_install_prefs) {
  // If the extension was previously an external extension that was uninstalled,
  // clear the external uninstall bit.
  // TODO(devlin): We previously did this because we indicated external
  // uninstallation through the extension dictionary itself (on the "state"
  // key), and needed a way to have other installation - such as user or policy
  // installations - override that state. Now that external uninstalls are
  // stored separately, we shouldn't necessarily have to do this - a new install
  // can still override the external uninstall without clearing the bit.
  // However, it's not clear if existing subsystems may also be relying on this
  // bit being set/unset. For now, maintain existing behavior.
  if (IsExternalExtensionUninstalled(extension->id()))
    ClearExternalExtensionUninstalled(extension->id());

  ScopedExtensionPrefUpdate update(prefs_, extension->id());
  auto extension_dict = update.Get();
  const base::Time install_time = clock_->Now();

  base::Value::List prefs_to_remove;
  PopulateExtensionInfoPrefs(
      extension, install_time, initial_state, install_flags, install_parameter,
      std::move(ruleset_install_prefs), extension_dict.get(), prefs_to_remove);

  for (const auto& pref_to_remove : prefs_to_remove) {
    extension_dict->Remove(pref_to_remove.GetString());
  }

  FinishExtensionInfoPrefs(extension->id(), install_time,
                           AppDisplayInfo::RequiresSortOrdinal(*extension),
                           page_ordinal, extension_dict.get());
}

void ExtensionPrefs::OnExtensionUninstalled(const ExtensionId& extension_id,
                                            const ManifestLocation location,
                                            bool external_uninstall) {
  app_sorting()->ClearOrdinals(extension_id);

  // For external extensions, we save a preference reminding ourself not to try
  // and install the extension anymore (except when |external_uninstall| is
  // true, which signifies that the registry key was deleted or the pref file
  // no longer lists the extension).
  if (!external_uninstall && Manifest::IsExternalLocation(location)) {
    ScopedListPrefUpdate update(prefs_, kExternalUninstalls);
    update->Append(extension_id);
  }

  DeleteExtensionPrefs(extension_id);
}

void ExtensionPrefs::SetExtensionEnabled(const ExtensionId& extension_id) {
  UpdateExtensionPref(extension_id, kPrefState,
                      base::Value(Extension::ENABLED));
  extension_pref_value_map_->SetExtensionState(extension_id, true);
  UpdateExtensionPref(extension_id, kPrefDisableReasons, std::nullopt);
  for (auto& observer : observer_list_)
    observer.OnExtensionStateChanged(extension_id, true);
}

void ExtensionPrefs::SetExtensionDisabled(const ExtensionId& extension_id,
                                          int disable_reasons) {
  UpdateExtensionPref(extension_id, kPrefState,
                      base::Value(Extension::DISABLED));
  extension_pref_value_map_->SetExtensionState(extension_id, false);
  UpdateExtensionPref(extension_id, kPrefDisableReasons,
                      base::Value(disable_reasons));
  for (auto& observer : observer_list_)
    observer.OnExtensionStateChanged(extension_id, false);
}

std::string ExtensionPrefs::GetVersionString(
    const ExtensionId& extension_id) const {
  const base::Value::Dict* extension = GetExtensionPref(extension_id);
  if (!extension)
    return std::string();

  const std::string* version =
      extension->FindStringByDottedPath(kPrefManifestVersion);
  return version ? *version : std::string();
}

void ExtensionPrefs::UpdateManifest(const Extension* extension) {
  if (!Manifest::IsUnpackedLocation(extension->location())) {
    const base::Value::Dict* extension_dict = GetExtensionPref(extension->id());
    if (!extension_dict)
      return;
    const base::Value::Dict* old_manifest =
        extension_dict->FindDict(kPrefManifest);
    bool update_required =
        !old_manifest || *extension->manifest()->value() != *old_manifest;
    if (update_required) {
      UpdateExtensionPref(extension->id(), kPrefManifest,
                          base::Value(extension->manifest()->value()->Clone()));
    }
  }
}

void ExtensionPrefs::SetInstallLocation(const ExtensionId& extension_id,
                                        ManifestLocation location) {
  UpdateExtensionPref(extension_id, kPrefLocation,
                      base::Value(static_cast<int>(location)));
}

std::optional<ExtensionInfo> ExtensionPrefs::GetInstalledInfoHelper(
    const ExtensionId& extension_id,
    const base::Value::Dict& extension,
    bool include_component_extensions) const {
  std::optional<int> location_value = extension.FindInt(kPrefLocation);
  if (!location_value) {
    return std::nullopt;
  }

  ManifestLocation location = static_cast<ManifestLocation>(*location_value);
  if (location == ManifestLocation::kComponent &&
      !include_component_extensions) {
    // Component extensions are ignored by default. Component extensions may
    // have data saved in preferences, but they are already loaded at this point
    // (by ComponentLoader) and shouldn't be populated into the result of
    // GetInstalledExtensionsInfo, otherwise InstalledLoader would also want to
    // load them.
    return std::nullopt;
  }

  // Only the following extension types have data saved in the preferences.
  if (location != ManifestLocation::kInternal &&
      location != ManifestLocation::kComponent &&
      !Manifest::IsUnpackedLocation(location) &&
      !Manifest::IsExternalLocation(location)) {
    NOTREACHED_IN_MIGRATION();
    return std::nullopt;
  }

  const base::Value* manifest = extension.Find(kPrefManifest);
  if (!Manifest::IsUnpackedLocation(location) &&
      !(manifest && manifest->is_dict())) {
    LOG(WARNING) << "Missing manifest for extension " << extension_id;
    // Just a warning for now.
  }

  // Extensions with login screen context can only be policy extensions.
  // However, the manifest location in the pref store could get corrupted
  // (crbug.com/1466188). Thus, we don't construct the extension info for these
  // cases.
  int flags = GetCreationFlags(extension_id);
  if (!Manifest::IsPolicyLocation(location) &&
      flags & Extension::FOR_LOGIN_SCREEN) {
    return std::nullopt;
  }

  const std::string* path = extension.FindString(kPrefPath);
  if (!path) {
    return std::nullopt;
  }

  // The old creation flag value for indicating an extension was a bookmark app.
  // This matches the commented-out entry in extension.h.
  constexpr int kOldBookmarkAppFlag = 1 << 4;
  std::optional<int> creation_flags = extension.FindInt(kPrefCreationFlags);
  if (creation_flags && (*creation_flags & kOldBookmarkAppFlag)) {
    // This is an old bookmark app entry. Ignore it.
    return std::nullopt;
  }

  base::FilePath file_path = base::FilePath::FromUTF8Unsafe(*path);

  // Make path absolute. Most (but not all) extension types have relative paths.
  if (!file_path.IsAbsolute()) {
    file_path = install_directory_.Append(file_path);
  }
  const base::Value::Dict* manifest_dict =
      manifest && manifest->is_dict() ? &manifest->GetDict() : nullptr;
  return ExtensionInfo(manifest_dict, extension_id, file_path, location);
}

std::optional<ExtensionInfo> ExtensionPrefs::GetInstalledExtensionInfo(
    const ExtensionId& extension_id,
    bool include_component_extensions) const {
  const base::Value::Dict& extensions =
      prefs_->GetDict(pref_names::kExtensions);
  const base::Value::Dict* ext = extensions.FindDict(extension_id);
  if (!ext) {
    return std::nullopt;
  }

  std::optional<int> state_value = ext->FindInt(kPrefState);
  // TODO(devlin): Remove this once all clients are updated with
  // MigrateToNewExternalUninstallPref().
  if (state_value == Extension::DEPRECATED_EXTERNAL_EXTENSION_UNINSTALLED) {
    return std::nullopt;
  }

  return GetInstalledInfoHelper(extension_id, *ext,
                                include_component_extensions);
}

ExtensionPrefs::ExtensionsInfo ExtensionPrefs::GetInstalledExtensionsInfo(
    bool include_component_extensions) const {
  ExtensionsInfo extensions_info;

  const base::Value::Dict& extensions =
      prefs_->GetDict(pref_names::kExtensions);
  for (const auto [extension_id, _] : extensions) {
    if (!crx_file::id_util::IdIsValid(extension_id)) {
      continue;
    }

    std::optional<ExtensionInfo> info =
        GetInstalledExtensionInfo(extension_id, include_component_extensions);
    if (info) {
      extensions_info.push_back(*std::move(info));
    }
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
    base::Value::Dict ruleset_install_prefs) {
  ScopedDictionaryUpdate update(this, extension->id(), kDelayedInstallInfo);
  auto extension_dict = update.Create();
  base::Value::List prefs_to_remove;
  PopulateExtensionInfoPrefs(
      extension, clock_->Now(), initial_state, install_flags, install_parameter,
      std::move(ruleset_install_prefs), extension_dict.get(), prefs_to_remove);

  // Add transient data that is needed by FinishDelayedInstallInfo(), but
  // should not be in the final extension prefs. All entries here should have
  // a corresponding Remove() call in FinishDelayedInstallInfo().
  extension_dict->Set(kDelayedInstallInfoDeletedPrefKeys,
                      base::Value(std::move(prefs_to_remove)));
  if (AppDisplayInfo::RequiresSortOrdinal(*extension)) {
    extension_dict->SetString(kPrefSuggestedPageOrdinal,
                              page_ordinal.IsValid()
                                  ? page_ordinal.ToInternalValue()
                                  : std::string());
  }
  extension_dict->SetInteger(kDelayedInstallReason,
                             static_cast<int>(delay_reason));
}

bool ExtensionPrefs::RemoveDelayedInstallInfo(const ExtensionId& extension_id) {
  if (!GetExtensionPref(extension_id))
    return false;
  ScopedExtensionPrefUpdate update(prefs_, extension_id);
  bool result = update->Remove(kDelayedInstallInfo);
  return result;
}

bool ExtensionPrefs::FinishDelayedInstallInfo(const ExtensionId& extension_id) {
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
    pending_install_dict->Remove(kPrefSuggestedPageOrdinal);
  }
  pending_install_dict->Remove(kDelayedInstallReason);

  const base::Time install_time = clock_->Now();
  std::string install_time_str = base::NumberToString(
      install_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  pending_install_dict->SetString(kPrefLastUpdateTime, install_time_str);

  // Update first install time only if it does not already exist in committed
  // data. Otherwise, remove the key from the temp dictionary so it does not
  // incorrectly update the committed data.
  if (!extension_dict->HasKey(kPrefFirstInstallTime))
    pending_install_dict->SetString(kPrefFirstInstallTime, install_time_str);
  else
    pending_install_dict->Remove(kPrefFirstInstallTime);

  base::Value::List* prefs_to_remove = nullptr;
  if (pending_install_dict->GetListWithoutPathExpansion(
          kDelayedInstallInfoDeletedPrefKeys, &prefs_to_remove)) {
    for (const auto& pref_to_remove : *prefs_to_remove) {
      extension_dict->Remove(pref_to_remove.GetString());
    }

    pending_install_dict->Remove(kDelayedInstallInfoDeletedPrefKeys);
  }

  // Commit the delayed install data.
  for (const auto [key, value] : *pending_install_dict->AsConstDict()) {
    extension_dict->Set(key, value.Clone());
  }
  FinishExtensionInfoPrefs(extension_id, install_time, needs_sort_ordinal,
                           suggested_page_ordinal, extension_dict.get());
  return true;
}

std::optional<ExtensionInfo> ExtensionPrefs::GetDelayedInstallInfo(
    const ExtensionId& extension_id) const {
  const base::Value::Dict* extension_prefs = GetExtensionPref(extension_id);
  if (!extension_prefs) {
    return std::nullopt;
  }

  const base::Value::Dict* ext = extension_prefs->FindDict(kDelayedInstallInfo);
  if (!ext) {
    return std::nullopt;
  }

  return GetInstalledInfoHelper(extension_id, *ext,
                                /*include_component_extensions = */ false);
}

ExtensionPrefs::DelayReason ExtensionPrefs::GetDelayedInstallReason(
    const ExtensionId& extension_id) const {
  const base::Value::Dict* extension_prefs = GetExtensionPref(extension_id);
  if (!extension_prefs)
    return DelayReason::kNone;

  const base::Value::Dict* ext = extension_prefs->FindDict(kDelayedInstallInfo);
  if (!ext)
    return DelayReason::kNone;

  std::optional<int> delay_reason = ext->FindInt(kDelayedInstallReason);
  if (!delay_reason)
    return DelayReason::kNone;

  return static_cast<DelayReason>(*delay_reason);
}

ExtensionPrefs::ExtensionsInfo ExtensionPrefs::GetAllDelayedInstallInfo()
    const {
  ExtensionsInfo extensions_info;

  const base::Value::Dict& extensions =
      prefs_->GetDict(pref_names::kExtensions);
  for (const auto [extension_id, _] : extensions) {
    if (!crx_file::id_util::IdIsValid(extension_id))
      continue;

    std::optional<ExtensionInfo> info = GetDelayedInstallInfo(extension_id);
    if (info) {
      extensions_info.push_back(*std::move(info));
    }
  }

  return extensions_info;
}

bool ExtensionPrefs::IsFromWebStore(const ExtensionId& extension_id) const {
  const base::Value::Dict* dictionary = GetExtensionPref(extension_id);
  if (dictionary)
    return dictionary->FindBool(kPrefFromWebStore).value_or(false);
  return false;
}

int ExtensionPrefs::GetCreationFlags(const ExtensionId& extension_id) const {
  int creation_flags = Extension::NO_FLAGS;
  if (!ReadPrefAsInteger(extension_id, kPrefCreationFlags, &creation_flags)) {
    // Since kPrefCreationFlags was added later, it will be missing for
    // previously installed extensions.
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
    const ExtensionId& extension_id) const {
  int creation_flags = Extension::NO_FLAGS;
  const base::Value::Dict* delayed_info =
      ReadPrefAsDict(extension_id, kDelayedInstallInfo);
  if (delayed_info) {
    if (std::optional<int> flags = delayed_info->FindInt(kPrefCreationFlags)) {
      creation_flags = *flags;
    }
  }
  return creation_flags;
}

bool ExtensionPrefs::WasInstalledByDefault(
    const ExtensionId& extension_id) const {
  const base::Value::Dict* dictionary = GetExtensionPref(extension_id);
  if (!dictionary)
    return false;
  return dictionary->FindBool(kPrefWasInstalledByDefault).value_or(false);
}

bool ExtensionPrefs::WasInstalledByOem(const ExtensionId& extension_id) const {
  const base::Value::Dict* dictionary = GetExtensionPref(extension_id);
  if (dictionary)
    return dictionary->FindBool(kPrefWasInstalledByOem).value_or(false);
  return false;
}

base::Time ExtensionPrefs::GetFirstInstallTime(
    const ExtensionId& extension_id) const {
  static constexpr PrefMap kMap = {kPrefFirstInstallTime, PrefType::kTime,
                                   PrefScope::kExtensionSpecific};

  return ReadPrefAsTime(extension_id, kMap);
}

base::Time ExtensionPrefs::GetLastUpdateTime(
    const ExtensionId& extension_id) const {
  static constexpr PrefMap kMap = {kPrefLastUpdateTime, PrefType::kTime,
                                   PrefScope::kExtensionSpecific};

  return ReadPrefAsTime(extension_id, kMap);
}

bool ExtensionPrefs::DoNotSync(const ExtensionId& extension_id) const {
  bool do_not_sync;
  if (!ReadPrefAsBoolean(extension_id, kPrefDoNotSync, &do_not_sync))
    return false;

  return do_not_sync;
}

base::Time ExtensionPrefs::GetLastLaunchTime(
    const ExtensionId& extension_id) const {
  static constexpr PrefMap kMap = {kPrefLastLaunchTime, PrefType::kTime,
                                   PrefScope::kExtensionSpecific};

  return ReadPrefAsTime(extension_id, kMap);
}

void ExtensionPrefs::SetLastLaunchTime(const ExtensionId& extension_id,
                                       const base::Time& time) {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));
  {
    ScopedExtensionPrefUpdate update(prefs_, extension_id);
    SaveTime(update.Get().get(), kPrefLastLaunchTime, time);
  }
  for (auto& observer : observer_list_)
    observer.OnExtensionLastLaunchTimeChanged(extension_id, time);
}

void ExtensionPrefs::ClearLastLaunchTimes() {
  const base::Value::Dict& dict = prefs_->GetDict(pref_names::kExtensions);
  if (dict.empty())
    return;

  // Collect all the keys to remove the last launched preference from.
  prefs::ScopedDictionaryPrefUpdate update(prefs_, pref_names::kExtensions);
  auto update_dict = update.Get();
  for (const auto [key, _] : *update_dict->AsConstDict()) {
    std::unique_ptr<prefs::DictionaryValueUpdate> extension_dict;
    if (!update_dict->GetDictionary(key, &extension_dict))
      continue;

    if (extension_dict->HasKey(kPrefLastLaunchTime))
      extension_dict->Remove(kPrefLastLaunchTime);
  }
}

void ExtensionPrefs::SetIntegerPref(const PrefMap& pref, int value) {
  DCHECK_EQ(PrefScope::kProfile, pref.scope);
  DCHECK_EQ(PrefType::kInteger, pref.type);
  prefs_->SetInteger(pref.name, value);
}

void ExtensionPrefs::SetBooleanPref(const PrefMap& pref, bool value) {
  DCHECK_EQ(PrefScope::kProfile, pref.scope);
  DCHECK_EQ(PrefType::kBool, pref.type);
  prefs_->SetBoolean(pref.name, value);
}

void ExtensionPrefs::SetStringPref(const PrefMap& pref,
                                   const std::string& value) {
  DCHECK_EQ(PrefScope::kProfile, pref.scope);
  DCHECK_EQ(PrefType::kString, pref.type);
  prefs_->SetString(pref.name, value);
}

void ExtensionPrefs::SetTimePref(const PrefMap& pref, base::Time value) {
  DCHECK_EQ(PrefScope::kProfile, pref.scope);
  DCHECK_EQ(PrefType::kTime, pref.type);
  prefs_->SetTime(pref.name, value);
}

void ExtensionPrefs::SetGURLPref(const PrefMap& pref, const GURL& value) {
  DCHECK_EQ(PrefScope::kProfile, pref.scope);
  DCHECK_EQ(PrefType::kGURL, pref.type);
  DCHECK(value.is_valid())
      << "Invalid GURL was passed in. The pref will not be updated.";
  prefs_->SetString(pref.name, value.spec());
}

void ExtensionPrefs::SetDictionaryPref(const PrefMap& pref,
                                       base::Value::Dict value) {
  DCHECK_EQ(PrefScope::kProfile, pref.scope);
  DCHECK_EQ(PrefType::kDictionary, pref.type);
  prefs_->SetDict(pref.name, std::move(value));
}

int ExtensionPrefs::GetPrefAsInteger(const PrefMap& pref) const {
  DCHECK_EQ(PrefScope::kProfile, pref.scope);
  DCHECK_EQ(PrefType::kInteger, pref.type);
  return prefs_->GetInteger(pref.name);
}

bool ExtensionPrefs::GetPrefAsBoolean(const PrefMap& pref) const {
  DCHECK_EQ(PrefScope::kProfile, pref.scope);
  DCHECK_EQ(PrefType::kBool, pref.type);
  return prefs_->GetBoolean(pref.name);
}

std::string ExtensionPrefs::GetPrefAsString(const PrefMap& pref) const {
  DCHECK_EQ(PrefScope::kProfile, pref.scope);
  DCHECK_EQ(PrefType::kString, pref.type);
  return prefs_->GetString(pref.name);
}

base::Time ExtensionPrefs::GetPrefAsTime(const PrefMap& pref) const {
  DCHECK_EQ(PrefScope::kProfile, pref.scope);
  DCHECK_EQ(PrefType::kTime, pref.type);
  return prefs_->GetTime(pref.name);
}

GURL ExtensionPrefs::GetPrefAsGURL(const PrefMap& pref) const {
  DCHECK_EQ(PrefScope::kProfile, pref.scope);
  DCHECK_EQ(PrefType::kGURL, pref.type);
  return GURL(prefs_->GetString(pref.name));
}

const base::Value::Dict& ExtensionPrefs::GetPrefAsDictionary(
    const PrefMap& pref) const {
  DCHECK_EQ(PrefScope::kProfile, pref.scope);
  DCHECK_EQ(PrefType::kDictionary, pref.type);
  return prefs_->GetDict(pref.name);
}

std::unique_ptr<prefs::ScopedDictionaryPrefUpdate>
ExtensionPrefs::CreatePrefUpdate(const PrefMap& pref) {
  DCHECK_EQ(PrefScope::kProfile, pref.scope);
  DCHECK_EQ(PrefType::kDictionary, pref.type);
  return std::make_unique<prefs::ScopedDictionaryPrefUpdate>(prefs_, pref.name);
}

void ExtensionPrefs::IncrementPref(const PrefMap& pref) {
  int count = GetPrefAsInteger(pref);
  SetIntegerPref(pref, count + 1);
}

void ExtensionPrefs::DecrementPref(const PrefMap& pref) {
  int count = GetPrefAsInteger(pref);
  SetIntegerPref(pref, count - 1);
}

ExtensionIdList ExtensionPrefs::GetExtensions() const {
  ExtensionIdList result;

  const ExtensionsInfo infos = GetInstalledExtensionsInfo();
  result.reserve(infos.size());
  base::ranges::transform(infos, std::back_inserter(result),
                          [](const auto& info) { return info.extension_id; });

  return result;
}

void ExtensionPrefs::AddObserver(ExtensionPrefsObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ExtensionPrefs::RemoveObserver(ExtensionPrefsObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void ExtensionPrefs::InitPrefStore() {
  TRACE_EVENT0("browser,startup", "ExtensionPrefs::InitPrefStore");

  // When this is called, the PrefService is initialized and provides access
  // to the user preferences stored in a JSON file.
  ExtensionsInfo extensions_info =
      GetInstalledExtensionsInfo(/*include_component_extensions=*/true);

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
          info.extension_manifest
              ? Manifest::GetTypeFromManifestValue(*info.extension_manifest)
              : Manifest::TYPE_UNKNOWN;
      bool is_theme = type == Manifest::TYPE_THEME;
      // Erase the entry if the extension won't be loaded.
      return !Manifest::ShouldAlwaysLoadExtension(info.extension_location,
                                                  is_theme);
    };
    std::erase_if(extensions_info, predicate);
  }

  InitExtensionControlledPrefs(extensions_info);

  extension_pref_value_map_->NotifyInitializationCompleted();
}

bool ExtensionPrefs::HasIncognitoPrefValue(const std::string& pref_key) const {
  bool has_incognito_pref_value = false;
  extension_pref_value_map_->GetEffectivePrefValue(pref_key, true,
                                                   &has_incognito_pref_value);
  return has_incognito_pref_value;
}

const base::Value::Dict* ExtensionPrefs::GetGeometryCache(
    const ExtensionId& extension_id) const {
  const base::Value::Dict* extension_prefs = GetExtensionPref(extension_id);
  if (!extension_prefs)
    return nullptr;

  return extension_prefs->FindDict(kPrefGeometryCache);
}

void ExtensionPrefs::SetGeometryCache(const ExtensionId& extension_id,
                                      base::Value::Dict cache) {
  UpdateExtensionPref(extension_id, kPrefGeometryCache,
                      base::Value(std::move(cache)));
}

const base::Value::Dict& ExtensionPrefs::GetInstallSignature() const {
  return prefs_->GetDict(kInstallSignature);
}

void ExtensionPrefs::SetInstallSignature(base::Value::Dict* signature) {
  if (signature) {
    prefs_->Set(kInstallSignature, base::Value(std::move(*signature)));
    DVLOG(1) << "SetInstallSignature - saving";
  } else {
    DVLOG(1) << "SetInstallSignature - clearing";
    prefs_->ClearPref(kInstallSignature);
  }
}

bool ExtensionPrefs::NeedsSync(const ExtensionId& extension_id) const {
  return ReadPrefAsBooleanAndReturn(extension_id, kPrefNeedsSync);
}

void ExtensionPrefs::SetNeedsSync(const ExtensionId& extension_id,
                                  bool needs_sync) {
  std::optional<base::Value> value;
  if (needs_sync) {
    value = base::Value(true);
  }
  UpdateExtensionPref(extension_id, kPrefNeedsSync, std::move(value));
}

// static
void ExtensionPrefs::SetRunAlertsInFirstRunForTest() {
  g_run_alerts_in_first_run_for_testing = true;
}

const char ExtensionPrefs::kFakeObsoletePrefForTesting[] =
    "__fake_obsolete_pref_for_testing";

// Stores preferences corresponding to static indexed rulesets for the
// Declarative Net Request API.
//
// TODO(blee@igalia.com) Need to move all the DNR related codes to the helper.
//                       (DeclarativeNetRequestPrefsHelper)
const char ExtensionPrefs::kDNRStaticRulesetPref[] = "dnr_static_ruleset";

// static
std::string ExtensionPrefs::JoinPrefs(
    const std::vector<std::string_view>& parts) {
  return base::JoinString(parts, ".");
}

ExtensionPrefs::ExtensionPrefs(
    content::BrowserContext* browser_context,
    PrefService* prefs,
    const base::FilePath& root_dir,
    ExtensionPrefValueMap* extension_pref_value_map,
    base::Clock* clock,
    bool extensions_disabled,
    const std::vector<EarlyExtensionPrefsObserver*>& early_observers)
    : browser_context_(browser_context),
      prefs_(prefs),
      install_directory_(root_dir),
      extension_pref_value_map_(extension_pref_value_map),
      clock_(clock),
      extensions_disabled_(extensions_disabled) {
  MakePathsRelative();

  // Ensure that any early observers are watching before prefs are initialized.
  for (auto* observer : early_observers)
    observer->OnExtensionPrefsAvailable(this);

  InitPrefStore();

  BackfillAndMigrateInstallTimePrefs();

  MigrateToNewWithholdingPref();

  MigrateToNewExternalUninstallPref();

  MigrateDeprecatedDisableReasons();
}

AppSorting* ExtensionPrefs::app_sorting() const {
  return ExtensionSystem::Get(browser_context_)->app_sorting();
}

bool ExtensionPrefs::NeedsStorageGarbageCollection() const {
  return prefs_->GetBoolean(pref_names::kStorageGarbageCollect);
}

// static
void ExtensionPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(pref_names::kExtensions);
  registry->RegisterListPref(pref_names::kPinnedExtensions,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(pref_names::kDeletedComponentExtensions);
  registry->RegisterDictionaryPref(kExtensionsBlocklistUpdate);
  registry->RegisterListPref(pref_names::kInstallAllowList);
  registry->RegisterListPref(pref_names::kInstallDenyList);
  registry->RegisterDictionaryPref(pref_names::kInstallForceList);
  registry->RegisterDictionaryPref(pref_names::kOAuthRedirectUrls);
  registry->RegisterListPref(pref_names::kAllowedTypes);
  registry->RegisterIntegerPref(pref_names::kManifestV2Availability, 0);
  registry->RegisterBooleanPref(pref_names::kStorageGarbageCollect, false);
  registry->RegisterListPref(pref_names::kAllowedInstallSites);
  registry->RegisterStringPref(pref_names::kLastChromeVersion, std::string());
  registry->RegisterDictionaryPref(kInstallSignature);
  registry->RegisterListPref(kExternalUninstalls);
  registry->RegisterListPref(
      pref_names::kExtendedBackgroundLifetimeForPortConnectionsToUrls);

  registry->RegisterListPref(pref_names::kNativeMessagingBlocklist);
  registry->RegisterListPref(pref_names::kNativeMessagingAllowlist);
  registry->RegisterBooleanPref(pref_names::kNativeMessagingUserLevelHosts,
                                true);
  // TODO(archanasimha): move pref registration to where the variable is
  // defined.
  registry->RegisterIntegerPref(kCorruptedDisableCount.name, 0);

#if BUILDFLAG(ENABLE_SUPERVISED_USERS) && BUILDFLAG(ENABLE_EXTENSIONS)
  registry->RegisterBooleanPref(
      prefs::kSupervisedUserExtensionsMayRequestPermissions, false);
  registry->RegisterBooleanPref(prefs::kSkipParentApprovalToInstallExtensions,
                                false);
  registry->RegisterDictionaryPref(
      prefs::kSupervisedUserApprovedExtensions,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kSupervisedUserLocallyParentApprovedExtensions);
#endif  // #if BUILDFLAG(ENABLE_SUPERVISED_USERS) &&
        // BUILDFLAG(ENABLE_EXTENSIONS)

#if !BUILDFLAG(IS_MAC)
  registry->RegisterBooleanPref(pref_names::kAppFullscreenAllowed, true);
#endif

  registry->RegisterBooleanPref(pref_names::kBlockExternalExtensions, false);
  registry->RegisterIntegerPref(pref_names::kExtensionUnpublishedAvailability,
                                0);
  registry->RegisterListPref(pref_names::kExtensionInstallTypeBlocklist);
  registry->RegisterBooleanPref(
      kMV2DeprecationWarningAcknowledgedGloballyPref.name, false);
  registry->RegisterBooleanPref(
      kMV2DeprecationDisabledAcknowledgedGloballyPref.name, false);
  registry->RegisterBooleanPref(
      kMV2DeprecationUnsupportedAcknowledgedGloballyPref.name, false);
}

template <class ExtensionIdContainer>
bool ExtensionPrefs::GetUserExtensionPrefIntoContainer(
    const char* pref,
    ExtensionIdContainer* id_container_out) const {
  DCHECK(id_container_out->empty());

  const base::Value* user_pref_value = prefs_->GetUserPrefValue(pref);
  if (!user_pref_value || !user_pref_value->is_list())
    return false;

  std::insert_iterator<ExtensionIdContainer> insert_iterator(
      *id_container_out, id_container_out->end());
  for (const auto& entry : user_pref_value->GetList()) {
    if (!entry.is_string()) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }
    insert_iterator = entry.GetString();
  }
  return true;
}

template <class ExtensionIdContainer>
void ExtensionPrefs::SetExtensionPrefFromContainer(
    const char* pref,
    const ExtensionIdContainer& strings) {
  ScopedListPrefUpdate update(prefs_, pref);
  base::Value::List& list_of_values = update.Get();
  list_of_values.clear();
  for (auto iter = strings.cbegin(); iter != strings.cend(); ++iter) {
    list_of_values.Append(*iter);
  }
}

void ExtensionPrefs::PopulateExtensionInfoPrefs(
    const Extension* extension,
    const base::Time install_time,
    Extension::State initial_state,
    int install_flags,
    const std::string& install_parameter,
    base::Value::Dict ruleset_install_prefs,
    prefs::DictionaryValueUpdate* extension_dict,
    base::Value::List& removed_prefs) {
  extension_dict->SetInteger(kPrefState, initial_state);
  extension_dict->SetInteger(kPrefLocation,
                             static_cast<int>(extension->location()));
  extension_dict->SetInteger(kPrefCreationFlags, extension->creation_flags());
  extension_dict->SetBoolean(kPrefFromWebStore, extension->from_webstore());
  extension_dict->SetBoolean(kPrefWasInstalledByDefault,
                             extension->was_installed_by_default());
  extension_dict->SetBoolean(kPrefWasInstalledByOem,
                             extension->was_installed_by_oem());

  std::string install_time_str = base::NumberToString(
      install_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  // Don't overwrite any existing first_install_time pref value so that we
  // preserve the original install time.
  if (!extension_dict->HasKey(kPrefFirstInstallTime))
    extension_dict->SetString(kPrefFirstInstallTime, install_time_str);
  extension_dict->SetString(kPrefLastUpdateTime, install_time_str);
  if (install_flags & kInstallFlagIsBlocklistedForMalware) {
    // Don't reset the acknowledged state during an update, because we wouldn't
    // want to reset the acknowledged state if the extension was already on the
    // blocklist.
    blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
        extension->id(), BitMapBlocklistState::BLOCKLISTED_MALWARE, this);
  }

  // If |ruleset_install_prefs| is empty, explicitly remove
  // the |kDNRStaticRulesetPref| entry to ensure any remaining old entries from
  // the previous install are cleared up in case of an update. Else just set the
  // entry (which will overwrite any existing value).
  if (ruleset_install_prefs.empty()) {
    removed_prefs.Append(kDNRStaticRulesetPref);
  } else {
    extension_dict->SetDictionary(kDNRStaticRulesetPref,
                                  std::move(ruleset_install_prefs));
  }

  // Clear the list of enabled static rulesets for the extension since it
  // shouldn't persist across extension updates.
  removed_prefs.Append(kDNREnabledStaticRulesetIDs);

  if (util::CanWithholdPermissionsFromExtension(*extension)) {
    // If the withhold permission creation flag is present it takes precedence
    // over any previous stored value.
    if (extension->creation_flags() & Extension::WITHHOLD_PERMISSIONS) {
      extension_dict->SetBoolean(kPrefWithholdingPermissions, true);
    } else if (!HasWithholdingPermissionsSetting(extension->id())) {
      // If no withholding creation flag was specified and there is no value
      // stored already, we set the default value.
      extension_dict->SetBoolean(kPrefWithholdingPermissions,
                                 kDefaultWithholdingBehavior);
    }
  }

  std::string path = MakePathRelative(install_directory_, extension->path());
  extension_dict->SetString(kPrefPath, path);
  if (!install_parameter.empty()) {
    extension_dict->SetString(kPrefInstallParam, install_parameter);
  }
  // We store prefs about LOAD extensions, but don't cache their manifest
  // since it may change on disk.
  if (!Manifest::IsUnpackedLocation(extension->location())) {
    extension_dict->SetKey(
        kPrefManifest, base::Value(extension->manifest()->value()->Clone()));
  }

  // Only writes kPrefDoNotSync when it is not the default.
  if (install_flags & kInstallFlagDoNotSync)
    extension_dict->SetBoolean(kPrefDoNotSync, true);
  else
    removed_prefs.Append(kPrefDoNotSync);
}

void ExtensionPrefs::InitExtensionControlledPrefs(
    const ExtensionsInfo& extensions_info) {
  TRACE_EVENT0("browser,startup",
               "ExtensionPrefs::InitExtensionControlledPrefs");

  for (const auto& info : extensions_info) {
    const ExtensionId& extension_id = info.extension_id;

    base::Time install_time = GetLastUpdateTime(extension_id);
    bool is_enabled = !IsExtensionDisabled(extension_id);
    bool is_incognito_enabled = IsIncognitoEnabled(extension_id);
    extension_pref_value_map_->RegisterExtension(
        extension_id, install_time, is_enabled, is_incognito_enabled);

    for (auto& observer : observer_list_)
      observer.OnExtensionRegistered(extension_id, install_time, is_enabled);

    // Set regular extension controlled prefs.
    LoadExtensionControlledPrefs(extension_id, ChromeSettingScope::kRegular);
    // Set incognito extension controlled prefs.
    LoadExtensionControlledPrefs(extension_id,
                                 ChromeSettingScope::kIncognitoPersistent);
    // Set regular-only extension controlled prefs.
    LoadExtensionControlledPrefs(extension_id,
                                 ChromeSettingScope::kRegularOnly);

    for (auto& observer : observer_list_)
      observer.OnExtensionPrefsLoaded(extension_id, this);
  }
}

void ExtensionPrefs::LoadExtensionControlledPrefs(
    const ExtensionId& extension_id,
    ChromeSettingScope scope) {
  std::string scope_string;
  if (!pref_names::ScopeToPrefName(scope, &scope_string))
    return;
  std::string key = extension_id + "." + scope_string;

  const base::Value::Dict& source_dict =
      pref_service()->GetDict(pref_names::kExtensions);

  const base::Value::Dict* preferences = source_dict.FindDictByDottedPath(key);
  if (!preferences)
    return;

  for (auto pair : *preferences) {
    extension_pref_value_map_->SetExtensionPref(extension_id, pair.first, scope,
                                                pair.second.Clone());
  }
}

void ExtensionPrefs::FinishExtensionInfoPrefs(
    const ExtensionId& extension_id,
    const base::Time install_time,
    bool needs_sort_ordinal,
    const syncer::StringOrdinal& suggested_page_ordinal,
    prefs::DictionaryValueUpdate* extension_dict) {
  // Reinitializes various preferences with empty dictionaries.
  if (!extension_dict->HasKey(pref_names::kPrefPreferences)) {
    extension_dict->Set(pref_names::kPrefPreferences,
                        base::Value(base::Value::Type::DICT));
  }

  if (!extension_dict->HasKey(pref_names::kPrefIncognitoPreferences)) {
    extension_dict->Set(pref_names::kPrefIncognitoPreferences,
                        base::Value(base::Value::Type::DICT));
  }

  if (!extension_dict->HasKey(pref_names::kPrefRegularOnlyPreferences)) {
    extension_dict->Set(pref_names::kPrefRegularOnlyPreferences,
                        base::Value(base::Value::Type::DICT));
  }

  if (!extension_dict->HasKey(pref_names::kPrefContentSettings))
    extension_dict->Set(pref_names::kPrefContentSettings,
                        base::Value(base::Value::Type::LIST));

  if (!extension_dict->HasKey(pref_names::kPrefIncognitoContentSettings)) {
    extension_dict->Set(pref_names::kPrefIncognitoContentSettings,
                        base::Value(base::Value::Type::LIST));
  }

  // If this point has been reached, any pending installs should be considered
  // out of date.
  extension_dict->Remove(kDelayedInstallInfo);

  // Clear state that may be registered from a previous install.
  extension_dict->Remove(EventRouter::kRegisteredLazyEvents);
  extension_dict->Remove(EventRouter::kRegisteredServiceWorkerEvents);

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

void ExtensionPrefs::BackfillAndMigrateInstallTimePrefs() {
  // Get information for for all extensions including component extensions
  // since the install time pref is saved for them too.
  const ExtensionsInfo extensions_info =
      GetInstalledExtensionsInfo(/*include_component_extensions=*/true);

  for (const auto& info : extensions_info) {
    ScopedExtensionPrefUpdate update(prefs_, info.extension_id);
    auto ext_dict = update.Get();
    if (ext_dict->HasKey(kPrefDeprecatedInstallTime)) {
      std::string install_time_string;
      ext_dict->GetString(kPrefDeprecatedInstallTime, &install_time_string);
      // Populate the new 'last_update_time' pref.
      ext_dict->SetString(kPrefLastUpdateTime, install_time_string);
      // Backfill the 'first_install_time' pref with the existing install time.
      ext_dict->SetString(kPrefFirstInstallTime, install_time_string);
      // Remove the deprecated 'install_time' pref.
      ext_dict->Remove(kPrefDeprecatedInstallTime);
    }
  }
}

void ExtensionPrefs::MigrateDeprecatedDisableReasons() {
  const ExtensionsInfo extensions_info = GetInstalledExtensionsInfo();

  for (const auto& info : extensions_info) {
    const ExtensionId& extension_id = info.extension_id;
    int disable_reasons = GetDisableReasons(extension_id);
    if ((disable_reasons &
         disable_reason::DEPRECATED_DISABLE_UNKNOWN_FROM_SYNC) == 0)
      continue;
    disable_reasons &= ~disable_reason::DEPRECATED_DISABLE_UNKNOWN_FROM_SYNC;
    if (disable_reasons == 0) {
      // We don't know exactly why the extension was disabled, but we don't
      // want to just suddenly re-enable it. Default to disabling it by the
      // user (which was most likely for coming in from sync, and is
      // reversible).
      disable_reasons = disable_reason::DISABLE_USER_ACTION;
    }
    ReplaceDisableReasons(extension_id, disable_reasons);
  }
}

void ExtensionPrefs::MigrateObsoleteExtensionPrefs() {
  const base::Value::Dict& extensions_dictionary =
      prefs_->GetDict(pref_names::kExtensions);

  // Please clean this list up periodically, removing any entries added more
  // than a year ago (with the exception of the testing key).
  constexpr const char* kObsoleteKeys[] = {
      // Permanent testing-only key.
      kFakeObsoletePrefForTesting,

      // Added 2023-11.
      "ack_proxy_bubble",
      "ack_wiped",
  };

  for (auto key_value : extensions_dictionary) {
    if (!crx_file::id_util::IdIsValid(key_value.first))
      continue;
    ScopedExtensionPrefUpdate update(prefs_, key_value.first);
    std::unique_ptr<prefs::DictionaryValueUpdate> inner_update = update.Get();

    for (const char* key : kObsoleteKeys)
      inner_update->Remove(key);
  }
}

void ExtensionPrefs::MigrateToNewWithholdingPref() {
  const ExtensionsInfo extensions_info = GetInstalledExtensionsInfo();

  for (const auto& info : extensions_info) {
    const ExtensionId& extension_id = info.extension_id;
    // The manifest may be null in some cases, such as unpacked extensions
    // retrieved from the Preference file.
    if (!info.extension_manifest) {
      continue;
    }

    // If the new key is present in the prefs already, we don't need to check
    // further.
    bool value = false;
    if (ReadPrefAsBoolean(extension_id, kPrefWithholdingPermissions, &value)) {
      continue;
    }

    // We only want to migrate extensions we can actually withhold permissions
    // from.
    Manifest::Type type =
        Manifest::GetTypeFromManifestValue(*info.extension_manifest);
    ManifestLocation location = info.extension_location;
    if (!util::CanWithholdPermissionsFromExtension(extension_id, type,
                                                   location))
      continue;

    bool old_pref_value = false;
    // If there was an old preference set, use the same (conceptual) value.
    // Otherwise, use the default setting.
    bool new_pref_value = kDefaultWithholdingBehavior;
    if (ReadPrefAsBoolean(extension_id, kGrantExtensionAllHostPermissions,
                          &old_pref_value)) {
      // We invert the value as the previous pref stored if the extension was
      // granted all the requested permissions, whereas the new pref stores if
      // requested permissions are currently being withheld.
      new_pref_value = !old_pref_value;
    }

    UpdateExtensionPref(extension_id, kPrefWithholdingPermissions,
                        base::Value(new_pref_value));
  }
}

void ExtensionPrefs::MigrateToNewExternalUninstallPref() {
  const base::Value::Dict& extensions =
      prefs_->GetDict(pref_names::kExtensions);

  std::vector<std::string> uninstalled_ids;
  for (auto item : extensions) {
    if (!crx_file::id_util::IdIsValid(item.first) || !item.second.is_dict()) {
      continue;
    }

    std::optional<int> state_value = item.second.GetDict().FindInt(kPrefState);
    if (!state_value ||
        *state_value != Extension::DEPRECATED_EXTERNAL_EXTENSION_UNINSTALLED) {
      continue;
    }
    uninstalled_ids.push_back(item.first);
  }

  if (uninstalled_ids.empty())
    return;

  ScopedListPrefUpdate update(prefs_, kExternalUninstalls);
  base::Value::List& current_ids = update.Get();
  for (const auto& id : uninstalled_ids) {
    auto existing_entry =
        base::ranges::find_if(current_ids, [&id](const base::Value& value) {
          return value.is_string() && value.GetString() == id;
        });
    if (existing_entry == current_ids.end())
      current_ids.Append(id);

    DeleteExtensionPrefs(id);
  }
}

bool ExtensionPrefs::ShouldInstallObsoleteComponentExtension(
    const ExtensionId& extension_id) {
  ScopedListPrefUpdate update(prefs_, pref_names::kDeletedComponentExtensions);
  base::Value::List& current_ids = update.Get();
  auto existing_entry = base::ranges::find_if(
      current_ids, [&extension_id](const base::Value& value) {
        return value.is_string() && value.GetString() == extension_id;
      });
  return (existing_entry == current_ids.end());
}

void ExtensionPrefs::MarkObsoleteComponentExtensionAsRemoved(
    const ExtensionId& extension_id,
    const ManifestLocation location) {
  ScopedListPrefUpdate update(prefs_, pref_names::kDeletedComponentExtensions);
  base::Value::List& current_ids = update.Get();
  auto existing_entry = base::ranges::find_if(
      current_ids, [&extension_id](const base::Value& value) {
        return value.is_string() && value.GetString() == extension_id;
      });
  // This should only be called once per extension.
  DCHECK(existing_entry == current_ids.end());
  current_ids.Append(extension_id);
  OnExtensionUninstalled(extension_id, location, false);
}

}  // namespace extensions
