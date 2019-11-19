// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/api/file_system/shell_file_system_delegate.h"

#include "apps/saved_files_service.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "extensions/browser/api/file_system/saved_files_service_interface.h"

namespace extensions {

ShellFileSystemDelegate::ShellFileSystemDelegate() = default;

ShellFileSystemDelegate::~ShellFileSystemDelegate() {}

base::FilePath ShellFileSystemDelegate::GetDefaultDirectory() {
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
    const base::string16& app_name,
    content::WebContents* web_contents,
    const base::Closure& on_accept,
    const base::Closure& on_cancel) {
  NOTIMPLEMENTED();

  // Run the cancel callback by default.
  on_cancel.Run();
}

int ShellFileSystemDelegate::GetDescriptionIdForAcceptType(
    const std::string& accept_type) {
  NOTIMPLEMENTED();
  return 0;
}

SavedFilesServiceInterface* ShellFileSystemDelegate::GetSavedFilesService(
    content::BrowserContext* browser_context) {
  return apps::SavedFilesService::Get(browser_context);
}

}  // namespace extensions
