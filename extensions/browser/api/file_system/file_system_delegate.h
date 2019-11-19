// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FILE_SYSTEM_FILE_SYSTEM_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_FILE_SYSTEM_FILE_SYSTEM_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "extensions/common/api/file_system.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class ExtensionFunction;

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace extensions {

class Extension;
class SavedFilesServiceInterface;

// Delegate class for embedder-specific file system access.
class FileSystemDelegate {
 public:
  using ErrorCallback = base::Callback<void(const std::string&)>;
  using FileSystemCallback =
      base::Callback<void(const std::string& id, const std::string& path)>;
  using FilesSelectedCallback =
      base::OnceCallback<void(const std::vector<base::FilePath>& paths)>;
  using VolumeListCallback =
      base::Callback<void(const std::vector<api::file_system::Volume>&)>;

  enum GrantVolumesMode { kGrantAll, kGrantNone, kGrantPerVolume };

  virtual ~FileSystemDelegate() {}

  virtual base::FilePath GetDefaultDirectory() = 0;

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
      const base::string16& app_name,
      content::WebContents* web_contents,
      const base::Closure& on_accept,
      const base::Closure& on_cancel) = 0;

  // Finds a string describing the accept type. Returns 0 if no applicable
  // string ID is found.
  virtual int GetDescriptionIdForAcceptType(const std::string& accept_type) = 0;

#if defined(OS_CHROMEOS)
  // Checks whether the extension can be granted access.
  virtual GrantVolumesMode GetGrantVolumesMode(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* render_frame_host,
      const Extension& extension) = 0;

  // Grants or denies an extension's request for access to the named file
  // system. May prompt the user for consent.
  virtual void RequestFileSystem(content::BrowserContext* browser_context,
                                 scoped_refptr<ExtensionFunction> requester,
                                 const Extension& extension,
                                 std::string volume_id,
                                 bool writable,
                                 const FileSystemCallback& success_callback,
                                 const ErrorCallback& error_callback) = 0;

  // Immediately calls VolumeListCallback or ErrorCallback.
  virtual void GetVolumeList(content::BrowserContext* browser_context,
                             const Extension& extension,
                             const VolumeListCallback& success_callback,
                             const ErrorCallback& error_callback) = 0;
#endif

  virtual SavedFilesServiceInterface* GetSavedFilesService(
      content::BrowserContext* browser_context) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FILE_SYSTEM_FILE_SYSTEM_DELEGATE_H_
