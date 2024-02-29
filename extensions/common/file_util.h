// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FILE_UTIL_H_
#define EXTENSIONS_COMMON_FILE_UTIL_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "third_party/skia/include/core/SkColor.h"

class ExtensionIconSet;
class GURL;

namespace extension_l10n_util {
enum class GzippedMessagesPermission;
}

namespace extensions {
class Extension;
struct InstallWarning;

// Utilities for manipulating the on-disk storage of extensions.
namespace file_util {

extern const base::FilePath::CharType kTempDirectoryName[];

// Sets the flag to enable safe installation (i.e. flush all installed files).
void SetUseSafeInstallation(bool use_safe_installation);

// Copies |unpacked_source_dir| into the right location under |extensions_dir|.
// The destination directory is returned on success, or empty path is returned
// on failure.
base::FilePath InstallExtension(const base::FilePath& unpacked_source_dir,
                                const std::string& id,
                                const std::string& version,
                                const base::FilePath& extensions_dir);

// Removes all versions of the extension from `extension_dir_to_delete` by
// deleting the folder. `profile_dir` is the path to the current Chrome profile
// directory. Requirements:
//   *)  all paths cannot be empty
//   *) all paths must be absolute must be absolute
//   *) `extensions_dir` must be a direct subdir of `profile_dir`
//   *  `extension_dir_to_delete` must be a direct subdir of `extensions_dir`
// Otherwise the deletion will not be performed to avoid the risk of dangerous
// paths like ".", "..", etc.
void UninstallExtension(const base::FilePath& profile_dir,
                        const base::FilePath& extensions_install_dir,
                        const base::FilePath& extension_dir_to_delete);

// Loads and validates an extension from the specified directory. Uses
// the default manifest filename. Returns nullptr on failure, with a
// description of the error in |error|.
scoped_refptr<Extension> LoadExtension(const base::FilePath& extension_root,
                                       mojom::ManifestLocation location,
                                       int flags,
                                       std::string* error);

// The same as LoadExtension except use the provided |extension_id|.
scoped_refptr<Extension> LoadExtension(const base::FilePath& extension_root,
                                       const ExtensionId& extension_id,
                                       mojom::ManifestLocation location,
                                       int flags,
                                       std::string* error);

// The same as LoadExtension except use the provided |manifest_file| and
// |extension_id|.  If manifest_file is not specified, uses the default
// manifest filename.
scoped_refptr<Extension> LoadExtension(
    const base::FilePath& extension_root,
    const base::FilePath::CharType* manifest_file,
    const ExtensionId& extension_id,
    mojom::ManifestLocation location,
    int flags,
    std::string* error);

// Loads an extension manifest from the specified directory. Returns
// `std::nullopt` on failure, with a description of the error in |error|.
std::optional<base::Value::Dict> LoadManifest(
    const base::FilePath& extension_root,
    std::string* error);

// Convenience overload for specifying a manifest filename.
std::optional<base::Value::Dict> LoadManifest(
    const base::FilePath& extension_root,
    const base::FilePath::CharType* manifest_filename,
    std::string* error);

// Returns true if the given extension object is valid and consistent.
// May also append a series of warning messages to |warnings|, but they
// should not prevent the extension from running.
//
// Otherwise, returns false, and a description of the error is
// returned in |error|.
bool ValidateExtension(const Extension* extension,
                       std::string* error,
                       std::vector<InstallWarning>* warnings);

// Returns a list of files that contain private keys inside |extension_dir|.
std::vector<base::FilePath> FindPrivateKeyFiles(
    const base::FilePath& extension_dir);

// We need to reserve the namespace of entries that start with "_" for future
// use by Chrome.
// If any files or directories are found using "_" prefix and are not on
// reserved list we return false, and set error message.
bool CheckForIllegalFilenames(const base::FilePath& extension_path,
                              std::string* error);

// We need to reserve the names of special Windows filenames, such as
// "com2.zip."
// If any files or directories are found to be using a reserved Windows
// filename, we return false, and set error message.
bool CheckForWindowsReservedFilenames(const base::FilePath& extension_dir,
                                      std::string* error);

// Returns a path to a temporary directory for unpacking an extension that will
// be installed into |extensions_dir|. Creates the directory if necessary.
// The directory will be on the same file system as |extensions_dir| so
// that the extension directory can be efficiently renamed into place. Returns
// an empty file path on failure.
base::FilePath GetInstallTempDir(const base::FilePath& extensions_dir);

// Get a relative file path from a chrome-extension:// URL.
base::FilePath ExtensionURLToRelativeFilePath(const GURL& url);

// If |value| is true, when ValidateExtensionIconSet is called for unpacked
// extensions, an icon which is not sufficiently visible will be reported as
// an error.
void SetReportErrorForInvisibleIconForTesting(bool value);

// Returns true if the icons in |icon_set| exist, and, if enabled, checks that
// they are sufficiently visible compared to |background_color|. On failure,
// populates |error|, which will include the given |manifest_key|.
bool ValidateExtensionIconSet(const ExtensionIconSet& icon_set,
                              const Extension* extension,
                              const char* manifest_key,
                              std::string* error);

// Loads extension message catalogs and returns message bundle. Passes
// |gzip_permission| to extension_l10n_util::LoadMessageCatalogs (see
// extension_l10n_util.h for details).
// Returns null on error or if the extension is not localized.
MessageBundle* LoadMessageBundle(
    const base::FilePath& extension_path,
    const std::string& default_locale,
    extension_l10n_util::GzippedMessagesPermission gzip_permission,
    std::string* error);

// Helper functions for getting paths for files used in content verification.
base::FilePath GetVerifiedContentsPath(const base::FilePath& extension_path);
base::FilePath GetComputedHashesPath(const base::FilePath& extension_path);

// Helper function to get the relative path for the directory containing static
// indexed rulesets. Path is relative to the extension path. Used by the
// Declarative Net Request API.
base::FilePath GetIndexedRulesetDirectoryRelativePath();

// Helper function to get the relative path for a given static indexed ruleset.
// Path is relative to the extension path. This is used by the Declarative Net
// Request API.
base::FilePath GetIndexedRulesetRelativePath(int static_ruleset_id);

// Returns the list of file-paths reserved for use by the Extension system in
// the kMetadataFolder.
std::vector<base::FilePath> GetReservedMetadataFilePaths(
    const base::FilePath& extension_path);

}  // namespace file_util
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FILE_UTIL_H_
