// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FILE_UTIL_H_
#define EXTENSIONS_COMMON_FILE_UTIL_H_

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "extensions/common/manifest.h"
#include "extensions/common/message_bundle.h"
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

// Removes all versions of the extension with |id| from |extensions_dir|.
void UninstallExtension(const base::FilePath& extensions_dir,
                        const std::string& id);

// Loads and validates an extension from the specified directory. Returns NULL
// on failure, with a description of the error in |error|.
scoped_refptr<Extension> LoadExtension(const base::FilePath& extension_root,
                                       Manifest::Location location,
                                       int flags,
                                       std::string* error);

// The same as LoadExtension except use the provided |extension_id|.
scoped_refptr<Extension> LoadExtension(const base::FilePath& extension_root,
                                       const std::string& extension_id,
                                       Manifest::Location location,
                                       int flags,
                                       std::string* error);

// Loads an extension manifest from the specified directory. Returns NULL
// on failure, with a description of the error in |error|.
std::unique_ptr<base::DictionaryValue> LoadManifest(
    const base::FilePath& extension_root,
    std::string* error);

// Convenience overload for specifying a manifest filename.
std::unique_ptr<base::DictionaryValue> LoadManifest(
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

// Helper function to delete files. This is used to avoid ugly casts which
// would be necessary with PostMessage since base::Delete is overloaded.
// TODO(skerner): Make a version of Delete that is not overloaded in file_util.
void DeleteFile(const base::FilePath& path, bool recursive);

// Get a relative file path from a chrome-extension:// URL.
base::FilePath ExtensionURLToRelativeFilePath(const GURL& url);

// If |value| is true, when ValidateExtensionIconSet is called for unpacked
// extensions, an icon which is not sufficiently visible will be reported as
// an error.
void SetReportErrorForInvisibleIconForTesting(bool value);

// Returns true if the icons in |icon_set| exist. Otherwise, populates
// |error| with the |error_message_id| for an invalid file. If an icon
// is not sufficiently visible, and error checking is enabled, |error|
// is populated with a different message, rather than one specified
// by |error_message_id|.
bool ValidateExtensionIconSet(const ExtensionIconSet& icon_set,
                              const Extension* extension,
                              int error_message_id,
                              SkColor background_color,
                              std::string* error);

// Loads extension message catalogs and returns message bundle. Passes
// |gzip_permission| to extension_l10n_util::LoadMessageCatalogs for
// trused sources (see extension_l10n_util.h for details).
// Returns null on error or if the extension is not localized.
MessageBundle* LoadMessageBundle(
    const base::FilePath& extension_path,
    const std::string& default_locale,
    extension_l10n_util::GzippedMessagesPermission gzip_permission,
    std::string* error);

// Loads the extension message bundle substitution map. Contains at least
// the extension_id item. Does not supported compressed locale files.
MessageBundle::SubstitutionMap* LoadMessageBundleSubstitutionMap(
    const base::FilePath& extension_path,
    const std::string& extension_id,
    const std::string& default_locale);

// Loads the extension message bundle substitution map for a non-localized
// extension. Contains only the extension_id item.
// This doesn't require hitting disk, so it's safe to call on any thread.
MessageBundle::SubstitutionMap* LoadNonLocalizedMessageBundleSubstitutionMap(
    const std::string& extension_id);

// Loads the extension message bundle substitution map from the specified paths.
// Contains at least the extension_id item.
MessageBundle::SubstitutionMap* LoadMessageBundleSubstitutionMapFromPaths(
    const std::vector<base::FilePath>& paths,
    const std::string& extension_id,
    const std::string& default_locale);

// Helper functions for getting paths for files used in content verification.
base::FilePath GetVerifiedContentsPath(const base::FilePath& extension_path);
base::FilePath GetComputedHashesPath(const base::FilePath& extension_path);

// Helper function to get path used for the indexed ruleset by the Declarative
// Net Request API.
base::FilePath GetIndexedRulesetPath(const base::FilePath& extension_path);

// Returns the list of file-paths reserved for use by the Extension system in
// the kMetadataFolder.
std::vector<base::FilePath> GetReservedMetadataFilePaths(
    const base::FilePath& extension_path);

}  // namespace file_util
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FILE_UTIL_H_
