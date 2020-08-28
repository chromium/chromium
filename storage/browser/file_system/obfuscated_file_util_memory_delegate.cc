// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/numerics/checked_math.h"
#include "build/build_config.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace storage {

// Struct keeping one entry of the directory tree.
struct ObfuscatedFileUtilMemoryDelegate::Entry {
  enum Type { kDirectory, kFile };

  Entry(Type type) : type(type) {
    creation_time = base::Time::Now();
    last_modified = creation_time;
    last_accessed = last_modified;
  }

  Entry(Entry&&) = default;

  ~Entry() = default;

  Type type;
  base::Time creation_time;
  base::Time last_modified;
  base::Time last_accessed;

  std::map<base::FilePath::StringType, Entry> directory_content;
  std::vector<uint8_t> file_content;

  DISALLOW_COPY_AND_ASSIGN(Entry);
};

// Keeps a decomposed FilePath.
struct ObfuscatedFileUtilMemoryDelegate::DecomposedPath {
  // Entry in the directory structure that the input |path| referes to,
  // nullptr if the entry does not exist.
  Entry* entry = nullptr;

  // Parent of the |path| in the directory structure, nullptr if not exists.
  Entry* parent = nullptr;

  // Normalized components of the path after the |root_|. E.g., if the root
  // is 'foo/' and the path is 'foo/./bar/baz', it will be ['bar', 'baz'].
  std::vector<base::FilePath::StringType> components;
};

ObfuscatedFileUtilMemoryDelegate::ObfuscatedFileUtilMemoryDelegate(
    const base::FilePath& file_system_directory)
    : root_(std::make_unique<Entry>(Entry::kDirectory)) {
  file_system_directory.GetComponents(&root_path_components_);
}

ObfuscatedFileUtilMemoryDelegate::~ObfuscatedFileUtilMemoryDelegate() = default;

base::Optional<ObfuscatedFileUtilMemoryDelegate::DecomposedPath>
ObfuscatedFileUtilMemoryDelegate::ParsePath(const base::FilePath& path) {
  DecomposedPath dp;

  path.GetComponents(&dp.components);

  // Ensure |path| is under |root_|.
  if (dp.components.size() < root_path_components_.size())
    return base::nullopt;

  for (size_t i = 0; i < root_path_components_.size(); i++)
    if (dp.components[i] != root_path_components_[i])
      return base::nullopt;

  dp.components.erase(dp.components.begin(),
                      dp.components.begin() + root_path_components_.size());

  // Normalize path.
  for (size_t i = 0; i < dp.components.size(); i++) {
    if (dp.components[i] == base::FilePath::kCurrentDirectory) {
      dp.components.erase(dp.components.begin() + i);
      i--;
    } else if (dp.components[i] == base::FilePath::kParentDirectory) {
      // Beyond |root|?
      if (!i)
        return base::nullopt;
      dp.components.erase(dp.components.begin() + i - 1,
                          dp.components.begin() + i + 1);
      i -= 2;
    }
  }

  // Find entry and parent.
  dp.parent = nullptr;
  dp.entry = root_.get();

  for (size_t i = 0; i < dp.components.size(); i++) {
    auto child = dp.entry->directory_content.find(dp.components[i]);
    if (child == dp.entry->directory_content.end()) {
      // If just the last component is not found and the last found part is a
      // directory keep the parent.
      if (i == dp.components.size() - 1 && dp.entry->type == Entry::kDirectory)
        dp.parent = dp.entry;
      else
        dp.parent = nullptr;
      dp.entry = nullptr;
      break;
    }
    dp.parent = dp.entry;
    dp.entry = &child->second;
  }

  return dp;
}

