// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FILE_SYSTEM_FILE_SYSTEM_API_H_
#define EXTENSIONS_BROWSER_API_FILE_SYSTEM_FILE_SYSTEM_API_H_

#include <memory>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/file_system.h"
#include "extensions/common/extension_id.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "extensions/browser/api/file_system/consent_provider.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace extensions {
class ExtensionPrefs;

namespace file_system_api {

// Methods to get and set the path of the directory containing the last file
// chosen by the user in response to a chrome.fileSystem.chooseEntry() call for
// the given extension.

// Returns an empty path on failure.
base::FilePath GetLastChooseEntryDirectory(const ExtensionPrefs* prefs,
                                           const ExtensionId& extension_id);

void SetLastChooseEntryDirectory(ExtensionPrefs* prefs,
                                 const ExtensionId& extension_id,
                                 const base::FilePath& path);

}  // namespace file_system_api

class FileSystemGetDisplayPathFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.getDisplayPath",
                             FILESYSTEM_GETDISPLAYPATH)

 protected:
  ~FileSystemGetDisplayPathFunction() override;
  ResponseAction Run() override;
};

class FileSystemEntryFunction : public ExtensionFunction {
 protected:
  FileSystemEntryFunction();
  ~FileSystemEntryFunction() override;

  // This is called when writable file entries are being returned. The function
  // will ensure the files exist, creating them if necessary, and also check
  // that none of the files are links. If it succeeds it proceeds to
  // RegisterFileSystemsAndSendResponse, otherwise to HandleWritableFileError.
  void PrepareFilesForWritableApp(const std::vector<base::FilePath>& path);

  // This will finish the choose file process. This is either called directly
  // from FilesSelected, or from WritableFileChecker. It is called on the UI
  // thread.
  void RegisterFileSystemsAndSendResponse(
      const std::vector<base::FilePath>& path);

  // Creates a result dictionary.
  base::Value::Dict CreateResult();

  // Adds an entry to the result dictionary.
  void AddEntryToResult(const base::FilePath& path,
                        const std::string& id_override,
                        base::Value::Dict& result);

  // called on the UI thread if there is a problem checking a writable file.
  void HandleWritableFileError(const base::FilePath& error_path);

  // Whether multiple entries have been requested.
  bool multiple_ = false;

  // Whether a directory has been requested.
  bool is_directory_ = false;
};

class FileSystemGetWritableEntryFunction : public FileSystemEntryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.getWritableEntry",
                             FILESYSTEM_GETWRITABLEENTRY)

 protected:
  ~FileSystemGetWritableEntryFunction() override;
  ResponseAction Run() override;

 private:
  void CheckPermissionAndSendResponse();
  void SetIsDirectoryAsync();

  // The path to the file for which a writable entry has been requested.
  base::FilePath path_;
};

class FileSystemIsWritableEntryFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.isWritableEntry",
                             FILESYSTEM_ISWRITABLEENTRY)

 protected:
  ~FileSystemIsWritableEntryFunction() override;
  ResponseAction Run() override;
};

class FileSystemChooseEntryFunction : public FileSystemEntryFunction {
 public:
  struct TestOptions {
    // These first three options are mutually exclusive and are chosen in
    // this order.
    bool use_suggested_path = false;
    const raw_ptr<base::FilePath> path_to_be_picked = nullptr;
    const raw_ptr<std::vector<base::FilePath>> paths_to_be_picked = nullptr;
    bool skip_directory_confirmation = false;
    // This option is true and is only set to false in tests that do not
    // expect a dialog box to be displayed and want the test to fail if
    // it is displayed. See
    // FileSystemApiTest.FileSystemApiOpenDirectoryOnGraylistTest as an
    // example.
    bool allow_directory_access = true;
  };

  static base::AutoReset<const TestOptions*> SetOptionsForTesting(
      const TestOptions& options);
  // Call this with the directory for test file paths. On Chrome OS, accessed
  // path needs to be explicitly registered for smooth integration with Google
  // Drive support.
  static void RegisterTempExternalFileSystemForTest(const std::string& name,
                                                    const base::FilePath& path);
  DECLARE_EXTENSION_FUNCTION("fileSystem.chooseEntry", FILESYSTEM_CHOOSEENTRY)

  using AcceptOptions = std::vector<api::file_system::AcceptOption>;

  static void BuildFileTypeInfo(
      ui::SelectFileDialog::FileTypeInfo* file_type_info,
      const base::FilePath::StringType& suggested_extension,
      const std::optional<AcceptOptions>& accepts,
      const std::optional<bool>& accepts_all_types);
  static void BuildSuggestion(const std::optional<std::string>& opt_name,
                              base::FilePath* suggested_name,
                              base::FilePath::StringType* suggested_extension);

