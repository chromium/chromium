// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/file_system/file_system_api.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/file_handlers/app_file_handler_util.h"
#include "extensions/browser/api/file_system/file_system_delegate.h"
#include "extensions/browser/api/file_system/saved_file_entry.h"
#include "extensions/browser/api/file_system/saved_files_service_interface.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/granted_file_entry.h"
#include "extensions/browser/path_util.h"
#include "extensions/common/api/file_system.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/mime_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#include "base/mac/foundation_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "extensions/browser/api/file_handlers/non_native_file_system_delegate.h"
#endif

using storage::IsolatedContext;

const char kInvalidCallingPage[] =
    "Invalid calling page. "
    "This function can't be called from a background page.";
const char kUserCancelled[] = "User cancelled";
const char kWritableFileErrorFormat[] = "Error opening %s";
const char kRequiresFileSystemWriteError[] =
    "Operation requires fileSystem.write permission";
const char kRequiresFileSystemDirectoryError[] =
    "Operation requires fileSystem.directory permission";
const char kMultipleUnsupportedError[] =
    "acceptsMultiple: true is only supported for 'openFile'";
const char kUnknownIdError[] = "Unknown id";
const char kNotSupportedOnCurrentPlatformError[] =
    "Operation not supported on the current platform.";
const char kRetainEntryError[] = "Could not retain file entry.";
const char kRetainEntryIncognitoError[] =
    "Could not retain file entry in incognito mode";

#if defined(OS_CHROMEOS)
const char kNotSupportedOnNonKioskSessionError[] =
    "Operation only supported for kiosk apps running in a kiosk session.";
#endif

namespace extensions {

namespace file_system = api::file_system;
namespace ChooseEntry = file_system::ChooseEntry;

namespace {

bool g_skip_picker_for_test = false;
bool g_use_suggested_path_for_test = false;
base::FilePath* g_path_to_be_picked_for_test;
std::vector<base::FilePath>* g_paths_to_be_picked_for_test;
bool g_skip_directory_confirmation_for_test = false;
bool g_allow_directory_access_for_test = false;

// Expand the mime-types and extensions provided in an AcceptOption, returning
// them within the passed extension vector. Returns false if no valid types
// were found.
bool GetFileTypesFromAcceptOption(
    const file_system::AcceptOption& accept_option,
    std::vector<base::FilePath::StringType>* extensions,
    base::string16* description) {
  std::set<base::FilePath::StringType> extension_set;
  int description_id = 0;

  if (accept_option.mime_types.get()) {
    std::vector<std::string>* list = accept_option.mime_types.get();
    bool valid_type = false;
    for (std::vector<std::string>::const_iterator iter = list->begin();
         iter != list->end(); ++iter) {
      std::vector<base::FilePath::StringType> inner;
      std::string accept_type = base::ToLowerASCII(*iter);
      net::GetExtensionsForMimeType(accept_type, &inner);
      if (inner.empty())
        continue;

      if (valid_type) {
        description_id = 0;  // We already have an accept type with label; if
                             // we find another, give up and use the default.
      } else {
        FileSystemDelegate* delegate =
            ExtensionsAPIClient::Get()->GetFileSystemDelegate();
        DCHECK(delegate);
        description_id = delegate->GetDescriptionIdForAcceptType(accept_type);
      }

      extension_set.insert(inner.begin(), inner.end());
      valid_type = true;
    }
  }

  if (accept_option.extensions.get()) {
    std::vector<std::string>* list = accept_option.extensions.get();
    for (std::vector<std::string>::const_iterator iter = list->begin();
         iter != list->end(); ++iter) {
      std::string extension = base::ToLowerASCII(*iter);
#if defined(OS_WIN)
      extension_set.insert(base::UTF8ToWide(*iter));
#else
      extension_set.insert(*iter);
#endif
    }
  }

  extensions->assign(extension_set.begin(), extension_set.end());
  if (extensions->empty())
    return false;

  if (accept_option.description.get())
    *description = base::UTF8ToUTF16(*accept_option.description);
  else if (description_id)
    *description = l10n_util::GetStringUTF16(description_id);

  return true;
}

// Key for the path of the directory of the file last chosen by the user in
// response to a chrome.fileSystem.chooseEntry() call.
const char kLastChooseEntryDirectory[] = "last_choose_file_directory";

const int kGraylistedPaths[] = {
    base::DIR_HOME,
#if defined(OS_WIN)
    base::DIR_PROGRAM_FILES, base::DIR_PROGRAM_FILESX86, base::DIR_WINDOWS,
#endif
};

typedef base::Callback<void(std::unique_ptr<base::File::Info>)>
    FileInfoOptCallback;

// Passes optional file info to the UI thread depending on |result| and |info|.
void PassFileInfoToUIThread(const FileInfoOptCallback& callback,
                            base::File::Error result,
                            const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::unique_ptr<base::File::Info> file_info(
      result == base::File::FILE_OK ? new base::File::Info(info) : NULL);
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(callback, base::Passed(&file_info)));
}

// Gets a WebContents instance handle for a platform app hosted in
// |render_frame_host|. If not found, then returns NULL.
content::WebContents* GetWebContentsForRenderFrameHost(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  // Check if there is an app window associated with the web contents; if not,
  // return null.
  return AppWindowRegistry::Get(browser_context)
                 ->GetAppWindowForWebContents(web_contents)
             ? web_contents
             : nullptr;
}

}  // namespace

