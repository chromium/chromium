// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/user_script_loader.h"

#include <stddef.h>

#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/types/pass_key.h"
#include "base/version.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/script_injection_tracker.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#endif

using content::BrowserThread;
using content::BrowserContext;

namespace extensions {

namespace {

// The error message passed inside ScriptsLoadedCallback if the callback is
// fired when the UserScriptLoader is destroyed.
const char kUserScriptLoaderDestroyedErrorMsg[] =
    "Scripts could not be loaded as the script loader has been destroyed.";

// The error message massed inside ScriptsLoadedCallback if the operation
// associated with the callback will not cause any script changes.
const char kNoScriptChangesErrorMsg[] =
    "No changes to loaded scripts would result from this operation.";

#if DCHECK_IS_ON()
bool AreScriptsUnique(const UserScriptList& scripts) {
  std::set<std::string> script_ids;
  for (const std::unique_ptr<UserScript>& script : scripts) {
    if (script_ids.count(script->id()))
      return false;
    script_ids.insert(script->id());
  }
  return true;
}
#endif  // DCHECK_IS_ON()

// Helper function to parse greasesmonkey headers
bool GetDeclarationValue(std::string_view line,
                         std::string_view prefix,
                         std::string* value) {
  std::string_view::size_type index = line.find(prefix);
  if (index == std::string_view::npos) {
    return false;
  }

  std::string temp(line.data() + index + prefix.length(),
                   line.length() - index - prefix.length());

  if (temp.empty() || !base::IsAsciiWhitespace(temp[0]))
    return false;

  base::TrimWhitespaceASCII(temp, base::TRIM_ALL, value);
  return true;
}

#if BUILDFLAG(ENABLE_GUEST_VIEW)
bool CanExecuteScriptEverywhere(BrowserContext* browser_context,
                                const mojom::HostID& host_id) {
  if (host_id.type == mojom::HostID::HostType::kWebUi)
    return true;

  const Extension* extension = ExtensionRegistry::Get(browser_context)
                                   ->enabled_extensions()
                                   .GetByID(host_id.id);

  return extension && PermissionsData::CanExecuteScriptEverywhere(
                          extension->id(), extension->location());
}
#endif

}  // namespace

// static
bool UserScriptLoader::ParseMetadataHeader(std::string_view script_text,
                                           UserScript* script) {
  // http://wiki.greasespot.net/Metadata_block
  std::string_view line;
  size_t line_start = 0;
  size_t line_end = line_start;
  bool in_metadata = false;

  static const std::string_view kUserScriptBegin("// ==UserScript==");
  static const std::string_view kUserScriptEng("// ==/UserScript==");
  static const std::string_view kNamespaceDeclaration("// @namespace");
  static const std::string_view kNameDeclaration("// @name");
  static const std::string_view kVersionDeclaration("// @version");
  static const std::string_view kDescriptionDeclaration("// @description");
  static const std::string_view kIncludeDeclaration("// @include");
  static const std::string_view kExcludeDeclaration("// @exclude");
  static const std::string_view kMatchDeclaration("// @match");
  static const std::string_view kExcludeMatchDeclaration("// @exclude_match");
  static const std::string_view kRunAtDeclaration("// @run-at");
  static const std::string_view kRunAtDocumentStartValue("document-start");
  static const std::string_view kRunAtDocumentEndValue("document-end");
  static const std::string_view kRunAtDocumentIdleValue("document-idle");

  while (line_start < script_text.length()) {
    line_end = script_text.find('\n', line_start);

    // Handle the case where there is no trailing newline in the file.
    if (line_end == std::string::npos)
      line_end = script_text.length() - 1;

    line = script_text.substr(line_start, line_end - line_start);

    if (!in_metadata) {
      if (base::StartsWith(line, kUserScriptBegin))
        in_metadata = true;
    } else {
      if (base::StartsWith(line, kUserScriptEng))
        break;

      std::string value;
      if (GetDeclarationValue(line, kIncludeDeclaration, &value)) {
        // We escape some characters that MatchPattern() considers special.
        base::ReplaceSubstringsAfterOffset(&value, 0, "\\", "\\\\");
        base::ReplaceSubstringsAfterOffset(&value, 0, "?", "\\?");
        script->add_glob(value);
      } else if (GetDeclarationValue(line, kExcludeDeclaration, &value)) {
        base::ReplaceSubstringsAfterOffset(&value, 0, "\\", "\\\\");
        base::ReplaceSubstringsAfterOffset(&value, 0, "?", "\\?");
        script->add_exclude_glob(value);
      } else if (GetDeclarationValue(line, kNamespaceDeclaration, &value)) {
        script->set_name_space(value);
      } else if (GetDeclarationValue(line, kNameDeclaration, &value)) {
        script->set_name(value);
      } else if (GetDeclarationValue(line, kVersionDeclaration, &value)) {
        base::Version version(value);
        if (version.IsValid())
          script->set_version(version.GetString());
      } else if (GetDeclarationValue(line, kDescriptionDeclaration, &value)) {
        script->set_description(value);
      } else if (GetDeclarationValue(line, kMatchDeclaration, &value)) {
        URLPattern pattern(UserScript::ValidUserScriptSchemes());
        if (URLPattern::ParseResult::kSuccess != pattern.Parse(value))
          return false;
        script->add_url_pattern(pattern);
      } else if (GetDeclarationValue(line, kExcludeMatchDeclaration, &value)) {
        URLPattern exclude(UserScript::ValidUserScriptSchemes());
        if (URLPattern::ParseResult::kSuccess != exclude.Parse(value))
          return false;
        script->add_exclude_url_pattern(exclude);
      } else if (GetDeclarationValue(line, kRunAtDeclaration, &value)) {
        if (value == kRunAtDocumentStartValue)
          script->set_run_location(mojom::RunLocation::kDocumentStart);
        else if (value == kRunAtDocumentEndValue)
          script->set_run_location(mojom::RunLocation::kDocumentEnd);
        else if (value == kRunAtDocumentIdleValue)
          script->set_run_location(mojom::RunLocation::kDocumentIdle);
        else
          return false;
      }

      // TODO(aa): Handle more types of metadata.
    }

    line_start = line_end + 1;
  }

  // If no patterns were specified, default to @include *. This is what
  // Greasemonkey does.
  if (script->globs().empty() && script->url_patterns().is_empty())
    script->add_glob("*");

  return true;
}

UserScriptLoader::UserScriptLoader(BrowserContext* browser_context,
                                   const mojom::HostID& host_id)
    : loaded_scripts_(UserScriptList()),
      ready_(false),
      queued_load_(false),
      browser_context_(browser_context),
      host_id_(host_id) {}

UserScriptLoader::~UserScriptLoader() {
  std::optional<std::string> error =
      std::make_optional(kUserScriptLoaderDestroyedErrorMsg);

  // Clean up state by firing all remaining callbacks with |error| populated to
  // alert consumers that scripts are not loaded.
  std::list<ScriptsLoadedCallback> remaining_callbacks;
  remaining_callbacks.splice(remaining_callbacks.end(), queued_load_callbacks_);
  remaining_callbacks.splice(remaining_callbacks.end(), loading_callbacks_);

  for (auto& callback : remaining_callbacks)
    std::move(callback).Run(this, error);

  for (auto& observer : observers_)
    observer.OnUserScriptLoaderDestroyed(this);
}

void UserScriptLoader::AddScripts(UserScriptList scripts,
                                  ScriptsLoadedCallback callback) {
#if DCHECK_IS_ON()
  // |scripts| with non-unique IDs will work, but that would indicate we are
  // doing something wrong somewhere, so DCHECK that.
  DCHECK(AreScriptsUnique(scripts))
      << "AddScripts() expects scripts with unique IDs.";
#endif  // DCHECK_IS_ON()
  for (std::unique_ptr<UserScript>& user_script : scripts) {
    const std::string& id = user_script->id();
    removed_script_ids_.erase(id);
    if (added_scripts_map_.count(id) == 0)
      added_scripts_map_[id] = std::move(user_script);
  }

  AttemptLoad(std::move(callback));
}

void UserScriptLoader::AddScripts(UserScriptList scripts,
                                  int render_process_id,
                                  int render_frame_id,
                                  ScriptsLoadedCallback callback) {
  AddScripts(std::move(scripts), std::move(callback));
}

void UserScriptLoader::RemoveScripts(const std::set<std::string>& script_ids,
                                     ScriptsLoadedCallback callback) {
  for (const auto& id : script_ids) {
    removed_script_ids_.insert(id);
    // TODO(lazyboy): We shouldn't be trying to remove scripts that were never
    // a) added to |added_scripts_map_| or b) being loaded or has done loading
    // through |loaded_scripts_|. This would reduce sending redundant IPC.
    added_scripts_map_.erase(id);
  }

  AttemptLoad(std::move(callback));
}

void UserScriptLoader::OnRenderProcessHostCreated(
    content::RenderProcessHost* process_host) {
  if (!ExtensionsBrowserClient::Get()->IsSameContext(
          browser_context_, process_host->GetBrowserContext()))
    return;
  if (initial_load_complete()) {
    SendUpdateResult update_result = SendUpdate(process_host, shared_memory_);
    if (update_result == SendUpdateResult::kRendererHasBeenNotified) {
      ScriptInjectionTracker::DidUpdateScriptsInRenderer(
          base::PassKey<UserScriptLoader>(), host_id_, *process_host);
    }
  }
}

bool UserScriptLoader::ScriptsMayHaveChanged() const {
  // Scripts may have changed if there are scripts added or removed.
  return (added_scripts_map_.size() || removed_script_ids_.size());
}

void UserScriptLoader::AttemptLoad(ScriptsLoadedCallback callback) {
  bool scripts_changed = ScriptsMayHaveChanged();
  if (!callback.is_null()) {
    // If an operation will change the set of loaded scripts, add the callback
    // to |queued_load_callbacks_|. Otherwise, we run the callback immediately.
    if (scripts_changed) {
      queued_load_callbacks_.push_back(std::move(callback));
    } else {
      std::move(callback).Run(this,
                              std::make_optional(kNoScriptChangesErrorMsg));
    }
  }

  // If the loader isn't ready yet, the load will be kicked off when it becomes
  // ready.
  if (ready_ && scripts_changed) {
    if (is_loading()) {
      queued_load_ = true;
    } else {
      StartLoad();
    }
  }
}

void UserScriptLoader::StartLoad() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_loading());

  // Reload any loaded scripts, and clear out |loaded_scripts_| to indicate that
  // the scripts aren't currently ready.
  UserScriptList scripts_to_load = std::move(*loaded_scripts_);
  loaded_scripts_.reset();

  // Filter out any scripts that are queued for removal.
  std::erase_if(scripts_to_load,
                [this](const std::unique_ptr<UserScript>& script) {
                  return removed_script_ids_.count(script->id()) > 0u;
                });

  // Since all scripts managed by an instance of this class should have unique
  // IDs, remove any already loaded scripts from `scripts_to_load` that will be
  // updated from `added_scripts_map_`.
  std::erase_if(scripts_to_load,
                [this](const std::unique_ptr<UserScript>& script) {
                  return added_scripts_map_.count(script->id()) > 0u;
                });

  std::set<std::string> added_script_ids;
  scripts_to_load.reserve(scripts_to_load.size() + added_scripts_map_.size());
  for (auto& id_and_script : added_scripts_map_) {
    std::unique_ptr<UserScript>& script = id_and_script.second;
    added_script_ids.insert(script->id());
    // Move script from |added_scripts_map_| into |scripts_to_load|.
    scripts_to_load.push_back(std::move(script));
  }

  // All queued updates are now being loaded. Similarly, move all
  // |queued_load_callbacks_| to |loading_callbacks_|.
  loading_callbacks_.splice(loading_callbacks_.end(), queued_load_callbacks_);
  LoadScripts(std::move(scripts_to_load), added_script_ids,
              base::BindOnce(&UserScriptLoader::OnScriptsLoaded,
                             weak_factory_.GetWeakPtr()));

  added_scripts_map_.clear();
  removed_script_ids_.clear();
}

