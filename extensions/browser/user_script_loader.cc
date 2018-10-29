// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/user_script_loader.h"

#include <stddef.h>

#include <set>
#include <string>
#include <utility>

#include "base/version.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension_messages.h"

using content::BrowserThread;
using content::BrowserContext;

namespace extensions {

namespace {

#if DCHECK_IS_ON()
bool AreScriptsUnique(const UserScriptList& scripts) {
  std::set<int> script_ids;
  for (const std::unique_ptr<UserScript>& script : scripts) {
    if (script_ids.count(script->id()))
      return false;
    script_ids.insert(script->id());
  }
  return true;
}
#endif  // DCHECK_IS_ON()

// Helper function to parse greasesmonkey headers
bool GetDeclarationValue(const base::StringPiece& line,
                         const base::StringPiece& prefix,
                         std::string* value) {
  base::StringPiece::size_type index = line.find(prefix);
  if (index == base::StringPiece::npos)
    return false;

  std::string temp(line.data() + index + prefix.length(),
                   line.length() - index - prefix.length());

  if (temp.empty() || !base::IsUnicodeWhitespace(temp[0]))
    return false;

  base::TrimWhitespaceASCII(temp, base::TRIM_ALL, value);
  return true;
}

}  // namespace

// static
bool UserScriptLoader::ParseMetadataHeader(const base::StringPiece& script_text,
                                           UserScript* script) {
  // http://wiki.greasespot.net/Metadata_block
  base::StringPiece line;
  size_t line_start = 0;
  size_t line_end = line_start;
  bool in_metadata = false;

  static const base::StringPiece kUserScriptBegin("// ==UserScript==");
  static const base::StringPiece kUserScriptEng("// ==/UserScript==");
  static const base::StringPiece kNamespaceDeclaration("// @namespace");
  static const base::StringPiece kNameDeclaration("// @name");
  static const base::StringPiece kVersionDeclaration("// @version");
  static const base::StringPiece kDescriptionDeclaration("// @description");
  static const base::StringPiece kIncludeDeclaration("// @include");
  static const base::StringPiece kExcludeDeclaration("// @exclude");
  static const base::StringPiece kMatchDeclaration("// @match");
  static const base::StringPiece kExcludeMatchDeclaration("// @exclude_match");
  static const base::StringPiece kRunAtDeclaration("// @run-at");
  static const base::StringPiece kRunAtDocumentStartValue("document-start");
  static const base::StringPiece kRunAtDocumentEndValue("document-end");
  static const base::StringPiece kRunAtDocumentIdleValue("document-idle");

  while (line_start < script_text.length()) {
    line_end = script_text.find('\n', line_start);

    // Handle the case where there is no trailing newline in the file.
    if (line_end == std::string::npos)
      line_end = script_text.length() - 1;

    line.set(script_text.data() + line_start, line_end - line_start);

    if (!in_metadata) {
      if (line.starts_with(kUserScriptBegin))
        in_metadata = true;
    } else {
      if (line.starts_with(kUserScriptEng))
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
          script->set_run_location(UserScript::DOCUMENT_START);
        else if (value == kRunAtDocumentEndValue)
          script->set_run_location(UserScript::DOCUMENT_END);
        else if (value == kRunAtDocumentIdleValue)
          script->set_run_location(UserScript::DOCUMENT_IDLE);
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
                                   const HostID& host_id)
    : user_scripts_(new UserScriptList()),
      clear_scripts_(false),
      ready_(false),
      pending_load_(false),
      browser_context_(browser_context),
      host_id_(host_id),
      weak_factory_(this) {
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

UserScriptLoader::~UserScriptLoader() {
  for (auto& observer : observers_)
    observer.OnUserScriptLoaderDestroyed(this);
}

void UserScriptLoader::AddScripts(std::unique_ptr<UserScriptList> scripts) {
#if DCHECK_IS_ON()
  // |scripts| with non-unique IDs will work, but that would indicate we are
  // doing something wrong somewhere, so DCHECK that.
  DCHECK(AreScriptsUnique(*scripts))
      << "AddScripts() expects scripts with unique IDs.";
#endif  // DCHECK_IS_ON()
  for (std::unique_ptr<UserScript>& user_script : *scripts) {
    int id = user_script->id();
    removed_script_hosts_.erase(UserScriptIDPair(id));
    if (added_scripts_map_.count(id) == 0)
      added_scripts_map_[id] = std::move(user_script);
  }
  AttemptLoad();
}

void UserScriptLoader::AddScripts(std::unique_ptr<UserScriptList> scripts,
                                  int render_process_id,
                                  int render_frame_id) {
  AddScripts(std::move(scripts));
}

void UserScriptLoader::RemoveScripts(
    const std::set<UserScriptIDPair>& scripts) {
  for (const UserScriptIDPair& id_pair : scripts) {
    removed_script_hosts_.insert(UserScriptIDPair(id_pair.id, id_pair.host_id));
    // TODO(lazyboy): We shouldn't be trying to remove scripts that were never
    // a) added to |added_scripts_map_| or b) being loaded or has done loading
    // through |user_scripts_|. This would reduce sending redundant IPC.
    added_scripts_map_.erase(id_pair.id);
  }
  AttemptLoad();
}

void UserScriptLoader::ClearScripts() {
  clear_scripts_ = true;
  added_scripts_map_.clear();
  removed_script_hosts_.clear();
  AttemptLoad();
}

void UserScriptLoader::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_RENDERER_PROCESS_CREATED);
  content::RenderProcessHost* process =
      content::Source<content::RenderProcessHost>(source).ptr();
  if (!ExtensionsBrowserClient::Get()->IsSameContext(
          browser_context_, process->GetBrowserContext()))
    return;
  if (scripts_ready()) {
    SendUpdate(process, shared_memory_.get(),
               std::set<HostID>());  // Include all hosts.
  }
}

bool UserScriptLoader::ScriptsMayHaveChanged() const {
  // Scripts may have changed if there are scripts added, scripts removed, or
  // if scripts were cleared and either:
  // (1) A load is in progress (which may result in a non-zero number of
  //     scripts that need to be cleared), or
  // (2) The current set of scripts is non-empty (so they need to be cleared).
  return (added_scripts_map_.size() || removed_script_hosts_.size() ||
          (clear_scripts_ && (is_loading() || user_scripts_->size())));
}

void UserScriptLoader::AttemptLoad() {
  if (ready_ && ScriptsMayHaveChanged()) {
    if (is_loading())
      pending_load_ = true;
    else
      StartLoad();
  }
}

void UserScriptLoader::StartLoad() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_loading());