namespace file_system_api {

base::FilePath GetLastChooseEntryDirectory(const ExtensionPrefs* prefs,
                                           const std::string& extension_id) {
  base::FilePath path;
  std::string string_path;
  if (prefs->ReadPrefAsString(extension_id, kLastChooseEntryDirectory,
                              &string_path)) {
    path = base::FilePath::FromUTF8Unsafe(string_path);
  }
  return path;
}

void SetLastChooseEntryDirectory(ExtensionPrefs* prefs,
                                 const std::string& extension_id,
                                 const base::FilePath& path) {
  prefs->UpdateExtensionPref(
      extension_id, kLastChooseEntryDirectory,
      base::Value::ToUniquePtrValue(base::CreateFilePathValue(path)));
}

}  // namespace file_system_api

ExtensionFunction::ResponseAction FileSystemGetDisplayPathFunction::Run() {
  std::string filesystem_name;
  std::string filesystem_path;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &filesystem_name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_path));

  base::FilePath file_path;
  std::string error;
  if (!app_file_handler_util::ValidateFileEntryAndGetPath(
          filesystem_name, filesystem_path, source_process_id(), &file_path,
          &error)) {
    return RespondNow(Error(error));
  }

  file_path = path_util::PrettifyPath(file_path);
  return RespondNow(
      OneArgument(std::make_unique<base::Value>(file_path.value())));
}

FileSystemEntryFunction::FileSystemEntryFunction()
    : multiple_(false), is_directory_(false) {}

void FileSystemEntryFunction::PrepareFilesForWritableApp(
    const std::vector<base::FilePath>& paths) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(cmihail): Path directory set should be initialized only with the
  // paths that are actually directories, but for now we will consider
  // all paths directories in case is_directory_ is true, otherwise
  // all paths files, as this was the previous logic.
  std::set<base::FilePath> path_directory_set_ =
      is_directory_ ? std::set<base::FilePath>(paths.begin(), paths.end())
                    : std::set<base::FilePath>{};
  app_file_handler_util::PrepareFilesForWritableApp(
      paths, browser_context(), path_directory_set_,
      base::Bind(&FileSystemEntryFunction::RegisterFileSystemsAndSendResponse,
                 this, paths),
      base::Bind(&FileSystemEntryFunction::HandleWritableFileError, this));
}

void FileSystemEntryFunction::RegisterFileSystemsAndSendResponse(
    const std::vector<base::FilePath>& paths) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!render_frame_host())
    return;

  std::unique_ptr<base::DictionaryValue> result = CreateResult();
  for (const auto& path : paths)
    AddEntryToResult(path, std::string(), result.get());
  Respond(OneArgument(std::move(result)));
}

std::unique_ptr<base::DictionaryValue> FileSystemEntryFunction::CreateResult() {
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->Set("entries", std::make_unique<base::ListValue>());
  result->SetBoolean("multiple", multiple_);
  return result;
}

