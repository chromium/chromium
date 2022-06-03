// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/pepper_plugin_list.h"

#include <stddef.h>
#include <stdint.h>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/pepper_plugin_info.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

namespace content {
namespace {

// The maximum number of plugins allowed to be registered from command line.
const size_t kMaxPluginsToRegisterFromCommandLine = 64;

// Appends any plugins from the command line to the given vector.
void ComputePluginsFromCommandLine(std::vector<PepperPluginInfo>* plugins) {
  // On Linux, once we're sandboxed, we can't know if a plugin is available or
  // not. But (on Linux) this function is always called once before we're
  // sandboxed. So when this function is called for the first time we set a
  // flag if the plugin file is available. Then we can skip the check on file
  // existence in subsequent calls if the flag is set.
  // NOTE: In theory we could have unlimited number of plugins registered in
  // command line. But in practice, 64 plugins should be more than enough.
  static uint64_t skip_file_check_flags = 0;
  static_assert(
      kMaxPluginsToRegisterFromCommandLine <= sizeof(skip_file_check_flags) * 8,
      "max plugins to register from command line exceeds limit");

  bool out_of_process = true;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPpapiInProcess))
    out_of_process = false;

  const std::string value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kRegisterPepperPlugins);
  if (value.empty())
    return;

  // FORMAT:
  // command-line = <plugin-entry> + *( LWS + "," + LWS + <plugin-entry> )
  // plugin-entry =
  //    <file-path> +
  //    ["#" + <name> + ["#" + <description> + ["#" + <version>]]] +
  //    *1( LWS + ";" + LWS + <mime-type-data> )
  // mime-type-data = <mime-type> + [ LWS + "#" + LWS + <extension> ]
  std::vector<std::string> modules = base::SplitString(
      value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  size_t plugins_to_register = modules.size();
  if (plugins_to_register > kMaxPluginsToRegisterFromCommandLine) {
    DVLOG(1) << plugins_to_register << " pepper plugins registered from"
             << " command line which exceeds the limit (maximum "
             << kMaxPluginsToRegisterFromCommandLine << " plugins allowed)";
    plugins_to_register = kMaxPluginsToRegisterFromCommandLine;
  }

  for (size_t i = 0; i < plugins_to_register; ++i) {
    std::vector<std::string> parts = base::SplitString(
        modules[i], ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (parts.size() < 2) {
      DVLOG(1) << "Required mime-type not found";
      continue;
    }

    std::vector<std::string> name_parts = base::SplitString(
        parts[0], "#", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    PepperPluginInfo plugin;
    plugin.is_out_of_process = out_of_process;
    plugin.path = base::FilePath::FromUTF8Unsafe(name_parts[0]);

    uint64_t index_mask = 1ULL << i;
    if (!(skip_file_check_flags & index_mask)) {
      if (base::PathExists(plugin.path)) {
        skip_file_check_flags |= index_mask;
      } else {
        DVLOG(1) << "Plugin doesn't exist: " << plugin.path.MaybeAsASCII();
        continue;
      }
    }

    if (name_parts.size() > 1)
      plugin.name = name_parts[1];
    if (name_parts.size() > 2)
      plugin.description = name_parts[2];
    if (name_parts.size() > 3)
      plugin.version = name_parts[3];
    for (size_t j = 1; j < parts.size(); ++j) {
      std::vector<std::string> mime_parts = base::SplitString(
          parts[j], "#", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      DCHECK_GE(mime_parts.size(), 1u);
      std::string mime_extension;
      if (mime_parts.size() > 1)
        mime_extension = mime_parts[1];
      WebPluginMimeType mime_type(mime_parts[0],
                                  mime_extension,
                                  plugin.description);
      plugin.mime_types.push_back(mime_type);
    }

    // If the plugin name is empty, use the filename.
    if (plugin.name.empty()) {
      plugin.name =
          base::UTF16ToUTF8(plugin.path.BaseName().LossyDisplayName());
    }

    // Command-line plugins get full permissions.
    plugin.permissions = ppapi::PERMISSION_ALL_BITS;

    plugins->push_back(plugin);
  }
}

}  // namespace

bool MakePepperPluginInfo(const WebPluginInfo& webplugin_info,
                          PepperPluginInfo* pepper_info) {
  if (!webplugin_info.is_pepper_plugin())
    return false;

  pepper_info->is_out_of_process =
      webplugin_info.type == WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS;

  pepper_info->path = base::FilePath(webplugin_info.path);
  pepper_info->name = base::UTF16ToASCII(webplugin_info.name);
  pepper_info->description = base::UTF16ToASCII(webplugin_info.desc);
  pepper_info->version = base::UTF16ToASCII(webplugin_info.version);
  pepper_info->mime_types = webplugin_info.mime_types;
  pepper_info->permissions = webplugin_info.pepper_permissions;

  return true;
}

void ComputePepperPluginList(std::vector<PepperPluginInfo>* plugins) {
  GetContentClient()->AddPepperPlugins(plugins);
  ComputePluginsFromCommandLine(plugins);
}

}  // namespace content
