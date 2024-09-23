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

void SettingsFunction::OnWriteOperationFinished(
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

ExtensionFunction::ResponseAction StorageStorageAreaGetFunction::Run() {
  if (args().empty()) {
    return RespondNow(BadMessage());
  }

  base::Value input = std::move(mutable_args()[0]);
  mutable_args().erase(args().begin());

  std::optional<std::vector<std::string>> keys;
  std::optional<base::Value::Dict> defaults;

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

    case base::Value::Type::DICT: {
      keys = std::optional(GetKeysFromDict(input.GetDict()));

      // When the input holds a dictionary, the values are default values for
      // any keys not present in storage. This is only the case for this
      // parameter type.
      defaults = std::move(input).TakeDict();
      break;
    }

    default:
      return RespondNow(BadMessage());
  }

  StorageFrontend* frontend = StorageFrontend::Get(browser_context());
  frontend->GetValues(
      extension(), storage_area(), std::move(keys),
      base::BindOnce(&StorageStorageAreaGetFunction::OnGetOperationFinished,
                     this, std::move(defaults)));

  return RespondLater();
}

void StorageStorageAreaGetFunction::OnGetOperationFinished(
    std::optional<base::Value::Dict> defaults,
    StorageFrontend::GetResult result) {
  // Since the storage access happens asynchronously, the browser context can
  // be torn down in the interim. If this happens, early-out.
  if (!browser_context()) {
    return;
  }

  StorageFrontend::ResultStatus status = result.status;

  if (!status.success) {
    CHECK(status.error.has_value());
    Respond(Error(*status.error));
    return;
  }

  CHECK(result.data.has_value());

  base::Value::Dict values =
      defaults ? std::move(*defaults) : base::Value::Dict();

  // It's important that we merge the values into the defaults, and not the
  // other way around, to avoid the defaults overwriting any existing values.
  values.Merge(std::move(*result.data));

  Respond(WithArguments(std::move(values)));
}

ExtensionFunction::ResponseAction StorageStorageAreaGetKeysFunction::Run() {
  StorageFrontend* frontend = StorageFrontend::Get(browser_context());
  frontend->GetKeys(
      extension(), storage_area(),
      base::BindOnce(
          &StorageStorageAreaGetKeysFunction::OnGetKeysOperationFinished,
          this));

  return RespondLater();
}

void StorageStorageAreaGetKeysFunction::OnGetKeysOperationFinished(
    StorageFrontend::GetKeysResult result) {
  // Since the storage access happens asynchronously, the browser context can
  // be torn down in the interim. If this happens, early-out.
  if (!browser_context()) {
    return;
  }

  StorageFrontend::ResultStatus status = result.status;

  if (!status.success) {
    CHECK(status.error.has_value());
    Respond(Error(*status.error));
    return;
  }

  CHECK(result.data.has_value());
  Respond(WithArguments(std::move(*result.data)));
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

ExtensionFunction::ResponseAction StorageStorageAreaSetFunction::Run() {
  if (args().empty() || !args()[0].is_dict()) {
    return RespondNow(BadMessage());
  }

  // Retrieve and delete input from `args_` since they will be moved to storage.
  base::Value input = std::move(mutable_args()[0]);
  mutable_args().erase(args().begin());

  StorageFrontend* frontend = StorageFrontend::Get(browser_context());
  frontend->Set(
      extension(), storage_area(), std::move(input).TakeDict(),
      base::BindOnce(&StorageStorageAreaSetFunction::OnWriteOperationFinished,
                     this));

  return RespondLater();
}

void StorageStorageAreaSetFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

ExtensionFunction::ResponseAction StorageStorageAreaRemoveFunction::Run() {
  if (args().empty()) {
    return RespondNow(BadMessage());
  }

  const base::Value& input = args()[0];
  std::vector<std::string> keys;

  switch (input.type()) {
    case base::Value::Type::STRING:
      keys = std::vector<std::string>(1, input.GetString());
      break;

    case base::Value::Type::LIST:
      keys = GetKeysFromList(input.GetList());
      break;

    default:
      return RespondNow(BadMessage());
  }

  StorageFrontend* frontend = StorageFrontend::Get(browser_context());
  frontend->Remove(
      extension(), storage_area(), keys,
      base::BindOnce(
          &StorageStorageAreaRemoveFunction::OnWriteOperationFinished, this));

  return RespondLater();
}

void StorageStorageAreaRemoveFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

ExtensionFunction::ResponseAction StorageStorageAreaClearFunction::Run() {
  StorageFrontend* frontend = StorageFrontend::Get(browser_context());
  frontend->Clear(
      extension(), storage_area(),
      base::BindOnce(&StorageStorageAreaClearFunction::OnWriteOperationFinished,
                     this));

  return RespondLater();
}

void StorageStorageAreaClearFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  GetModificationQuotaLimitHeuristics(heuristics);
}

ExtensionFunction::ResponseAction
StorageStorageAreaSetAccessLevelFunction::Run() {
  if (storage_area() != StorageAreaNamespace::kSession) {
    // TODO(crbug.com/40949182). Support storage areas other than kSession. For
    // now, we return an error.
    return RespondNow(
        Error("This StorageArea is not available for setting access level"));
  }

  if (source_context_type() != mojom::ContextType::kPrivilegedExtension) {
    return RespondNow(Error("Context cannot set the storage access level"));
  }

  std::optional<api::storage::StorageArea::SetAccessLevel::Params> params =
      api::storage::StorageArea::SetAccessLevel::Params::Create(args());

  if (!params) {
    return RespondNow(BadMessage());
  }

  // The parsing code ensures `access_level` is sane.
  DCHECK(params->access_options.access_level ==
             api::storage::AccessLevel::kTrustedContexts ||
         params->access_options.access_level ==
             api::storage::AccessLevel::kTrustedAndUntrustedContexts);

  storage_utils::SetSessionAccessLevel(extension_id(), *browser_context(),
                                       params->access_options.access_level);

  return RespondNow(NoArguments());
}

}  // namespace extensions
