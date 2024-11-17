// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/api/file_system/file_system_api.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/json/values_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/filename_generation/filename_generation.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/file_handlers/app_file_handler_util.h"
#include "extensions/browser/api/file_system/consent_provider.h"
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
#include "extensions/common/extension_id.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/mime_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "url/origin.h"

#if BUILDFLAG(IS_MAC)
#include <CoreFoundation/CoreFoundation.h>
#include "base/apple/foundation_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "extensions/browser/api/file_handlers/non_native_file_system_delegate.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

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

#if BUILDFLAG(IS_CHROMEOS)
const char kNotSupportedOnNonKioskSessionError[] =
    "Operation only supported for kiosk apps running in a kiosk session.";
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace extensions {

namespace file_system = api::file_system;
namespace ChooseEntry = file_system::ChooseEntry;

namespace {

// Expand the mime-types and extensions provided in an AcceptOption, returning
// them within the passed extension vector. Returns false if no valid types
// were found.
bool GetFileTypesFromAcceptOption(
    const file_system::AcceptOption& accept_option,
    std::vector<base::FilePath::StringType>* extensions,
    std::u16string* description) {
  std::set<base::FilePath::StringType> extension_set;
  int description_id = 0;

  if (accept_option.mime_types) {
    bool valid_type = false;
    for (const auto& item : *accept_option.mime_types) {
      std::vector<base::FilePath::StringType> inner;
      std::string accept_type = base::ToLowerASCII(item);
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

  if (accept_option.extensions) {
    for (const auto& item : *accept_option.extensions) {
#if BUILDFLAG(IS_WIN)
      extension_set.insert(base::UTF8ToWide(item));
#else
      extension_set.insert(item);
#endif
    }
  }

  extensions->assign(extension_set.begin(), extension_set.end());
  if (extensions->empty())
    return false;

  if (accept_option.description)
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
#if BUILDFLAG(IS_WIN)
    base::DIR_PROGRAM_FILES,
    base::DIR_PROGRAM_FILESX86,
    base::DIR_WINDOWS,
#endif
};

using FileInfoOptCallback =
    base::OnceCallback<void(std::unique_ptr<base::File::Info>)>;

// Passes optional file info to the UI thread depending on |result| and |info|.
void PassFileInfoToUIThread(FileInfoOptCallback callback,
                            base::File::Error result,
                            const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::unique_ptr<base::File::Info> file_info(
      result == base::File::FILE_OK ? new base::File::Info(info) : nullptr);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(file_info)));
}

// Gets a WebContents instance handle for a platform app hosted in
// |render_frame_host|. If not found, then returns NULL.
content::WebContents* GetWebContentsForRenderFrameHost(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host)
    return nullptr;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  // Check if there is an app window associated with the web contents; if not,
  // return null.
  return AppWindowRegistry::Get(browser_context)
                 ->GetAppWindowForWebContents(web_contents)
             ? web_contents
             : nullptr;
}

// Creates a unique filename by appending a uniquifier if needed. Returns the
// generated file path on success, or an empty path on failure.
base::FilePath GenerateUniqueSavePath(const base::FilePath& path) {
  int limit = base::GetMaximumPathComponentLength(path.DirName());
  if (limit < 0)
    return base::FilePath();

  for (int i = 0; i <= base::kMaxUniqueFiles + 1; ++i) {
    base::FilePath unique_path;
    if (i == 0) {
      // Try the original path.
      unique_path = path;
    } else if (i <= base::kMaxUniqueFiles) {
      // Try a number suffix, like base::GetUniquePath().
      std::string suffix = base::StringPrintf(" (%d)", i);
      unique_path = path.InsertBeforeExtensionASCII(suffix);
    } else {
      // Try a timestamp suffix.
      // Generate an ISO8601 compliant local timestamp suffix that avoids
      // reserved characters that are forbidden on some OSes like Windows.
      unique_path = path.InsertBeforeExtensionASCII(
          base::UnlocalizedTimeFormatWithPattern(base::Time::Now(),
                                                 " - yyyy-MM-dd'T'HHmmss.SSS"));
    }
    if (!filename_generation::TruncateFilename(&unique_path, limit))
      return base::FilePath();

    if (!base::PathExists(unique_path))
      return unique_path;
  }

  return base::FilePath();
}

}  // namespace

