// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/common/shell_content_client.h"

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "components/nacl/common/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/shell/common/version.h"  // Generated file.
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(ENABLE_NACL)
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "components/nacl/common/nacl_constants.h"              // nogncheck
#include "components/nacl/renderer/plugin/ppapi_entrypoints.h"  // nogncheck
#include "content/public/common/pepper_plugin_info.h"           // nogncheck
#include "ppapi/shared_impl/ppapi_permissions.h"                // nogncheck
#endif

namespace extensions {
namespace {

#if BUILDFLAG(ENABLE_NACL)
bool GetNaClPluginPath(base::FilePath* path) {
  // On Posix, plugins live in the module directory.
  base::FilePath module;
  if (!base::PathService::Get(base::DIR_MODULE, &module))
    return false;
  *path = module.Append(nacl::kInternalNaClPluginFileName);
  return true;
}
#endif  // BUILDFLAG(ENABLE_NACL)

}  // namespace

ShellContentClient::ShellContentClient() {
}

ShellContentClient::~ShellContentClient() {
}

void ShellContentClient::AddPepperPlugins(
    std::vector<content::PepperPluginInfo>* plugins) {
#if BUILDFLAG(ENABLE_NACL)
  base::FilePath path;
  if (!GetNaClPluginPath(&path))
    return;

  content::PepperPluginInfo nacl;
  // The nacl plugin is now built into the binary.
  nacl.is_internal = true;
  nacl.path = path;
  nacl.name = nacl::kNaClPluginName;
  content::WebPluginMimeType nacl_mime_type(nacl::kNaClPluginMimeType,
                                            nacl::kNaClPluginExtension,
                                            nacl::kNaClPluginDescription);
  nacl.mime_types.push_back(nacl_mime_type);
  content::WebPluginMimeType pnacl_mime_type(nacl::kPnaclPluginMimeType,
                                             nacl::kPnaclPluginExtension,
                                             nacl::kPnaclPluginDescription);
  nacl.mime_types.push_back(pnacl_mime_type);
  nacl.internal_entry_points.get_interface = nacl_plugin::PPP_GetInterface;
  nacl.internal_entry_points.initialize_module =
      nacl_plugin::PPP_InitializeModule;
  nacl.internal_entry_points.shutdown_module =
      nacl_plugin::PPP_ShutdownModule;
  nacl.permissions = ppapi::PERMISSION_PRIVATE | ppapi::PERMISSION_DEV;
  plugins->push_back(nacl);
#endif  // BUILDFLAG(ENABLE_NACL)
}

void ShellContentClient::AddAdditionalSchemes(Schemes* schemes) {
  schemes->standard_schemes.push_back(extensions::kExtensionScheme);
  schemes->savable_schemes.push_back(kExtensionScheme);
  schemes->secure_schemes.push_back(kExtensionScheme);
  schemes->cors_enabled_schemes.push_back(kExtensionScheme);
  schemes->csp_bypassing_schemes.push_back(kExtensionScheme);
}

base::string16 ShellContentClient::GetLocalizedString(int message_id) {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece ShellContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ShellContentClient::GetDataResourceBytes(
    int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

gfx::Image& ShellContentClient::GetNativeImageNamed(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

}  // namespace extensions
