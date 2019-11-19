// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_USER_SCRIPT_SET_H_
#define EXTENSIONS_RENDERER_USER_SCRIPT_SET_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/observer_list.h"
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
    // Called when the set of user scripts is updated. |changed_hosts| contains
    // the hosts whose scripts have been altered. Note that *all* script objects
    // are invalidated, even if they aren't in |changed_hosts|.
    virtual void OnUserScriptsUpdated(const std::set<HostID>& changed_hosts,
                                      const UserScriptList& scripts) = 0;
  };

  UserScriptSet();
  ~UserScriptSet();

  // Adds or removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Appends the ids of the extensions that have user scripts to |ids|.
  void GetActiveExtensionIds(std::set<std::string>* ids) const;

  // Append any ScriptInjections that should run on the given |render_frame| and
  // |tab_id|, at the given |run_location|, to |injections|.
  // |extensions| is passed in to verify the corresponding extension is still
  // valid.
  void GetInjections(std::vector<std::unique_ptr<ScriptInjection>>* injections,
                     content::RenderFrame* render_frame,
                     int tab_id,
                     UserScript::RunLocation run_location,
                     bool log_activity);

  std::unique_ptr<ScriptInjection> GetDeclarativeScriptInjection(
      int script_id,
      content::RenderFrame* render_frame,
      int tab_id,
      UserScript::RunLocation run_location,
      const GURL& document_url,
      bool log_activity);

  // Updates scripts given the shared memory region containing user scripts.
  // Returns true if the scripts were successfully updated.
  bool UpdateUserScripts(base::ReadOnlySharedMemoryRegion shared_memory,
                         const std::set<HostID>& changed_hosts,
                         bool whitelisted_only);

  // Returns the contents of a script file.
  // Note that copying is cheap as this uses WebString.
  blink::WebString GetJsSource(const UserScript::File& file,
                               bool emulate_greasemonkey);
  blink::WebString GetCssSource(const UserScript::File& file);

 private:
  // Returns a new ScriptInjection for the given |script| to execute in the
  // |render_frame|, or NULL if the script should not execute.
  std::unique_ptr<ScriptInjection> GetInjectionForScript(
      const UserScript* script,
      content::RenderFrame* render_frame,
      int tab_id,
      UserScript::RunLocation run_location,
      const GURL& document_url,
      bool is_declarative,
      bool log_activity);

  // Shared memory mapping containing raw script data.
  base::ReadOnlySharedMemoryMapping shared_memory_mapping_;

  // The UserScripts this injector manages.
  UserScriptList scripts_;

  // Map of user script file url -> source.
  std::map<GURL, blink::WebString> script_sources_;

  // The associated observers.
  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptSet);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_USER_SCRIPT_SET_H_
