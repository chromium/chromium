// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_FILEAPI_FILE_SYSTEM_UTIL_H_
#define STORAGE_COMMON_FILEAPI_FILE_SYSTEM_UTIL_H_

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "storage/common/fileapi/file_system_info.h"
#include "storage/common/fileapi/file_system_types.h"
#include "storage/common/storage_common_export.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "third_party/blink/public/platform/web_file_system_type.h"

class GURL;

namespace storage {

STORAGE_COMMON_EXPORT extern const char kPersistentDir[];
STORAGE_COMMON_EXPORT extern const char kTemporaryDir[];
STORAGE_COMMON_EXPORT extern const char kExternalDir[];
STORAGE_COMMON_EXPORT extern const char kIsolatedDir[];
STORAGE_COMMON_EXPORT extern const char kTestDir[];

class STORAGE_COMMON_EXPORT VirtualPath {
 public:
  static const base::FilePath::CharType kRoot[];
  static const base::FilePath::CharType kSeparator;

  // Use this instead of base::FilePath::BaseName when operating on virtual
  // paths. FilePath::BaseName will get confused by ':' on Windows when it
  // looks like a drive letter separator; this will treat it as just another
  // character.
  static base::FilePath BaseName(const base::FilePath& virtual_path);

  // Use this instead of base::FilePath::DirName when operating on virtual
  // paths.
  static base::FilePath DirName(const base::FilePath& virtual_path);

  // Likewise, use this instead of base::FilePath::GetComponents when
  // operating on virtual paths.
  // Note that this assumes very clean input, with no leading slash, and
  // it will not evaluate '..' components.
  static std::vector<base::FilePath::StringType> GetComponents(
      const base::FilePath& path);

  static std::vector<std::string> GetComponentsUTF8Unsafe(
      const base::FilePath& path);

  // Returns a path name ensuring that it begins with kRoot and all path
  // separators are forward slashes /.
  static base::FilePath::StringType GetNormalizedFilePath(
      const base::FilePath& path);

  // Returns true if the given path begins with kRoot.
  static bool IsAbsolute(const base::FilePath::StringType& path);

  // Returns true if the given path points to the root.
  static bool IsRootPath(const base::FilePath& path);
};

// Parses filesystem scheme |url| into uncracked file system URL components.
// Example: For a URL 'filesystem:http://foo.com/temporary/foo/bar',
// |origin_url| is set to 'http://foo.com', |type| is set to
// kFileSystemTypeTemporary, and |virtual_path| is set to 'foo/bar'.
STORAGE_COMMON_EXPORT bool ParseFileSystemSchemeURL(
    const GURL& url,
    GURL* origin_url,
    FileSystemType* type,
    base::FilePath* virtual_path);

// Returns the root URI of the filesystem that can be specified by a pair of
// |origin_url| and |type|.  The returned URI can be used as a root path
// of the filesystem (e.g. <returned_URI> + "/relative/path" will compose
// a path pointing to the entry "/relative/path" in the filesystem).
//
// For Isolated filesystem this returns the 'common' root part, e.g.
// returns URL without the filesystem ID.
//
// |type| needs to be public type as the returned URI is given to the renderer.
STORAGE_COMMON_EXPORT GURL GetFileSystemRootURI(const GURL& origin_url,
                                                       FileSystemType type);

// Returns the name for the filesystem that is specified by a pair of
// |origin_url| and |type|.
// (The name itself is neither really significant nor a formal identifier
// but can be read as the .name field of the returned FileSystem object
// as a user-friendly name in the javascript layer).
//
// |type| needs to be public type as the returned name is given to the renderer.
//
// Example:
//   The name for a TEMPORARY filesystem of "http://www.example.com:80/"
//   should look like: "http_www.example.host_80:temporary"
STORAGE_COMMON_EXPORT std::string
GetFileSystemName(const GURL& origin_url, FileSystemType type);

// Converts FileSystemType |type| to/from the StorageType |storage_type| that
// is used for the unified quota system.
// (Basically this naively maps TEMPORARY storage type to TEMPORARY filesystem
// type, PERSISTENT storage type to PERSISTENT filesystem type and vice versa.)
STORAGE_COMMON_EXPORT FileSystemType
QuotaStorageTypeToFileSystemType(blink::mojom::StorageType storage_type);

STORAGE_COMMON_EXPORT blink::mojom::StorageType
FileSystemTypeToQuotaStorageType(FileSystemType type);

// Returns the string representation of the given filesystem |type|.
// Returns an empty string if the |type| is invalid.
STORAGE_COMMON_EXPORT std::string
GetFileSystemTypeString(FileSystemType type);

// Sets type to FileSystemType enum that corresponds to the string name.
// Returns false if the |type_string| is invalid.
STORAGE_COMMON_EXPORT bool GetFileSystemPublicType(
    std::string type_string,
    blink::WebFileSystemType* type);

// Encodes |file_path| to a string.
// Following conditions should be held:
//  - StringToFilePath(FilePathToString(path)) == path
//  - StringToFilePath(FilePathToString(path) + "/" + "SubDirectory") ==
//    path.AppendASCII("SubDirectory");
STORAGE_COMMON_EXPORT std::string FilePathToString(
    const base::FilePath& file_path);

// Decode a file path from |file_path_string|.
STORAGE_COMMON_EXPORT base::FilePath StringToFilePath(
    const std::string& file_path_string);

// Generate a file system name for the given arguments. Should only be used by
// platform apps.
STORAGE_COMMON_EXPORT std::string GetIsolatedFileSystemName(
    const GURL& origin_url,
    const std::string& filesystem_id);

// Find the file system id from |filesystem_name|. Should only be used by
// platform apps. This function will return false if the file system name is
// not of the form {origin}:Isolated_{id}, and will also check that there is an
// origin and id present. It will not check that the origin or id are valid.
STORAGE_COMMON_EXPORT bool CrackIsolatedFileSystemName(
    const std::string& filesystem_name,
    std::string* filesystem_id);

// Validates the given isolated file system id.
STORAGE_COMMON_EXPORT bool ValidateIsolatedFileSystemId(
    const std::string& filesystem_id);

// Returns the root URI for an isolated filesystem for origin |origin_url|
// and |filesystem_id|. If the |optional_root_name| is given the resulting
// root URI will point to the subfolder within the isolated filesystem.
STORAGE_COMMON_EXPORT std::string GetIsolatedFileSystemRootURIString(
    const GURL& origin_url,
    const std::string& filesystem_id,
    const std::string& optional_root_name);

// Returns the root URI for an external filesystem for origin |origin_url|
// and |mount_name|.
STORAGE_COMMON_EXPORT std::string GetExternalFileSystemRootURIString(
    const GURL& origin_url,
    const std::string& mount_name);

// Translates the net::Error to base::File::Error.
STORAGE_COMMON_EXPORT base::File::Error
NetErrorToFileError(int error);

}  // namespace storage

#endif  // STORAGE_COMMON_FILEAPI_FILE_SYSTEM_UTIL_H_
