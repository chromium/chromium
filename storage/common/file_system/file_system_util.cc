// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "storage/common/file_system/file_system_util.h"

#include <stddef.h>

#include <algorithm>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "storage/common/database/database_identifier.h"
#include "url/gurl.h"

namespace storage {

const char kPersistentDir[] = "/persistent";
const char kTemporaryDir[] = "/temporary";
const char kIsolatedDir[] = "/isolated";
const char kExternalDir[] = "/external";
const char kTestDir[] = "/test";

const base::FilePath::CharType VirtualPath::kRoot[] = FILE_PATH_LITERAL("/");
const base::FilePath::CharType VirtualPath::kSeparator = FILE_PATH_LITERAL('/');

// TODO(ericu): Consider removing support for '\', even on Windows, if possible.
// There's a lot of test code that will need reworking, and we may have trouble
// with base::FilePath elsewhere [e.g. DirName and other methods may also need
// replacement].
base::FilePath VirtualPath::BaseName(const base::FilePath& virtual_path) {
  base::FilePath::StringType path = virtual_path.value();

  // Keep everything after the final separator, but if the pathname is only
  // one character and it's a separator, leave it alone.
  while (path.size() > 1 && base::FilePath::IsSeparator(path.back()))
    path.resize(path.size() - 1);
  base::FilePath::StringType::size_type last_separator =
      path.find_last_of(base::FilePath::kSeparators);
  if (last_separator != base::FilePath::StringType::npos &&
      last_separator < path.size() - 1)
    path.erase(0, last_separator + 1);

  return base::FilePath(path);
}

base::FilePath VirtualPath::DirName(const base::FilePath& virtual_path) {
  using StringType = base::FilePath::StringType;
  StringType path = virtual_path.value();

  // The logic below is taken from that of base::FilePath::DirName, except
  // that this version never cares about '//' or drive-letters even on win32.

  // Strip trailing separators.
  while (path.size() > 1 && base::FilePath::IsSeparator(path.back()))
    path.resize(path.size() - 1);

  StringType::size_type last_separator =
      path.find_last_of(base::FilePath::kSeparators);
  if (last_separator == StringType::npos) {
    // path_ is in the current directory.
    return base::FilePath(base::FilePath::kCurrentDirectory);
  }
  if (last_separator == 0) {
    // path_ is in the root directory.
    return base::FilePath(path.substr(0, 1));
  }
  // path_ is somewhere else, trim the basename.
  path.resize(last_separator);

  // Strip trailing separators.
  while (path.size() > 1 && base::FilePath::IsSeparator(path.back()))
    path.resize(path.size() - 1);

  if (path.empty())
    return base::FilePath(base::FilePath::kCurrentDirectory);

  return base::FilePath(path);
}

std::vector<base::FilePath::StringType> VirtualPath::GetComponents(
    const base::FilePath& path) {
  using StringType = base::FilePath::StringType;

  std::vector<StringType> components;
  if (path.value().empty())
    return components;

  StringType::size_type begin = 0, end = 0;
  while (begin < path.value().length() && end != StringType::npos) {
    end = path.value().find_first_of(base::FilePath::kSeparators, begin);
    StringType component = path.value().substr(
        begin, end == StringType::npos ? StringType::npos : end - begin);
    if (!component.empty() && component != base::FilePath::kCurrentDirectory)
      components.push_back(component);
    begin = end + 1;
  }
  return components;
}

std::vector<std::string> VirtualPath::GetComponentsUTF8Unsafe(
    const base::FilePath& path) {
  std::vector<base::FilePath::StringType> stringtype_components =
      VirtualPath::GetComponents(path);
  std::vector<std::string> components;
  components.reserve(stringtype_components.size());
  for (const auto& component : stringtype_components)
    components.push_back(base::FilePath(component).AsUTF8Unsafe());
  return components;
}

base::FilePath::StringType VirtualPath::GetNormalizedFilePath(
    const base::FilePath& path) {
  base::FilePath::StringType normalized_path = path.value();
  const size_t num_separators =
      base::FilePath::StringType(base::FilePath::kSeparators).length();
  for (size_t i = 0; i < num_separators; ++i) {
    std::replace(normalized_path.begin(), normalized_path.end(),
                 base::FilePath::kSeparators[i], kSeparator);
  }

  return (IsAbsolute(normalized_path))
             ? normalized_path
             : base::FilePath::StringType(kRoot) + normalized_path;
}

bool VirtualPath::IsAbsolute(const base::FilePath::StringType& path) {
  return base::StartsWith(path, kRoot, base::CompareCase::SENSITIVE);
}

bool VirtualPath::IsRootPath(const base::FilePath& path) {
  std::vector<base::FilePath::StringType> components =
      VirtualPath::GetComponents(path);
  return (path.empty() || components.empty() ||
          (components.size() == 1 && components[0] == VirtualPath::kRoot));
}

bool ParseFileSystemSchemeURL(const GURL& url,
                              GURL* origin_url,
                              FileSystemType* type,
                              base::FilePath* virtual_path) {
  GURL origin;
  FileSystemType file_system_type = kFileSystemTypeUnknown;

  if (!url.is_valid() || !url.SchemeIsFileSystem())
    return false;

  const struct {
    FileSystemType type;
    const char* dir;
  } kValidTypes[] = {
      {kFileSystemTypePersistent, kPersistentDir},
      {kFileSystemTypeTemporary, kTemporaryDir},
      {kFileSystemTypeIsolated, kIsolatedDir},
      {kFileSystemTypeExternal, kExternalDir},
      {kFileSystemTypeTest, kTestDir},
  };

  // A path of the inner_url contains only mount type part (e.g. "/temporary").
  DCHECK(url.inner_url());
  std::string inner_path = url.inner_url()->path();
  for (const auto& valid_type : kValidTypes) {
    if (inner_path == valid_type.dir) {
      file_system_type = valid_type.type;
      break;
    }
  }

  if (file_system_type == kFileSystemTypeUnknown)
    return false;

  std::string path = base::UnescapeBinaryURLComponent(url.path_piece());

  // Ensure the path is relative.
  while (!path.empty() && path[0] == '/')
    path.erase(0, 1);

  base::FilePath converted_path = base::FilePath::FromUTF8Unsafe(path);

  // All parent references should have been resolved in the renderer.
  if (converted_path.ReferencesParent())
    return false;

  if (origin_url)
    *origin_url = url.DeprecatedGetOriginAsURL();
  if (type)
    *type = file_system_type;
  if (virtual_path)
    *virtual_path =
        converted_path.NormalizePathSeparators().StripTrailingSeparators();

  return true;
}

GURL GetFileSystemRootURI(const GURL& origin_url, FileSystemType type) {
  // origin_url is based on a security origin, so http://foo.com or file:///
  // instead of the corresponding filesystem URL.
  DCHECK(!origin_url.SchemeIsFileSystem());

  std::string url = "filesystem:" + origin_url.GetWithEmptyPath().spec();
  switch (type) {
    case kFileSystemTypeTemporary:
      url += (kTemporaryDir + 1);  // We don't want the leading slash.
      return GURL(url + "/");
    case kFileSystemTypePersistent:
      url += (kPersistentDir + 1);  // We don't want the leading slash.
      return GURL(url + "/");
    case kFileSystemTypeExternal:
      url += (kExternalDir + 1);  // We don't want the leading slash.
      return GURL(url + "/");
    case kFileSystemTypeIsolated:
      url += (kIsolatedDir + 1);  // We don't want the leading slash.
      return GURL(url + "/");
    case kFileSystemTypeTest:
      url += (kTestDir + 1);  // We don't want the leading slash.
      return GURL(url + "/");
      // Internal types are always pointed via isolated or external URLs.
    default:
      NOTREACHED();
  }
}

std::string GetFileSystemName(const GURL& origin_url, FileSystemType type) {
  std::string origin_identifier = storage::GetIdentifierFromOrigin(origin_url);
  std::string type_string = GetFileSystemTypeString(type);
  DCHECK(!type_string.empty());
  return origin_identifier + ":" + type_string;
}

std::string GetFileSystemTypeString(FileSystemType type) {
  switch (type) {
    case kFileSystemTypeTemporary:
      return "Temporary";
    case kFileSystemTypePersistent:
      return "Persistent";
    case kFileSystemTypeIsolated:
      return "Isolated";
    case kFileSystemTypeExternal:
      return "External";
    case kFileSystemTypeTest:
      return "Test";
    case kFileSystemTypeLocal:
      return "Local";
    case kFileSystemTypeDragged:
      return "Dragged";
    case kFileSystemTypeLocalMedia:
      return "LocalMedia";
    case kFileSystemTypeDeviceMedia:
      return "DeviceMedia";
    case kFileSystemTypeSyncable:
    case kFileSystemTypeSyncableForInternalSync:
      return "Syncable";
    case kFileSystemTypeLocalForPlatformApp:
      return "LocalForPlatformApp";
    case kFileSystemTypeForTransientFile:
      return "TransientFile";
    case kFileSystemTypeProvided:
      return "Provided";
    case kFileSystemTypeDeviceMediaAsFileStorage:
      return "DeviceMediaStorage";
    case kFileSystemTypeArcContent:
      return "ArcContent";
    case kFileSystemTypeArcDocumentsProvider:
      return "ArcDocumentsProvider";
    case kFileSystemTypeDriveFs:
      return "DriveFs";
    case kFileSystemTypeSmbFs:
      return "SmbFs";
    case kFileSystemTypeFuseBox:
      return "FuseBox";
    case kFileSystemInternalTypeEnumStart:
    case kFileSystemInternalTypeEnumEnd:
      NOTREACHED();
    case kFileSystemTypeUnknown:
      return "Unknown";
  }
  NOTREACHED();
}

std::string FilePathToString(const base::FilePath& file_path) {
  // TODO(pkasting): Probably this should use AsUTF8Unsafe() across platforms.
#if BUILDFLAG(IS_WIN)
  return file_path.AsUTF8Unsafe();
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return file_path.value();
#endif
}

base::FilePath StringToFilePath(const std::string& file_path_string) {
  // TODO(pkasting): Probably this should use FromUTF8Unsafe() across platforms.
#if BUILDFLAG(IS_WIN)
  return base::FilePath::FromUTF8Unsafe(file_path_string);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return base::FilePath(file_path_string);
#endif
}

bool GetFileSystemPublicType(const std::string& type_string,
                             blink::WebFileSystemType* type) {
  DCHECK(type);
  if (type_string == "Temporary") {
    *type = blink::kWebFileSystemTypeTemporary;
    return true;
  }
  if (type_string == "Persistent") {
    *type = blink::kWebFileSystemTypePersistent;
    return true;
  }
  if (type_string == "Isolated") {
    *type = blink::kWebFileSystemTypeIsolated;
    return true;
  }
  if (type_string == "External") {
    *type = blink::kWebFileSystemTypeExternal;
    return true;
  }
  NOTREACHED();
}

std::string GetIsolatedFileSystemName(const GURL& origin_url,
                                      const std::string& filesystem_id) {
  std::string name(
      storage::GetFileSystemName(origin_url, storage::kFileSystemTypeIsolated));
  name.append("_");
  name.append(filesystem_id);
  return name;
}

bool CrackIsolatedFileSystemName(const std::string& filesystem_name,
                                 std::string* filesystem_id) {
  DCHECK(filesystem_id);

  // |filesystem_name| is of the form {origin}:isolated_{filesystem_id}.
  std::string start_token(":");
  start_token =
      start_token.append(GetFileSystemTypeString(kFileSystemTypeIsolated))
          .append("_");
  // WebKit uses different case in its constant for isolated file system
  // names, so we do a case insensitive compare by converting both strings
  // to uppercase.
  // TODO(benwells): Remove this when WebKit uses the same constant.
  start_token = base::ToUpperASCII(start_token);
  std::string filesystem_name_upper = base::ToUpperASCII(filesystem_name);
  size_t pos = filesystem_name_upper.find(start_token);
  if (pos == std::string::npos)
    return false;
  if (pos == 0)
    return false;

  *filesystem_id =
      filesystem_name.substr(pos + start_token.length(), std::string::npos);
  if (filesystem_id->empty())
    return false;

  return true;
}

bool ValidateIsolatedFileSystemId(const std::string& filesystem_id) {
  const size_t kExpectedFileSystemIdSize = 32;
  return (filesystem_id.size() == kExpectedFileSystemIdSize) &&
         base::ContainsOnlyChars(filesystem_id, "ABCDEF0123456789");
}

std::string GetIsolatedFileSystemRootURIString(
    const GURL& origin_url,
    const std::string& filesystem_id,
    const std::string& optional_root_name) {
  std::string root =
      GetFileSystemRootURI(origin_url, kFileSystemTypeIsolated).spec();
  if (base::FilePath::FromUTF8Unsafe(filesystem_id).ReferencesParent())
    return std::string();
  root.append(base::EscapePath(filesystem_id));
  root.append("/");
  if (!optional_root_name.empty()) {
    if (base::FilePath::FromUTF8Unsafe(optional_root_name).ReferencesParent())
      return std::string();
    root.append(base::EscapePath(optional_root_name));
    root.append("/");
  }
  return root;
}

std::string GetExternalFileSystemRootURIString(const GURL& origin_url,
                                               const std::string& mount_name) {
  std::string root =
      GetFileSystemRootURI(origin_url, kFileSystemTypeExternal).spec();
  if (base::FilePath::FromUTF8Unsafe(mount_name).ReferencesParent())
    return std::string();
  root.append(base::EscapePath(mount_name));
  root.append("/");
  return root;
}

base::File::Error NetErrorToFileError(int error) {
  switch (error) {
    case net::OK:
      return base::File::FILE_OK;
    case net::ERR_ADDRESS_IN_USE:
      return base::File::FILE_ERROR_IN_USE;
    case net::ERR_FILE_EXISTS:
      return base::File::FILE_ERROR_EXISTS;
    case net::ERR_FILE_NOT_FOUND:
      return base::File::FILE_ERROR_NOT_FOUND;
    case net::ERR_ACCESS_DENIED:
      return base::File::FILE_ERROR_ACCESS_DENIED;
    case net::ERR_INSUFFICIENT_RESOURCES:
      return base::File::FILE_ERROR_TOO_MANY_OPENED;
    case net::ERR_OUT_OF_MEMORY:
      return base::File::FILE_ERROR_NO_MEMORY;
    case net::ERR_FILE_NO_SPACE:
      return base::File::FILE_ERROR_NO_SPACE;
    case net::ERR_INVALID_ARGUMENT:
    case net::ERR_INVALID_HANDLE:
      return base::File::FILE_ERROR_INVALID_OPERATION;
    case net::ERR_ABORTED:
    case net::ERR_CONNECTION_ABORTED:
      return base::File::FILE_ERROR_ABORT;
    case net::ERR_ADDRESS_INVALID:
    case net::ERR_INVALID_URL:
      return base::File::FILE_ERROR_INVALID_URL;
    default:
      return base::File::FILE_ERROR_FAILED;
  }
}

}  // namespace storage