namespace file_system_api {

base::FilePath GetLastChooseEntryDirectory(const ExtensionPrefs* prefs,
                                           const ExtensionId& extension_id) {
  base::FilePath path;
  std::string string_path;
  if (prefs->ReadPrefAsString(extension_id, kLastChooseEntryDirectory,
                              &string_path)) {
    path = base::FilePath::FromUTF8Unsafe(string_path);
  }
  return path;
}

void SetLastChooseEntryDirectory(ExtensionPrefs* prefs,
                                 const ExtensionId& extension_id,
                                 const base::FilePath& path) {
  prefs->UpdateExtensionPref(extension_id, kLastChooseEntryDirectory,
                             ::base::FilePathToValue(path));
}

}  // namespace file_system_api

FileSystemGetDisplayPathFunction::~FileSystemGetDisplayPathFunction() = default;

ExtensionFunction::ResponseAction FileSystemGetDisplayPathFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_string());

  const std::string& filesystem_name = args()[0].GetString();
  const std::string& filesystem_path = args()[1].GetString();

  base::FilePath file_path;
  std::string error;
  if (!app_file_handler_util::ValidateFileEntryAndGetPath(
          filesystem_name, filesystem_path, source_process_id(), &file_path,
          &error)) {
    return RespondNow(Error(std::move(error)));
  }

  file_path = path_util::PrettifyPath(file_path);
  return RespondNow(WithArguments(file_path.AsUTF8Unsafe()));
}

FileSystemEntryFunction::FileSystemEntryFunction() = default;

FileSystemEntryFunction::~FileSystemEntryFunction() = default;

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
      base::BindOnce(
          &FileSystemEntryFunction::RegisterFileSystemsAndSendResponse, this,
          paths),
      base::BindOnce(&FileSystemEntryFunction::HandleWritableFileError, this));
}

void FileSystemEntryFunction::RegisterFileSystemsAndSendResponse(
    const std::vector<base::FilePath>& paths) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!render_frame_host())
    return;

  base::Value::Dict result = CreateResult();
  for (const auto& path : paths)
    AddEntryToResult(path, std::string(), result);
  Respond(WithArguments(std::move(result)));
}

base::Value::Dict FileSystemEntryFunction::CreateResult() {
  base::Value::Dict result;
  result.Set("entries", base::Value::List());
  result.Set("multiple", multiple_);
  return result;
}

void FileSystemEntryFunction::AddEntryToResult(const base::FilePath& path,
                                               const std::string& id_override,
                                               base::Value::Dict& result) {
  GrantedFileEntry file_entry = app_file_handler_util::CreateFileEntry(
      browser_context(), extension(), source_process_id(), path, is_directory_);
  base::Value::List* entries = result.FindList("entries");
  DCHECK(entries);

  base::Value::Dict entry;
  entry.Set("fileSystemId", file_entry.filesystem_id);
  entry.Set("baseName", file_entry.registered_name);
  if (id_override.empty()) {
    entry.Set("id", file_entry.id);
  } else {
    entry.Set("id", id_override);
  }
  entry.Set("isDirectory", is_directory_);
  entries->Append(std::move(entry));
}

void FileSystemEntryFunction::HandleWritableFileError(
    const base::FilePath& error_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Respond(Error(base::StringPrintf(
      kWritableFileErrorFormat, error_path.BaseName().AsUTF8Unsafe().c_str())));
}

FileSystemGetWritableEntryFunction::~FileSystemGetWritableEntryFunction() =
    default;

