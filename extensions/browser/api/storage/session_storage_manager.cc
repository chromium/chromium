// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/session_storage_manager.h"

#include "base/no_destructor.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/api/storage.h"

using base::trace_event::EstimateMemoryUsage;

namespace extensions {

namespace {

class SessionStorageManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  SessionStorageManagerFactory();
  SessionStorageManagerFactory(const SessionStorageManagerFactory&) = delete;
  SessionStorageManagerFactory& operator=(const SessionStorageManagerFactory&) =
      delete;
  ~SessionStorageManagerFactory() override = default;

  SessionStorageManager* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* browser_context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;
};

SessionStorageManagerFactory::SessionStorageManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "SessionStorageManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

SessionStorageManager* SessionStorageManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<SessionStorageManager*>(
      GetServiceForBrowserContext(browser_context, /*create=*/true));
}

content::BrowserContext* SessionStorageManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* browser_context) const {
  // Share storage between incognito and on-the-record profiles by using the
  // original context of an incognito window.
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      browser_context, /*force_guest_profile=*/true);
}

KeyedService* SessionStorageManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new SessionStorageManager(api::storage::session::QUOTA_BYTES,
                                   browser_context);
}

}  // namespace

// Implementation of SessionValue.
SessionStorageManager::SessionValue::SessionValue(base::Value value,
                                                  size_t size)
    : value(std::move(value)), size(size) {}

// Implementation of ValueChange.
SessionStorageManager::ValueChange::ValueChange(
    std::string key,
    std::optional<base::Value> old_value,
    base::Value* new_value)
    : key(key), old_value(std::move(old_value)), new_value(new_value) {}

SessionStorageManager::ValueChange::~ValueChange() = default;

SessionStorageManager::ValueChange::ValueChange(ValueChange&& other) = default;

// Implementation of ExtensionStorage.
SessionStorageManager::ExtensionStorage::ExtensionStorage(size_t quota_bytes)
    : quota_bytes_(quota_bytes) {}

SessionStorageManager::ExtensionStorage::~ExtensionStorage() = default;

size_t SessionStorageManager::ExtensionStorage::CalculateUsage(
    std::map<std::string, base::Value> input_values,
    std::map<std::string, std::unique_ptr<SessionValue>>& session_values)
    const {
  size_t updated_used_total = used_total_;

  for (auto& input_it : input_values) {
    size_t input_size = EstimateMemoryUsage(input_it.first) +
                        EstimateMemoryUsage(input_it.second);
    updated_used_total += input_size;

    // Remove session value size of existent key from total used bytes.
    auto existent_value_it = values_.find(input_it.first);
    if (existent_value_it != values_.end()) {
      // `updated_used_total` is guaranteed to be at least as large as any
      // individual value that's already in the map.
      DCHECK_GE(updated_used_total, existent_value_it->second->size);
      updated_used_total -= existent_value_it->second->size;
    }

    // Add input to the session values map.
    session_values.emplace(
        std::move(input_it.first),
        std::make_unique<SessionValue>(std::move(input_it.second), input_size));
  }

  if (updated_used_total >= quota_bytes_) {
    session_values.clear();
    return quota_bytes_;
  }

  return updated_used_total;
}

std::map<std::string, const base::Value*>
SessionStorageManager::ExtensionStorage::Get(
    const std::vector<std::string>& keys) const {
  std::map<std::string, const base::Value*> values;
  for (auto& key : keys) {
    auto value_it = values_.find(key);
    if (value_it != values_.end())
      values.emplace(key, &value_it->second->value);
  }
  return values;
}

std::map<std::string, const base::Value*>
SessionStorageManager::ExtensionStorage::GetAll() const {
  std::map<std::string, const base::Value*> values;
  for (auto& value : values_) {
    values.emplace(value.first, &value.second->value);
  }
  return values;
}

bool SessionStorageManager::ExtensionStorage::Set(
    std::map<std::string, base::Value> input_values,
    std::vector<ValueChange>& changes,
    std::string* error) {
  std::map<std::string, std::unique_ptr<SessionValue>> session_values;
  size_t updated_used_total =
      CalculateUsage(std::move(input_values), session_values);
  if (updated_used_total == quota_bytes_) {
    *error = "Session storage quota bytes exceeded. Values were not stored.";
    return false;
  }

  // Insert values in storage map and update total bytes.
  for (auto& session_value : session_values) {
    // Do nothing if key's existent value is the same as the new value.
    auto& existent_value = values_[session_value.first];
    if (existent_value && existent_value->value == session_value.second->value)
      continue;

    // Add the change to the changes list.
    ValueChange change(session_value.first,
                       existent_value ? std::optional<base::Value>(
                                            std::move(existent_value->value))
                                      : std::nullopt,
                       &session_value.second->value);
    changes.push_back(std::move(change));

    existent_value = std::move(session_value.second);
  }
  used_total_ = updated_used_total;
  return true;
}

void SessionStorageManager::ExtensionStorage::Remove(
    const std::vector<std::string>& keys,
    std::vector<ValueChange>& changes) {
  for (auto& key : keys) {
    auto value_it = values_.find(key);
    if (value_it == values_.end())
      continue;

    // Add the change to the changes list.
    ValueChange change(
        key, std::optional<base::Value>(std::move(value_it->second->value)),
        nullptr);
    changes.push_back(std::move(change));

    used_total_ -= value_it->second->size;
    values_.erase(value_it);
  }
}

