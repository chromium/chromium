// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_system_url.h"

#include <sstream>

#include "base/check.h"
#include "base/files/safe_base_name.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "storage/browser/file_system/file_system_util.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {

namespace {

bool AreSameStorageKey(const FileSystemURL& a, const FileSystemURL& b) {
  // TODO(crbug.com/40249324): Make the `storage_key_` member optional.
  // This class improperly uses a StorageKey with an opaque origin to indicate a
  // lack of origin for FileSystemURLs corresponding to non-sandboxed file
  // systems. This leads to unexpected behavior when comparing two non-sandboxed
  // FileSystemURLs which differ only in the nonce of their default-constructed
  // StorageKey.
  return a.storage_key() == b.storage_key() ||
         (a.type() == b.type() &&
          (a.type() == storage::kFileSystemTypeExternal ||
           a.type() == storage::kFileSystemTypeLocal) &&
          a.storage_key().origin().opaque() &&
          b.storage_key().origin().opaque());
}

}  // namespace

FileSystemURL::FileSystemURL()
    : is_null_(true),
      is_valid_(false),
      mount_type_(kFileSystemTypeUnknown),
      type_(kFileSystemTypeUnknown),
      mount_option_(FlushPolicy::NO_FLUSH_ON_COMPLETION) {}

FileSystemURL::FileSystemURL(const FileSystemURL&) = default;

FileSystemURL::FileSystemURL(FileSystemURL&&) noexcept = default;

FileSystemURL& FileSystemURL::operator=(const FileSystemURL&) = default;

FileSystemURL& FileSystemURL::operator=(FileSystemURL&&) noexcept = default;

FileSystemURL::~FileSystemURL() = default;

FileSystemURL FileSystemURL::CreateSibling(
    const base::SafeBaseName& sibling_name) const {
  const base::FilePath& new_base_name = sibling_name.path();
  if (!is_valid_ ||
      new_base_name.empty()
#if BUILDFLAG(IS_ANDROID)
      // Android content-URIs do not support siblings.
      || path().IsContentUri()
#endif
  ) {
    return FileSystemURL();
  }

  const base::FilePath old_base_name = VirtualPath::BaseName(virtual_path_);
  if (!path_.empty() && (path_.BaseName() != old_base_name)) {
    return FileSystemURL();
  }

  FileSystemURL sibling(*this);
  sibling.virtual_path_ =
      VirtualPath::DirName(virtual_path_).Append(new_base_name);
  if (!path_.empty()) {
    sibling.path_ = path_.DirName().Append(new_base_name);
  }
  return sibling;
}

// static
FileSystemURL FileSystemURL::CreateForTest(const GURL& url) {
  return FileSystemURL(
      url, blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));
}

// static
FileSystemURL FileSystemURL::CreateForTest(const blink::StorageKey& storage_key,
                                           FileSystemType mount_type,
                                           const base::FilePath& virtual_path) {
  return FileSystemURL(storage_key, mount_type, virtual_path);
}

// static
FileSystemURL FileSystemURL::CreateForTest(
    const blink::StorageKey& storage_key,
    FileSystemType mount_type,
    const base::FilePath& virtual_path,
    const std::string& mount_filesystem_id,
    FileSystemType cracked_type,
    const base::FilePath& cracked_path,
    const std::string& filesystem_id,
    const FileSystemMountOption& mount_option) {
  return FileSystemURL(storage_key, mount_type, virtual_path,
                       mount_filesystem_id, cracked_type, cracked_path,
                       filesystem_id, mount_option);
}

// static
bool FileSystemURL::TypeImpliesPathIsReal(FileSystemType type) {
  switch (type) {
    // Public enum values, also exposed to JavaScript.
    case kFileSystemTypeTemporary:
    case kFileSystemTypePersistent:
    case kFileSystemTypeIsolated:
    case kFileSystemTypeExternal:
      break;

      // Everything else is a private (also known as internal) enum value.

    case kFileSystemInternalTypeEnumStart:
    case kFileSystemInternalTypeEnumEnd:
      NOTREACHED();

    case kFileSystemTypeLocal:
    case kFileSystemTypeLocalMedia:
    case kFileSystemTypeLocalForPlatformApp:
    case kFileSystemTypeDriveFs:
    case kFileSystemTypeSmbFs:
    case kFileSystemTypeFuseBox:
      return true;

    case kFileSystemTypeUnknown:
    case kFileSystemTypeTest:
    case kFileSystemTypeDragged:
    case kFileSystemTypeDeviceMedia:
    case kFileSystemTypeSyncable:
    case kFileSystemTypeSyncableForInternalSync:
    case kFileSystemTypeForTransientFile:
    case kFileSystemTypeProvided:
    case kFileSystemTypeDeviceMediaAsFileStorage:
    case kFileSystemTypeArcContent:
    case kFileSystemTypeArcDocumentsProvider:
      break;

      // We don't use a "default:" case. Whenever `FileSystemType` gains a
      // new enum value, raise a compiler error (with -Werror,-Wswitch) unless
      // this switch statement is also updated.
  }
  return false;
}

FileSystemURL::FileSystemURL(const GURL& url,
                             const blink::StorageKey& storage_key)
    : is_null_(false),
      mount_type_(kFileSystemTypeUnknown),
      type_(kFileSystemTypeUnknown),
      mount_option_(FlushPolicy::NO_FLUSH_ON_COMPLETION) {
  GURL origin_url;
  // URL should be able to be parsed and the parsed origin should match the
  // StorageKey's origin member.
  is_valid_ = ParseFileSystemSchemeURL(url, &origin_url, &mount_type_,
                                       &virtual_path_) &&
              storage_key.origin().IsSameOriginWith(origin_url);
  storage_key_ = storage_key;
  path_ = virtual_path_;
  type_ = mount_type_;
}