ExtensionFunction::ResponseAction FileSystemGetWritableEntryFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_string());

  const std::string& filesystem_name = args()[0].GetString();
  const std::string& filesystem_path = args()[1].GetString();

  if (!app_file_handler_util::HasFileSystemWritePermission(extension_.get())) {
    return RespondNow(Error(kRequiresFileSystemWriteError));
  }

  std::string error;
  if (!app_file_handler_util::ValidateFileEntryAndGetPath(
          filesystem_name, filesystem_path, source_process_id(), &path_,
          &error)) {
    return RespondNow(Error(std::move(error)));
  }

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
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
                           mojom::APIPermissionID::kFileSystemDirectory)) {
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

FileSystemIsWritableEntryFunction::~FileSystemIsWritableEntryFunction() =
    default;

ExtensionFunction::ResponseAction FileSystemIsWritableEntryFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());

  const std::string& filesystem_name = args()[0].GetString();

  std::string filesystem_id;
  if (!storage::CrackIsolatedFileSystemName(filesystem_name, &filesystem_id))
    return RespondNow(Error(app_file_handler_util::kInvalidParameters));

  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  bool is_writable =
      policy->CanReadWriteFileSystem(source_process_id(), filesystem_id);

  return RespondNow(WithArguments(is_writable));
}

const FileSystemChooseEntryFunction::TestOptions*
    FileSystemChooseEntryFunction::g_test_options = nullptr;

base::AutoReset<const FileSystemChooseEntryFunction::TestOptions*>
FileSystemChooseEntryFunction::SetOptionsForTesting(
    const TestOptions& options) {
  CHECK_EQ(nullptr, g_test_options);
  return base::AutoReset<const TestOptions*>(&g_test_options, &options);
}

void FileSystemChooseEntryFunction::ShowPicker(
    const ui::SelectFileDialog::FileTypeInfo& file_type_info,
    ui::SelectFileDialog::Type picker_type,
    const base::FilePath& initial_path) {
  if (g_test_options) {
    std::vector<base::FilePath> test_paths;
    if (g_test_options->use_suggested_path) {
      CHECK(!g_test_options->path_to_be_picked &&
            !g_test_options->paths_to_be_picked);
      test_paths.push_back(initial_path);
    } else if (g_test_options->path_to_be_picked) {
      CHECK(!g_test_options->paths_to_be_picked);
      test_paths.push_back(*g_test_options->path_to_be_picked);
    } else if (g_test_options->paths_to_be_picked) {
      test_paths = *g_test_options->paths_to_be_picked;
    }
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
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
          this, picker_type, initial_path, &file_type_info,
          base::BindOnce(&FileSystemChooseEntryFunction::FilesSelected, this),
          base::BindOnce(&FileSystemChooseEntryFunction::FileSelectionCanceled,
                         this))) {
    Respond(Error(kInvalidCallingPage));
  }
}

// static
void FileSystemChooseEntryFunction::RegisterTempExternalFileSystemForTest(
    const std::string& name,
    const base::FilePath& path) {
  // For testing on Chrome OS, where to deal with remote and local paths
  // smoothly, all accessed paths need to be registered in the list of
  // external mount points.
  storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      name, storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
      path);
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

  if (extension_->is_extension()) {
    ExtensionsBrowserClient::Get()->SetLastSaveFilePath(browser_context(),
                                                        last_choose_directory);
  } else {
    file_system_api::SetLastChooseEntryDirectory(
        ExtensionPrefs::Get(browser_context()), extension()->id(),
        last_choose_directory);
  }

  if (is_directory_) {
    DCHECK_EQ(paths.size(), 1u);
    bool non_native_path = false;
#if BUILDFLAG(IS_CHROMEOS)
    NonNativeFileSystemDelegate* delegate =
        ExtensionsAPIClient::Get()->GetNonNativeFileSystemDelegate();
    non_native_path = delegate && delegate->IsUnderNonNativeLocalPath(
                                      browser_context(), paths[0]);
#endif

    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            &FileSystemChooseEntryFunction::ConfirmDirectoryAccessAsync, this,
            non_native_path, paths));
    return;
  }

  OnDirectoryAccessConfirmed(paths);
}

