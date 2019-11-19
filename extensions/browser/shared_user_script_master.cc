// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/shared_user_script_master.h"

#include "extensions/browser/extension_util.h"
#include "extensions/common/host_id.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"

namespace extensions {

SharedUserScriptMaster::SharedUserScriptMaster(
    content::BrowserContext* browser_context)
    : loader_(browser_context,
              HostID(),
              true /* listen_for_extension_system_loaded */),
      browser_context_(browser_context) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

SharedUserScriptMaster::~SharedUserScriptMaster() {}

void SharedUserScriptMaster::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  loader_.AddScripts(GetScriptsMetadata(extension));
}

void SharedUserScriptMaster::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  const UserScriptList& script_list =
      ContentScriptsInfo::GetContentScripts(extension);
  std::set<UserScriptIDPair> scripts_to_remove;
  for (const std::unique_ptr<UserScript>& script : script_list)
    scripts_to_remove.insert(UserScriptIDPair(script->id(), script->host_id()));
  loader_.RemoveScripts(scripts_to_remove);
}

std::unique_ptr<UserScriptList> SharedUserScriptMaster::GetScriptsMetadata(
    const Extension* extension) {
  bool incognito_enabled =
      util::IsIncognitoEnabled(extension->id(), browser_context_);
  const UserScriptList& script_list =
      ContentScriptsInfo::GetContentScripts(extension);
  std::unique_ptr<UserScriptList> script_vector(new UserScriptList());
  script_vector->reserve(script_list.size());
  for (const std::unique_ptr<UserScript>& script : script_list) {
    std::unique_ptr<UserScript> script_copy =
        UserScript::CopyMetadataFrom(*script);
    script_copy->set_incognito_enabled(incognito_enabled);
    script_vector->push_back(std::move(script_copy));
  }
  return script_vector;
}

}  // namespace extensions
