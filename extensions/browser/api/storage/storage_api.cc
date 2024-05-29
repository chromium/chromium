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
#include "extensions/browser/api/storage/storage_utils.h"
#include "extensions/browser/quota_service.h"
#include "extensions/common/api/storage.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/mojom/context_type.mojom.h"

using value_store::ValueStore;

namespace extensions {

// Concrete settings functions

namespace {

// Returns a vector of any strings within the given list.
std::vector<std::string> GetKeysFromList(const base::Value::List& list) {
  std::vector<std::string> keys;
  keys.reserve(list.size());
  for (const auto& value : list) {
    auto* as_string = value.GetIfString();
    if (as_string) {
      keys.push_back(*as_string);
    }
  }
  return keys;
}

// Returns a vector of keys within the given dict.
std::vector<std::string> GetKeysFromDict(const base::Value::Dict& dict) {
  std::vector<std::string> keys;
  keys.reserve(dict.size());
  for (auto value : dict) {
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

bool SettingsFunction::PreRunValidation(std::string* error) {
  if (!ExtensionFunction::PreRunValidation(error)) {
    return false;
  }

  EXTENSION_FUNCTION_PRERUN_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_PRERUN_VALIDATE(args()[0].is_string());

  // Not a ref since we remove the underlying value after.
  std::string storage_area_string = args()[0].GetString();

  mutable_args().erase(args().begin());
  storage_area_ = StorageAreaFromString(storage_area_string);
  EXTENSION_FUNCTION_PRERUN_VALIDATE(storage_area_ !=
                                     StorageAreaNamespace::kInvalid);

  // Session is the only storage area that does not use ValueStore, and will
  // return synchronously.
  if (storage_area_ == StorageAreaNamespace::kSession) {
    // Currently only `session` can restrict the storage access. This call will
    // be moved after the other storage areas allow it.
    if (!IsAccessToStorageAllowed()) {
      *error = "Access to storage is not allowed from this context.";
      return false;
    }
    return true;
  }

  // All other StorageAreas use ValueStore with settings_namespace, and will
  // return asynchronously if successful.
  settings_namespace_ = StorageAreaToSettingsNamespace(storage_area_);
  EXTENSION_FUNCTION_PRERUN_VALIDATE(settings_namespace_ !=
                                     settings_namespace::INVALID);

  if (extension()->is_login_screen_extension() &&
      storage_area_ != StorageAreaNamespace::kManaged) {
    // Login screen extensions are not allowed to use local/sync storage for
    // security reasons (see crbug.com/978443).
    *error = base::StringPrintf(
        "\"%s\" is not available for login screen extensions",
        storage_area_string.c_str());
    return false;
  }

  StorageFrontend* frontend = StorageFrontend::Get(browser_context());
  if (!frontend->IsStorageEnabled(settings_namespace_)) {
    *error =
        base::StringPrintf("\"%s\" is not available in this instance of Chrome",
                           storage_area_string.c_str());
    return false;
  }

  return true;
}

ExtensionFunction::ResponseAction SettingsFunction::Run() {
  if (storage_area_ == StorageAreaNamespace::kSession) {
    return RespondNow(RunInSession());
  }

  StorageFrontend* frontend = StorageFrontend::Get(browser_context());

  observer_ = GetSequenceBoundSettingsChangedCallback(
      base::SequencedTaskRunner::GetCurrentDefault(), frontend->GetObserver());

  frontend->RunWithStorage(
      extension(), settings_namespace_,
      base::BindOnce(&SettingsFunction::AsyncRunWithStorage, this));
  return RespondLater();
}

ExtensionFunction::ResponseValue SettingsFunction::RunWithStorage(
    ValueStore* storage) {
  // TODO(crbug.com/40963428): Remove this when RunWithStorage has been removed
  // from all functions.
  NOTREACHED_IN_MIGRATION();
  return BadMessage();
}

ExtensionFunction::ResponseValue SettingsFunction::RunInSession() {
  // TODO(crbug.com/40963428): Remove this when RunWithStorage has been removed
  // from all functions.
  NOTREACHED_IN_MIGRATION();
  return BadMessage();
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

  return WithArguments(result.PassSettings());
}

ExtensionFunction::ResponseValue SettingsFunction::UseWriteResult(
    ValueStore::WriteResult result) {
  TRACE_EVENT2("browser", "SettingsFunction::UseWriteResult", "extension_id",
               extension_id(), "namespace", storage_area_);
  if (!result.status().ok())
    return Error(result.status().message);

  if (!result.changes().empty()) {
    observer_->Run(
        extension_id(), storage_area_, /*session_access_level=*/std::nullopt,
        value_store::ValueStoreChange::ToValue(result.PassChanges()));
  }

  return NoArguments();
}

void SettingsFunction::OnSessionSettingsChanged(
    std::vector<SessionStorageManager::ValueChange> changes) {
  if (!changes.empty()) {
    SettingsChangedCallback observer =
        StorageFrontend::Get(browser_context())->GetObserver();
    api::storage::AccessLevel access_level =
        storage_utils::GetSessionAccessLevel(extension()->id(),
                                             *browser_context());
    // This used to dispatch asynchronously as a result of a
    // ObserverListThreadSafe. Ideally, we'd just run this synchronously, but it
    // appears at least some tests rely on the asynchronous behavior.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(observer, extension_id(), storage_area_, access_level,
                       storage_utils::ValueChangeToValue(std::move(changes))));
  }
}

bool SettingsFunction::IsAccessToStorageAllowed() {
  api::storage::AccessLevel access_level = storage_utils::GetSessionAccessLevel(
      extension()->id(), *browser_context());

  // Only a privileged extension context is considered trusted.
  if (access_level == api::storage::AccessLevel::kTrustedContexts) {
    return source_context_type() == mojom::ContextType::kPrivilegedExtension;
  }

  // All contexts are allowed.
  DCHECK_EQ(api::storage::AccessLevel::kTrustedAndUntrustedContexts,
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
      return UseReadResult(storage->Get(GetKeysFromList(input.GetList())));

    case base::Value::Type::DICT: {
      ValueStore::ReadResult result =
          storage->Get(GetKeysFromDict(input.GetDict()));
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
      value_dict = MapAsValueDict(session_manager->Get(
          extension_id(), GetKeysFromList(input.GetList())));
      break;

    case base::Value::Type::DICT: {
      std::map<std::string, const base::Value*> values = session_manager->Get(
          extension_id(), GetKeysFromDict(input.GetDict()));

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

  return WithArguments(std::move(value_dict));
}

ExtensionFunction::ResponseAction
StorageStorageAreaGetBytesInUseFunction::Run() {
  if (args().empty()) {
    return RespondNow(BadMessage());
  }

  const base::Value& input = args()[0];
  std::optional<std::vector<std::string>> keys;

  switch (input.type()) {
    case base::Value::Type::NONE:
      keys = std::nullopt;
      break;

    case base::Value::Type::STRING:
      keys = std::optional(std::vector<std::string>(1, input.GetString()));
      break;

    case base::Value::Type::LIST:
      keys = std::optional(GetKeysFromList(input.GetList()));
      break;

    default:
      return RespondNow(BadMessage());
  }

  StorageFrontend* frontend = StorageFrontend::Get(browser_context());
  frontend->GetBytesInUse(
      extension(), storage_area(), keys,
      base::BindOnce(&StorageStorageAreaGetBytesInUseFunction::
                         OnGetBytesInUseOperationFinished,
                     this));

  return RespondLater();
}

void StorageStorageAreaGetBytesInUseFunction::OnGetBytesInUseOperationFinished(
    size_t bytes_in_use) {
  // Since the storage access happens asynchronously, the browser context can
  // be torn down in the interim. If this happens, early-out.
  if (!browser_context()) {
    return;
  }

  // Checked cast should not overflow since a double can represent up to 2*53
  // bytes before a loss of precision.
  Respond(WithArguments(base::checked_cast<double>(bytes_in_use)));
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
    // TODO(crbug.com/40171997): Add API test that triggers this behavior.
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
      return UseWriteResult(storage->Remove(GetKeysFromList(input.GetList())));

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
      session_manager->Remove(extension_id(), GetKeysFromList(input.GetList()),
                              changes);
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

ExtensionFunction::ResponseAction StorageStorageAreaClearFunction::Run() {
  StorageFrontend* frontend = StorageFrontend::Get(browser_context());
  frontend->Clear(
      extension(), storage_area(),
      base::BindOnce(&StorageStorageAreaClearFunction::OnClearOperationFinished,
                     this));

  return RespondLater();
}

void StorageStorageAreaClearFunction::OnClearOperationFinished(
    StorageFrontend::ResultStatus status) {
  // Since the storage access happens asynchronously, the browser context can
  // be torn down in the interim. If this happens, early-out.
  if (!browser_context()) {
    return;
  }

  if (!status.success) {
    CHECK(status.error.has_value());
    Respond(Error(*status.error));
    return;
  }

  Respond(NoArguments());
}

void StorageStorageAreaClearFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

ExtensionFunction::ResponseValue
StorageStorageAreaSetAccessLevelFunction::RunWithStorage(ValueStore* storage) {
  // TODO(crbug.com/40949182). Support these storage areas. For now, we return
  // an error.
  return Error("This StorageArea is not available for setting access level");
}

ExtensionFunction::ResponseValue
StorageStorageAreaSetAccessLevelFunction::RunInSession() {
  if (source_context_type() != mojom::ContextType::kPrivilegedExtension) {
    return Error("Context cannot set the storage access level");
  }

  std::optional<api::storage::StorageArea::SetAccessLevel::Params> params =
      api::storage::StorageArea::SetAccessLevel::Params::Create(args());

  if (!params)
    return BadMessage();

  // The parsing code ensures `access_level` is sane.
  DCHECK(params->access_options.access_level ==
             api::storage::AccessLevel::kTrustedContexts ||
         params->access_options.access_level ==
             api::storage::AccessLevel::kTrustedAndUntrustedContexts);

  storage_utils::SetSessionAccessLevel(extension_id(), *browser_context(),
                                       params->access_options.access_level);

  return NoArguments();
}

}  // namespace extensions