void FileSystemChooseEntryFunction::FileSelectionCanceled() {
  Respond(Error(kUserCancelled));
}

void FileSystemChooseEntryFunction::ConfirmDirectoryAccessAsync(
    bool non_native_path,
    const std::vector<base::FilePath>& paths) {
  const base::FilePath check_path =
      non_native_path ? paths[0] : base::MakeAbsoluteFilePath(paths[0]);
  if (check_path.empty()) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&FileSystemChooseEntryFunction::FileSelectionCanceled,
                       this));
    return;
  }

  for (size_t i = 0; i < std::size(kGraylistedPaths); i++) {
    base::FilePath graylisted_path;
    if (!base::PathService::Get(kGraylistedPaths[i], &graylisted_path))
      continue;
    if (check_path != graylisted_path && !check_path.IsParent(graylisted_path))
      continue;

    if (g_test_options && g_test_options->skip_directory_confirmation) {
      if (g_test_options->allow_directory_access) {
        break;
      }
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&FileSystemChooseEntryFunction::FileSelectionCanceled,
                         this));
      return;
    }

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &FileSystemChooseEntryFunction::ConfirmSensitiveDirectoryAccess,
            this, paths));
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FileSystemChooseEntryFunction::OnDirectoryAccessConfirmed,
                     this, paths));
}

