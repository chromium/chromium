// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/extension_installer.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

namespace {
using InstallError = update_client::InstallError;
using Result = update_client::CrxInstaller::Result;
}  // namespace

ExtensionInstaller::ExtensionInstaller(
    std::string extension_id,
    const base::FilePath& extension_root,
    bool install_immediately,
    ExtensionInstallerCallback extension_installer_callback)
    : extension_id_(extension_id),
      extension_root_(extension_root),
      install_immediately_(install_immediately),
      extension_installer_callback_(std::move(extension_installer_callback)) {}

void ExtensionInstaller::OnUpdateError(int error) {
  VLOG(1) << "OnUpdateError (" << extension_id_ << ") " << error;
}

void ExtensionInstaller::Install(const base::FilePath& unpack_path,
                                 const std::string& public_key,
                                 UpdateClientCallback update_client_callback) {
  auto ui_thread =
      base::CreateSingleThreadTaskRunner({content::BrowserThread::UI});
  DCHECK(ui_thread);
  DCHECK(!extension_installer_callback_.is_null());
  if (base::PathExists(unpack_path)) {
    ui_thread->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(extension_installer_callback_), extension_id_,
                       public_key, unpack_path, install_immediately_,
                       std::move(update_client_callback)));
    return;
  }
  ui_thread->PostTask(FROM_HERE,
                      base::BindOnce(std::move(update_client_callback),
                                     Result(InstallError::GENERIC_ERROR)));
}

bool ExtensionInstaller::GetInstalledFile(const std::string& file,
                                          base::FilePath* installed_file) {
  base::FilePath relative_path = base::FilePath::FromUTF8Unsafe(file);
  if (relative_path.IsAbsolute() || relative_path.ReferencesParent())
    return false;
  *installed_file = extension_root_.Append(relative_path);
  if (!extension_root_.IsParent(*installed_file) ||
      !base::PathExists(*installed_file)) {
    VLOG(1) << "GetInstalledFile failed to find " << installed_file->value();
    installed_file->clear();
    return false;
  }
  return true;
}

bool ExtensionInstaller::Uninstall() {
  NOTREACHED();
  return false;
}

ExtensionInstaller::~ExtensionInstaller() {}

}  // namespace extensions
