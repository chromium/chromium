// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/user_script_set_manager.h"

#include "base/observer_list.h"
#include "components/crx_file/id_util.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/script_injection.h"
#include "extensions/renderer/user_script_set.h"

namespace extensions {

UserScriptSetManager::UserScriptSetManager()
    : activity_logging_enabled_(false) {
}

UserScriptSetManager::~UserScriptSetManager() {
}

void UserScriptSetManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UserScriptSetManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::unique_ptr<ScriptInjection>
UserScriptSetManager::GetInjectionForDeclarativeScript(
    const std::string& script_id,
    content::RenderFrame* render_frame,
    int tab_id,
    const GURL& url,
    const ExtensionId& extension_id) {
  UserScriptSet* user_script_set = GetScriptsByHostID(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension_id));
  if (!user_script_set)
    return nullptr;

  return user_script_set->GetDeclarativeScriptInjection(
      script_id, render_frame, tab_id, mojom::RunLocation::kBrowserDriven, url,
      activity_logging_enabled_);
}

void UserScriptSetManager::GetAllInjections(
    std::vector<std::unique_ptr<ScriptInjection>>* injections,
    content::RenderFrame* render_frame,
    int tab_id,
    mojom::RunLocation run_location) {
  for (auto it = scripts_.begin(); it != scripts_.end(); ++it) {
    it->second->GetInjections(injections, render_frame, tab_id, run_location,
                              activity_logging_enabled_);
  }
}

void UserScriptSetManager::GetAllActiveExtensionIds(
    std::set<ExtensionId>* ids) const {
  DCHECK(ids);
  for (auto it = scripts_.cbegin(); it != scripts_.cend(); ++it) {
    if (it->first.type == mojom::HostID::HostType::kExtensions &&
        it->second->HasScripts()) {
      ids->insert(it->first.id);
    }
  }
}

UserScriptSet* UserScriptSetManager::GetScriptsByHostID(
    const mojom::HostID& host_id) {
  UserScriptSetMap::const_iterator it = scripts_.find(host_id);
  return it != scripts_.end() ? it->second.get() : nullptr;
}

void UserScriptSetManager::OnUpdateUserScripts(
    base::ReadOnlySharedMemoryRegion shared_memory,
    const mojom::HostID& host_id) {
  if (!shared_memory.IsValid()) {
    NOTREACHED_IN_MIGRATION() << "Bad scripts handle";
    return;
  }

  if (host_id.type == mojom::HostID::HostType::kExtensions &&
      !crx_file::id_util::IdIsValid(host_id.id)) {
    NOTREACHED_IN_MIGRATION() << "Invalid extension id: " << host_id.id;
    return;
  }

  auto& scripts = scripts_[host_id];
  if (!scripts)
    scripts = std::make_unique<UserScriptSet>(host_id);

  if (scripts->UpdateUserScripts(std::move(shared_memory))) {
    for (auto& observer : observers_)
      observer.OnUserScriptsUpdated(host_id);
  }
}

void UserScriptSetManager::OnExtensionUnloaded(
    const ExtensionId& extension_id) {
  auto it = scripts_.find(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension_id));
  if (it != scripts_.end()) {
    it->second->ClearUserScripts();
    scripts_.erase(it);
  }
}

}  // namespace extensions