void FileSystemChooseEntryFunction::ConfirmSensitiveDirectoryAccess(
    const std::vector<base::FilePath>& paths) {
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

  content::WebContents* const web_contents =
      GetWebContentsForRenderFrameHost(browser_context(), render_frame_host());
  if (!web_contents) {
    Respond(Error(kInvalidCallingPage));
    return;
  }

  delegate->ConfirmSensitiveDirectoryAccess(
      app_file_handler_util::HasFileSystemWritePermission(extension_.get()),
      base::UTF8ToUTF16(extension_->name()), web_contents,
      base::BindOnce(&FileSystemChooseEntryFunction::OnDirectoryAccessConfirmed,
                     this, paths),
      base::BindOnce(&FileSystemChooseEntryFunction::FileSelectionCanceled,
                     this));
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
    const std::optional<AcceptOptions>& accepts,
    const std::optional<bool>& accepts_all_types) {
  file_type_info->include_all_files = accepts_all_types.value_or(true);

  bool need_suggestion =
      !file_type_info->include_all_files && !suggested_extension.empty();

  if (accepts) {
    for (const file_system::AcceptOption& option : *accepts) {
      std::u16string description;
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
    const std::optional<std::string>& opt_name,
    base::FilePath* suggested_name,
    base::FilePath::StringType* suggested_extension) {
  if (opt_name) {
    std::string name;
    base::ReplaceChars(*opt_name, "%", "_", &name);
    *suggested_name = base::FilePath::FromUTF8Unsafe(name);

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

void FileSystemChooseEntryFunction::CalculateInitialPathAndShowPicker(
    const base::FilePath& previous_path,
    const base::FilePath& suggested_name,
    const ui::SelectFileDialog::FileTypeInfo& file_type_info,
    ui::SelectFileDialog::Type picker_type,
    bool is_previous_path_directory) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::FilePath initial_path;
  if (is_previous_path_directory) {
    initial_path = previous_path.Append(suggested_name);
  } else {
    FileSystemDelegate* delegate =
        ExtensionsAPIClient::Get()->GetFileSystemDelegate();
    DCHECK(delegate);
    const base::FilePath default_directory = delegate->GetDefaultDirectory();
    if (!default_directory.empty())
      initial_path = default_directory.Append(suggested_name);
    else
      initial_path = suggested_name;
  }
  ShowPicker(file_type_info, picker_type, initial_path);
}

void FileSystemChooseEntryFunction::MaybeUseManagedSavePath(
    base::OnceClosure fallback_file_picker_callback,
    const base::FilePath& path) {
  if (path.empty())
    std::move(fallback_file_picker_callback).Run();
  else
    FilesSelected({path});
}

FileSystemChooseEntryFunction::~FileSystemChooseEntryFunction() = default;

ExtensionFunction::ResponseAction FileSystemChooseEntryFunction::Run() {
  std::optional<ChooseEntry::Params> params =
      ChooseEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  base::FilePath suggested_name;
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  ui::SelectFileDialog::Type picker_type =
      ui::SelectFileDialog::SELECT_OPEN_FILE;

  if (params->options) {
    const file_system::ChooseEntryOptions& options = *params->options;
    multiple_ = options.accepts_multiple && *options.accepts_multiple;
    if (multiple_)
      picker_type = ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE;

    if (options.type == file_system::ChooseEntryType::kOpenWritableFile &&
        !app_file_handler_util::HasFileSystemWritePermission(
            extension_.get())) {
      return RespondNow(Error(kRequiresFileSystemWriteError));
    } else if (options.type == file_system::ChooseEntryType::kSaveFile) {
      if (!app_file_handler_util::HasFileSystemWritePermission(
              extension_.get())) {
        return RespondNow(Error(kRequiresFileSystemWriteError));
      }
      if (multiple_) {
        return RespondNow(Error(kMultipleUnsupportedError));
      }
      picker_type = ui::SelectFileDialog::SELECT_SAVEAS_FILE;
    } else if (options.type == file_system::ChooseEntryType::kOpenDirectory) {
      is_directory_ = true;
      if (!extension_->permissions_data()->HasAPIPermission(
              mojom::APIPermissionID::kFileSystemDirectory)) {
        return RespondNow(Error(kRequiresFileSystemDirectoryError));
      }
      if (multiple_) {
        return RespondNow(Error(kMultipleUnsupportedError));
      }
      picker_type = ui::SelectFileDialog::SELECT_FOLDER;
    }

    base::FilePath::StringType suggested_extension;
    BuildSuggestion(options.suggested_name, &suggested_name,
                    &suggested_extension);

    BuildFileTypeInfo(&file_type_info, suggested_extension, options.accepts,
                      options.accepts_all_types);
  }

  file_type_info.allowed_paths = ui::SelectFileDialog::FileTypeInfo::ANY_PATH;

  if (picker_type == ui::SelectFileDialog::SELECT_SAVEAS_FILE &&
      !suggested_name.empty()) {
    FileSystemDelegate* delegate =
        ExtensionsAPIClient::Get()->GetFileSystemDelegate();
    base::FilePath managed_saveas_dir =
        delegate->GetManagedSaveAsDirectory(browser_context(), *extension());
    if (!managed_saveas_dir.empty()) {
      base::OnceClosure file_picker_callback = base::BindOnce(
          &FileSystemChooseEntryFunction::CalculateInitialPathAndShowPicker,
          this,
          /*previous_path=*/base::FilePath(), suggested_name, file_type_info,
          picker_type,
          /*is_previous_path_directory=*/false);

      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
          base::BindOnce(&GenerateUniqueSavePath,
                         managed_saveas_dir.Append(suggested_name)),
          base::BindOnce(
              &FileSystemChooseEntryFunction::MaybeUseManagedSavePath, this,
              std::move(file_picker_callback)));
      return RespondLater();
    }
  }

  base::FilePath previous_path;
  if (extension_->is_extension()) {
    previous_path =
        ExtensionsBrowserClient::Get()->GetSaveFilePath(browser_context());
  } else {
    previous_path = file_system_api::GetLastChooseEntryDirectory(
        ExtensionPrefs::Get(browser_context()), extension()->id());
  }

  if (previous_path.empty()) {
    CalculateInitialPathAndShowPicker(previous_path, suggested_name,
                                      file_type_info, picker_type, false);
    return RespondLater();
  }

  base::OnceCallback<void(bool)> set_initial_path_callback = base::BindOnce(
      &FileSystemChooseEntryFunction::CalculateInitialPathAndShowPicker, this,
      previous_path, suggested_name, file_type_info, picker_type);

// Check whether the |previous_path| is a non-native directory.
#if BUILDFLAG(IS_CHROMEOS)
  NonNativeFileSystemDelegate* delegate =
      ExtensionsAPIClient::Get()->GetNonNativeFileSystemDelegate();
  if (delegate &&
      delegate->IsUnderNonNativeLocalPath(browser_context(), previous_path)) {
    delegate->IsNonNativeLocalPathDirectory(
        browser_context(), previous_path, std::move(set_initial_path_callback));
    return RespondLater();
  }
#endif
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&base::DirectoryExists, previous_path),
      std::move(set_initial_path_callback));

  return RespondLater();
}

