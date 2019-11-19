// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_system_url.h"

#include <sstream>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "net/base/escape.h"
#include "storage/common/file_system/file_system_util.h"

namespace storage {

namespace {}  // namespace

FileSystemURL::FileSystemURL()
    : is_null_(true),
      is_valid_(false),
      mount_type_(kFileSystemTypeUnknown),
      type_(kFileSystemTypeUnknown),
      mount_option_(FlushPolicy::NO_FLUSH_ON_COMPLETION) {}

FileSystemURL::FileSystemURL(const FileSystemURL& other) = default;

FileSystemURL::FileSystemURL(FileSystemURL&& other) = default;

FileSystemURL& FileSystemURL::operator=(FileSystemURL&& rhs) = default;

FileSystemURL& FileSystemURL::operator=(const FileSystemURL& rhs) = default;

// static
FileSystemURL FileSystemURL::CreateForTest(const GURL& url) {
  return FileSystemURL(url);
}

FileSystemURL FileSystemURL::CreateForTest(const url::Origin& origin,
                                           FileSystemType mount_type,
                                           const base::FilePath& virtual_path) {
  return FileSystemURL(origin, mount_type, virtual_path);
}

FileSystemURL FileSystemURL::CreateForTest(
    const url::Origin& origin,
    FileSystemType mount_type,
    const base::FilePath& virtual_path,
    const std::string& mount_filesystem_id,
    FileSystemType cracked_type,
    const base::FilePath& cracked_path,
    const std::string& filesystem_id,
    const FileSystemMountOption& mount_option) {
  return FileSystemURL(origin, mount_type, virtual_path, mount_filesystem_id,
                       cracked_type, cracked_path, filesystem_id, mount_option);
}

FileSystemURL::FileSystemURL(const GURL& url)
    : is_null_(false),
      mount_type_(kFileSystemTypeUnknown),
      type_(kFileSystemTypeUnknown),
      mount_option_(FlushPolicy::NO_FLUSH_ON_COMPLETION) {
  GURL origin_url;
  is_valid_ =
      ParseFileSystemSchemeURL(url, &origin_url, &mount_type_, &virtual_path_);
  origin_ = url::Origin::Create(origin_url);
  path_ = virtual_path_;
  type_ = mount_type_;
}

FileSystemURL::FileSystemURL(const url::Origin& origin,
                             FileSystemType mount_type,
                             const base::FilePath& virtual_path)
    : is_null_(false),
      is_valid_(true),
      origin_(origin),
      mount_type_(mount_type),
      virtual_path_(virtual_path.NormalizePathSeparators()),
      type_(mount_type),
      path_(virtual_path.NormalizePathSeparators()),
      mount_option_(FlushPolicy::NO_FLUSH_ON_COMPLETION) {}

FileSystemURL::FileSystemURL(const url::Origin& origin,
                             FileSystemType mount_type,
                             const base::FilePath& virtual_path,
                             const std::string& mount_filesystem_id,
                             FileSystemType cracked_type,
                             const base::FilePath& cracked_path,
                             const std::string& filesystem_id,
                             const FileSystemMountOption& mount_option)
    : is_null_(false),
      is_valid_(true),
      origin_(origin),
      mount_type_(mount_type),
      virtual_path_(virtual_path.NormalizePathSeparators()),
      mount_filesystem_id_(mount_filesystem_id),
      type_(cracked_type),
      path_(cracked_path.NormalizePathSeparators()),
      filesystem_id_(filesystem_id),
      mount_option_(mount_option) {}

FileSystemURL::~FileSystemURL() = default;

GURL FileSystemURL::ToGURL() const {
  if (!is_valid_)
    return GURL();

  std::string url = GetFileSystemRootURI(origin_.GetURL(), mount_type_).spec();
  if (url.empty())
    return GURL();

  // Exactly match with DOMFileSystemBase::createFileSystemURL()'s encoding
  // behavior, where the path is escaped by KURL::encodeWithURLEscapeSequences
  // which is essentially encodeURIComponent except '/'.
  std::string escaped = net::EscapeQueryParamValue(
      virtual_path_.NormalizePathSeparatorsTo('/').AsUTF8Unsafe(),
      false /* use_plus */);
  base::ReplaceSubstringsAfterOffset(&escaped, 0, "%2F", "/");
  url.append(escaped);

  // Build nested GURL.
  return GURL(url);
}

std::string FileSystemURL::DebugString() const {
  if (!is_valid_)
    return "invalid filesystem: URL";
  std::ostringstream ss;
  ss << GetFileSystemRootURI(origin_.GetURL(), mount_type_);

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
  return ss.str();
}

bool FileSystemURL::IsParent(const FileSystemURL& child) const {
  return IsInSameFileSystem(child) && path().IsParent(child.path());
}

bool FileSystemURL::IsInSameFileSystem(const FileSystemURL& other) const {
  return origin() == other.origin() && type() == other.type() &&
         filesystem_id() == other.filesystem_id();
}

bool FileSystemURL::operator==(const FileSystemURL& that) const {
  if (is_null_ && that.is_null_) {
    return true;
  } else {
    return origin_ == that.origin_ && type_ == that.type_ &&
           path_ == that.path_ && filesystem_id_ == that.filesystem_id_ &&
           is_valid_ == that.is_valid_;
  }
}

bool FileSystemURL::Comparator::operator()(const FileSystemURL& lhs,
                                           const FileSystemURL& rhs) const {
  DCHECK(lhs.is_valid_ && rhs.is_valid_);
  if (lhs.origin_ != rhs.origin_)
    return lhs.origin_ < rhs.origin_;
  if (lhs.type_ != rhs.type_)
    return lhs.type_ < rhs.type_;
  if (lhs.filesystem_id_ != rhs.filesystem_id_)
    return lhs.filesystem_id_ < rhs.filesystem_id_;
  return lhs.path_ < rhs.path_;
}

}  // namespace storage
