// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/storage_api.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/storage/session_storage_manager.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/quota_service.h"
#include "extensions/common/api/storage.h"
#include "extensions/common/features/feature_channel.h"

namespace extensions {

// Concrete settings functions

namespace {

constexpr char kSessionStorageManagerKeyName[] =
    "StorageAPI SessionStorageManager";

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
  keys.reserve(dict.DictSize());
  for (const auto& value : dict.DictItems()) {
    keys.push_back(value.first);
  }
  return keys;
}

// Converts a map to a Value::Type::DICTIONARY.
base::Value MapAsValueDict(
    const std::map<std::string, const base::Value*>& values) {
  base::Value dict(base::Value::Type::DICTIONARY);
  for (const auto& value : values)
    dict.SetKey(value.first, value.second->Clone());
  return dict;
}

// Creates quota heuristics for settings modification.
void GetModificationQuotaLimitHeuristics(QuotaLimitHeuristics* heuristics) {
  // See storage.json for the current value of these limits.
  QuotaLimitHeuristic::Config short_limit_config = {
      api::storage::sync::MAX_WRITE_OPERATIONS_PER_MINUTE,
      base::TimeDelta::FromMinutes(1)};
  QuotaLimitHeuristic::Config long_limit_config = {
      api::storage::sync::MAX_WRITE_OPERATIONS_PER_HOUR,
      base::TimeDelta::FromHours(1)};
  heuristics->push_back(std::make_unique<QuotaService::TimedLimit>(
      short_limit_config,
      std::make_unique<QuotaLimitHeuristic::SingletonBucketMapper>(),
      "MAX_WRITE_OPERATIONS_PER_MINUTE"));
  heuristics->push_back(std::make_unique<QuotaService::TimedLimit>(
      long_limit_config,
      std::make_unique<QuotaLimitHeuristic::SingletonBucketMapper>(),
      "MAX_WRITE_OPERATIONS_PER_HOUR"));
}

// Creates the SessionStorageManager if it doesn't exist and returns it.
SessionStorageManager* GetOrCreateSessionStorage(
    content::BrowserContext* context) {
  // Share storage between incognito and on-the-record profiles by using the
  // original context of an incognito window.
  content::BrowserContext* original_context =
      ExtensionsBrowserClient::Get()->GetOriginalContext(context);

  SessionStorageManager* storage = static_cast<SessionStorageManager*>(
      original_context->GetUserData(kSessionStorageManagerKeyName));
  if (storage)
    return storage;

  auto session_manager_ptr = std::make_unique<SessionStorageManager>(
      api::storage::session::QUOTA_BYTES);
  auto* session_manager = session_manager_ptr.get();
  original_context->SetUserData(kSessionStorageManagerKeyName,
                                std::move(session_manager_ptr));
  return session_manager;
}

}  // namespace

// SettingsFunction

SettingsFunction::SettingsFunction() = default;

SettingsFunction::~SettingsFunction() = default;

bool SettingsFunction::ShouldSkipQuotaLimiting() const {
  // Only apply quota if this is for sync storage.
  std::string storage_area_string;
  if (!args_->GetString(0, &storage_area_string)) {
    // This should be EXTENSION_FUNCTION_VALIDATE(false) but there is no way
    // to signify that from this function. It will be caught in Run().
    return false;
  }
  return StorageAreaFromString(storage_area_string) !=
         StorageAreaNamespace::kSync;
}

ExtensionFunction::ResponseAction SettingsFunction::Run() {
  std::string storage_area_string;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &storage_area_string));
  args_->Remove(0, nullptr);
  storage_area_ = StorageAreaFromString(storage_area_string);
  EXTENSION_FUNCTION_VALIDATE(storage_area_ != StorageAreaNamespace::kInvalid);

  // Session is the only storage area that does not use ValueStore, and will
  // return synchronously.
  if (storage_area_ == StorageAreaNamespace::kSession) {
    // TODO(crbug.com/1185226): Get observers to dispatch OnChanged event after
    // creating OnChangedEventSession in the observer.
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

  observers_ = frontend->GetObservers();
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

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->Swap(&result.settings());
  return OneArgument(base::Value::FromUniquePtrValue(std::move(dict)));
}

ExtensionFunction::ResponseValue SettingsFunction::UseWriteResult(
    ValueStore::WriteResult result) {
  TRACE_EVENT2("browser", "SettingsFunction::UseWriteResult", "extension_id",
               extension_id(), "namespace", storage_area_);
  if (!result.status().ok())
    return Error(result.status().message);

  if (!result.changes().empty()) {
    observers_->Notify(FROM_HERE, &SettingsObserver::OnSettingsChanged,
                       extension_id(), storage_area_,
                       ValueStoreChange::ToValue(result.PassChanges()));
  }

  return NoArguments();
}

ExtensionFunction::ResponseValue StorageStorageAreaGetFunction::RunWithStorage(
    ValueStore* storage) {
  TRACE_EVENT1("browser", "StorageStorageAreaGetFunction::RunWithStorage",
               "extension_id", extension_id());
  base::Value* input = nullptr;
  if (!args_->Get(0, &input))
    return BadMessage();

  switch (input->type()) {
    case base::Value::Type::NONE:
      return UseReadResult(storage->Get());

    case base::Value::Type::STRING:
      return UseReadResult(storage->Get(input->GetString()));

    case base::Value::Type::LIST:
      return UseReadResult(storage->Get(GetKeysFromList(*input)));

    case base::Value::Type::DICTIONARY: {
      ValueStore::ReadResult result = storage->Get(GetKeysFromDict(*input));
      if (!result.status().ok()) {
        return UseReadResult(std::move(result));
      }
      std::unique_ptr<base::DictionaryValue> with_default_values =
          static_cast<base::DictionaryValue*>(input)->CreateDeepCopy();
      with_default_values->MergeDictionary(&result.settings());
      return UseReadResult(ValueStore::ReadResult(
          std::move(with_default_values), result.PassStatus()));
    }

    default:
      return BadMessage();
  }
}