void FileSystemEntryFunction::AddEntryToResult(const base::FilePath& path,
                                               const std::string& id_override,
                                               base::DictionaryValue* result) {
  GrantedFileEntry file_entry = app_file_handler_util::CreateFileEntry(
      browser_context(), extension(), source_process_id(), path, is_directory_);
  base::ListValue* entries;
  bool success = result->GetList("entries", &entries);
  DCHECK(success);

  std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue());
  entry->SetString("fileSystemId", file_entry.filesystem_id);
  entry->SetString("baseName", file_entry.registered_name);
  if (id_override.empty())
    entry->SetString("id", file_entry.id);
  else
    entry->SetString("id", id_override);
  entry->SetBoolean("isDirectory", is_directory_);
  entries->Append(std::move(entry));
}

void FileSystemEntryFunction::HandleWritableFileError(
    const base::FilePath& error_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Respond(Error(base::StringPrintf(
      kWritableFileErrorFormat, error_path.BaseName().AsUTF8Unsafe().c_str())));
}

ExtensionFunction::ResponseAction FileSystemGetWritableEntryFunction::Run() {
  std::string filesystem_name;
  std::string filesystem_path;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &filesystem_name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_path));

  if (!app_file_handler_util::HasFileSystemWritePermission(extension_.get())) {
    return RespondNow(Error(kRequiresFileSystemWriteError));
  }

  std::string error;
  if (!app_file_handler_util::ValidateFileEntryAndGetPath(
          filesystem_name, filesystem_path, source_process_id(), &path_,
          &error)) {
    return RespondNow(Error(error));
  }

  base::PostTaskAndReply(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&FileSystemGetWritableEntryFunction::SetIsDirectoryAsync,
                     this),
      base::BindOnce(
          &FileSystemGetWritableEntryFunction::CheckPermissionAndSendResponse,
          this));
  return RespondLater();
}

void FileSystemGetWritableEntryFunction::CheckPermissionAndSendResponse() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_directory_ && !extension_->permissions_data()->HasAPIPermission(
                           APIPermission::kFileSystemDirectory)) {
    Respond(Error(kRequiresFileSystemDirectoryError));
    return;
  }
  std::vector<base::FilePath> paths;
  paths.push_back(path_);
  PrepareFilesForWritableApp(paths);
}

void FileSystemGetWritableEntryFunction::SetIsDirectoryAsync() {
  if (base::DirectoryExists(path_)) {
    is_directory_ = true;
  }
}

ExtensionFunction::ResponseAction FileSystemIsWritableEntryFunction::Run() {
  std::string filesystem_name;
  std::string filesystem_path;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &filesystem_name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_path));

  std::string filesystem_id;
  if (!storage::CrackIsolatedFileSystemName(filesystem_name, &filesystem_id))
    return RespondNow(Error(app_file_handler_util::kInvalidParameters));

  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  bool is_writable =
      policy->CanReadWriteFileSystem(source_process_id(), filesystem_id);

  return RespondNow(OneArgument(std::make_unique<base::Value>(is_writable)));
}

void FileSystemChooseEntryFunction::ShowPicker(
    const ui::SelectFileDialog::FileTypeInfo& file_type_info,
    ui::SelectFileDialog::Type picker_type) {
  // TODO(michaelpg): Use the FileSystemDelegate to override functionality for
  // tests instead of using global variables.
  if (g_skip_picker_for_test) {
    std::vector<base::FilePath> test_paths;
    if (g_use_suggested_path_for_test)
      test_paths.push_back(initial_path_);
    else if (g_path_to_be_picked_for_test)
      test_paths.push_back(*g_path_to_be_picked_for_test);
    else if (g_paths_to_be_picked_for_test)
      test_paths = *g_paths_to_be_picked_for_test;

    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        test_paths.size() > 0
            ? base::BindOnce(&FileSystemChooseEntryFunction::FilesSelected,
                             this, test_paths)
            : base::BindOnce(
                  &FileSystemChooseEntryFunction::FileSelectionCanceled, this));
    return;
  }

  FileSystemDelegate* delegate =
      ExtensionsAPIClient::Get()->GetFileSystemDelegate();
  DCHECK(delegate);

  // The callbacks passed to the dialog will retain references to this
  // UIThreadExtenisonFunction, preventing its destruction (and subsequent
  // sending of the function response) until the user has selected a file or
  // cancelled the picker.
  if (!delegate->ShowSelectFileDialog(
          this, picker_type, initial_path_, &file_type_info,
          base::BindOnce(&FileSystemChooseEntryFunction::FilesSelected, this),
          base::BindOnce(&FileSystemChooseEntryFunction::FileSelectionCanceled,
                         this))) {
    Respond(Error(kInvalidCallingPage));
  }
}

