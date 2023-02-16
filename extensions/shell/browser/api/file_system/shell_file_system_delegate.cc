// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/api/file_system/shell_file_system_delegate.h"

#include "apps/saved_files_service.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "extensions/browser/api/file_system/saved_files_service_interface.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

ShellFileSystemDelegate::ShellFileSystemDelegate() = default;

ShellFileSystemDelegate::~ShellFileSystemDelegate() = default;

base::FilePath ShellFileSystemDelegate::GetDefaultDirectory() {
  NOTIMPLEMENTED();
  return base::FilePath();
}

base::FilePath ShellFileSystemDelegate::GetManagedSaveAsDirectory(
    content::BrowserContext* browser_context,
    const Extension& extension) {
  NOTIMPLEMENTED();
  return base::FilePath();
}

bool ShellFileSystemDelegate::ShowSelectFileDialog(
    scoped_refptr<ExtensionFunction> extension_function,
    ui::SelectFileDialog::Type type,
    const base::FilePath& default_path,
    const ui::SelectFileDialog::FileTypeInfo* file_types,
    FileSystemDelegate::FilesSelectedCallback files_selected_callback,
    base::OnceClosure file_selection_canceled_callback) {
  NOTIMPLEMENTED();

  // Run the cancel callback by default.
  std::move(file_selection_canceled_callback).Run();

  // Return true since this isn't a disallowed call, just not implemented.
  return true;
}

void ShellFileSystemDelegate::ConfirmSensitiveDirectoryAccess(
    bool has_write_permission,
    const std::u16string& app_name,
    content::WebContents* web_contents,
    base::OnceClosure on_accept,
    base::OnceClosure on_cancel) {
  NOTIMPLEMENTED();

  // Run the cancel callback by default.
  std::move(on_cancel).Run();
}

int ShellFileSystemDelegate::GetDescriptionIdForAcceptType(
    const std::string& accept_type) {
  NOTIMPLEMENTED();
  return 0;
}

#if BUILDFLAG(IS_CHROMEOS)
void ShellFileSystemDelegate::RequestFileSystem(
    content::BrowserContext* browser_context,
    scoped_refptr<ExtensionFunction> requester,
    ConsentProvider* consent_provider,
    const Extension& extension,
    std::string volume_id,
    bool writable,
    FileSystemCallback success_callback,
    ErrorCallback error_callback) {}

void ShellFileSystemDelegate::GetVolumeList(
    content::BrowserContext* browser_context,
    VolumeListCallback success_callback,
    ErrorCallback error_callback) {}
#endif  // BUILDFLAG(IS_CHROMEOS)

SavedFilesServiceInterface* ShellFileSystemDelegate::GetSavedFilesService(
    content::BrowserContext* browser_context) {
  return apps::SavedFilesService::Get(browser_context);
}

}  // namespace extensions
