// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/storage_api.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/storage/session_storage_manager.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/quota_service.h"
#include "extensions/common/api/storage.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_channel.h"

using value_store::ValueStore;

namespace extensions {

// Concrete settings functions

namespace {

constexpr PrefMap kPrefSessionStorageAccessLevel = {
    "storage_session_access_level", PrefType::kInteger,
    PrefScope::kExtensionSpecific};

// Returns a vector of any strings within the given list.
std::vector<std::string> GetKeysFromList(const base::Value& list) {
  DCHECK(list.is_list());
  std::vector<std::string> keys;
  keys.reserve(list.GetList().size());
  for (const auto& value : list.GetList()) {
    auto* as_string = value.GetIfString();
    if (as_string)
      keys.push_back(*as_string);
  }
  return keys;
}

// Returns a vector of keys within the given dict.
std::vector<std::string> GetKeysFromDict(const base::Value& dict) {
  DCHECK(dict.is_dict());
  std::vector<std::string> keys;
  keys.reserve(dict.GetDict().size());
  for (auto value : dict.GetDict()) {
    keys.push_back(value.first);
  }
  return keys;
}

// Converts a map to a Value::Dict.
base::Value::Dict MapAsValueDict(
    const std::map<std::string, const base::Value*>& values) {
  base::Value::Dict dict;
  for (const auto& value : values)
    dict.Set(value.first, value.second->Clone());
  return dict;
}

// Creates quota heuristics for settings modification.
void GetModificationQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) {
  // See storage.json for the current value of these limits.
  QuotaLimitHeuristic::Config short_limit_config = {
      api::storage::sync::MAX_WRITE_OPERATIONS_PER_MINUTE, base::Minutes(1)};
  QuotaLimitHeuristic::Config long_limit_config = {
      api::storage::sync::MAX_WRITE_OPERATIONS_PER_HOUR, base::Hours(1)};
  heuristics->push_back(std::make_unique<QuotaService::TimedLimit>(
      short_limit_config,
      std::make_unique<QuotaLimitHeuristic::SingletonBucketMapper>(),
      "MAX_WRITE_OPERATIONS_PER_MINUTE"));
  heuristics->push_back(std::make_unique<QuotaService::TimedLimit>(
      long_limit_config,
      std::make_unique<QuotaLimitHeuristic::SingletonBucketMapper>(),
      "MAX_WRITE_OPERATIONS_PER_HOUR"));
}

// Returns a nested dictionary Value converted from a ValueChange.
base::Value ValueChangeToValue(
    std::vector<SessionStorageManager::ValueChange> changes) {
  base::Value::Dict changes_value;
  for (auto& change : changes) {
    base::Value::Dict change_value;
    if (change.old_value.has_value())
      change_value.Set("oldValue", std::move(change.old_value.value()));
    if (change.new_value)
      change_value.Set("newValue", change.new_value->Clone());
    changes_value.Set(change.key, std::move(change_value));
  }
  return base::Value(std::move(changes_value));
}

}  // namespace

// SettingsFunction

SettingsFunction::SettingsFunction() = default;

SettingsFunction::~SettingsFunction() = default;

bool SettingsFunction::ShouldSkipQuotaLimiting() const {
  // Only apply quota if this is for sync storage.
  if (args().empty() || !args()[0].is_string()) {
    // This should be EXTENSION_FUNCTION_VALIDATE(false) but there is no way
    // to signify that from this function. It will be caught in Run().
    return false;
  }
  const std::string& storage_area_string = args()[0].GetString();
  return StorageAreaFromString(storage_area_string) !=
         StorageAreaNamespace::kSync;
}

