// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_extension_loader.h"

#include "apps/launcher.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"

namespace extensions {

namespace {

scoped_refptr<const Extension> LoadUnpacked(
    const base::FilePath& extension_dir) {
  // app_shell only supports unpacked extensions.
  // NOTE: If you add packed extension support consider removing the flag
  // FOLLOW_SYMLINKS_ANYWHERE below. Packed extensions should not have symlinks.
  if (!base::DirectoryExists(extension_dir)) {
    LOG(ERROR) << "Extension directory not found: "
               << extension_dir.AsUTF8Unsafe();
    return nullptr;
  }

  int load_flags = Extension::FOLLOW_SYMLINKS_ANYWHERE;
  std::u16string load_error;
  scoped_refptr<Extension> extension = file_util::LoadExtension(
      extension_dir, mojom::ManifestLocation::kCommandLine, load_flags,
      &load_error);
  if (!extension.get()) {
    LOG(ERROR) << "Loading extension at " << extension_dir.value()
               << " failed with: " << load_error;
    return nullptr;
  }

  // Log warnings.
  if (extension->install_warnings().size()) {
    LOG(WARNING) << "Warnings loading extension at " << extension_dir.value()
                 << ":";
    for (const auto& warning : extension->install_warnings())
      LOG(WARNING) << warning.message;
  }

  return extension;
}

}  // namespace

ShellExtensionLoader::ShellExtensionLoader(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      extension_registrar_(ExtensionRegistrar::Get(browser_context)),
      keep_alive_requester_(browser_context) {
  extension_registrar_->Init(
      this, /*extensions_enabled=*/true, base::CommandLine::ForCurrentProcess(),
      browser_context_->GetPath().AppendASCII(kInstallDirectoryName),
      browser_context_->GetPath().AppendASCII(kUnpackedInstallDirectoryName));
}

ShellExtensionLoader::~ShellExtensionLoader() = default;

const Extension* ShellExtensionLoader::LoadExtension(
    const base::FilePath& extension_dir) {
  scoped_refptr<const Extension> extension = LoadUnpacked(extension_dir);
  if (extension)
    extension_registrar_->AddExtension(extension);

  return extension.get();
}

void ShellExtensionLoader::ReloadExtension(ExtensionId extension_id) {
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->GetInstalledExtension(extension_id);
  // We shouldn't be trying to reload extensions that haven't been added.
  DCHECK(extension);

  // This should always start false since it's only set here, or in
  // LoadExtensionForReload() as a result of the call below.
  DCHECK_EQ(false, did_schedule_reload_);
  base::AutoReset<bool> reset_did_schedule_reload(&did_schedule_reload_, false);

  // Set up a keep-alive while the extension reloads. Do this before starting
  // the reload so that the first step, disabling the extension, doesn't release
  // the last remaining keep-alive and shut down the application.
  keep_alive_requester_.StartTrackingReload(extension);
  extension_registrar_->ReloadExtensionWithQuietFailure(extension_id);
  if (did_schedule_reload_)
    return;

  // ExtensionRegistrar didn't invoke us to schedule the reload, so the reload
  // wasn't actually started. Clear the keep-alive so we don't wait forever.
  keep_alive_requester_.StopTrackingReload(extension_id);
}

void ShellExtensionLoader::FinishExtensionReload(
    const ExtensionId old_extension_id,
    scoped_refptr<const Extension> extension) {
  if (extension) {
    extension_registrar_->AddExtension(extension);
    // If the extension is a platform app, adding it above caused
    // ShellKeepAliveRequester to create a new keep-alive to wait for the app to
    // open its first window.
    // Launch the app now.
    if (extension->is_platform_app())
      apps::LaunchPlatformApp(browser_context_, extension.get(),
                              AppLaunchSource::kSourceReload);
  }

  // Whether or not the reload succeeded, we should stop waiting for it.
  keep_alive_requester_.StopTrackingReload(old_extension_id);
}

void ShellExtensionLoader::PreAddExtension(const Extension* extension,
                                           const Extension* old_extension) {
  if (old_extension)
    return;

  // The extension might be disabled if a previous reload attempt failed. In
  // that case, we want to remove that disable reason.
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(browser_context_);
  extension_prefs->RemoveDisableReason(extension->id(),
                                       disable_reason::DISABLE_RELOAD);
}

void ShellExtensionLoader::OnAddNewOrUpdatedExtension(
    const Extension* extension) {}

void ShellExtensionLoader::PostActivateExtension(
    scoped_refptr<const Extension> extension) {}

void ShellExtensionLoader::PostDeactivateExtension(
    scoped_refptr<const Extension> extension) {}

void ShellExtensionLoader::PreUninstallExtension(
    scoped_refptr<const Extension> extension) {}

void ShellExtensionLoader::PostUninstallExtension(
    scoped_refptr<const Extension> extension,
    base::OnceClosure done_callback) {}

void ShellExtensionLoader::DoLoadExtensionForReload(
    const ExtensionId& extension_id,
    const base::FilePath& path) {
  CHECK(!path.empty());

  GetExtensionFileTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&LoadUnpacked, path),
      base::BindOnce(&ShellExtensionLoader::FinishExtensionReload,
                     weak_factory_.GetWeakPtr(), extension_id));
  did_schedule_reload_ = true;
}

void ShellExtensionLoader::LoadExtensionForReload(
    const ExtensionId& extension_id,
    const base::FilePath& path) {
  DoLoadExtensionForReload(extension_id, path);
}

void ShellExtensionLoader::LoadExtensionForReloadWithQuietFailure(
    const ExtensionId& extension_id,
    const base::FilePath& path) {
  DoLoadExtensionForReload(extension_id, path);
}

void ShellExtensionLoader::ShowExtensionDisabledError(
    const Extension* extension,
    bool is_remote_install) {}

bool ShellExtensionLoader::CanEnableExtension(const Extension* extension) {
  return true;
}

bool ShellExtensionLoader::CanDisableExtension(const Extension* extension) {
  // Extensions cannot be disabled by the user.
  return false;
}

void ShellExtensionLoader::GrantActivePermissions(const Extension* extension) {
  NOTIMPLEMENTED();
}

void ShellExtensionLoader::UpdateExternalExtensionAlert() {
  NOTIMPLEMENTED();
}

void ShellExtensionLoader::OnExtensionInstalled(
    const Extension* extension,
    const syncer::StringOrdinal& page_ordinal,
    int install_flags,
    base::Value::Dict ruleset_install_prefs) {
  NOTIMPLEMENTED();
}

}  // namespace extensions
