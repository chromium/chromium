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
#include "base/task/post_task.h"
#include "base/values.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/quota_service.h"
#include "extensions/common/api/storage.h"

namespace extensions {

using content::BrowserThread;

// SettingsFunction

SettingsFunction::SettingsFunction()
    : settings_namespace_(settings_namespace::INVALID) {}

SettingsFunction::~SettingsFunction() {}

bool SettingsFunction::ShouldSkipQuotaLimiting() const {
  // Only apply quota if this is for sync storage.
  std::string settings_namespace_string;
  if (!args_->GetString(0, &settings_namespace_string)) {
    // This should be EXTENSION_FUNCTION_VALIDATE(false) but there is no way
    // to signify that from this function. It will be caught in Run().
    return false;
  }
  return settings_namespace_string != "sync";
}

ExtensionFunction::ResponseAction SettingsFunction::Run() {
  std::string settings_namespace_string;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &settings_namespace_string));
  args_->Remove(0, NULL);
  settings_namespace_ =
      settings_namespace::FromString(settings_namespace_string);
  EXTENSION_FUNCTION_VALIDATE(settings_namespace_ !=
                              settings_namespace::INVALID);

  if (extension()->is_login_screen_extension() &&
      settings_namespace_ != settings_namespace::MANAGED) {
    // Login screen extensions are not allowed to use local/sync storage for
    // security reasons (see crbug.com/978443).
    return RespondNow(Error(base::StringPrintf(
        "\"%s\" is not available for login screen extensions",
        settings_namespace_string.c_str())));
  }

  StorageFrontend* frontend = StorageFrontend::Get(browser_context());
  if (!frontend->IsStorageEnabled(settings_namespace_)) {
    return RespondNow(Error(
        base::StringPrintf("\"%s\" is not available in this instance of Chrome",
                           settings_namespace_string.c_str())));
  }

  observers_ = frontend->GetObservers();
  frontend->RunWithStorage(
      extension(),
      settings_namespace_,
      base::Bind(&SettingsFunction::AsyncRunWithStorage, this));
  return RespondLater();
}

void SettingsFunction::AsyncRunWithStorage(ValueStore* storage) {
  ResponseValue response = RunWithStorage(storage);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&SettingsFunction::Respond, this, std::move(response)));
}

ExtensionFunction::ResponseValue SettingsFunction::UseReadResult(
    ValueStore::ReadResult result) {
  if (!result.status().ok())
    return Error(result.status().message);

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->Swap(&result.settings());
  return OneArgument(std::move(dict));
}

ExtensionFunction::ResponseValue SettingsFunction::UseWriteResult(
    ValueStore::WriteResult result) {
  if (!result.status().ok())
    return Error(result.status().message);

  if (!result.changes().empty()) {
    observers_->Notify(FROM_HERE, &SettingsObserver::OnSettingsChanged,
                       extension_id(), settings_namespace_,
                       ValueStoreChange::ToJson(result.changes()));
  }

  return NoArguments();
}

// Concrete settings functions

namespace {

// Adds all StringValues from a ListValue to a vector of strings.
void AddAllStringValues(const base::ListValue& from,
                        std::vector<std::string>* to) {
  DCHECK(to->empty());
  std::string as_string;
  for (auto it = from.begin(); it != from.end(); ++it) {
    if (it->GetAsString(&as_string)) {
      to->push_back(as_string);
    }
  }
}

// Gets the keys of a DictionaryValue.
std::vector<std::string> GetKeys(const base::DictionaryValue& dict) {
  std::vector<std::string> keys;
  for (base::DictionaryValue::Iterator it(dict); !it.IsAtEnd(); it.Advance()) {
    keys.push_back(it.key());
  }
  return keys;
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
      short_limit_config, new QuotaLimitHeuristic::SingletonBucketMapper(),
      "MAX_WRITE_OPERATIONS_PER_MINUTE"));
  heuristics->push_back(std::make_unique<QuotaService::TimedLimit>(
      long_limit_config, new QuotaLimitHeuristic::SingletonBucketMapper(),
      "MAX_WRITE_OPERATIONS_PER_HOUR"));
}

}  // namespace