// static
void FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
    base::FilePath* path) {
  g_skip_picker_for_test = true;
  g_use_suggested_path_for_test = false;
  g_path_to_be_picked_for_test = path;
  g_paths_to_be_picked_for_test = NULL;
}

// static
void FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathsForTest(
    std::vector<base::FilePath>* paths) {
  g_skip_picker_for_test = true;
  g_use_suggested_path_for_test = false;
  g_paths_to_be_picked_for_test = paths;
}

// static
void FileSystemChooseEntryFunction::SkipPickerAndSelectSuggestedPathForTest() {
  g_skip_picker_for_test = true;
  g_use_suggested_path_for_test = true;
  g_path_to_be_picked_for_test = NULL;
  g_paths_to_be_picked_for_test = NULL;
}

// static
void FileSystemChooseEntryFunction::SkipPickerAndAlwaysCancelForTest() {
  g_skip_picker_for_test = true;
  g_use_suggested_path_for_test = false;
  g_path_to_be_picked_for_test = NULL;
  g_paths_to_be_picked_for_test = NULL;
}

// static
void FileSystemChooseEntryFunction::StopSkippingPickerForTest() {
  g_skip_picker_for_test = false;
}

// static
void FileSystemChooseEntryFunction::SkipDirectoryConfirmationForTest() {
  g_skip_directory_confirmation_for_test = true;
  g_allow_directory_access_for_test = true;
}

// static
void FileSystemChooseEntryFunction::AutoCancelDirectoryConfirmationForTest() {
  g_skip_directory_confirmation_for_test = true;
  g_allow_directory_access_for_test = false;
}

// static
void FileSystemChooseEntryFunction::StopSkippingDirectoryConfirmationForTest() {
  g_skip_directory_confirmation_for_test = false;
}

// static
void FileSystemChooseEntryFunction::RegisterTempExternalFileSystemForTest(
    const std::string& name,
    const base::FilePath& path) {
  // For testing on Chrome OS, where to deal with remote and local paths
  // smoothly, all accessed paths need to be registered in the list of
  // external mount points.
  storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      name, storage::kFileSystemTypeNativeLocal,
      storage::FileSystemMountOption(), path);
}

void FileSystemChooseEntryFunction::FilesSelected(
    const std::vector<base::FilePath>& paths) {
  DCHECK(!paths.empty());
  base::FilePath last_choose_directory;
  if (is_directory_) {
    last_choose_directory = paths[0];
  } else {
    last_choose_directory = paths[0].DirName();
  }
  file_system_api::SetLastChooseEntryDirectory(
      ExtensionPrefs::Get(browser_context()), extension()->id(),
      last_choose_directory);
  if (is_directory_) {
    // Get the WebContents for the app window to be the parent window of the
    // confirmation dialog if necessary.
    content::WebContents* const web_contents = GetWebContentsForRenderFrameHost(
        browser_context(), render_frame_host());
    if (!web_contents) {
      Respond(Error(kInvalidCallingPage));
      return;
    }

    DCHECK_EQ(paths.size(), 1u);
    bool non_native_path = false;
#if defined(OS_CHROMEOS)
    NonNativeFileSystemDelegate* delegate =
        ExtensionsAPIClient::Get()->GetNonNativeFileSystemDelegate();
    non_native_path = delegate && delegate->IsUnderNonNativeLocalPath(
                                      browser_context(), paths[0]);
#endif

    base::PostTask(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            &FileSystemChooseEntryFunction::ConfirmDirectoryAccessAsync, this,
            non_native_path, paths, web_contents));
    return;
  }

  OnDirectoryAccessConfirmed(paths);
}

void FileSystemChooseEntryFunction::FileSelectionCanceled() {
  Respond(Error(kUserCancelled));
}