FileSystemRetainEntryFunction::~FileSystemRetainEntryFunction() = default;

ExtensionFunction::ResponseAction FileSystemRetainEntryFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());

  std::string entry_id = args()[0].GetString();

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
    EXTENSION_FUNCTION_VALIDATE(args().size() >= 3);
    EXTENSION_FUNCTION_VALIDATE(args()[1].is_string());
    EXTENSION_FUNCTION_VALIDATE(args()[2].is_string());
    std::string filesystem_name = args()[1].GetString();
    std::string filesystem_path = args()[2].GetString();

    base::FilePath path;
    std::string error;
    if (!app_file_handler_util::ValidateFileEntryAndGetPath(
            filesystem_name, filesystem_path, source_process_id(), &path,
            &error)) {
      return RespondNow(Error(std::move(error)));
    }

    std::string filesystem_id;
    if (!storage::CrackIsolatedFileSystemName(filesystem_name, &filesystem_id))
      return RespondNow(Error(kRetainEntryError));

    storage::FileSystemContext* const file_system_context =
        util::GetStoragePartitionForExtensionId(extension_id(),
                                                browser_context())
            ->GetFileSystemContext();

    const storage::FileSystemURL url =
        file_system_context->CreateCrackedFileSystemURL(
            blink::StorageKey::CreateFirstParty(extension()->origin()),
            storage::kFileSystemTypeIsolated,
            storage::IsolatedContext::GetInstance()
                ->CreateVirtualRootPath(filesystem_id)
                .Append(base::FilePath::FromUTF8Unsafe(filesystem_path)));

    // It is safe to use base::Unretained() for operation_runner(), since it
    // is owned by |file_system_context| which will delete it on the IO thread.
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(
                &storage::FileSystemOperationRunner::GetMetadata),
            base::Unretained(file_system_context->operation_runner()), url,
            storage::FileSystemOperation::GetMetadataFieldSet(
                {storage::FileSystemOperation::GetMetadataField::kIsDirectory}),
            base::BindOnce(
                &PassFileInfoToUIThread,
                base::BindOnce(&FileSystemRetainEntryFunction::RetainFileEntry,
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

FileSystemIsRestorableFunction::~FileSystemIsRestorableFunction() = default;

ExtensionFunction::ResponseAction FileSystemIsRestorableFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  const std::string& entry_id = args()[0].GetString();

  FileSystemDelegate* delegate =
      ExtensionsAPIClient::Get()->GetFileSystemDelegate();
  DCHECK(delegate);

  SavedFilesServiceInterface* saved_files_service =
      delegate->GetSavedFilesService(browser_context());
  DCHECK(saved_files_service);

  return RespondNow(WithArguments(
      saved_files_service->IsRegistered(extension_->id(), entry_id)));
}

FileSystemRestoreEntryFunction::~FileSystemRestoreEntryFunction() = default;

ExtensionFunction::ResponseAction FileSystemRestoreEntryFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_bool());
  const std::string& entry_id = args()[0].GetString();
  bool needs_new_entry = args()[1].GetBool();

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
    base::Value::Dict result = CreateResult();
    AddEntryToResult(file->path, file->id, result);
    return RespondNow(WithArguments(std::move(result)));
  }
  return RespondNow(NoArguments());
}