ExtensionFunction::ResponseValue StorageStorageAreaGetFunction::RunInSession() {
  base::Value* input = nullptr;
  if (!args_->Get(0, &input))
    return BadMessage();

  base::Value value_dict(base::Value::Type::DICTIONARY);
  SessionStorageManager* session_manager =
      GetOrCreateSessionStorage(browser_context());

  switch (input->type()) {
    case base::Value::Type::NONE:
      value_dict = MapAsValueDict(session_manager->GetAll(extension_id()));
      break;

    case base::Value::Type::STRING:
      value_dict = MapAsValueDict(session_manager->Get(
          extension_id(), std::vector<std::string>(1, input->GetString())));
      break;

    case base::Value::Type::LIST:
      value_dict = MapAsValueDict(
          session_manager->Get(extension_id(), GetKeysFromList(*input)));
      break;

    case base::Value::Type::DICTIONARY: {
      std::map<std::string, const base::Value*> values =
          session_manager->Get(extension_id(), GetKeysFromDict(*input));

      for (auto default_value : input->DictItems()) {
        auto value_it = values.find(default_value.first);
        value_dict.SetKey(default_value.first,
                          value_it != values.end()
                              ? value_it->second->Clone()
                              : std::move(default_value.second));
      }
      break;
    }
    default:
      return BadMessage();
  }

  return OneArgument(std::move(value_dict));
}

ExtensionFunction::ResponseValue
StorageStorageAreaGetBytesInUseFunction::RunWithStorage(ValueStore* storage) {
  TRACE_EVENT1("browser",
               "StorageStorageAreaGetBytesInUseFunction::RunWithStorage",
               "extension_id", extension_id());

  base::Value* input = nullptr;
  if (!args_->Get(0, &input))
    return BadMessage();

  size_t bytes_in_use = 0;

  switch (input->type()) {
    case base::Value::Type::NONE:
      bytes_in_use = storage->GetBytesInUse();
      break;

    case base::Value::Type::STRING:
      bytes_in_use = storage->GetBytesInUse(input->GetString());
      break;

    case base::Value::Type::LIST:
      bytes_in_use = storage->GetBytesInUse(GetKeysFromList(*input));
      break;

    default:
      return BadMessage();
  }

  return OneArgument(base::Value(static_cast<int>(bytes_in_use)));
}

ExtensionFunction::ResponseValue
StorageStorageAreaGetBytesInUseFunction::RunInSession() {
  // TODO(crbug.com/1185226): Implement RunInSession for
  // chrome.storage.session.getBytesInUse .
  return NoArguments();
}

ExtensionFunction::ResponseValue StorageStorageAreaSetFunction::RunWithStorage(
    ValueStore* storage) {
  TRACE_EVENT1("browser", "StorageStorageAreaSetFunction::RunWithStorage",
               "extension_id", extension_id());
  base::DictionaryValue* input = nullptr;
  if (!args_->GetDictionary(0, &input))
    return BadMessage();
  return UseWriteResult(storage->Set(ValueStore::DEFAULTS, *input));
}

ExtensionFunction::ResponseValue StorageStorageAreaSetFunction::RunInSession() {
  // Retrieve and delete input from `args_` since they will be moved to storage.
  auto list = args_->GetList();
  if (list.empty() || !list[0].is_dict())
    return BadMessage();
  base::Value input = std::move(list[0]);
  args_->EraseListIter(list.begin());

  std::map<std::string, base::Value> values;
  for (auto item : input.DictItems()) {
    values.emplace(std::move(item.first), std::move(item.second));
  }

  std::vector<SessionStorageManager::ValueChange> changes;
  bool result = GetOrCreateSessionStorage(browser_context())
                    ->Set(extension_id(), std::move(values), changes);

  if (!result) {
    // TODO(crbug.com/1185226): Add API test that triggers this behavior.
    return Error(
        "Session storage quota bytes exceeded. Values were not stored.");
  }

  if (!changes.empty()) {
    // TODO(crbug.com/1185226): Notify changes after creating
    // OnChangedEventSession in the observer.
  }

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
  base::Value* input = nullptr;
  if (!args_->Get(0, &input))
    return BadMessage();

  switch (input->type()) {
    case base::Value::Type::STRING:
      return UseWriteResult(storage->Remove(input->GetString()));

    case base::Value::Type::LIST:
      return UseWriteResult(storage->Remove(GetKeysFromList(*input)));

    default:
      return BadMessage();
  }
}

ExtensionFunction::ResponseValue
StorageStorageAreaRemoveFunction::RunInSession() {
  // TODO(crbug.com/1185226): Implement RunInSession for
  // chrome.storage.session.remove .
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
  // TODO(crbug.com/1185226): Implement RunInSession for
  // chrome.storage.session.clear .
  return NoArguments();
}

void StorageStorageAreaClearFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

}  // namespace extensions
