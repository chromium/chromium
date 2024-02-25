// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_USER_SCRIPT_SET_MANAGER_H_
#define EXTENSIONS_RENDERER_USER_SCRIPT_SET_MANAGER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/observer_list.h"
#include "content/public/renderer/render_thread_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/host_id.mojom-forward.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/user_script.h"
#include "extensions/renderer/user_script_set.h"

namespace content {
class RenderFrame;
}

namespace extensions {

class ScriptInjection;

// Manager for separate UserScriptSets, one for each shared memory region.
// Regions are organized as follows:
// static_scripts -- contains all extensions' scripts that are statically
//                   declared in the extension manifest.
// programmatic_scripts -- one region per host (extension or WebUI) containing
//                         only programmatically-declared scripts, instantiated
//                         when an extension first creates a declarative rule
//                         that would, if triggered, request a script injection.
class UserScriptSetManager {
 public:
  // Like a UserScriptSet::Observer, but automatically subscribes to all sets
  // associated with the manager.
  class Observer {
   public:
    virtual void OnUserScriptsUpdated(const mojom::HostID& changed_host) = 0;
  };

  UserScriptSetManager();

  UserScriptSetManager(const UserScriptSetManager&) = delete;
  UserScriptSetManager& operator=(const UserScriptSetManager&) = delete;

  ~UserScriptSetManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Looks up the script injection associated with |script_id| and
  // |extension_id| in the context of the given |web_frame|, |tab_id|,
  // and |url|.
  std::unique_ptr<ScriptInjection> GetInjectionForDeclarativeScript(
      const std::string& script_id,
      content::RenderFrame* render_frame,
      int tab_id,
      const GURL& url,
      const ExtensionId& extension_id);

  // Append all injections from |static_scripts| and each of
  // |programmatic_scripts_| to |injections|.
  void GetAllInjections(
      std::vector<std::unique_ptr<ScriptInjection>>* injections,
      content::RenderFrame* render_frame,
      int tab_id,
      mojom::RunLocation run_location);

  // Get active extension IDs from `static_scripts_`.
  void GetAllActiveExtensionIds(std::set<ExtensionId>* ids) const;

  // Handle the UpdateUserScripts extension message.
  void OnUpdateUserScripts(base::ReadOnlySharedMemoryRegion shared_memory,
                           const mojom::HostID& host_id);

  // Invalidates script injections for the UserScriptSet in `scripts_`
  // corresponding to `extension_id` and deletes the script set.
  void OnExtensionUnloaded(const ExtensionId& extension_id);

  void set_activity_logging_enabled(bool enabled) {
    activity_logging_enabled_ = enabled;
  }

 private:
  // Map for per-host script sets.
  using UserScriptSetMap =
      std::map<mojom::HostID, std::unique_ptr<UserScriptSet>>;

  UserScriptSet* GetScriptsByHostID(const mojom::HostID& host_id);

  // Stores all scripts, defined in extension manifests and programmatically
  // from extension APIs and webview tags. Each UserScriptSet is keyed by a
  // HostID.
  UserScriptSetMap scripts_;

  // Whether or not dom activity should be logged for injected scripts.
  bool activity_logging_enabled_;

  // The associated observers.
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_USER_SCRIPT_SET_MANAGER_H_
