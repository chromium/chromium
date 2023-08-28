// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_WATCH_SCOPE_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_WATCH_SCOPE_H_

#include "content/common/content_export.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "storage/browser/file_system/file_system_url.h"

namespace content {

// Describes the extent of the file system that is being observed, which can be
// a single file, a directory and its contents, or a directory and all its
// subdirectories.
class CONTENT_EXPORT FileSystemAccessWatchScope {
 public:
  // TODO(https://crbug.com/1019297): Consider using something like a PassKey
  // to restrict access to these initializers.
  static FileSystemAccessWatchScope GetScopeForFileWatch(
      const storage::FileSystemURL& file_url);
  static FileSystemAccessWatchScope GetScopeForDirectoryWatch(
      const storage::FileSystemURL& directory_url,
      bool is_recursive);

  ~FileSystemAccessWatchScope();

  // Copyable and movable.
  FileSystemAccessWatchScope(const FileSystemAccessWatchScope&);
  FileSystemAccessWatchScope(FileSystemAccessWatchScope&&) noexcept;
  FileSystemAccessWatchScope& operator=(const FileSystemAccessWatchScope&);
  FileSystemAccessWatchScope& operator=(FileSystemAccessWatchScope&&) noexcept;

  // Returns true if `url` is contained within this `Scope`.
  bool Contains(const storage::FileSystemURL& url) const;
  // Returns true if `scope` is contained within this `Scope`.
  bool Contains(const FileSystemAccessWatchScope& scope) const;

  bool IsRecursive() const {
    return watch_type_ == WatchType::kDirectoryRecursive;
  }

  const storage::FileSystemURL& root_url() const { return root_url_; }
  FileSystemAccessPermissionContext::HandleType handle_type() const {
    switch (watch_type_) {
      case WatchType::kFile:
        return FileSystemAccessPermissionContext::HandleType::kFile;
      case WatchType::kDirectoryNonRecursive:
      case WatchType::kDirectoryRecursive:
        return FileSystemAccessPermissionContext::HandleType::kDirectory;
    }
  }

  bool operator==(const FileSystemAccessWatchScope& other) const {
    return root_url_ == other.root_url_ && watch_type_ == other.watch_type_;
  }

 private:
  enum class WatchType {
    kFile,
    kDirectoryNonRecursive,
    kDirectoryRecursive,
  };

  FileSystemAccessWatchScope(storage::FileSystemURL root_url,
                             WatchType watch_type);

  storage::FileSystemURL root_url_;
  WatchType watch_type_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_WATCH_SCOPE_H_
