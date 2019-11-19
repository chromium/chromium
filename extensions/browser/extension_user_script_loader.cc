// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_user_script_loader.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/one_shot_event.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/version.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#include "extensions/common/message_bundle.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserContext;

namespace extensions {

namespace {

using SubstitutionMap = std::map<std::string, std::string>;

struct VerifyContentInfo {
  VerifyContentInfo(const scoped_refptr<ContentVerifier>& verifier,
                    const ExtensionId& extension_id,
                    const base::FilePath& extension_root,
                    const base::FilePath relative_path,
                    const std::string& content)
      : verifier(verifier),
        extension_id(extension_id),
        extension_root(extension_root),
        relative_path(relative_path),
        content(content) {}

  scoped_refptr<ContentVerifier> verifier;
  ExtensionId extension_id;
  base::FilePath extension_root;
  base::FilePath relative_path;
  std::string content;
};

// Verifies file contents as they are read.
void VerifyContent(const VerifyContentInfo& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(info.verifier);
  scoped_refptr<ContentVerifyJob> job(info.verifier->CreateAndStartJobFor(
      info.extension_id, info.extension_root, info.relative_path));
  if (job.get()) {
    job->Read(info.content.data(), info.content.size(), MOJO_RESULT_OK);
    job->Done();
  }
}

void ForwardVerifyContentToIO(const VerifyContentInfo& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 base::BindOnce(&VerifyContent, info));
}

// Loads user scripts from the extension who owns these scripts.
bool LoadScriptContent(const HostID& host_id,
                       UserScript::File* script_file,
                       const SubstitutionMap* localization_messages,
                       const scoped_refptr<ContentVerifier>& verifier) {
  DCHECK(script_file);
  std::string content;
  const base::FilePath& path = ExtensionResource::GetFilePath(
      script_file->extension_root(), script_file->relative_path(),
      ExtensionResource::SYMLINKS_MUST_RESOLVE_WITHIN_ROOT);
  if (path.empty()) {
    int resource_id = 0;
    if (ExtensionsBrowserClient::Get()
            ->GetComponentExtensionResourceManager()
            ->IsComponentExtensionResource(script_file->extension_root(),
                                           script_file->relative_path(),
                                           &resource_id)) {
      const ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      DCHECK(!rb.IsGzipped(resource_id));
      content = rb.GetRawDataResource(resource_id).as_string();
    } else {
      LOG(WARNING) << "Failed to get file path to "
                   << script_file->relative_path().value() << " from "
                   << script_file->extension_root().value();
      return false;
    }
  } else {
    if (!base::ReadFileToString(path, &content)) {
      LOG(WARNING) << "Failed to load user script file: " << path.value();
      return false;
    }
    if (verifier.get()) {
      // Call VerifyContent() after yielding on UI thread so it is ensured that
      // ContentVerifierIOData is populated at the time we call VerifyContent().
      // Priority set explicitly to avoid unwanted task priority inheritance.
      base::PostTask(
          FROM_HERE,
          {content::BrowserThread::UI, base::TaskPriority::USER_BLOCKING},
          base::BindOnce(
              &ForwardVerifyContentToIO,
              VerifyContentInfo(verifier, host_id.id(),
                                script_file->extension_root(),
                                script_file->relative_path(), content)));
    }
  }

  // Localize the content.
  if (localization_messages) {
    std::string error;
    MessageBundle::ReplaceMessagesWithExternalDictionary(*localization_messages,
                                                         &content, &error);
    if (!error.empty())
      LOG(WARNING) << "Failed to replace messages in script: " << error;
  }

  // Remove BOM from the content.
  if (base::StartsWith(content, base::kUtf8ByteOrderMark,
                       base::CompareCase::SENSITIVE)) {
    script_file->set_content(content.substr(strlen(base::kUtf8ByteOrderMark)));
  } else {
    script_file->set_content(content);
  }

  return true;
}

SubstitutionMap* GetLocalizationMessages(
    const ExtensionUserScriptLoader::HostsInfo& hosts_info,
    const HostID& host_id) {
  auto iter = hosts_info.find(host_id);
  if (iter == hosts_info.end())
    return nullptr;
  return file_util::LoadMessageBundleSubstitutionMap(
      iter->second.first, host_id.id(), iter->second.second);
}

void LoadUserScripts(UserScriptList* user_scripts,
                     const ExtensionUserScriptLoader::HostsInfo& hosts_info,
                     const std::set<int>& added_script_ids,
                     const scoped_refptr<ContentVerifier>& verifier) {
  for (const std::unique_ptr<UserScript>& script : *user_scripts) {
    if (added_script_ids.count(script->id()) == 0)
      continue;
    for (const std::unique_ptr<UserScript::File>& script_file :
         script->js_scripts()) {
      if (script_file->GetContent().empty())
        LoadScriptContent(script->host_id(), script_file.get(), nullptr,
                          verifier);
    }
    if (script->css_scripts().size() > 0) {
      std::unique_ptr<SubstitutionMap> localization_messages(
          GetLocalizationMessages(hosts_info, script->host_id()));
      for (const std::unique_ptr<UserScript::File>& script_file :
           script->css_scripts()) {
        if (script_file->GetContent().empty()) {
          LoadScriptContent(script->host_id(), script_file.get(),
                            localization_messages.get(), verifier);
        }
      }
    }
  }
}

void LoadScriptsOnFileTaskRunner(
    std::unique_ptr<UserScriptList> user_scripts,
    const ExtensionUserScriptLoader::HostsInfo& hosts_info,
    const std::set<int>& added_script_ids,
    const scoped_refptr<ContentVerifier>& verifier,
    UserScriptLoader::LoadScriptsCallback callback) {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(user_scripts.get());
  LoadUserScripts(user_scripts.get(), hosts_info, added_script_ids, verifier);
  base::ReadOnlySharedMemoryRegion memory =
      UserScriptLoader::Serialize(*user_scripts);
  // Explicit priority to prevent unwanted task priority inheritance.
  base::PostTask(
      FROM_HERE,
      {content::BrowserThread::UI, base::TaskPriority::USER_BLOCKING},
      base::BindOnce(std::move(callback), std::move(user_scripts),
                     std::move(memory)));
}

}  // namespace

