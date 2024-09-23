// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/file_util.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/image_util.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "net/base/filename_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using extensions::mojom::ManifestLocation;

namespace extensions::file_util {
namespace {

enum SafeInstallationFlag {
  DEFAULT,   // Default case, controlled by a field trial.
  DISABLED,  // Safe installation is disabled.
  ENABLED,   // Safe installation is enabled.
};
SafeInstallationFlag g_use_safe_installation = DEFAULT;

bool g_report_error_for_invisible_icon = false;

// Returns true if the given file path exists and is not zero-length.
bool ValidateFilePath(const base::FilePath& path) {
  int64_t size = 0;
  return base::PathExists(path) && base::GetFileSize(path, &size) && size != 0;
}

// Returns true if the extension installation should flush all files and the
// directory.
bool UseSafeInstallation() {
  if (g_use_safe_installation == DEFAULT) {
    const char kFieldTrialName[] = "ExtensionUseSafeInstallation";
    const char kEnable[] = "Enable";
    return base::FieldTrialList::FindFullName(kFieldTrialName) == kEnable;
  }

  return g_use_safe_installation == ENABLED;
}

enum FlushOneOrAllFiles {
   ONE_FILE_ONLY,
   ALL_FILES
};

// Flush all files in a directory or just one.  When flushing all files, it
// makes sure every file is on disk.  When flushing one file only, it ensures
// all parent directories are on disk.
void FlushFilesInDir(const base::FilePath& path,
                     FlushOneOrAllFiles one_or_all_files) {
  if (!UseSafeInstallation()) {
    return;
  }
  base::FileEnumerator temp_traversal(path,
                                      true,  // recursive
                                      base::FileEnumerator::FILES);
  for (base::FilePath current = temp_traversal.Next(); !current.empty();
      current = temp_traversal.Next()) {
    base::File currentFile(current,
                           base::File::FLAG_OPEN | base::File::FLAG_WRITE);
    currentFile.Flush();
    currentFile.Close();
    if (one_or_all_files == ONE_FILE_ONLY) {
      break;
    }
  }
}

}  // namespace

const base::FilePath::CharType kTempDirectoryName[] = FILE_PATH_LITERAL("Temp");

void SetUseSafeInstallation(bool use_safe_installation) {
  g_use_safe_installation = use_safe_installation ? ENABLED : DISABLED;
}

base::FilePath InstallExtension(const base::FilePath& unpacked_source_dir,
                                const std::string& id,
                                const std::string& version,
                                const base::FilePath& extensions_dir) {
  base::FilePath extension_dir = extensions_dir.AppendASCII(id);
  base::FilePath version_dir;

  // Create the extension directory if it doesn't exist already.
  if (!base::PathExists(extension_dir)) {
    if (!base::CreateDirectory(extension_dir)) {
      return base::FilePath();
    }
  }

  // Get a temp directory on the same file system as the profile.
  base::FilePath install_temp_dir = GetInstallTempDir(extensions_dir);
  base::ScopedTempDir extension_temp_dir;
  if (install_temp_dir.empty() ||
      !extension_temp_dir.CreateUniqueTempDirUnderPath(install_temp_dir)) {
    LOG(ERROR) << "Creating of temp dir under in the profile failed.";
    return base::FilePath();
  }
  base::FilePath crx_temp_source =
      extension_temp_dir.GetPath().Append(unpacked_source_dir.BaseName());
  if (!base::Move(unpacked_source_dir, crx_temp_source)) {
    LOG(ERROR) << "Moving extension from : " << unpacked_source_dir.value()
               << " to : " << crx_temp_source.value() << " failed.";
    return base::FilePath();
  }

  // Try to find a free directory. There can be legitimate conflicts in the case
  // of overinstallation of the same version.
  const int kMaxAttempts = 100;
  for (int i = 0; i < kMaxAttempts; ++i) {
    base::FilePath candidate = extension_dir.AppendASCII(
        base::StringPrintf("%s_%u", version.c_str(), i));
    if (!base::PathExists(candidate)) {
      version_dir = candidate;
      break;
    }
  }

  if (version_dir.empty()) {
    LOG(ERROR) << "Could not find a home for extension " << id << " with "
               << "version " << version << ".";
    return base::FilePath();
  }

  // Flush the source dir completely before moving to make sure everything is
  // on disk. Otherwise a sudden power loss could cause the newly installed
  // extension to be in a corrupted state. Note that empty sub-directories
  // may still be lost.
  FlushFilesInDir(crx_temp_source, ALL_FILES);

  // The target version_dir does not exists yet, so base::Move() is using
  // rename() on POSIX systems. It is atomic in the sense that it will
  // either complete successfully or in the event of data loss be reverted.
  if (!base::Move(crx_temp_source, version_dir)) {
    LOG(ERROR) << "Installing extension from : " << crx_temp_source.value()
               << " into : " << version_dir.value() << " failed.";
    return base::FilePath();
  }

  // Flush one file in the new version_dir to make sure the dir move above is
  // persisted on disk. This is guaranteed on POSIX systems. ExtensionPrefs
  // is going to be updated with the new version_dir later. In the event of
  // data loss ExtensionPrefs should be pointing to the previous version which
  // is still fine.
  FlushFilesInDir(version_dir, ONE_FILE_ONLY);

  return version_dir;
}

void UninstallExtension(const base::FilePath& profile_dir,
                        const base::FilePath& extensions_install_dir,
                        const base::FilePath& extension_dir_to_delete) {
  // The below conditions are asserting that we should only be deleting
  // directories that are inside the `extensions_install_dir` which should be
  // inside the profile directory. Anything outside of that would be considered
  // invalid and dangerous since this is effectively an `rm -rf
  // <extension_delete_path>`.

  // Confirm that all the directories involved are not empty and are absolute so
  // that the subsequent comparisons have some value.
  if (profile_dir.empty() || extensions_install_dir.empty() ||
      extension_dir_to_delete.empty() || !profile_dir.IsAbsolute() ||
      !extensions_install_dir.IsAbsolute() ||
      !extension_dir_to_delete.IsAbsolute()) {
    return;
  }

  // Confirm the directory where we install extensions is a direct subdir of the
  // profile dir.
  if (extensions_install_dir.DirName() != profile_dir) {
    return;
  }

  // Confirm the directory we are obliterating is a direct subdir of the
  // extensions install directory.
  if (extension_dir_to_delete.DirName() != extensions_install_dir) {
    return;
  }

  base::DeletePathRecursively(extension_dir_to_delete);

  // We don't care about the return value. If this fails (and it can, due to
  // plugins that aren't unloaded yet), it will get cleaned up by
  // ExtensionGarbageCollector::GarbageCollectExtensions.
}

scoped_refptr<Extension> LoadExtension(const base::FilePath& extension_path,
                                       ManifestLocation location,
                                       int flags,
                                       std::string* error) {
  return LoadExtension(extension_path, nullptr, std::string(), location, flags,
                       error);
}

scoped_refptr<Extension> LoadExtension(const base::FilePath& extension_path,
                                       const ExtensionId& extension_id,
                                       ManifestLocation location,
                                       int flags,
                                       std::string* error) {
  return LoadExtension(extension_path, nullptr, extension_id, location, flags,
                       error);
}

scoped_refptr<Extension> LoadExtension(
    const base::FilePath& extension_path,
    const base::FilePath::CharType* manifest_file,
    const ExtensionId& extension_id,
    ManifestLocation location,
    int flags,
    std::string* error) {
  std::optional<base::Value::Dict> manifest;
  if (!manifest_file) {
    manifest = LoadManifest(extension_path, error);
  } else {
    manifest = LoadManifest(extension_path, manifest_file, error);
  }
  if (!manifest) {
    return nullptr;
  }

  if (!extension_l10n_util::LocalizeExtension(
          extension_path, &manifest.value(),
          extension_l10n_util::GetGzippedMessagesPermissionForLocation(
              location),
          error)) {
    return nullptr;
  }

  scoped_refptr<Extension> extension(Extension::Create(
      extension_path, location, *manifest, flags, extension_id, error));
  if (!extension.get()) {
    return nullptr;
  }

  std::vector<InstallWarning> warnings;
  if (!ValidateExtension(extension.get(), error, &warnings)) {
    return nullptr;
  }
  extension->AddInstallWarnings(std::move(warnings));

  return extension;
}

std::optional<base::Value::Dict> LoadManifest(
    const base::FilePath& extension_path,
    std::string* error) {
  return LoadManifest(extension_path, kManifestFilename, error);
}

std::optional<base::Value::Dict> LoadManifest(
    const base::FilePath& extension_path,
    const base::FilePath::CharType* manifest_filename,
    std::string* error) {
  base::FilePath manifest_path = extension_path.Append(manifest_filename);
  if (!base::PathExists(manifest_path)) {
    *error = l10n_util::GetStringUTF8(IDS_EXTENSION_MANIFEST_UNREADABLE);
    return std::nullopt;
  }

  JSONFileValueDeserializer deserializer(manifest_path);
  std::unique_ptr<base::Value> root(deserializer.Deserialize(nullptr, error));
  if (!root.get()) {
    if (error->empty()) {
      // If |error| is empty, then the file could not be read.
      // It would be cleaner to have the JSON reader give a specific error
      // in this case, but other code tests for a file error with
      // error->empty().  For now, be consistent.
      *error = l10n_util::GetStringUTF8(IDS_EXTENSION_MANIFEST_UNREADABLE);
    } else {
      *error = base::StringPrintf(
          "%s  %s", manifest_errors::kManifestParseError, error->c_str());
    }
    return std::nullopt;
  }

  if (!root->is_dict()) {
    *error = l10n_util::GetStringUTF8(IDS_EXTENSION_MANIFEST_INVALID);
    return std::nullopt;
  }

  return std::move(*root).TakeDict();
}

bool ValidateExtension(const Extension* extension,
                       std::string* error,
                       std::vector<InstallWarning>* warnings) {
  // Ask registered manifest handlers to validate their paths.
  if (!ManifestHandler::ValidateExtension(extension, error, warnings)) {
    return false;
  }

  // Check children of extension root to see if any of them start with _ and is
  // not on the reserved list. We only warn, and do not block the loading of the
  // extension.
  std::string warning;
  if (!CheckForIllegalFilenames(extension->path(), &warning)) {
    warnings->emplace_back(warning);
  }

  // Check that the extension does not include any Windows reserved filenames.
  std::string windows_reserved_warning;
  if (!CheckForWindowsReservedFilenames(extension->path(),
                                        &windows_reserved_warning)) {
    warnings->emplace_back(windows_reserved_warning);
  }

  // Check that extensions don't include private key files.
  std::vector<base::FilePath> private_keys =
      FindPrivateKeyFiles(extension->path());
  if (extension->creation_flags() & Extension::ERROR_ON_PRIVATE_KEY) {
    if (!private_keys.empty()) {
      // Only print one of the private keys because l10n_util doesn't have a way
      // to translate a list of strings.
      *error =
          l10n_util::GetStringFUTF8(IDS_EXTENSION_CONTAINS_PRIVATE_KEY,
                                    private_keys.front().LossyDisplayName());
      return false;
    }
  } else {
    for (const auto& private_key : private_keys) {
      warnings->emplace_back(l10n_util::GetStringFUTF8(
          IDS_EXTENSION_CONTAINS_PRIVATE_KEY, private_key.LossyDisplayName()));
    }
    // Only warn; don't block loading the extension.
  }
  return true;
}

std::vector<base::FilePath> FindPrivateKeyFiles(
    const base::FilePath& extension_dir) {
  std::vector<base::FilePath> result;
  // Pattern matching only works at the root level, so filter manually.
  base::FileEnumerator traversal(
      extension_dir, /*recursive=*/true, base::FileEnumerator::FILES);
  for (base::FilePath current = traversal.Next(); !current.empty();
       current = traversal.Next()) {
    if (!current.MatchesExtension(kExtensionKeyFileExtension)) {
      continue;
    }

    std::string key_contents;
    if (!base::ReadFileToString(current, &key_contents)) {
      // If we can't read the file, assume it's not a private key.
      continue;
    }
    std::string key_bytes;
    if (!Extension::ParsePEMKeyBytes(key_contents, &key_bytes)) {
      // If we can't parse the key, assume it's ok too.
      continue;
    }

    result.push_back(current);
  }
  return result;
}

bool CheckForIllegalFilenames(const base::FilePath& extension_path,
                              std::string* error) {
  // Enumerate all files and directories in the extension root.
  // There is a problem when using pattern "_*" with FileEnumerator, so we have
  // to cheat with find_first_of and match all.
  const int kFilesAndDirectories =
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES;
  base::FileEnumerator all_files(extension_path, false, kFilesAndDirectories);

  base::FilePath file;
  while (!(file = all_files.Next()).empty()) {
    base::FilePath::StringType filename = file.BaseName().value();

    // Skip all filenames that don't start with "_".
    if (filename.find_first_of(FILE_PATH_LITERAL("_")) != 0) {
      continue;
    }

    // Some filenames are special and allowed to start with "_".
    if (filename == kLocaleFolder || filename == kPlatformSpecificFolder ||
        filename == FILE_PATH_LITERAL("__MACOSX")) {
      continue;
    }

    *error = base::StringPrintf(
        "Cannot load extension with file or directory name %s. "
        "Filenames starting with \"_\" are reserved for use by the system.",
        file.BaseName().AsUTF8Unsafe().c_str());
    return false;
  }

  return true;
}

bool CheckForWindowsReservedFilenames(const base::FilePath& extension_dir,
                                      std::string* error) {
  const int kFilesAndDirectories =
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES;
  base::FileEnumerator traversal(extension_dir, true, kFilesAndDirectories);

  for (base::FilePath current = traversal.Next(); !current.empty();
       current = traversal.Next()) {
    base::FilePath::StringType filename = current.BaseName().value();
    bool is_reserved_filename = net::IsReservedNameOnWindows(filename);
    if (is_reserved_filename) {
      *error = base::StringPrintf(
          "Cannot load extension with file or directory name %s. "
          "The filename is illegal.",
          current.BaseName().AsUTF8Unsafe().c_str());
      return false;
    }
  }

  return true;
}

base::FilePath GetInstallTempDir(const base::FilePath& extensions_dir) {
  // We do file IO in this function, but only when the current profile's
  // Temp directory has never been used before, or in a rare error case.
  // Developers are not likely to see these situations often.

  // Create the temp directory as a sub-directory of the Extensions directory.
  // This guarantees it is on the same file system as the extension's eventual
  // install target.
  base::FilePath temp_path = extensions_dir.Append(kTempDirectoryName);
  if (base::PathExists(temp_path)) {
    if (!base::DirectoryExists(temp_path)) {
      DLOG(WARNING) << "Not a directory: " << temp_path.value();
      return base::FilePath();
    }
    if (!base::PathIsWritable(temp_path)) {
      DLOG(WARNING) << "Can't write to path: " << temp_path.value();
      return base::FilePath();
    }
    // This is a directory we can write to.
    return temp_path;
  }

  // Directory doesn't exist, so create it.
  if (!base::CreateDirectory(temp_path)) {
    DLOG(WARNING) << "Couldn't create directory: " << temp_path.value();
    return base::FilePath();
  }
  return temp_path;
}

base::FilePath ExtensionURLToRelativeFilePath(const GURL& url) {
  std::string_view url_path = url.path_piece();
  if (url_path.empty() || url_path[0] != '/') {
    return base::FilePath();
  }

  // Convert %-encoded UTF8 to regular UTF8.
  std::string file_path;
  if (!base::UnescapeBinaryURLComponentSafe(
          url_path, true /* fail_on_path_separators */, &file_path)) {
    // There shouldn't be any escaped path separators or control characters in
    // the path. However, if there are, it's best to just fail.
    return base::FilePath();
  }

  // Drop the leading slashes.
  size_t skip = file_path.find_first_not_of("/\\");
  if (skip != file_path.npos) {
    file_path = file_path.substr(skip);
  }

  base::FilePath path = base::FilePath::FromUTF8Unsafe(file_path);

  // It's still possible for someone to construct an annoying URL whose path
  // would still wind up not being considered relative at this point.
  // For example: chrome-extension://id/c:////foo.html
  if (path.IsAbsolute()) {
    return base::FilePath();
  }

  return path;
}

void SetReportErrorForInvisibleIconForTesting(bool value) {
  g_report_error_for_invisible_icon = value;
}

bool ValidateExtensionIconSet(const ExtensionIconSet& icon_set,
                              const Extension* extension,
                              const char* manifest_key,
                              std::string* error) {
  for (const auto& entry : icon_set.map()) {
    const base::FilePath path =
        extension->GetResource(entry.second).GetFilePath();
    if (!ValidateFilePath(path)) {
      constexpr char kIconMissingError[] =
          "Could not load icon '%s' specified in '%s'.";
      *error = base::StringPrintf(kIconMissingError, entry.second.c_str(),
                                  manifest_key);
      return false;
    }

    if (extension->location() == ManifestLocation::kUnpacked) {
      const bool is_sufficiently_visible =
          image_util::IsIconAtPathSufficientlyVisible(path);
      if (!is_sufficiently_visible && g_report_error_for_invisible_icon) {
        constexpr char kIconNotSufficientlyVisibleError[] =
            "Icon '%s' specified in '%s' is not sufficiently visible.";
        *error = base::StringPrintf(kIconNotSufficientlyVisibleError,
                                    entry.second.c_str(), manifest_key);
        return false;
      }
    }
  }
  return true;
}

MessageBundle* LoadMessageBundle(
    const base::FilePath& extension_path,
    const std::string& default_locale,
    extension_l10n_util::GzippedMessagesPermission gzip_permission,
    std::string* error) {
  error->clear();
  // Load locale information if available.
  base::FilePath locale_path = extension_path.Append(kLocaleFolder);
  if (!base::PathExists(locale_path)) {
    return nullptr;
  }

  std::set<std::string> chrome_locales;
  extension_l10n_util::GetAllLocales(&chrome_locales);

  base::FilePath default_locale_path = locale_path.AppendASCII(default_locale);
  if (default_locale.empty() ||
      !base::Contains(chrome_locales, default_locale) ||
      !base::PathExists(default_locale_path)) {
    *error = l10n_util::GetStringUTF8(
        IDS_EXTENSION_LOCALES_NO_DEFAULT_LOCALE_SPECIFIED);
    return nullptr;
  }

  MessageBundle* message_bundle = extension_l10n_util::LoadMessageCatalogs(
      locale_path, default_locale, gzip_permission, error);

  return message_bundle;
}

base::FilePath GetVerifiedContentsPath(const base::FilePath& extension_path) {
  return extension_path.Append(kMetadataFolder)
      .Append(kVerifiedContentsFilename);
}
base::FilePath GetComputedHashesPath(const base::FilePath& extension_path) {
  return extension_path.Append(kMetadataFolder).Append(kComputedHashesFilename);
}
base::FilePath GetIndexedRulesetDirectoryRelativePath() {
  return base::FilePath(kMetadataFolder).Append(kIndexedRulesetDirectory);
}
base::FilePath GetIndexedRulesetRelativePath(int static_ruleset_id) {
  const char* kRulesetPrefix = "_ruleset";
  std::string filename =
      kRulesetPrefix + base::NumberToString(static_ruleset_id);
  return GetIndexedRulesetDirectoryRelativePath().AppendASCII(filename);
}

std::vector<base::FilePath> GetReservedMetadataFilePaths(
    const base::FilePath& extension_path) {
  return {GetVerifiedContentsPath(extension_path),
          GetComputedHashesPath(extension_path),
          extension_path.Append(GetIndexedRulesetDirectoryRelativePath())};
}

}  // namespace extensions::file_util