ExtensionFunction::ResponseAction SettingsFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());

  // Not a ref since we remove the underlying value after.
  std::string storage_area_string = args()[0].GetString();

  mutable_args().erase(args().begin());
  storage_area_ = StorageAreaFromString(storage_area_string);
  EXTENSION_FUNCTION_VALIDATE(storage_area_ != StorageAreaNamespace::kInvalid);

  // Session is the only storage area that does not use ValueStore, and will
  // return synchronously.
  if (storage_area_ == StorageAreaNamespace::kSession) {
    // Currently only `session` can restrict the storage access. This call will
    // be moved after the other storage areas allow it.
    if (!IsAccessToStorageAllowed()) {
      return RespondNow(
          Error("Access to storage is not allowed from this context."));
    }
    return RespondNow(RunInSession());
  }

  // All other StorageAreas use ValueStore with settings_namespace, and will
  // return asynchronously if successful.
  settings_namespace_ = StorageAreaToSettingsNamespace(storage_area_);
  EXTENSION_FUNCTION_VALIDATE(settings_namespace_ !=
                              settings_namespace::INVALID);

  if (extension()->is_login_screen_extension() &&
      storage_area_ != StorageAreaNamespace::kManaged) {
    // Login screen extensions are not allowed to use local/sync storage for
    // security reasons (see crbug.com/978443).
    return RespondNow(Error(base::StringPrintf(
        "\"%s\" is not available for login screen extensions",
        storage_area_string.c_str())));
  }

  StorageFrontend* frontend = StorageFrontend::Get(browser_context());
  if (!frontend->IsStorageEnabled(settings_namespace_)) {
    return RespondNow(Error(
        base::StringPrintf("\"%s\" is not available in this instance of Chrome",
                           storage_area_string.c_str())));
  }

  observer_ = GetSequenceBoundSettingsChangedCallback(
      base::SequencedTaskRunner::GetCurrentDefault(), frontend->GetObserver());

  frontend->RunWithStorage(
      extension(), settings_namespace_,
      base::BindOnce(&SettingsFunction::AsyncRunWithStorage, this));
  return RespondLater();
}

void SettingsFunction::AsyncRunWithStorage(ValueStore* storage) {
  ResponseValue response = RunWithStorage(storage);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&SettingsFunction::Respond, this, std::move(response)));
}

ExtensionFunction::ResponseValue SettingsFunction::UseReadResult(
    ValueStore::ReadResult result) {
  TRACE_EVENT2("browser", "SettingsFunction::UseReadResult", "extension_id",
               extension_id(), "namespace", storage_area_);
  if (!result.status().ok())
    return Error(result.status().message);

  return OneArgument(base::Value(result.PassSettings()));
}

ExtensionFunction::ResponseValue SettingsFunction::UseWriteResult(
    ValueStore::WriteResult result) {
  TRACE_EVENT2("browser", "SettingsFunction::UseWriteResult", "extension_id",
               extension_id(), "namespace", storage_area_);
  if (!result.status().ok())
    return Error(result.status().message);

  if (!result.changes().empty()) {
    observer_->Run(
        extension_id(), storage_area_,
        value_store::ValueStoreChange::ToValue(result.PassChanges()));
  }

  return NoArguments();
}

void SettingsFunction::OnSessionSettingsChanged(
    std::vector<SessionStorageManager::ValueChange> changes) {
  if (!changes.empty()) {
    SettingsChangedCallback observer =
        StorageFrontend::Get(browser_context())->GetObserver();
    // This used to dispatch asynchronously as a result of a
    // ObserverListThreadSafe. Ideally, we'd just run this synchronously, but it
    // appears at least some tests rely on the asynchronous behavior.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(observer, extension_id(), storage_area_,
                                  ValueChangeToValue(std::move(changes))));
  }
}

bool SettingsFunction::IsAccessToStorageAllowed() {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
  // Default access level is only secure contexts.
  int access_level = api::storage::ACCESS_LEVEL_TRUSTED_CONTEXTS;
  prefs->ReadPrefAsInteger(extension()->id(), kPrefSessionStorageAccessLevel,
                           &access_level);

  // Only a blessed extension context is considered trusted.
  if (access_level == api::storage::ACCESS_LEVEL_TRUSTED_CONTEXTS)
    return source_context_type() == Feature::BLESSED_EXTENSION_CONTEXT;

  // All contexts are allowed.
  DCHECK_EQ(api::storage::ACCESS_LEVEL_TRUSTED_AND_UNTRUSTED_CONTEXTS,
            access_level);
  return true;
}