bool UserScriptLoader::HasLoadedScripts() const {
  // There are loaded scripts if all three conditions are met:
  // 1) The initial load was completed and no load queued.
  // 2) At least one script was loaded, as a direct result of 1).
  // 3) There are no pending script changes.
  return (loaded_scripts_ && !loaded_scripts_->empty() &&
          added_scripts_map_.empty() && removed_script_ids_.empty());
}

// static
base::ReadOnlySharedMemoryRegion UserScriptLoader::Serialize(
    const UserScriptList& scripts) {
  base::Pickle pickle;
  pickle.WriteUInt32(scripts.size());
  for (const std::unique_ptr<UserScript>& script : scripts) {
    // TODO(aa): This can be replaced by sending content script metadata to
    // renderers along with other extension data in ExtensionMsg_Loaded.
    // See crbug.com/70516.
    script->Pickle(&pickle);
    // Write scripts as 'data' so that we can read it out in the slave without
    // allocating a new string.
    for (const std::unique_ptr<UserScript::Content>& js_file :
         script->js_scripts()) {
      std::string_view contents = js_file->GetContent();
      pickle.WriteData(contents.data(), contents.length());
    }
    for (const std::unique_ptr<UserScript::Content>& css_file :
         script->css_scripts()) {
      std::string_view contents = css_file->GetContent();
      pickle.WriteData(contents.data(), contents.length());
    }
  }

  // Create the shared memory object.
  base::MappedReadOnlyRegion shared_memory =
      base::ReadOnlySharedMemoryRegion::Create(pickle.size());
  if (!shared_memory.IsValid())
    return {};

  // Copy the pickle to shared memory.
  memcpy(shared_memory.mapping.memory(), pickle.data(), pickle.size());
  return std::move(shared_memory.region);
}