  // If scripts were marked for clearing before adding and removing, then clear
  // them.
  if (clear_scripts_) {
    user_scripts_->clear();
  } else {
    for (auto it = user_scripts_->begin(); it != user_scripts_->end();) {
      UserScriptIDPair id_pair(it->get()->id());
      if (removed_script_hosts_.count(id_pair) > 0u)
        it = user_scripts_->erase(it);
      else
        ++it;
    }
  }

  std::set<int> added_script_ids;
  user_scripts_->reserve(user_scripts_->size() + added_scripts_map_.size());
  for (auto& id_and_script : added_scripts_map_) {
    std::unique_ptr<UserScript>& script = id_and_script.second;
    added_script_ids.insert(script->id());
    // Expand |changed_hosts_| for OnScriptsLoaded, which will use it in
    // its IPC message. This must be done before we clear |added_scripts_map_|
    // and |removed_script_hosts_| below.
    changed_hosts_.insert(script->host_id());
    // Move script from |added_scripts_map_| into |user_scripts_|.
    user_scripts_->push_back(std::move(script));
  }
  for (const UserScriptIDPair& id_pair : removed_script_hosts_)
    changed_hosts_.insert(id_pair.host_id);

  LoadScripts(std::move(user_scripts_), changed_hosts_, added_script_ids,
              base::Bind(&UserScriptLoader::OnScriptsLoaded,
                         weak_factory_.GetWeakPtr()));