void FileSystemChooseEntryFunction::ConfirmDirectoryAccessAsync(
    bool non_native_path,
    const std::vector<base::FilePath>& paths,
    content::WebContents* web_contents) {
  const base::FilePath check_path =
      non_native_path ? paths[0] : base::MakeAbsoluteFilePath(paths[0]);
  if (check_path.empty()) {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&FileSystemChooseEntryFunction::FileSelectionCanceled,
                       this));
    return;
  }

  for (size_t i = 0; i < base::size(kGraylistedPaths); i++) {
    base::FilePath graylisted_path;
    if (!base::PathService::Get(kGraylistedPaths[i], &graylisted_path))
      continue;
    if (check_path != graylisted_path && !check_path.IsParent(graylisted_path))
      continue;

    if (g_skip_directory_confirmation_for_test) {
      if (g_allow_directory_access_for_test)
        break;
      base::PostTask(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(&FileSystemChooseEntryFunction::FileSelectionCanceled,
                         this));
      return;
    }

    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            &FileSystemChooseEntryFunction::ConfirmSensitiveDirectoryAccess,
            this, paths, web_contents));
    return;
  }

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&FileSystemChooseEntryFunction::OnDirectoryAccessConfirmed,
                     this, paths));
}

void FileSystemChooseEntryFunction::ConfirmSensitiveDirectoryAccess(
    const std::vector<base::FilePath>& paths,
    content::WebContents* web_contents) {
  if (ExtensionsBrowserClient::Get()->IsShuttingDown()) {
    FileSelectionCanceled();
    return;
  }

  FileSystemDelegate* delegate =
      ExtensionsAPIClient::Get()->GetFileSystemDelegate();
  if (!delegate) {
    Respond(Error(kNotSupportedOnCurrentPlatformError));
    return;
  }

  delegate->ConfirmSensitiveDirectoryAccess(
      app_file_handler_util::HasFileSystemWritePermission(extension_.get()),
      base::UTF8ToUTF16(extension_->name()), web_contents,
      base::Bind(&FileSystemChooseEntryFunction::OnDirectoryAccessConfirmed,
                 this, paths),
      base::Bind(&FileSystemChooseEntryFunction::FileSelectionCanceled, this));
}

void FileSystemChooseEntryFunction::OnDirectoryAccessConfirmed(
    const std::vector<base::FilePath>& paths) {
  if (app_file_handler_util::HasFileSystemWritePermission(extension_.get())) {
    PrepareFilesForWritableApp(paths);
    return;
  }

  // Don't need to check the file, it's for reading.
  RegisterFileSystemsAndSendResponse(paths);
}

void FileSystemChooseEntryFunction::BuildFileTypeInfo(
    ui::SelectFileDialog::FileTypeInfo* file_type_info,
    const base::FilePath::StringType& suggested_extension,
    const AcceptOptions* accepts,
    const bool* acceptsAllTypes) {
  file_type_info->include_all_files = true;
  if (acceptsAllTypes)
    file_type_info->include_all_files = *acceptsAllTypes;

  bool need_suggestion =
      !file_type_info->include_all_files && !suggested_extension.empty();

  if (accepts) {
    for (const file_system::AcceptOption& option : *accepts) {
      base::string16 description;
      std::vector<base::FilePath::StringType> extensions;

      if (!GetFileTypesFromAcceptOption(option, &extensions, &description))
        continue;  // No extensions were found.

      file_type_info->extensions.push_back(extensions);
      file_type_info->extension_description_overrides.push_back(description);

      // If we still need to find suggested_extension, hunt for it inside the
      // extensions returned from GetFileTypesFromAcceptOption.
      if (need_suggestion && base::Contains(extensions, suggested_extension)) {
        need_suggestion = false;
      }
    }
  }

  // If there's nothing in our accepted extension list or we couldn't find the
  // suggested extension required, then default to accepting all types.
  if (file_type_info->extensions.empty() || need_suggestion)
    file_type_info->include_all_files = true;
}

void FileSystemChooseEntryFunction::BuildSuggestion(
    const std::string* opt_name,
    base::FilePath* suggested_name,
    base::FilePath::StringType* suggested_extension) {
  if (opt_name) {
    *suggested_name = base::FilePath::FromUTF8Unsafe(*opt_name);

    // Don't allow any path components; shorten to the base name. This should
    // result in a relative path, but in some cases may not. Clear the
    // suggestion for safety if this is the case.
    *suggested_name = suggested_name->BaseName();
    if (suggested_name->IsAbsolute())
      *suggested_name = base::FilePath();

    *suggested_extension = suggested_name->Extension();
    if (!suggested_extension->empty())
      suggested_extension->erase(suggested_extension->begin());  // drop the .
  }
}