void UserScriptLoader::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UserScriptLoader::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UserScriptLoader::StartLoadForTesting(ScriptsLoadedCallback callback) {
  if (!callback.is_null())
    queued_load_callbacks_.push_back(std::move(callback));
  if (is_loading()) {
    queued_load_ = true;
  } else {
    StartLoad();
  }
}

void UserScriptLoader::SetReady(bool ready) {
  bool was_ready = ready_;
  ready_ = ready;
  if (ready_ && !was_ready)
    AttemptLoad(UserScriptLoader::ScriptsLoadedCallback());
}

void UserScriptLoader::OnScriptsLoaded(
    UserScriptList user_scripts,
    base::ReadOnlySharedMemoryRegion shared_memory) {
  loaded_scripts_ = std::move(user_scripts);

  if (queued_load_) {
    // While we were loading, there were further changes. Don't bother
    // notifying about these scripts and instead just immediately reload.
    queued_load_ = false;
    StartLoad();
    return;
  }

  if (!shared_memory.IsValid()) {
    // This can happen if we run out of file descriptors.  In that case, we
    // have a choice between silently omitting all user scripts for new tabs,
    // by nulling out shared_memory_, or only silently omitting new ones by
    // leaving the existing object in place. The second seems less bad, even
    // though it removes the possibility that freeing the shared memory block
    // would open up enough FDs for long enough for a retry to succeed.

    // Pretend the extension change didn't happen.
    return;
  }

  // We've got scripts ready to go.
  shared_memory_ = std::move(shared_memory);

  std::vector<int> ids_of_newly_notified_processes;
  for (content::RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    content::RenderProcessHost* process = i.GetCurrentValue();
    SendUpdateResult update_result = SendUpdate(process, shared_memory_);
    if (update_result == SendUpdateResult::kRendererHasBeenNotified) {
      ids_of_newly_notified_processes.push_back(process->GetID());
    }
  }

  for (auto& observer : observers_)
    observer.OnScriptsLoaded(this, browser_context_);

  // Move callbacks in |loading_callbacks_| into a temporary container. This
  // guards callbacks which modify |loading_callbacks_| mid-iteration.
  std::list<ScriptsLoadedCallback> loaded_callbacks;
  loaded_callbacks.splice(loaded_callbacks.end(), loading_callbacks_);
  for (auto& callback : loaded_callbacks)
    std::move(callback).Run(this, /*error=*/std::nullopt);

  // Notify `ScriptInjectionTracker` at the very end - *after* all the observers
  // and callbacks above have already been run. In particular, this needs to
  // happen after `ExtensionUserScriptLoader::OnDynamicScriptsAdded` has been
  // called if needed (see also https://crbug.com/1439642).
  for (const int& process_id : ids_of_newly_notified_processes) {
    // It's theoretically possible the process was destroyed by one of the
    // callbacks or observers above.
    content::RenderProcessHost* process =
        content::RenderProcessHost::FromID(process_id);
    if (process) {
      ScriptInjectionTracker::DidUpdateScriptsInRenderer(
          base::PassKey<UserScriptLoader>(), host_id_, *process);
    }
  }
}

