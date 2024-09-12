// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/extension_installer.h"

#include <optional>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension_id.h"

namespace extensions {

namespace {
using InstallError = update_client::InstallError;
using Result = update_client::CrxInstaller::Result;
}  // namespace

ExtensionInstaller::ExtensionInstaller(
    ExtensionId extension_id,
    const base::FilePath& extension_root,
    bool install_immediately,
    ExtensionInstallerCallback extension_installer_callback)
    : extension_id_(extension_id),
      extension_root_(extension_root),
      install_immediately_(install_immediately),
      extension_installer_callback_(extension_installer_callback) {}

void ExtensionInstaller::OnUpdateError(int error) {
  VLOG(1) << "OnUpdateError (" << extension_id_ << ") " << error;
}

void ExtensionInstaller::Install(
    const base::FilePath& unpack_path,
    const std::string& public_key,
    std::unique_ptr<InstallParams> /*install_params*/,
    ProgressCallback /*progress_callback*/,
    UpdateClientCallback update_client_callback) {
  auto ui_thread = content::GetUIThreadTaskRunner({});
  DCHECK(ui_thread);
  DCHECK(!extension_installer_callback_.is_null());
  if (base::PathExists(unpack_path)) {
    ui_thread->PostTask(
        FROM_HERE, base::BindOnce(extension_installer_callback_, extension_id_,
                                  public_key, unpack_path, install_immediately_,
                                  std::move(update_client_callback)));
    return;
  }
  ui_thread->PostTask(FROM_HERE,
                      base::BindOnce(std::move(update_client_callback),
                                     Result(InstallError::GENERIC_ERROR)));
}

std::optional<base::FilePath> ExtensionInstaller::GetInstalledFile(
    const std::string& file) {
  return std::nullopt;
}

bool ExtensionInstaller::Uninstall() {
  NOTREACHED_IN_MIGRATION();
  return false;
}

ExtensionInstaller::~ExtensionInstaller() = default;

}  // namespace extensions
