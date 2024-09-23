// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handler_registry.h"

#include <stddef.h>

#include <map>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/permissions/manifest_permission.h"
#include "extensions/common/permissions/manifest_permission_set.h"

namespace extensions {

namespace {

static base::LazyInstance<ManifestHandlerRegistry>::DestructorAtExit
    g_registry = LAZY_INSTANCE_INITIALIZER;
static ManifestHandlerRegistry* g_registry_override = nullptr;

}  // namespace

// static
const size_t ManifestHandlerRegistry::kHandlerMax;

ManifestHandlerRegistry::ManifestHandlerRegistry() = default;

ManifestHandlerRegistry::~ManifestHandlerRegistry() = default;

void ManifestHandlerRegistry::Finalize() {
  CHECK(!is_finalized_);
  SortManifestHandlers();
  is_finalized_ = true;
}

void ManifestHandlerRegistry::RegisterHandler(
    std::unique_ptr<ManifestHandler> handler) {
  CHECK(!is_finalized_);

  ManifestHandler* raw_handler = handler.get();
  owned_manifest_handlers_.push_back(std::move(handler));

  for (const char* key : raw_handler->Keys()) {
    auto insertion = handlers_.emplace(key, raw_handler);
    DCHECK(insertion.second)
        << "A ManifestHandler was already registered for key: " << key;
  }
}

bool ManifestHandlerRegistry::ParseExtension(Extension* extension,
                                             std::u16string* error) {
  std::map<int, ManifestHandler*> handlers_by_priority;
  for (const auto& iter : handlers_) {
    ManifestHandler* handler = iter.second;
    if (extension->manifest()->FindPath(iter.first) ||
        handler->AlwaysParseForType(extension->GetType())) {
      handlers_by_priority[priority_map_[handler]] = handler;
    }
  }
  for (const auto& iter : handlers_by_priority) {
    if (!iter.second->Parse(extension, error)) {
      return false;
    }
  }
  return true;
}

bool ManifestHandlerRegistry::ValidateExtension(
    const Extension* extension,
    std::string* error,
    std::vector<InstallWarning>* warnings) {
  std::set<ManifestHandler*> handlers;
  for (const auto& iter : handlers_) {
    ManifestHandler* handler = iter.second;
    if (extension->manifest()->FindPath(iter.first) ||
        handler->AlwaysValidateForType(extension->GetType())) {
      handlers.insert(handler);
    }
  }
  for (auto* handler : handlers) {
    if (!handler->Validate(extension, error, warnings)) {
      return false;
    }
  }
  return true;
}

ManifestPermission* ManifestHandlerRegistry::CreatePermission(
    const std::string& name) {
  ManifestHandlerMap::const_iterator it = handlers_.find(name);
  if (it == handlers_.end()) {
    return nullptr;
  }

  return it->second->CreatePermission();
}

void ManifestHandlerRegistry::AddExtensionInitialRequiredPermissions(
    const Extension* extension,
    ManifestPermissionSet* permission_set) {
  for (ManifestHandlerMap::const_iterator it = handlers_.begin();
       it != handlers_.end(); ++it) {
    std::unique_ptr<ManifestPermission> permission(
        it->second->CreateInitialRequiredPermission(extension));
    if (permission) {
      permission_set->insert(std::move(permission));
    }
  }
}

// static
ManifestHandlerRegistry* ManifestHandlerRegistry::Get() {
  return g_registry_override ? g_registry_override : g_registry.Pointer();
}

// static
ManifestHandlerRegistry* ManifestHandlerRegistry::SetForTesting(
    ManifestHandlerRegistry* new_registry) {
  ManifestHandlerRegistry* old_registry = ManifestHandlerRegistry::Get();
  if (new_registry != g_registry.Pointer()) {
    g_registry_override = new_registry;
  } else {
    g_registry_override = nullptr;
  }
  return old_registry;
}

// static
void ManifestHandlerRegistry::ResetForTesting() {
  ManifestHandlerRegistry* registry = Get();
  registry->priority_map_.clear();
  registry->handlers_.clear();
  registry->is_finalized_ = false;
}

void ManifestHandlerRegistry::SortManifestHandlers() {
  std::vector<ManifestHandler*> unsorted_handlers;
  unsorted_handlers.reserve(handlers_.size());
  for (const auto& key_value : handlers_) {
    unsorted_handlers.push_back(key_value.second);
  }

  int priority = 0;
  while (true) {
    std::vector<ManifestHandler*> next_unsorted_handlers;
    next_unsorted_handlers.reserve(unsorted_handlers.size());
    for (ManifestHandler* handler : unsorted_handlers) {
      const std::vector<std::string>& prerequisites =
          handler->PrerequisiteKeys();
      int unsatisfied = prerequisites.size();
      for (const std::string& key : prerequisites) {
        ManifestHandlerMap::const_iterator prereq_iter = handlers_.find(key);
        // If the prerequisite does not exist, crash.
        CHECK(prereq_iter != handlers_.end())
            << "Extension manifest handler depends on unrecognized key " << key;
        // Prerequisite is in our map.
        if (base::Contains(priority_map_, prereq_iter->second)) {
          unsatisfied--;
        }
      }
      if (unsatisfied == 0) {
        priority_map_[handler] = priority;
        priority++;
      } else {
        // Put in the list for next time.
        next_unsorted_handlers.push_back(handler);
      }
    }
    if (next_unsorted_handlers.size() == unsorted_handlers.size()) {
      break;
    }
    unsorted_handlers.swap(next_unsorted_handlers);
  }

  // If there are any leftover unsorted handlers, they must have had
  // circular dependencies.
  CHECK(unsorted_handlers.empty())
      << "Extension manifest handlers have circular dependencies!";
}

}  // namespace extensions
