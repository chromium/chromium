// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_USER_SCRIPT_SET_MANAGER_H_
#define EXTENSIONS_RENDERER_USER_SCRIPT_SET_MANAGER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/observer_list.h"
#include "content/public/renderer/render_thread_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/user_script.h"
#include "extensions/renderer/user_script_set.h"

namespace content {
class RenderFrame;
}

namespace IPC {
class Message;
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
class UserScriptSetManager : public content::RenderThreadObserver {
 public:
  // Like a UserScriptSet::Observer, but automatically subscribes to all sets
  // associated with the manager.
  class Observer {
   public:
    virtual void OnUserScriptsUpdated(
        const std::set<HostID>& changed_hosts) = 0;
  };

  UserScriptSetManager();

  ~UserScriptSetManager() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Looks up the script injection associated with |script_id| and
  // |extension_id| in the context of the given |web_frame|, |tab_id|,
  // and |url|.
  std::unique_ptr<ScriptInjection> GetInjectionForDeclarativeScript(
      int script_id,
      content::RenderFrame* render_frame,
      int tab_id,
      const GURL& url,
      const std::string& extension_id);

  // Append all injections from |static_scripts| and each of
  // |programmatic_scripts_| to |injections|.
  void GetAllInjections(
      std::vector<std::unique_ptr<ScriptInjection>>* injections,
      content::RenderFrame* render_frame,
      int tab_id,
      UserScript::RunLocation run_location);

  // Get active extension IDs from |static_scripts| and each of
  // |programmatic_scripts_|.
  void GetAllActiveExtensionIds(std::set<std::string>* ids) const;

  const UserScriptSet* static_scripts() const { return &static_scripts_; }

  void set_activity_logging_enabled(bool enabled) {
    activity_logging_enabled_ = enabled;
  }

 private:
  // Map for per-extension sets that may be defined programmatically.
  using UserScriptSetMap = std::map<HostID, std::unique_ptr<UserScriptSet>>;

  // content::RenderThreadObserver implementation.
  bool OnControlMessageReceived(const IPC::Message& message) override;

  UserScriptSet* GetProgrammaticScriptsByHostID(const HostID& host_id);

  // Handle the UpdateUserScripts extension message.
  void OnUpdateUserScripts(base::ReadOnlySharedMemoryRegion shared_memory,
                           const HostID& host_id,
                           const std::set<HostID>& changed_hosts,
                           bool whitelisted_only);

  // Scripts statically defined in extension manifests.
  UserScriptSet static_scripts_;

  // Scripts programmatically-defined through API calls (initialized and stored
  // per-extension).
  UserScriptSetMap programmatic_scripts_;

  // Whether or not dom activity should be logged for injected scripts.
  bool activity_logging_enabled_;

  // The associated observers.
  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptSetManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_USER_SCRIPT_SET_MANAGER_H_