ExtensionUserScriptLoader::ExtensionUserScriptLoader(
    BrowserContext* browser_context,
    const HostID& host_id,
    bool listen_for_extension_system_loaded)
    : UserScriptLoader(browser_context, host_id),
      content_verifier_(
          ExtensionSystem::Get(browser_context)->content_verifier()) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context));
  if (listen_for_extension_system_loaded) {
    ExtensionSystem::Get(browser_context)
        ->ready()
        .Post(FROM_HERE,
              base::BindOnce(&ExtensionUserScriptLoader::OnExtensionSystemReady,
                             weak_factory_.GetWeakPtr()));
  } else {
    SetReady(true);
  }
}

ExtensionUserScriptLoader::~ExtensionUserScriptLoader() {
}

void ExtensionUserScriptLoader::LoadScriptsForTest(
    UserScriptList* user_scripts) {
  HostsInfo info;
  std::set<int> added_script_ids;
  for (const std::unique_ptr<UserScript>& script : *user_scripts)
    added_script_ids.insert(script->id());

  LoadUserScripts(user_scripts, info, added_script_ids,
                  nullptr /* no verifier for testing */);
}

void ExtensionUserScriptLoader::LoadScripts(
    std::unique_ptr<UserScriptList> user_scripts,
    const std::set<HostID>& changed_hosts,
    const std::set<int>& added_script_ids,
    LoadScriptsCallback callback) {
  UpdateHostsInfo(changed_hosts);

  GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&LoadScriptsOnFileTaskRunner, std::move(user_scripts),
                     hosts_info_, added_script_ids, content_verifier_,
                     std::move(callback)));
}

void ExtensionUserScriptLoader::UpdateHostsInfo(
    const std::set<HostID>& changed_hosts) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  for (const HostID& host_id : changed_hosts) {
    const Extension* extension =
        registry->GetExtensionById(host_id.id(), ExtensionRegistry::ENABLED);
    // |changed_hosts_| may include hosts that have been removed,
    // which leads to the above lookup failing. In this case, just continue.
    if (!extension)
      continue;
    if (hosts_info_.find(host_id) != hosts_info_.end())
      continue;
    hosts_info_[host_id] = ExtensionSet::ExtensionPathAndDefaultLocale(
        extension->path(), LocaleInfo::GetDefaultLocale(extension));
  }
}

void ExtensionUserScriptLoader::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  hosts_info_.erase(HostID(HostID::EXTENSIONS, extension->id()));
}

void ExtensionUserScriptLoader::OnExtensionSystemReady() {
  SetReady(true);
}

}  // namespace extensions