UserScriptLoader::SendUpdateResult UserScriptLoader::SendUpdate(
    content::RenderProcessHost* process,
    const base::ReadOnlySharedMemoryRegion& shared_memory) {
  // Make sure we only send user scripts to processes in our browser_context.
  if (!ExtensionsBrowserClient::Get()->IsSameContext(
          browser_context_, process->GetBrowserContext())) {
    return SendUpdateResult::kNoActionTaken;
  }

  // If the process is being started asynchronously, early return.  We'll end up
  // calling InitUserScripts when it's created which will call this again.
  base::ProcessHandle handle = process->GetProcess().Handle();
  if (!handle) {
    return SendUpdateResult::kNoActionTaken;
  }

  base::ReadOnlySharedMemoryRegion region_for_process =
      shared_memory.Duplicate();
  if (!region_for_process.IsValid()) {
    return SendUpdateResult::kNoActionTaken;
  }

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  // If the process only hosts guest frames, then those guest frames share the
  // same embedder/owner. In this case, only scripts from allowlisted hosts or
  // from the guest frames' owner should be injected.
  // Concrete example: This prevents a scenario where manifest scripts from
  // other extensions are injected into webviews.
  if (process->IsForGuestsOnly() &&
      !CanExecuteScriptEverywhere(browser_context_, host_id())) {
    DCHECK(WebViewRendererState::GetInstance()->IsGuest(process->GetID()));

    std::string owner_host;
    bool found_owner = WebViewRendererState::GetInstance()->GetOwnerInfo(
        process->GetID(), /*owner_process_id=*/nullptr, &owner_host);
    DCHECK(found_owner);

    // Keep this check in sync with the approach and formatting in:
    // - UserScriptLoader's HostID, created in |GenerateHostIDFromEmbedder()|
    // - ScriptContextSet's HostID, created in |ScriptContextSet::Register()|
    // - GuestView's owner host, set in |GuestViewBase::SetOwnerHost()|
    switch (host_id().type) {
      case mojom::HostID::HostType::kExtensions:
      case mojom::HostID::HostType::kWebUi:
      case mojom::HostID::HostType::kControlledFrameEmbedder:
        // For extensions, |owner_host| will be the extension ID.
        // For WebUI, |owner_host| will be the full owning RFH's URL spec,
        // including trailing slash.
        // For Controlled Frame embedders, |owner_host| will be a serialized
        // form of the embedder's origin, using scheme, host, port [if
        // applicable] tuple in origin form. No trailing slash.
        // TODO(crbug.com/41490369): Use an actual Origin object in the
        // Controlled Frame comparison rather than a serialized Origin.
        if (owner_host != host_id().id) {
          return SendUpdateResult::kNoActionTaken;
        }
        break;
    }
  }
#endif

  mojom::Renderer* renderer =
      RendererStartupHelperFactory::GetForBrowserContext(browser_context())
          ->GetRenderer(process);
  renderer->UpdateUserScripts(std::move(region_for_process),
                              mojom::HostID::New(host_id().type, host_id().id));
  return SendUpdateResult::kRendererHasBeenNotified;
}

}  // namespace extensions