FileSystemURL::FileSystemURL(const blink::StorageKey& storage_key,
                             FileSystemType mount_type,
                             const base::FilePath& virtual_path)
    : is_null_(false),
      is_valid_(true),
      storage_key_(storage_key),
      mount_type_(mount_type),
      virtual_path_(virtual_path.NormalizePathSeparators()),
      type_(mount_type),
      path_(virtual_path.NormalizePathSeparators()),
      mount_option_(FlushPolicy::NO_FLUSH_ON_COMPLETION) {}

FileSystemURL::FileSystemURL(const blink::StorageKey& storage_key,
                             FileSystemType mount_type,
                             const base::FilePath& virtual_path,
                             const std::string& mount_filesystem_id,
                             FileSystemType cracked_type,
                             const base::FilePath& cracked_path,
                             const std::string& filesystem_id,
                             const FileSystemMountOption& mount_option)
    : is_null_(false),
      is_valid_(true),
      storage_key_(storage_key),
      mount_type_(mount_type),
      virtual_path_(virtual_path.NormalizePathSeparators()),
      mount_filesystem_id_(mount_filesystem_id),
      type_(cracked_type),
      path_(cracked_path.NormalizePathSeparators()),
      filesystem_id_(filesystem_id),
      mount_option_(mount_option) {}

GURL FileSystemURL::ToGURL() const {
  if (!is_valid_)
    return GURL();

  GURL url = GetFileSystemRootURI(storage_key_.origin().GetURL(), mount_type_);
  if (!url.is_valid())
    return GURL();

  std::string url_string = url.spec();

  // Exactly match with DOMFileSystemBase::createFileSystemURL()'s encoding
  // behavior, where the path is escaped by KURL::encodeWithURLEscapeSequences
  // which is essentially encodeURIComponent except '/'.
  std::string escaped = base::EscapeQueryParamValue(
      virtual_path_.NormalizePathSeparatorsTo('/').AsUTF8Unsafe(),
      false /* use_plus */);
  base::ReplaceSubstringsAfterOffset(&escaped, 0, "%2F", "/");
  url_string.append(escaped);

  // Build nested GURL.
  return GURL(url_string);
}

std::string FileSystemURL::DebugString() const {
  if (!is_valid_)
    return "invalid filesystem: URL";
  std::ostringstream ss;
  switch (mount_type_) {
    // Include GURL if GURL serialization is possible.
    case kFileSystemTypeTemporary:
    case kFileSystemTypePersistent:
    case kFileSystemTypeExternal:
    case kFileSystemTypeIsolated:
    case kFileSystemTypeTest:
      ss << "{ uri: ";
      ss << GetFileSystemRootURI(storage_key_.origin().GetURL(), mount_type_);
      break;
    // Otherwise list the origin and path separately.
    default:
      ss << "{ path: ";
  }

  // filesystem_id_ will be non empty for (and only for) cracked URLs.
  if (!filesystem_id_.empty()) {
    ss << virtual_path_.value();
    ss << " (";
    ss << GetFileSystemTypeString(type_) << "@" << filesystem_id_ << ":";
    ss << path_.value();
    ss << ")";
  } else {
    ss << path_.value();
  }
  ss << ", storage key: " << storage_key_.GetDebugString();
  if (bucket_.has_value()) {
    ss << ", bucket id: " << bucket_->id;
  }
  ss << " }";
  return ss.str();
}

BucketLocator FileSystemURL::GetBucket() const {
  if (bucket())
    return *bucket_;

  auto bucket = storage::BucketLocator::ForDefaultBucket(storage_key());
  bucket.type = storage::FileSystemTypeToQuotaStorageType(type());
  return bucket;
}

bool FileSystemURL::IsParent(const FileSystemURL& child) const {
  return IsInSameFileSystem(child) &&
         (path().IsParent(child.path()) ||
          (VirtualPath::IsRootPath(path()) &&
           !VirtualPath::IsRootPath(child.path())));
}

bool FileSystemURL::IsInSameFileSystem(const FileSystemURL& other) const {
  // Invalid FileSystemURLs should never be considered of the same file system.
  return AreSameStorageKey(*this, other) && is_valid() && other.is_valid() &&
         type() == other.type() && filesystem_id() == other.filesystem_id() &&
         bucket() == other.bucket()
#if BUILDFLAG(IS_ANDROID)
         // Android content-URIs do not support same-FS ops such as rename().
         && !path().IsContentUri() && !other.path().IsContentUri()
#endif
      ;
}

bool FileSystemURL::operator==(const FileSystemURL& that) const {
  if (is_null_ && that.is_null_) {
    return true;
  }

  return AreSameStorageKey(*this, that) && type_ == that.type_ &&
         path_ == that.path_ && filesystem_id_ == that.filesystem_id_ &&
         is_valid_ == that.is_valid_ && bucket_ == that.bucket_;
}

bool FileSystemURL::Comparator::operator()(const FileSystemURL& lhs,
                                           const FileSystemURL& rhs) const {
  DCHECK(lhs.is_valid_ && rhs.is_valid_);
  if (!AreSameStorageKey(lhs, rhs)) {
    return lhs.storage_key() < rhs.storage_key();
  }
  if (lhs.type_ != rhs.type_) {
    return lhs.type_ < rhs.type_;
  }
  if (lhs.filesystem_id_ != rhs.filesystem_id_) {
    return lhs.filesystem_id_ < rhs.filesystem_id_;
  }
  if (lhs.bucket_ != rhs.bucket_) {
    return lhs.bucket_ < rhs.bucket_;
  }
  return lhs.path_ < rhs.path_;
}

}  // namespace storage
