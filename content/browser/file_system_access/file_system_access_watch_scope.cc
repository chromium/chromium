// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_watch_scope.h"

namespace content {

namespace {

bool IsStrictParent(const storage::FileSystemURL& parent,
                    const storage::FileSystemURL& child) {
  return parent.IsParent(child) && parent.path() == child.path().DirName();
}

}  // namespace

// static
FileSystemAccessWatchScope FileSystemAccessWatchScope::GetScopeForFileWatch(
    const storage::FileSystemURL& file_url) {
  return {file_url, WatchType::kFile};
}

// static
FileSystemAccessWatchScope
FileSystemAccessWatchScope::GetScopeForDirectoryWatch(
    const storage::FileSystemURL& directory_url,
    bool is_recursive) {
  return {directory_url, is_recursive ? WatchType::kDirectoryRecursive
                                      : WatchType::kDirectoryNonRecursive};
}

FileSystemAccessWatchScope::FileSystemAccessWatchScope(
    storage::FileSystemURL root_url,
    WatchType watch_type)
    : root_url_(std::move(root_url)), watch_type_(watch_type) {}
FileSystemAccessWatchScope::~FileSystemAccessWatchScope() = default;

FileSystemAccessWatchScope::FileSystemAccessWatchScope(
    const FileSystemAccessWatchScope&) = default;
FileSystemAccessWatchScope::FileSystemAccessWatchScope(
    FileSystemAccessWatchScope&&) noexcept = default;
FileSystemAccessWatchScope& FileSystemAccessWatchScope::operator=(
    const FileSystemAccessWatchScope&) = default;
FileSystemAccessWatchScope& FileSystemAccessWatchScope::operator=(
    FileSystemAccessWatchScope&&) noexcept = default;

bool FileSystemAccessWatchScope::Contains(
    const storage::FileSystemURL& url) const {
  switch (watch_type_) {
    case WatchType::kFile:
      return url == root_url();
    case WatchType::kDirectoryNonRecursive:
      return url == root_url() || IsStrictParent(root_url(), url);
    case WatchType::kDirectoryRecursive:
      return url == root_url() || root_url().IsParent(url);
  }
}

bool FileSystemAccessWatchScope::Contains(
    const FileSystemAccessWatchScope& scope) const {
  switch (watch_type_) {
    case WatchType::kFile:
      return *this == scope;
    case WatchType::kDirectoryNonRecursive:
      return *this == scope || (scope.watch_type_ == WatchType::kFile &&
                                IsStrictParent(root_url(), scope.root_url()));
    case WatchType::kDirectoryRecursive:
      return Contains(scope.root_url());
  }
}

}  // namespace content