void FileSystemChooseEntryFunction::SetInitialPathAndShowPicker(
    const base::FilePath& previous_path,
    const base::FilePath& suggested_name,
    const ui::SelectFileDialog::FileTypeInfo& file_type_info,
    ui::SelectFileDialog::Type picker_type,
    bool is_previous_path_directory) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_previous_path_directory) {
    initial_path_ = previous_path.Append(suggested_name);
  } else {
    FileSystemDelegate* delegate =
        ExtensionsAPIClient::Get()->GetFileSystemDelegate();
    DCHECK(delegate);
    const base::FilePath default_directory = delegate->GetDefaultDirectory();
    if (!default_directory.empty())
      initial_path_ = default_directory.Append(suggested_name);
    else
      initial_path_ = suggested_name;
  }
  ShowPicker(file_type_info, picker_type);
}

ExtensionFunction::ResponseAction FileSystemChooseEntryFunction::Run() {
  std::unique_ptr<ChooseEntry::Params> params(
      ChooseEntry::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::FilePath suggested_name;
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  ui::SelectFileDialog::Type picker_type =
      ui::SelectFileDialog::SELECT_OPEN_FILE;

  file_system::ChooseEntryOptions* options = params->options.get();
  if (options) {
    multiple_ = options->accepts_multiple && *options->accepts_multiple;
    if (multiple_)
      picker_type = ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE;

    if (options->type == file_system::CHOOSE_ENTRY_TYPE_OPENWRITABLEFILE &&
        !app_file_handler_util::HasFileSystemWritePermission(
            extension_.get())) {
      return RespondNow(Error(kRequiresFileSystemWriteError));
    } else if (options->type == file_system::CHOOSE_ENTRY_TYPE_SAVEFILE) {
      if (!app_file_handler_util::HasFileSystemWritePermission(
              extension_.get())) {
        return RespondNow(Error(kRequiresFileSystemWriteError));
      }
      if (multiple_) {
        return RespondNow(Error(kMultipleUnsupportedError));
      }
      picker_type = ui::SelectFileDialog::SELECT_SAVEAS_FILE;
    } else if (options->type == file_system::CHOOSE_ENTRY_TYPE_OPENDIRECTORY) {
      is_directory_ = true;
      if (!extension_->permissions_data()->HasAPIPermission(
              APIPermission::kFileSystemDirectory)) {
        return RespondNow(Error(kRequiresFileSystemDirectoryError));
      }
      if (multiple_) {
        return RespondNow(Error(kMultipleUnsupportedError));
      }
      picker_type = ui::SelectFileDialog::SELECT_FOLDER;
    }

    base::FilePath::StringType suggested_extension;
    BuildSuggestion(options->suggested_name.get(), &suggested_name,
                    &suggested_extension);

    BuildFileTypeInfo(&file_type_info, suggested_extension,
                      options->accepts.get(), options->accepts_all_types.get());
  }

  file_type_info.allowed_paths = ui::SelectFileDialog::FileTypeInfo::ANY_PATH;

  base::FilePath previous_path = file_system_api::GetLastChooseEntryDirectory(
      ExtensionPrefs::Get(browser_context()), extension()->id());

  if (previous_path.empty()) {
    SetInitialPathAndShowPicker(previous_path, suggested_name, file_type_info,
                                picker_type, false);
    return RespondLater();
  }

  base::Callback<void(bool)> set_initial_path_callback = base::Bind(
      &FileSystemChooseEntryFunction::SetInitialPathAndShowPicker, this,
      previous_path, suggested_name, file_type_info, picker_type);

// Check whether the |previous_path| is a non-native directory.
#if defined(OS_CHROMEOS)
  NonNativeFileSystemDelegate* delegate =
      ExtensionsAPIClient::Get()->GetNonNativeFileSystemDelegate();
  if (delegate &&
      delegate->IsUnderNonNativeLocalPath(browser_context(), previous_path)) {
    delegate->IsNonNativeLocalPathDirectory(browser_context(), previous_path,
                                            set_initial_path_callback);
    return RespondLater();
  }
#endif
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&base::DirectoryExists, previous_path),
      set_initial_path_callback);

  return RespondLater();
}