void SessionStorageManager::ExtensionStorage::Clear(
    std::vector<ValueChange>& changes) {
  for (auto& value : values_) {
    ValueChange change(
        value.first, std::optional<base::Value>(std::move(value.second->value)),
        nullptr);
    changes.push_back(std::move(change));
  }
  Clear();
}

void SessionStorageManager::ExtensionStorage::Clear() {
  used_total_ = 0;
  values_.clear();
}

size_t SessionStorageManager::ExtensionStorage::GetBytesInUse(
    const std::vector<std::string>& keys) const {
  size_t total = 0;
  for (const auto& key : keys) {
    auto value_it = values_.find(key);
    if (value_it != values_.end())
      total += value_it->second->size;
  }
  return total;
}

size_t SessionStorageManager::ExtensionStorage::GetTotalBytesInUse() const {
  return used_total_;
}

// Implementation of SessionStorageManager.
SessionStorageManager::SessionStorageManager(
    size_t quota_bytes_per_extension,
    content::BrowserContext* browser_context)
    : quota_bytes_per_extension_(quota_bytes_per_extension) {
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context));
}

SessionStorageManager::~SessionStorageManager() = default;

// static
SessionStorageManager* SessionStorageManager::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<SessionStorageManagerFactory*>(GetFactory())
      ->GetForBrowserContext(browser_context);
}

// static
BrowserContextKeyedServiceFactory* SessionStorageManager::GetFactory() {
  static base::NoDestructor<SessionStorageManagerFactory> g_factory;
  return g_factory.get();
}

const base::Value* SessionStorageManager::Get(const ExtensionId& extension_id,
                                              const std::string& key) const {
  std::map<std::string, const base::Value*> values =
      Get(extension_id, std::vector<std::string>(1, key));
  if (values.empty())
    return nullptr;

  // Only a single value should be returned.
  DCHECK_EQ(1u, values.size());
  return values.begin()->second;
}

std::map<std::string, const base::Value*> SessionStorageManager::Get(
    const ExtensionId& extension_id,
    const std::vector<std::string>& keys) const {
  auto storage_it = extensions_storage_.find(extension_id);
  if (storage_it == extensions_storage_.end()) {
    return std::map<std::string, const base::Value*>();
  }

  return storage_it->second->Get(keys);
}

std::map<std::string, const base::Value*> SessionStorageManager::GetAll(
    const ExtensionId& extension_id) const {
  auto storage_it = extensions_storage_.find(extension_id);
  if (storage_it == extensions_storage_.end()) {
    return std::map<std::string, const base::Value*>();
  }

  return storage_it->second->GetAll();
}

bool SessionStorageManager::Set(const ExtensionId& extension_id,
                                std::map<std::string, base::Value> input_values,
                                std::vector<ValueChange>& changes,
                                std::string* error) {
  auto& storage = extensions_storage_[extension_id];

  // Initialize the extension storage, if it doesn't already exist.
  if (!storage)
    storage = std::make_unique<ExtensionStorage>(quota_bytes_per_extension_);

  return storage->Set(std::move(input_values), changes, error);
}

void SessionStorageManager::Remove(const ExtensionId& extension_id,
                                   const std::vector<std::string>& keys,
                                   std::vector<ValueChange>& changes) {
  auto storage_it = extensions_storage_.find(extension_id);
  if (storage_it != extensions_storage_.end())
    storage_it->second->Remove(keys, changes);
}

void SessionStorageManager::Remove(const ExtensionId& extension_id,
                                   const std::string& key,
                                   std::vector<ValueChange>& changes) {
  Remove(extension_id, std::vector<std::string>(1, key), changes);
}

void SessionStorageManager::Clear(const ExtensionId& extension_id,
                                  std::vector<ValueChange>& changes) {
  auto storage_it = extensions_storage_.find(extension_id);
  if (storage_it != extensions_storage_.end())
    storage_it->second->Clear(changes);
}

void SessionStorageManager::Clear(const ExtensionId& extension_id) {
  auto storage_it = extensions_storage_.find(extension_id);
  if (storage_it != extensions_storage_.end())
    storage_it->second->Clear();
}

size_t SessionStorageManager::GetBytesInUse(const ExtensionId& extension_id,
                                            const std::string& key) const {
  return GetBytesInUse(extension_id, std::vector<std::string>(1, key));
}

size_t SessionStorageManager::GetBytesInUse(
    const ExtensionId& extension_id,
    const std::vector<std::string>& keys) const {
  auto storage_it = extensions_storage_.find(extension_id);
  if (storage_it != extensions_storage_.end())
    return storage_it->second->GetBytesInUse(keys);
  return 0;
}

size_t SessionStorageManager::GetTotalBytesInUse(
    const ExtensionId& extension_id) const {
  auto storage_it = extensions_storage_.find(extension_id);
  if (storage_it != extensions_storage_.end())
    return storage_it->second->GetTotalBytesInUse();
  return 0;
}

void SessionStorageManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  Clear(extension->id());
}

}  // namespace extensions