ExtensionFunction::ResponseValue StorageStorageAreaGetFunction::RunWithStorage(
    ValueStore* storage) {
  TRACE_EVENT1("browser", "StorageStorageAreaGetFunction::RunWithStorage",
               "extension_id", extension_id());
  if (args().empty())
    return BadMessage();
  const base::Value& input = args()[0];

  switch (input.type()) {
    case base::Value::Type::NONE:
      return UseReadResult(storage->Get());

    case base::Value::Type::STRING:
      return UseReadResult(storage->Get(input.GetString()));

    case base::Value::Type::LIST:
      return UseReadResult(storage->Get(GetKeysFromList(input)));

    case base::Value::Type::DICTIONARY: {
      ValueStore::ReadResult result = storage->Get(GetKeysFromDict(input));
      if (!result.status().ok()) {
        return UseReadResult(std::move(result));
      }
      base::Value::Dict with_default_values = input.GetDict().Clone();
      with_default_values.Merge(result.PassSettings());
      return UseReadResult(ValueStore::ReadResult(
          std::move(with_default_values), result.PassStatus()));
    }

    default:
      return BadMessage();
  }
}

ExtensionFunction::ResponseValue StorageStorageAreaGetFunction::RunInSession() {
  if (args().empty())
    return BadMessage();
  base::Value& input = mutable_args()[0];

  base::Value::Dict value_dict;
  SessionStorageManager* session_manager =
      SessionStorageManager::GetForBrowserContext(browser_context());

  switch (input.type()) {
    case base::Value::Type::NONE:
      value_dict = MapAsValueDict(session_manager->GetAll(extension_id()));
      break;

    case base::Value::Type::STRING:
      value_dict = MapAsValueDict(session_manager->Get(
          extension_id(), std::vector<std::string>(1, input.GetString())));
      break;

    case base::Value::Type::LIST:
      value_dict = MapAsValueDict(
          session_manager->Get(extension_id(), GetKeysFromList(input)));
      break;

    case base::Value::Type::DICTIONARY: {
      std::map<std::string, const base::Value*> values =
          session_manager->Get(extension_id(), GetKeysFromDict(input));

      for (auto default_value : input.GetDict()) {
        auto value_it = values.find(default_value.first);
        value_dict.Set(default_value.first,
                       value_it != values.end()
                           ? value_it->second->Clone()
                           : std::move(default_value.second));
      }
      break;
    }
    default:
      return BadMessage();
  }

  return OneArgument(base::Value(std::move(value_dict)));
}

ExtensionFunction::ResponseValue
StorageStorageAreaGetBytesInUseFunction::RunWithStorage(ValueStore* storage) {
  TRACE_EVENT1("browser",
               "StorageStorageAreaGetBytesInUseFunction::RunWithStorage",
               "extension_id", extension_id());

  if (args().empty())
    return BadMessage();
  const base::Value& input = args()[0];

  size_t bytes_in_use = 0;

  switch (input.type()) {
    case base::Value::Type::NONE:
      bytes_in_use = storage->GetBytesInUse();
      break;

    case base::Value::Type::STRING:
      bytes_in_use = storage->GetBytesInUse(input.GetString());
      break;

    case base::Value::Type::LIST:
      bytes_in_use = storage->GetBytesInUse(GetKeysFromList(input));
      break;

    default:
      return BadMessage();
  }

  return OneArgument(base::Value(static_cast<int>(bytes_in_use)));
}

ExtensionFunction::ResponseValue
StorageStorageAreaGetBytesInUseFunction::RunInSession() {
  if (args().empty())
    return BadMessage();
  const base::Value& input = args()[0];

  size_t bytes_in_use = 0;
  SessionStorageManager* session_manager =
      SessionStorageManager::GetForBrowserContext(browser_context());

  switch (input.type()) {
    case base::Value::Type::NONE:
      bytes_in_use = session_manager->GetTotalBytesInUse(extension_id());
      break;

    case base::Value::Type::STRING:
      bytes_in_use = session_manager->GetBytesInUse(
          extension_id(), std::vector<std::string>(1, input.GetString()));
      break;

    case base::Value::Type::LIST:
      bytes_in_use = session_manager->GetBytesInUse(extension_id(),
                                                    GetKeysFromList(input));
      break;

    default:
      return BadMessage();
  }

  // Checked cast should not overflow since `bytes_in_use` is guaranteed to be a
  // small number, due to the quota limits we have in place for in-memory
  // storage
  return OneArgument(base::Value(base::checked_cast<int>(bytes_in_use)));
}

ExtensionFunction::ResponseValue StorageStorageAreaSetFunction::RunWithStorage(
    ValueStore* storage) {
  TRACE_EVENT1("browser", "StorageStorageAreaSetFunction::RunWithStorage",
               "extension_id", extension_id());
  if (args().empty() || !args()[0].is_dict())
    return BadMessage();
  return UseWriteResult(
      storage->Set(ValueStore::DEFAULTS, args()[0].GetDict()));
}

