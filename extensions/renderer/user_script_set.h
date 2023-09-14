// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_USER_SCRIPT_SET_H_
#define EXTENSIONS_RENDERER_USER_SCRIPT_SET_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/observer_list.h"
#include "extensions/common/mojom/host_id.mojom-forward.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/user_script.h"
#include "third_party/blink/public/platform/web_string.h"

class GURL;

namespace content {
class RenderFrame;
}

namespace extensions {
class ScriptInjection;

// The UserScriptSet is a collection of UserScripts which knows how to update
// itself from SharedMemory and create ScriptInjections for UserScripts to
// inject on a page.
class UserScriptSet {
 public:
  class Observer {
   public:
    // Called when the set of user scripts is updated, which invalidates all
    // previous script objects from this UserScriptSet.
    virtual void OnUserScriptsUpdated() = 0;

    // Called when this UserScriptSet is destroyed.
    virtual void OnUserScriptSetDestroyed() = 0;
  };

  explicit UserScriptSet(mojom::HostID host_id);

  UserScriptSet(const UserScriptSet&) = delete;
  UserScriptSet& operator=(const UserScriptSet&) = delete;

  ~UserScriptSet();

  // Adds or removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Append any ScriptInjections that should run on the given |render_frame| and
  // |tab_id|, at the given |run_location|, to |injections|.
  // |extensions| is passed in to verify the corresponding extension is still
  // valid.
  void GetInjections(std::vector<std::unique_ptr<ScriptInjection>>* injections,
                     content::RenderFrame* render_frame,
                     int tab_id,
                     mojom::RunLocation run_location,
                     bool log_activity);

  std::unique_ptr<ScriptInjection> GetDeclarativeScriptInjection(
      const std::string& script_id,
      content::RenderFrame* render_frame,
      int tab_id,
      mojom::RunLocation run_location,
      const GURL& document_url,
      bool log_activity);

  // Updates scripts given the shared memory region containing user scripts.
  // Returns true if the scripts were successfully updated.
  bool UpdateUserScripts(base::ReadOnlySharedMemoryRegion shared_memory);

  bool HasScripts() const { return !scripts_.empty(); }

  // Clears all user scripts managed by this set and notifies observers.
  void ClearUserScripts();

  // Returns the contents of a script file.
  // Note that copying is cheap as this uses WebString.
  blink::WebString GetJsSource(const UserScript::Content& file,
                               bool emulate_greasemonkey);
  blink::WebString GetCssSource(const UserScript::Content& file);

 private:
  // Returns a new ScriptInjection for the given |script| to execute in the
  // |render_frame|, or NULL if the script should not execute.
  std::unique_ptr<ScriptInjection> GetInjectionForScript(
      const UserScript* script,
      content::RenderFrame* render_frame,
      int tab_id,
      mojom::RunLocation run_location,
      const GURL& document_url,
      bool is_declarative,
      bool log_activity);

  // Shared memory mapping containing raw script data.
  base::ReadOnlySharedMemoryMapping shared_memory_mapping_;

  // The UserScripts this injector manages.
  UserScriptList scripts_;

  // Map of user script file url -> source.
  std::map<GURL, blink::WebString> script_sources_;

  // The HostID which |scripts_| is associated with.
  mojom::HostID host_id_;

  // The associated observers.
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_USER_SCRIPT_SET_H_