 protected:
  ~FileSystemChooseEntryFunction() override;
  ResponseAction Run() override;

 private:
  void CalculateInitialPathAndShowPicker(
      const base::FilePath& previous_path,
      const base::FilePath& suggested_name,
      const ui::SelectFileDialog::FileTypeInfo& file_type_info,
      ui::SelectFileDialog::Type picker_type,
      bool is_path_non_native_directory);
  void ShowPicker(const ui::SelectFileDialog::FileTypeInfo& file_type_info,
                  ui::SelectFileDialog::Type picker_type,
                  const base::FilePath& initial_path);
  void MaybeUseManagedSavePath(base::OnceClosure fallback_file_picker_callback,
                               const base::FilePath& path);

  // FilesSelected and FileSelectionCanceled are called by the file picker.
  void FilesSelected(const std::vector<base::FilePath>& paths);
  void FileSelectionCanceled();

  // Check if the chosen directory is or is an ancestor of a sensitive
  // directory. If so, calls ConfirmSensitiveDirectoryAccess. Otherwise, calls
  // OnDirectoryAccessConfirmed.
  void ConfirmDirectoryAccessAsync(bool non_native_path,
                                   const std::vector<base::FilePath>& paths);

  // Shows a dialog to confirm whether the user wants to open the directory.
  // Calls OnDirectoryAccessConfirmed or FileSelectionCanceled.
  void ConfirmSensitiveDirectoryAccess(
      const std::vector<base::FilePath>& paths);

  void OnDirectoryAccessConfirmed(const std::vector<base::FilePath>& paths);

  static const TestOptions* g_test_options;
};

class FileSystemRetainEntryFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.retainEntry", FILESYSTEM_RETAINENTRY)

 protected:
  ~FileSystemRetainEntryFunction() override;
  ResponseAction Run() override;

 private:
  // Retains the file entry referenced by |entry_id| in apps::SavedFilesService.
  // |entry_id| must refer to an entry in an isolated file system.  |path| is a
  // path of the entry.  |file_info| is base::File::Info of the entry if it can
  // be obtained.
  void RetainFileEntry(const std::string& entry_id,
                       const base::FilePath& path,
                       std::unique_ptr<base::File::Info> file_info);
};

class FileSystemIsRestorableFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.isRestorable", FILESYSTEM_ISRESTORABLE)

 protected:
  ~FileSystemIsRestorableFunction() override;
  ResponseAction Run() override;
};

class FileSystemRestoreEntryFunction : public FileSystemEntryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.restoreEntry", FILESYSTEM_RESTOREENTRY)

 protected:
  ~FileSystemRestoreEntryFunction() override;
  ResponseAction Run() override;
};

#if BUILDFLAG(IS_CHROMEOS)
// Requests a file system for the specified volume id.
class FileSystemRequestFileSystemFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.requestFileSystem",
                             FILESYSTEM_REQUESTFILESYSTEM)
  FileSystemRequestFileSystemFunction();

 protected:
  ~FileSystemRequestFileSystemFunction() override;

  // ExtensionFunction overrides.
  ExtensionFunction::ResponseAction Run() override;

 private:
  // Called when a user grants or rejects permissions for the file system
  // access.
  void OnGotFileSystem(const std::string& id, const std::string& path);
  void OnError(const std::string& error);

  std::unique_ptr<ConsentProvider> consent_provider_;
};

// Requests a list of available volumes.
class FileSystemGetVolumeListFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.getVolumeList",
                             FILESYSTEM_GETVOLUMELIST)
  FileSystemGetVolumeListFunction();

 protected:
  ~FileSystemGetVolumeListFunction() override;

  // ExtensionFunction overrides.
  ExtensionFunction::ResponseAction Run() override;

 private:
  void OnGotVolumeList(const std::vector<api::file_system::Volume>& volumes);
  void OnError(const std::string& error);

  std::unique_ptr<ConsentProvider> consent_provider_;
};
#else   // BUILDFLAG(IS_CHROMEOS)
// Stub for non Chrome OS operating systems.
class FileSystemRequestFileSystemFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.requestFileSystem",
                             FILESYSTEM_REQUESTFILESYSTEM)

 protected:
  ~FileSystemRequestFileSystemFunction() override;

  // ExtensionFunction overrides.
  ExtensionFunction::ResponseAction Run() override;
};

// Stub for non Chrome OS operating systems.
class FileSystemGetVolumeListFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.getVolumeList",
                             FILESYSTEM_GETVOLUMELIST)

 protected:
  ~FileSystemGetVolumeListFunction() override;

  // ExtensionFunction overrides.
  ExtensionFunction::ResponseAction Run() override;
};
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FILE_SYSTEM_FILE_SYSTEM_API_H_