#if BUILDFLAG(IS_CHROMEOS)
/******** FileSystemRequestFileSystemFunction ********/

FileSystemRequestFileSystemFunction::FileSystemRequestFileSystemFunction() =
    default;

FileSystemRequestFileSystemFunction::~FileSystemRequestFileSystemFunction() =
    default;

ExtensionFunction::ResponseAction FileSystemRequestFileSystemFunction::Run() {
  using file_system::RequestFileSystem::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  consent_provider_ =
      ExtensionsAPIClient::Get()->CreateConsentProvider(browser_context());

  FileSystemDelegate* delegate =
      ExtensionsAPIClient::Get()->GetFileSystemDelegate();
  DCHECK(delegate);
  // Only kiosk apps in kiosk sessions can use this API.
  // Additionally it is enabled for allowlisted component extensions and apps.
  if (!consent_provider_->IsGrantable(*extension())) {
    return RespondNow(Error(kNotSupportedOnNonKioskSessionError));
  }

  delegate->RequestFileSystem(
      browser_context(), this, consent_provider_.get(), *extension(),
      params->options.volume_id, params->options.writable.value_or(false),
      base::BindOnce(&FileSystemRequestFileSystemFunction::OnGotFileSystem,
                     this),
      base::BindOnce(&FileSystemRequestFileSystemFunction::OnError, this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void FileSystemRequestFileSystemFunction::OnGotFileSystem(
    const std::string& id,
    const std::string& path) {
  base::Value::Dict dict;
  dict.Set("file_system_id", id);
  dict.Set("file_system_path", path);
  Respond(WithArguments(std::move(dict)));
}

void FileSystemRequestFileSystemFunction::OnError(const std::string& error) {
  Respond(Error(error));
}

/******** FileSystemGetVolumeListFunction ********/

FileSystemGetVolumeListFunction::FileSystemGetVolumeListFunction() = default;

FileSystemGetVolumeListFunction::~FileSystemGetVolumeListFunction() = default;

ExtensionFunction::ResponseAction FileSystemGetVolumeListFunction::Run() {
  consent_provider_ =
      ExtensionsAPIClient::Get()->CreateConsentProvider(browser_context());

  FileSystemDelegate* delegate =
      ExtensionsAPIClient::Get()->GetFileSystemDelegate();
  DCHECK(delegate);
  // Only kiosk apps in kiosk sessions can use this API.
  // Additionally it is enabled for allowlisted component extensions and apps.
  if (!consent_provider_->IsGrantable(*extension())) {
    return RespondNow(Error(kNotSupportedOnNonKioskSessionError));
  }

  delegate->GetVolumeList(
      browser_context(),
      base::BindOnce(&FileSystemGetVolumeListFunction::OnGotVolumeList, this),
      base::BindOnce(&FileSystemGetVolumeListFunction::OnError, this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void FileSystemGetVolumeListFunction::OnGotVolumeList(
    const std::vector<file_system::Volume>& volumes) {
  Respond(ArgumentList(file_system::GetVolumeList::Results::Create(volumes)));
}

void FileSystemGetVolumeListFunction::OnError(const std::string& error) {
  Respond(Error(error));
}
#else   // BUILDFLAG(IS_CHROMEOS)
/******** FileSystemRequestFileSystemFunction ********/

FileSystemRequestFileSystemFunction::~FileSystemRequestFileSystemFunction() =
    default;

ExtensionFunction::ResponseAction FileSystemRequestFileSystemFunction::Run() {
  using file_system::RequestFileSystem::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  NOTIMPLEMENTED();
  return RespondNow(Error(kNotSupportedOnCurrentPlatformError));
}

/******** FileSystemGetVolumeListFunction ********/

FileSystemGetVolumeListFunction::~FileSystemGetVolumeListFunction() = default;

ExtensionFunction::ResponseAction FileSystemGetVolumeListFunction::Run() {
  NOTIMPLEMENTED();
  return RespondNow(Error(kNotSupportedOnCurrentPlatformError));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace extensions
