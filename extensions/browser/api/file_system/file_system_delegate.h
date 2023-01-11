// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FILE_SYSTEM_FILE_SYSTEM_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_FILE_SYSTEM_FILE_SYSTEM_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/common/api/file_system.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class ExtensionFunction;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {

class ConsentProvider;
class Extension;
class SavedFilesServiceInterface;

// Delegate class for embedder-specific file system access.
class FileSystemDelegate {
 public:
  using ErrorCallback = base::OnceCallback<void(const std::string&)>;
  using FileSystemCallback =
      base::OnceCallback<void(const std::string& id, const std::string& path)>;
  using FilesSelectedCallback =
      base::OnceCallback<void(const std::vector<base::FilePath>& paths)>;
  using VolumeListCallback =
      base::OnceCallback<void(const std::vector<api::file_system::Volume>&)>;

  virtual ~FileSystemDelegate() {}

  virtual base::FilePath GetDefaultDirectory() = 0;

  // If policies set downloads as managed, and `extension` respects the
  // downloads policies, then return the managed directory to use for save-as
  // operations.
  virtual base::FilePath GetManagedSaveAsDirectory(
      content::BrowserContext* browser_context,
      const Extension& extension) = 0;

  // Shows a dialog to prompt the user to select files/directories. Returns
  // false if the dialog cannot be shown, i.e. there is no valid WebContents.
  virtual bool ShowSelectFileDialog(
      scoped_refptr<ExtensionFunction> extension_function,
      ui::SelectFileDialog::Type type,
      const base::FilePath& default_path,
      const ui::SelectFileDialog::FileTypeInfo* file_types,
      FileSystemDelegate::FilesSelectedCallback files_selected_callback,
      base::OnceClosure file_selection_canceled_callback) = 0;

  // Confirms (e.g. with a dialog) whether the user wants to open the directory
  // for a given app.
  virtual void ConfirmSensitiveDirectoryAccess(
      bool has_write_permission,
      const std::u16string& app_name,
      content::WebContents* web_contents,
      base::OnceClosure on_accept,
      base::OnceClosure on_cancel) = 0;

  // Finds a string describing the accept type. Returns 0 if no applicable
  // string ID is found.
  virtual int GetDescriptionIdForAcceptType(const std::string& accept_type) = 0;

#if BUILDFLAG(IS_CHROMEOS)
  // Grants or denies an extension's request for access to the named file
  // system. May prompt the user for consent.
  virtual void RequestFileSystem(content::BrowserContext* browser_context,
                                 scoped_refptr<ExtensionFunction> requester,
                                 ConsentProvider* consent_provider,
                                 const Extension& extension,
                                 std::string volume_id,
                                 bool writable,
                                 FileSystemCallback success_callback,
                                 ErrorCallback error_callback) = 0;

  // Immediately calls VolumeListCallback or ErrorCallback.
  virtual void GetVolumeList(content::BrowserContext* browser_context,
                             VolumeListCallback success_callback,
                             ErrorCallback error_callback) = 0;
#endif  // BUILDFLAG(IS_CHROMEOS)

  virtual SavedFilesServiceInterface* GetSavedFilesService(
      content::BrowserContext* browser_context) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FILE_SYSTEM_FILE_SYSTEM_DELEGATE_H_