ExtensionFunction::ResponseAction FileSystemRetainEntryFunction::Run() {
  std::string entry_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &entry_id));

  FileSystemDelegate* delegate =
      ExtensionsAPIClient::Get()->GetFileSystemDelegate();
  DCHECK(delegate);

  content::BrowserContext* context = browser_context();
  // Check whether the context is incognito mode or not.
  if (context && context->IsOffTheRecord())
    return RespondNow(Error(kRetainEntryIncognitoError));

  SavedFilesServiceInterface* saved_files_service =
      delegate->GetSavedFilesService(context);
  DCHECK(saved_files_service);

  // Add the file to the retain list if it is not already on there.
  if (!saved_files_service->IsRegistered(extension_->id(), entry_id)) {
    std::string filesystem_name;
    std::string filesystem_path;
    base::FilePath path;
    EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_name));
    EXTENSION_FUNCTION_VALIDATE(args_->GetString(2, &filesystem_path));
    std::string error;
    if (!app_file_handler_util::ValidateFileEntryAndGetPath(
            filesystem_name, filesystem_path, source_process_id(), &path,
            &error)) {
      return RespondNow(Error(error));
    }

    std::string filesystem_id;
    if (!storage::CrackIsolatedFileSystemName(filesystem_name, &filesystem_id))
      return RespondNow(Error(kRetainEntryError));

    const GURL site =
        util::GetSiteForExtensionId(extension_id(), browser_context());
    storage::FileSystemContext* const context =
        content::BrowserContext::GetStoragePartitionForSite(browser_context(),
                                                            site)
            ->GetFileSystemContext();
    const storage::FileSystemURL url = context->CreateCrackedFileSystemURL(
        site, storage::kFileSystemTypeIsolated,
        storage::IsolatedContext::GetInstance()
            ->CreateVirtualRootPath(filesystem_id)
            .Append(base::FilePath::FromUTF8Unsafe(filesystem_path)));

    // It is safe to use base::Unretained() for operation_runner(), since it
    // is owned by |context| which will delete it on the IO thread.
    base::PostTask(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(
            base::IgnoreResult(
                &storage::FileSystemOperationRunner::GetMetadata),
            base::Unretained(context->operation_runner()), url,
            storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY,
            base::Bind(
                &PassFileInfoToUIThread,
                base::Bind(&FileSystemRetainEntryFunction::RetainFileEntry,
                           this, entry_id, path))));
    return RespondLater();
  }

  saved_files_service->EnqueueFileEntry(extension_->id(), entry_id);
  return RespondNow(NoArguments());
}

void FileSystemRetainEntryFunction::RetainFileEntry(
    const std::string& entry_id,
    const base::FilePath& path,
    std::unique_ptr<base::File::Info> file_info) {
  if (!file_info) {
    Respond(Error(kRetainEntryError));
    return;
  }

  FileSystemDelegate* delegate =
      ExtensionsAPIClient::Get()->GetFileSystemDelegate();
  DCHECK(delegate);

  SavedFilesServiceInterface* saved_files_service =
      delegate->GetSavedFilesService(browser_context());
  DCHECK(saved_files_service);
  saved_files_service->RegisterFileEntry(extension_->id(), entry_id, path,
                                         file_info->is_directory);
  saved_files_service->EnqueueFileEntry(extension_->id(), entry_id);
  Respond(NoArguments());
}

ExtensionFunction::ResponseAction FileSystemIsRestorableFunction::Run() {
  std::string entry_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &entry_id));

  FileSystemDelegate* delegate =
      ExtensionsAPIClient::Get()->GetFileSystemDelegate();
  DCHECK(delegate);

  SavedFilesServiceInterface* saved_files_service =
      delegate->GetSavedFilesService(browser_context());
  DCHECK(saved_files_service);

  return RespondNow(OneArgument(std::make_unique<base::Value>(
      saved_files_service->IsRegistered(extension_->id(), entry_id))));
}

