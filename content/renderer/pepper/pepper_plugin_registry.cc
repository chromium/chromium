// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_plugin_registry.h"

#include <stddef.h>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "content/common/pepper_plugin_list.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

namespace content {

// static
PepperPluginRegistry* PepperPluginRegistry::GetInstance() {
  static PepperPluginRegistry* registry = nullptr;
  // This object leaks.  It is a temporary hack to work around a crash.
  // http://code.google.com/p/chromium/issues/detail?id=63234
  if (!registry) {
    registry = new PepperPluginRegistry;
    registry->Initialize();
  }
  return registry;
}

const ContentPluginInfo* PepperPluginRegistry::GetInfoForPlugin(
    const WebPluginInfo& info) {
  for (const auto& plugin : plugin_list_) {
    if (info.path == plugin.path)
      return &plugin;
  }
  // We did not find the plugin in our list. But wait! the plugin can also
  // be a latecomer, as it happens with pepper flash. This information
  // is actually in |info| and we can use it to construct it and add it to
  // the list. This same deal needs to be done in the browser side in
  // PluginService.
  ContentPluginInfo plugin;
  if (!MakePepperPluginInfo(info, &plugin))
    return nullptr;

  plugin_list_.push_back(plugin);
  return &plugin_list_.back();
}

PluginModule* PepperPluginRegistry::GetLiveModule(
    const base::FilePath& path,
    const std::optional<url::Origin>& origin_lock) {
  auto module_iter = live_modules_.find({path, origin_lock});
  if (module_iter == live_modules_.end())
    return nullptr;

  // Check the instances for the module to see if they've all been Delete()d.
  // We don't want to return a PluginModule in that case, since the plugin may
  // have exited already.
  const PluginModule::PluginInstanceSet& instance_set =
      module_iter->second->GetAllInstances();

  // If instance_set is empty, InstanceCreated() hasn't been called yet, so
  // it's safe to return the PluginModule.
  if (instance_set.empty())
    return module_iter->second;

  auto instance_iter = instance_set.begin();
  while (instance_iter != instance_set.end()) {
    if (!(*instance_iter)->is_deleted())
      return module_iter->second;
    ++instance_iter;
  }
  return nullptr;
}

void PepperPluginRegistry::AddLiveModule(
    const base::FilePath& path,
    const std::optional<url::Origin>& origin_lock,
    PluginModule* module) {
  DCHECK(!base::Contains(live_modules_, std::make_pair(path, origin_lock)));
  live_modules_[{path, origin_lock}] = module;
}

void PepperPluginRegistry::PluginModuleDead(PluginModule* dead_module) {
  // DANGER: Don't dereference the dead_module pointer! It may be in the
  // process of being deleted.

  // Modules aren't destroyed very often and there are normally at most a
  // couple of them. So for now we just do a brute-force search.
  for (auto i = live_modules_.begin(); i != live_modules_.end(); ++i) {
    if (i->second == dead_module) {
      live_modules_.erase(i);
      return;
    }
  }
  // Can occur in tests.
}

PepperPluginRegistry::~PepperPluginRegistry() {
  // Explicitly clear all preloaded modules first. This will cause callbacks
  // to erase these modules from the live_modules_ list, and we don't want
  // that to happen implicitly out-of-order.
  preloaded_modules_.clear();

  DCHECK(live_modules_.empty());
}

PepperPluginRegistry::PepperPluginRegistry() {}

void PepperPluginRegistry::Initialize() {
  ComputePepperPluginList(&plugin_list_);

  // Note that in each case, AddLiveModule must be called before completing
  // initialization. If we bail out (in the continue clauses) before saving
  // the initialized module, it will still try to unregister itself in its
  // destructor.
  for (const auto& current : plugin_list_) {
    if (current.is_out_of_process)
      continue;  // Out of process plugins need no special pre-initialization.

    auto module = base::MakeRefCounted<PluginModule>(
        current.name, current.version, current.path,
        ppapi::PpapiPermissions(current.permissions));
    AddLiveModule(current.path, std::optional<url::Origin>(), module.get());
    if (current.is_internal) {
      if (!module->InitAsInternalPlugin(current.internal_entry_points)) {
        DVLOG(1) << "Failed to load pepper module: " << current.path.value();
        continue;
      }
    } else {
      // Preload all external plugins we're not running out of process.
      if (!module->InitAsLibrary(current.path)) {
        DVLOG(1) << "Failed to load pepper module: " << current.path.value();
        continue;
      }
    }
    preloaded_modules_[current.path] = module;
  }
}

}  // namespace content