ExtensionFunction::ResponseValue StorageStorageAreaGetFunction::RunWithStorage(
    ValueStore* storage) {
  base::Value* input = NULL;
  if (!args_->Get(0, &input))
    return BadMessage();

  switch (input->type()) {
    case base::Value::Type::NONE:
      return UseReadResult(storage->Get());

    case base::Value::Type::STRING: {
      std::string as_string;
      input->GetAsString(&as_string);
      return UseReadResult(storage->Get(as_string));
    }

    case base::Value::Type::LIST: {
      std::vector<std::string> as_string_list;
      AddAllStringValues(*static_cast<base::ListValue*>(input),
                         &as_string_list);
      return UseReadResult(storage->Get(as_string_list));
    }

    case base::Value::Type::DICTIONARY: {
      base::DictionaryValue* as_dict =
          static_cast<base::DictionaryValue*>(input);
      ValueStore::ReadResult result = storage->Get(GetKeys(*as_dict));
      if (!result.status().ok()) {
        return UseReadResult(std::move(result));
      }

      std::unique_ptr<base::DictionaryValue> with_default_values =
          as_dict->CreateDeepCopy();
      with_default_values->MergeDictionary(&result.settings());
      return UseReadResult(ValueStore::ReadResult(
          std::move(with_default_values), result.PassStatus()));
    }

    default:
      return BadMessage();
  }
}

ExtensionFunction::ResponseValue
StorageStorageAreaGetBytesInUseFunction::RunWithStorage(ValueStore* storage) {
  base::Value* input = NULL;
  if (!args_->Get(0, &input))
    return BadMessage();

  size_t bytes_in_use = 0;

  switch (input->type()) {
    case base::Value::Type::NONE:
      bytes_in_use = storage->GetBytesInUse();
      break;

    case base::Value::Type::STRING: {
      std::string as_string;
      input->GetAsString(&as_string);
      bytes_in_use = storage->GetBytesInUse(as_string);
      break;
    }

    case base::Value::Type::LIST: {
      std::vector<std::string> as_string_list;
      AddAllStringValues(*static_cast<base::ListValue*>(input),
                         &as_string_list);
      bytes_in_use = storage->GetBytesInUse(as_string_list);
      break;
    }

    default:
      return BadMessage();
  }

  return OneArgument(
      std::make_unique<base::Value>(static_cast<int>(bytes_in_use)));
}

ExtensionFunction::ResponseValue StorageStorageAreaSetFunction::RunWithStorage(
    ValueStore* storage) {
  base::DictionaryValue* input = NULL;
  if (!args_->GetDictionary(0, &input))
    return BadMessage();
  return UseWriteResult(storage->Set(ValueStore::DEFAULTS, *input));
}

void StorageStorageAreaSetFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

ExtensionFunction::ResponseValue
StorageStorageAreaRemoveFunction::RunWithStorage(ValueStore* storage) {
  base::Value* input = NULL;
  if (!args_->Get(0, &input))
    return BadMessage();

  switch (input->type()) {
    case base::Value::Type::STRING: {
      std::string as_string;
      input->GetAsString(&as_string);
      return UseWriteResult(storage->Remove(as_string));
    }

    case base::Value::Type::LIST: {
      std::vector<std::string> as_string_list;
      AddAllStringValues(*static_cast<base::ListValue*>(input),
                         &as_string_list);
      return UseWriteResult(storage->Remove(as_string_list));
    }

    default:
      return BadMessage();
  }
}

void StorageStorageAreaRemoveFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

ExtensionFunction::ResponseValue
StorageStorageAreaClearFunction::RunWithStorage(ValueStore* storage) {
  return UseWriteResult(storage->Clear());
}

void StorageStorageAreaClearFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

}  // namespace extensions