ExtensionFunction::ResponseValue StorageStorageAreaSetFunction::RunInSession() {
  // Retrieve and delete input from `args_` since they will be moved to storage.
  if (args().empty() || !args()[0].is_dict())
    return BadMessage();
  base::Value input = std::move(mutable_args()[0]);
  mutable_args().erase(args().begin());

  std::map<std::string, base::Value> values;
  for (auto item : input.GetDict()) {
    values.emplace(std::move(item.first), std::move(item.second));
  }

  std::vector<SessionStorageManager::ValueChange> changes;
  bool result = SessionStorageManager::GetForBrowserContext(browser_context())
                    ->Set(extension_id(), std::move(values), changes);

  if (!result) {
    // TODO(crbug.com/1185226): Add API test that triggers this behavior.
    return Error(
        "Session storage quota bytes exceeded. Values were not stored.");
  }

  OnSessionSettingsChanged(std::move(changes));
  return NoArguments();
}

void StorageStorageAreaSetFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

ExtensionFunction::ResponseValue
StorageStorageAreaRemoveFunction::RunWithStorage(ValueStore* storage) {
  TRACE_EVENT1("browser", "StorageStorageAreaRemoveFunction::RunWithStorage",
               "extension_id", extension_id());
  if (args().empty())
    return BadMessage();
  const base::Value& input = args()[0];

  switch (input.type()) {
    case base::Value::Type::STRING:
      return UseWriteResult(storage->Remove(input.GetString()));

    case base::Value::Type::LIST:
      return UseWriteResult(storage->Remove(GetKeysFromList(input)));

    default:
      return BadMessage();
  }
}

ExtensionFunction::ResponseValue
StorageStorageAreaRemoveFunction::RunInSession() {
  if (args().empty())
    return BadMessage();
  const base::Value& input = args()[0];

  SessionStorageManager* session_manager =
      SessionStorageManager::GetForBrowserContext(browser_context());
  std::vector<SessionStorageManager::ValueChange> changes;

  switch (input.type()) {
    case base::Value::Type::STRING:
      session_manager->Remove(extension_id(), input.GetString(), changes);
      break;

    case base::Value::Type::LIST:
      session_manager->Remove(extension_id(), GetKeysFromList(input), changes);
      break;

    default:
      return BadMessage();
  }

  OnSessionSettingsChanged(std::move(changes));
  return NoArguments();
}

void StorageStorageAreaRemoveFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

ExtensionFunction::ResponseValue
StorageStorageAreaClearFunction::RunWithStorage(ValueStore* storage) {
  TRACE_EVENT1("browser", "StorageStorageAreaClearFunction::RunWithStorage",
               "extension_id", extension_id());
  return UseWriteResult(storage->Clear());
}

ExtensionFunction::ResponseValue
StorageStorageAreaClearFunction::RunInSession() {
  std::vector<SessionStorageManager::ValueChange> changes;
  SessionStorageManager::GetForBrowserContext(browser_context())
      ->Clear(extension_id(), changes);

  OnSessionSettingsChanged(std::move(changes));
  return NoArguments();
}

void StorageStorageAreaClearFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

ExtensionFunction::ResponseValue
StorageStorageAreaSetAccessLevelFunction::RunWithStorage(ValueStore* storage) {
  // Not supported. Should return error.
  return Error("This StorageArea is not available for setting access level");
}

ExtensionFunction::ResponseValue
StorageStorageAreaSetAccessLevelFunction::RunInSession() {
  if (source_context_type() != Feature::BLESSED_EXTENSION_CONTEXT)
    return Error("Context cannot set the storage access level");

  std::unique_ptr<api::storage::StorageArea::SetAccessLevel::Params> params(
      api::storage::StorageArea::SetAccessLevel::Params::Create(args()));

  if (!params)
    return BadMessage();

  // The parsing code ensures `access_level` is sane.
  DCHECK(params->access_options.access_level ==
             api::storage::ACCESS_LEVEL_TRUSTED_CONTEXTS ||
         params->access_options.access_level ==
             api::storage::ACCESS_LEVEL_TRUSTED_AND_UNTRUSTED_CONTEXTS);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
  prefs->SetIntegerPref(extension_id(), kPrefSessionStorageAccessLevel,
                        params->access_options.access_level);

  return NoArguments();
}

}  // namespace extensions
