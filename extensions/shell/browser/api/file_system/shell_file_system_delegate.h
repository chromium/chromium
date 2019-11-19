// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_API_FILE_SYSTEM_SHELL_FILE_SYSTEM_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_API_FILE_SYSTEM_SHELL_FILE_SYSTEM_DELEGATE_H_

#include "extensions/browser/api/file_system/file_system_delegate.h"

namespace extensions {

class ShellFileSystemDelegate : public FileSystemDelegate {
 public:
  ShellFileSystemDelegate();
  ~ShellFileSystemDelegate() override;

  // FileSystemDelegate:
  base::FilePath GetDefaultDirectory() override;
  bool ShowSelectFileDialog(
      scoped_refptr<ExtensionFunction> extension_function,
      ui::SelectFileDialog::Type type,
      const base::FilePath& default_path,
      const ui::SelectFileDialog::FileTypeInfo* file_types,
      FileSystemDelegate::FilesSelectedCallback files_selected_callback,
      base::OnceClosure file_selection_canceled_callback) override;
  void ConfirmSensitiveDirectoryAccess(bool has_write_permission,
                                       const base::string16& app_name,
                                       content::WebContents* web_contents,
                                       const base::Closure& on_accept,
                                       const base::Closure& on_cancel) override;
  int GetDescriptionIdForAcceptType(const std::string& accept_type) override;
  SavedFilesServiceInterface* GetSavedFilesService(
      content::BrowserContext* browser_context) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellFileSystemDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_API_FILE_SYSTEM_SHELL_FILE_SYSTEM_DELEGATE_H_
