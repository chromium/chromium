// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_API_FILE_SYSTEM_SHELL_FILE_SYSTEM_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_API_FILE_SYSTEM_SHELL_FILE_SYSTEM_DELEGATE_H_

#include "extensions/browser/api/file_system/file_system_delegate.h"

#include "build/build_config.h"

namespace extensions {

class ConsentProvider;

class ShellFileSystemDelegate : public FileSystemDelegate {
 public:
  ShellFileSystemDelegate();

  ShellFileSystemDelegate(const ShellFileSystemDelegate&) = delete;
  ShellFileSystemDelegate& operator=(const ShellFileSystemDelegate&) = delete;

  ~ShellFileSystemDelegate() override;

  // FileSystemDelegate:
  base::FilePath GetDefaultDirectory() override;
  base::FilePath GetManagedSaveAsDirectory(
      content::BrowserContext* browser_context,
      const Extension& extension) override;
  bool ShowSelectFileDialog(
      scoped_refptr<ExtensionFunction> extension_function,
      ui::SelectFileDialog::Type type,
      const base::FilePath& default_path,
      const ui::SelectFileDialog::FileTypeInfo* file_types,
      FileSystemDelegate::FilesSelectedCallback files_selected_callback,
      base::OnceClosure file_selection_canceled_callback) override;
  void ConfirmSensitiveDirectoryAccess(bool has_write_permission,
                                       const std::u16string& app_name,
                                       content::WebContents* web_contents,
                                       base::OnceClosure on_accept,
                                       base::OnceClosure on_cancel) override;
  int GetDescriptionIdForAcceptType(const std::string& accept_type) override;
#if BUILDFLAG(IS_CHROMEOS)
  void RequestFileSystem(content::BrowserContext* browser_context,
                         scoped_refptr<ExtensionFunction> requester,
                         ConsentProvider* consent_provider,
                         const Extension& extension,
                         std::string volume_id,
                         bool writable,
                         FileSystemCallback success_callback,
                         ErrorCallback error_callback) override;
  void GetVolumeList(content::BrowserContext* browser_context,
                     VolumeListCallback success_callback,
                     ErrorCallback error_callback) override;
#endif  // BUILDFLAG(IS_CHROMEOS)
  SavedFilesServiceInterface* GetSavedFilesService(
      content::BrowserContext* browser_context) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_API_FILE_SYSTEM_SHELL_FILE_SYSTEM_DELEGATE_H_