  clear_scripts_ = false;
  added_scripts_map_.clear();
  removed_script_hosts_.clear();
  user_scripts_.reset();
}

// static
std::unique_ptr<base::SharedMemory> UserScriptLoader::Serialize(
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
    for (const std::unique_ptr<UserScript::File>& js_file :
         script->js_scripts()) {
      base::StringPiece contents = js_file->GetContent();
      pickle.WriteData(contents.data(), contents.length());
    }
    for (const std::unique_ptr<UserScript::File>& css_file :
         script->css_scripts()) {
      base::StringPiece contents = css_file->GetContent();
      pickle.WriteData(contents.data(), contents.length());
    }
  }

  // Create the shared memory object.
  base::SharedMemory shared_memory;

  base::SharedMemoryCreateOptions options;
  options.size = pickle.size();
  options.share_read_only = true;
  if (!shared_memory.Create(options))
    return std::unique_ptr<base::SharedMemory>();

  if (!shared_memory.Map(pickle.size()))
    return std::unique_ptr<base::SharedMemory>();

  // Copy the pickle to shared memory.
  memcpy(shared_memory.memory(), pickle.data(), pickle.size());

  base::SharedMemoryHandle readonly_handle = shared_memory.GetReadOnlyHandle();
  if (!readonly_handle.IsValid())
    return std::unique_ptr<base::SharedMemory>();

#if defined(OS_ANDROID)
  // Seal the region read-only now. http://crbug.com/789959
  readonly_handle.SetRegionReadOnly();
#endif

  return std::make_unique<base::SharedMemory>(readonly_handle,
                                              /*read_only=*/true);
}

void UserScriptLoader::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UserScriptLoader::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UserScriptLoader::SetReady(bool ready) {
  bool was_ready = ready_;
  ready_ = ready;
  if (ready_ && !was_ready)
    AttemptLoad();
}

void UserScriptLoader::OnScriptsLoaded(
    std::unique_ptr<UserScriptList> user_scripts,
    std::unique_ptr<base::SharedMemory> shared_memory) {
  user_scripts_ = std::move(user_scripts);
  if (pending_load_) {
    // While we were loading, there were further changes. Don't bother
    // notifying about these scripts and instead just immediately reload.
    pending_load_ = false;
    StartLoad();
    return;
  }

  if (shared_memory.get() == NULL) {
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

  for (content::RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    SendUpdate(i.GetCurrentValue(), shared_memory_.get(), changed_hosts_);
  }
  changed_hosts_.clear();

  // TODO(hanxi): Remove the NOTIFICATION_USER_SCRIPTS_UPDATED.
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::Source<BrowserContext>(browser_context_),
      content::Details<base::SharedMemory>(shared_memory_.get()));
  for (auto& observer : observers_)
    observer.OnScriptsLoaded(this);
}

void UserScriptLoader::SendUpdate(content::RenderProcessHost* process,
                                  base::SharedMemory* shared_memory,
                                  const std::set<HostID>& changed_hosts) {
  // Don't allow injection of non-whitelisted extensions' content scripts
  // into <webview>.
  bool whitelisted_only = process->IsForGuestsOnly() && host_id().id().empty();

  // Make sure we only send user scripts to processes in our browser_context.
  if (!ExtensionsBrowserClient::Get()->IsSameContext(
          browser_context_, process->GetBrowserContext()))
    return;

  // If the process is being started asynchronously, early return.  We'll end up
  // calling InitUserScripts when it's created which will call this again.
  base::ProcessHandle handle = process->GetProcess().Handle();
  if (!handle)
    return;

  base::SharedMemoryHandle handle_for_process =
      shared_memory->handle().Duplicate();
  if (!handle_for_process.IsValid())
    return;

  process->Send(new ExtensionMsg_UpdateUserScripts(
      handle_for_process, host_id(), changed_hosts, whitelisted_only));
}

}  // namespace extensions