ExtensionFunction::ResponseAction FileSystemRestoreEntryFunction::Run() {
  std::string entry_id;
  bool needs_new_entry;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &entry_id));
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(1, &needs_new_entry));
  FileSystemDelegate* delegate =
      ExtensionsAPIClient::Get()->GetFileSystemDelegate();
  DCHECK(delegate);

  SavedFilesServiceInterface* saved_files_service =
      delegate->GetSavedFilesService(browser_context());
  DCHECK(saved_files_service);
  const SavedFileEntry* file =
      saved_files_service->GetFileEntry(extension_->id(), entry_id);
  if (!file)
    return RespondNow(Error(kUnknownIdError));

  saved_files_service->EnqueueFileEntry(extension_->id(), entry_id);

  // Only create a new file entry if the renderer requests one.
  // |needs_new_entry| will be false if the renderer already has an Entry for
  // |entry_id|.
  if (needs_new_entry) {
    is_directory_ = file->is_directory;
    std::unique_ptr<base::DictionaryValue> result = CreateResult();
    AddEntryToResult(file->path, file->id, result.get());
    return RespondNow(OneArgument(std::move(result)));
  }
  return RespondNow(NoArguments());
}

#if !defined(OS_CHROMEOS)
ExtensionFunction::ResponseAction FileSystemRequestFileSystemFunction::Run() {
  using file_system::RequestFileSystem::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  NOTIMPLEMENTED();
  return RespondNow(Error(kNotSupportedOnCurrentPlatformError));
}

ExtensionFunction::ResponseAction FileSystemGetVolumeListFunction::Run() {
  NOTIMPLEMENTED();
  return RespondNow(Error(kNotSupportedOnCurrentPlatformError));
}
#else

FileSystemRequestFileSystemFunction::FileSystemRequestFileSystemFunction() {}

FileSystemRequestFileSystemFunction::~FileSystemRequestFileSystemFunction() {}

ExtensionFunction::ResponseAction FileSystemRequestFileSystemFunction::Run() {
  using file_system::RequestFileSystem::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  FileSystemDelegate* delegate =
      ExtensionsAPIClient::Get()->GetFileSystemDelegate();
  DCHECK(delegate);
  // Only kiosk apps in kiosk sessions can use this API.
  // Additionally it is enabled for whitelisted component extensions and apps.
  if (delegate->GetGrantVolumesMode(browser_context(), render_frame_host(),
                                    *extension()) ==
      FileSystemDelegate::kGrantNone) {
    return RespondNow(Error(kNotSupportedOnNonKioskSessionError));
  }

  delegate->RequestFileSystem(
      browser_context(), this, *extension(), params->options.volume_id,
      params->options.writable.get() && *params->options.writable.get(),
      base::Bind(&FileSystemRequestFileSystemFunction::OnGotFileSystem, this),
      base::Bind(&FileSystemRequestFileSystemFunction::OnError, this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void FileSystemRequestFileSystemFunction::OnGotFileSystem(
    const std::string& id,
    const std::string& path) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("file_system_id", id);
  dict->SetString("file_system_path", path);
  Respond(OneArgument(std::move(dict)));
}

void FileSystemRequestFileSystemFunction::OnError(const std::string& error) {
  Respond(Error(error));
}

FileSystemGetVolumeListFunction::FileSystemGetVolumeListFunction() {}

FileSystemGetVolumeListFunction::~FileSystemGetVolumeListFunction() {}

ExtensionFunction::ResponseAction FileSystemGetVolumeListFunction::Run() {
  FileSystemDelegate* delegate =
      ExtensionsAPIClient::Get()->GetFileSystemDelegate();
  DCHECK(delegate);
  // Only kiosk apps in kiosk sessions can use this API.
  // Additionally it is enabled for whitelisted component extensions and apps.
  if (delegate->GetGrantVolumesMode(browser_context(), render_frame_host(),
                                    *extension()) ==
      FileSystemDelegate::kGrantNone) {
    return RespondNow(Error(kNotSupportedOnNonKioskSessionError));
  }

  delegate->GetVolumeList(
      browser_context(), *extension(),
      base::Bind(&FileSystemGetVolumeListFunction::OnGotVolumeList, this),
      base::Bind(&FileSystemGetVolumeListFunction::OnError, this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void FileSystemGetVolumeListFunction::OnGotVolumeList(
    const std::vector<file_system::Volume>& volumes) {
  Respond(ArgumentList(file_system::GetVolumeList::Results::Create(volumes)));
}

void FileSystemGetVolumeListFunction::OnError(const std::string& error) {
  Respond(Error(error));
}
#endif

}  // namespace extensions