bool ObfuscatedFileUtilMemoryDelegate::DirectoryExists(
    const base::FilePath& path) {
  base::Optional<DecomposedPath> dp = ParsePath(path);
  return dp && dp->entry && dp->entry->type == Entry::kDirectory;
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::CreateDirectory(
    const base::FilePath& path,
    bool exclusive,
    bool recursive) {
  base::Optional<DecomposedPath> dp = ParsePath(path);
  if (!dp)
    return base::File::FILE_ERROR_NOT_FOUND;

  // If path already exists, ensure it's not a file and exclusive access is not
  // requested.
  if (dp->entry) {
    if (exclusive || dp->entry->type == Entry::kFile)
      return base::File::FILE_ERROR_EXISTS;
    return base::File::FILE_OK;
  }
  // If parent exists, add the new directory.
  if (dp->parent) {
    dp->parent->directory_content.emplace(dp->components.back(),
                                          Entry::kDirectory);
    return base::File::FILE_OK;
  }

  // Recursively add all required ancesstors if allowed.
  if (!recursive)
    return base::File::FILE_ERROR_NOT_FOUND;

  Entry* entry = root_.get();
  bool check_existings = true;
  for (const auto& c : dp->components) {
    if (check_existings) {
      auto child = entry->directory_content.find(c);
      if (child != entry->directory_content.end()) {
        entry = &child->second;
        continue;
      }
      check_existings = false;
    }
    entry =
        &entry->directory_content.emplace(c, Entry::kDirectory).first->second;
  }

  return base::File::FILE_OK;
}

bool ObfuscatedFileUtilMemoryDelegate::DeleteFileOrDirectory(
    const base::FilePath& path,
    bool recursive) {
  base::Optional<DecomposedPath> dp = ParsePath(path);
  if (!dp)
    return false;

  if (!dp->entry)
    return true;

  if (!recursive && dp->entry->directory_content.size())
    return false;

  dp->parent->directory_content.erase(dp->components.back());

  return true;
}

bool ObfuscatedFileUtilMemoryDelegate::IsLink(const base::FilePath& file_path) {
  // In-memory file system does not support links.
  return false;
}

bool ObfuscatedFileUtilMemoryDelegate::PathExists(const base::FilePath& path) {
  base::Optional<DecomposedPath> dp = ParsePath(path);
  return dp && dp->entry;
}

base::File ObfuscatedFileUtilMemoryDelegate::CreateOrOpen(
    const base::FilePath& path,
    int file_flags) {
  // TODO:(https://crbug.com/936722): Once the output of this function is
  // changed to base::File::Error, it can use CreateOrOpenInternal to perform
  // the task and return the result.
  return base::File(base::File::FILE_ERROR_INVALID_OPERATION);
}

void ObfuscatedFileUtilMemoryDelegate::CreateOrOpenInternal(
    const DecomposedPath& dp,
    int file_flags) {
  if (!dp.entry) {
    dp.parent->directory_content.emplace(dp.components.back(), Entry::kFile);
    return;
  }

  if (dp.entry->file_content.size()) {
    if (file_flags &
        (base::File::FLAG_OPEN_TRUNCATED | base::File::FLAG_CREATE_ALWAYS)) {
      dp.entry->file_content.clear();
    }
  }
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::DeleteFile(
    const base::FilePath& path) {
  base::Optional<DecomposedPath> dp = ParsePath(path);
  if (!dp || !dp->entry)
    return base::File::FILE_ERROR_NOT_FOUND;

  if (dp->entry->type != Entry::kFile)
    return base::File::FILE_ERROR_NOT_A_FILE;

  dp->parent->directory_content.erase(dp->components.back());
  return base::File::FILE_OK;
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::EnsureFileExists(
    const base::FilePath& path,
    bool* created) {
  base::Optional<DecomposedPath> dp = ParsePath(path);
  *created = false;
  if (!dp || !dp->parent)
    return base::File::FILE_ERROR_NOT_FOUND;

  if (dp->entry) {
    return dp->entry->type == Entry::kFile ? base::File::FILE_OK
                                           : base::File::FILE_ERROR_NOT_FOUND;
  }

  CreateOrOpenInternal(*dp, base::File::FLAG_CREATE);
  *created = true;
  return base::File::FILE_OK;
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::GetFileInfo(
    const base::FilePath& path,
    base::File::Info* file_info) {
  base::Optional<DecomposedPath> dp = ParsePath(path);
  if (!dp || !dp->entry)
    return base::File::FILE_ERROR_NOT_FOUND;

  file_info->size = dp->entry->file_content.size();
  file_info->is_directory = (dp->entry->type == Entry::kDirectory);
  file_info->is_symbolic_link = false;
  file_info->creation_time = dp->entry->creation_time;
  file_info->last_modified = dp->entry->last_modified;
  file_info->last_accessed = dp->entry->last_accessed;
  DCHECK(file_info->size == 0 || !file_info->is_directory);

  return base::File::FILE_OK;
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::Touch(
    const base::FilePath& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  base::Optional<DecomposedPath> dp = ParsePath(path);
  if (!dp || !dp->entry)
    return base::File::FILE_ERROR_FAILED;

  dp->entry->last_accessed = last_access_time;
  dp->entry->last_modified = last_modified_time;

  return base::File::FILE_OK;
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::Truncate(
    const base::FilePath& path,
    int64_t length) {
  base::Optional<DecomposedPath> dp = ParsePath(path);
  if (!dp || !dp->entry || dp->entry->type != Entry::kFile)
    return base::File::FILE_ERROR_NOT_FOUND;

  dp->entry->file_content.resize(length);
  return base::File::FILE_OK;
}

NativeFileUtil::CopyOrMoveMode
ObfuscatedFileUtilMemoryDelegate::CopyOrMoveModeForDestination(
    const FileSystemURL& /*dest_url*/,
    bool copy) {
  return copy ? NativeFileUtil::CopyOrMoveMode::COPY_SYNC
              : NativeFileUtil::CopyOrMoveMode::MOVE;
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::CopyOrMoveFile(
    const base::FilePath& src_path,
    const base::FilePath& dest_path,
    FileSystemOperation::CopyOrMoveOption option,
    NativeFileUtil::CopyOrMoveMode mode) {
  base::Optional<DecomposedPath> src_dp = ParsePath(src_path);
  base::Optional<DecomposedPath> dest_dp = ParsePath(dest_path);

  if (!src_dp || !src_dp->entry || !dest_dp || !dest_dp->parent)
    return base::File::FILE_ERROR_NOT_FOUND;

  bool src_is_directory = src_dp->entry->type == Entry::kDirectory;
  // For directories, only 'move' is supported.
  if (src_is_directory && mode != NativeFileUtil::CopyOrMoveMode::MOVE) {
    return base::File::FILE_ERROR_NOT_A_FILE;
  }

  base::Time last_modified = src_dp->entry->last_modified;

  if (dest_dp->entry) {
    if (dest_dp->entry->type != src_dp->entry->type)
      return base::File::FILE_ERROR_INVALID_OPERATION;

#if defined(OS_WIN)
    // Overwriting an empty directory with another directory isn't
    // supported natively on Windows.
    // To keep the behavior indistinguishable from on-disk operation,
    // in-memory implementation also fails.
    if (src_is_directory)
      return base::File::FILE_ERROR_NOT_A_FILE;
#endif
  }

  switch (mode) {
    case NativeFileUtil::CopyOrMoveMode::COPY_NOSYNC:
    case NativeFileUtil::CopyOrMoveMode::COPY_SYNC:
      DCHECK(!src_is_directory);
      if (!CopyOrMoveFileInternal(*src_dp, *dest_dp, false))
        return base::File::FILE_ERROR_FAILED;
      break;
    case NativeFileUtil::CopyOrMoveMode::MOVE:
      if (src_is_directory) {
        if (!MoveDirectoryInternal(*src_dp, *dest_dp))
          return base::File::FILE_ERROR_FAILED;
      } else {
        if (!CopyOrMoveFileInternal(*src_dp, *dest_dp, true))
          return base::File::FILE_ERROR_FAILED;
      }
      break;
  }

  if (option == FileSystemOperation::OPTION_PRESERVE_LAST_MODIFIED)
    Touch(dest_path, last_modified, last_modified);

  return base::File::FILE_OK;
}

bool ObfuscatedFileUtilMemoryDelegate::MoveDirectoryInternal(
    const DecomposedPath& src_dp,
    const DecomposedPath& dest_dp) {
  DCHECK(src_dp.entry->type == Entry::kDirectory);
  if (!dest_dp.entry) {
    dest_dp.parent->directory_content.insert(
        std::make_pair(dest_dp.components.back(), std::move(*src_dp.entry)));
  } else {
    dest_dp.entry->directory_content.insert(
        std::make_move_iterator(src_dp.entry->directory_content.begin()),
        std::make_move_iterator(src_dp.entry->directory_content.end()));
  }

  src_dp.parent->directory_content.erase(src_dp.components.back());
  return true;
}

bool ObfuscatedFileUtilMemoryDelegate::CopyOrMoveFileInternal(
    const DecomposedPath& src_dp,
    const DecomposedPath& dest_dp,
    bool move) {
  DCHECK(src_dp.entry->type == Entry::kFile);
  if (dest_dp.entry)
    dest_dp.parent->directory_content.erase(dest_dp.components.back());

  if (move) {
    dest_dp.parent->directory_content.insert(
        std::make_pair(dest_dp.components.back(), std::move(*src_dp.entry)));
    src_dp.parent->directory_content.erase(src_dp.components.back());
    return true;
  }

  // Copy the file.
  Entry* entry = &dest_dp.parent->directory_content
                      .emplace(dest_dp.components.back(), Entry::kFile)
                      .first->second;
  entry->creation_time = src_dp.entry->creation_time;
  entry->last_modified = src_dp.entry->last_modified;
  entry->last_accessed = src_dp.entry->last_accessed;
  entry->file_content = src_dp.entry->file_content;

  return true;
}

size_t ObfuscatedFileUtilMemoryDelegate::ComputeDirectorySize(
    const base::FilePath& path) {
  base::Optional<DecomposedPath> dp = ParsePath(path);
  if (!dp || !dp->entry || dp->entry->type != Entry::kDirectory)
    return 0;

  base::CheckedNumeric<size_t> running_sum = 0;
  std::vector<Entry*> directories;
  directories.push_back(dp->entry);

  while (!directories.empty()) {
    Entry* current = directories.back();
    directories.pop_back();
    for (auto& child : current->directory_content) {
      if (child.second.type == Entry::kDirectory)
        directories.push_back(&child.second);
      else
        running_sum += child.second.file_content.size();
    }
  }
  return running_sum.ValueOrDefault(0);
}

int ObfuscatedFileUtilMemoryDelegate::ReadFile(const base::FilePath& path,
                                               int64_t offset,
                                               net::IOBuffer* buf,
                                               int buf_len) {
  base::Optional<DecomposedPath> dp = ParsePath(path);
  if (!dp || dp->entry->type != Entry::kFile)
    return net::ERR_FILE_NOT_FOUND;

  int64_t remaining = dp->entry->file_content.size() - offset;
  if (offset < 0 || remaining < 0)
    return net::ERR_REQUEST_RANGE_NOT_SATISFIABLE;

  if (buf_len > remaining)
    buf_len = static_cast<int>(remaining);

  memcpy(buf->data(), dp->entry->file_content.data() + offset, buf_len);

  return buf_len;
}

int ObfuscatedFileUtilMemoryDelegate::WriteFile(const base::FilePath& path,
                                                int64_t offset,
                                                net::IOBuffer* buf,
                                                int buf_len) {
  base::Optional<DecomposedPath> dp = ParsePath(path);

  if (!dp || dp->entry->type != Entry::kFile)
    return net::ERR_FILE_NOT_FOUND;

  size_t offset_u = static_cast<size_t>(offset);
  // Fail if |offset| or |buf_len| not valid.
  if (offset < 0 || buf_len < 0 || offset_u > dp->entry->file_content.size())
    return net::ERR_REQUEST_RANGE_NOT_SATISFIABLE;

  // Fail if result doesn't fit in a std::vector.
  if (std::numeric_limits<size_t>::max() - offset_u <
      static_cast<size_t>(buf_len))
    return net::ERR_REQUEST_RANGE_NOT_SATISFIABLE;

  if (offset_u == dp->entry->file_content.size()) {
    dp->entry->file_content.insert(dp->entry->file_content.end(), buf->data(),
                                   buf->data() + buf_len);
  } else {
    if (offset_u + buf_len > dp->entry->file_content.size())
      dp->entry->file_content.resize(offset_u + buf_len);

    memcpy(dp->entry->file_content.data() + offset, buf->data(), buf_len);
  }
  return buf_len;
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::CreateFileForTesting(
    const base::FilePath& path,
    base::span<const char> content) {
  bool created;
  base::File::Error result = EnsureFileExists(path, &created);
  if (result != base::File::FILE_OK)
    return result;

  base::Optional<DecomposedPath> dp = ParsePath(path);
  DCHECK(dp && dp->entry->type == Entry::kFile);

  dp->entry->file_content =
      std::vector<uint8_t>(content.begin(), content.end());

  return base::File::FILE_OK;
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::CopyInForeignFile(
    const base::FilePath& src_path,
    const base::FilePath& dest_path,
    FileSystemOperation::CopyOrMoveOption /* option */,
    NativeFileUtil::CopyOrMoveMode /* mode */) {
  base::Optional<DecomposedPath> dest_dp = ParsePath(dest_path);

  if (!dest_dp || !dest_dp->parent)
    return base::File::FILE_ERROR_NOT_FOUND;

  base::File::Info source_info;
  if (!base::GetFileInfo(src_path, &source_info))
    return base::File::FILE_ERROR_NOT_FOUND;

  if (source_info.is_directory)
    return base::File::FILE_ERROR_NOT_A_FILE;

  // |size_t| limits the maximum size that the memory file can keep and |int|
  // limits the maximum size that base::ReadFile function reads.
  if (source_info.size > std::numeric_limits<size_t>::max() ||
      source_info.size > std::numeric_limits<int>::max()) {
    return base::File::FILE_ERROR_NO_SPACE;
  }

  // Create file.
  Entry* entry = &dest_dp->parent->directory_content
                      .emplace(dest_dp->components.back(), Entry::kFile)
                      .first->second;
  entry->creation_time = source_info.creation_time;
  entry->last_modified = source_info.last_modified;
  entry->last_accessed = source_info.last_accessed;

  // Read content.
  entry->file_content.resize(source_info.size);
  int read_bytes = base::ReadFile(
      src_path, reinterpret_cast<char*>(entry->file_content.data()),
      source_info.size);

  if (read_bytes != source_info.size) {
    // Delete file and return error if source could not be fully read or any
    // error happens.
    dest_dp->parent->directory_content.erase(dest_dp->components.back());
    return base::File::FILE_ERROR_FAILED;
  }

  return base::File::FILE_OK;
}
}  // namespace storage
