// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "storage/browser/file_system/obfuscated_file_util_memory_delegate.h"

#include <algorithm>
#include <utility>

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "partition_alloc/partition_alloc_constants.h"

namespace {

// We are giving a relatively large quota to in-memory filesystem (see
// quota_settings.cc), but we do not allocate this memory beforehand for the
// filesystem. Therefore, specially on low-end devices, a website can get to a
// state where it has not used all its quota, but there is no more memory
// available and memory allocation fails and results in Chrome crash.
// By checking for availability of memory before allocating it, we reduce the
// crash possibility.
// Note that quota assignment is the same for on-disk filesystem and the
// assigned quota is not guaranteed to be allocatable later.
bool IsMemoryAvailable(size_t required_memory) {
#if BUILDFLAG(IS_FUCHSIA)
  // This function is not implemented on FUCHSIA, yet. (crbug.com/986608)
  return true;
#else
  uint64_t max_allocatable =
      std::min(base::SysInfo::AmountOfAvailablePhysicalMemory(),
               static_cast<uint64_t>(partition_alloc::MaxDirectMapped()));

  return max_allocatable >= required_memory;
#endif
}

}  // namespace

namespace storage {

// Struct keeping one entry of the directory tree.
struct ObfuscatedFileUtilMemoryDelegate::Entry {
  enum Type { kDirectory, kFile };

  Entry(Type type) : type(type) {
    creation_time = base::Time::Now();
    last_modified = creation_time;
    last_accessed = last_modified;
  }

  Entry(const Entry&) = delete;
  Entry& operator=(const Entry&) = delete;

  Entry(Entry&&) = default;

  ~Entry() = default;

  Type type;
  base::Time creation_time;
  base::Time last_modified;
  base::Time last_accessed;

  std::map<base::FilePath::StringType, Entry> directory_content;
  std::vector<uint8_t> file_content;
};

// Keeps a decomposed FilePath.
struct ObfuscatedFileUtilMemoryDelegate::DecomposedPath {
  // Entry in the directory structure that the input |path| referes to,
  // nullptr if the entry does not exist.
  raw_ptr<Entry, DanglingUntriaged> entry = nullptr;

  // Parent of the |path| in the directory structure, nullptr if not exists.
  raw_ptr<Entry, DanglingUntriaged> parent = nullptr;

  // Normalized components of the path after the |root_|. E.g., if the root
  // is 'foo/' and the path is 'foo/./bar/baz', it will be ['bar', 'baz'].
  std::vector<base::FilePath::StringType> components;
};

ObfuscatedFileUtilMemoryDelegate::ObfuscatedFileUtilMemoryDelegate(
    const base::FilePath& file_system_directory)
    : root_(std::make_unique<Entry>(Entry::kDirectory)),
      root_path_components_(file_system_directory.GetComponents()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ObfuscatedFileUtilMemoryDelegate::~ObfuscatedFileUtilMemoryDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::optional<ObfuscatedFileUtilMemoryDelegate::DecomposedPath>
ObfuscatedFileUtilMemoryDelegate::ParsePath(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DecomposedPath dp;
  dp.components = path.GetComponents();

  // Ensure |path| is under |root_|.
  if (dp.components.size() < root_path_components_.size())
    return std::nullopt;

  for (size_t i = 0; i < root_path_components_.size(); i++)
    if (dp.components[i] != root_path_components_[i])
      return std::nullopt;

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
        return std::nullopt;
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<DecomposedPath> dp = ParsePath(path);
  return dp && dp->entry && dp->entry->type == Entry::kDirectory;
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::CreateDirectory(
    const base::FilePath& path,
    bool exclusive,
    bool recursive) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<DecomposedPath> dp = ParsePath(path);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<DecomposedPath> dp = ParsePath(path);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // In-memory file system does not support links.
  return false;
}

bool ObfuscatedFileUtilMemoryDelegate::PathExists(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<DecomposedPath> dp = ParsePath(path);
  return dp && dp->entry;
}

base::File ObfuscatedFileUtilMemoryDelegate::CreateOrOpen(
    const base::FilePath& path,
    uint32_t file_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO:(https://crbug.com/936722): Once the output of this function is
  // changed to base::File::Error, it can use CreateOrOpenInternal to perform
  // the task and return the result.
  return base::File(base::File::FILE_ERROR_INVALID_OPERATION);
}

void ObfuscatedFileUtilMemoryDelegate::CreateOrOpenInternal(
    const DecomposedPath& dp,
    uint32_t file_flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<DecomposedPath> dp = ParsePath(path);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<DecomposedPath> dp = ParsePath(path);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<DecomposedPath> dp = ParsePath(path);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<DecomposedPath> dp = ParsePath(path);
  if (!dp || !dp->entry)
    return base::File::FILE_ERROR_FAILED;

  dp->entry->last_accessed = last_access_time;
  dp->entry->last_modified = last_modified_time;

  return base::File::FILE_OK;
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::Truncate(
    const base::FilePath& path,
    int64_t length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<DecomposedPath> dp = ParsePath(path);
  if (!dp || !dp->entry || dp->entry->type != Entry::kFile)
    return base::File::FILE_ERROR_NOT_FOUND;

  // Fail if enough memory is not available.
  if (!base::IsValueInRangeForNumericType<size_t>(length) ||
      (static_cast<size_t>(length) > dp->entry->file_content.capacity() &&
       !IsMemoryAvailable(static_cast<size_t>(length)))) {
    return base::File::FILE_ERROR_NO_SPACE;
  }

  dp->entry->file_content.resize(static_cast<size_t>(length));
  return base::File::FILE_OK;
}

NativeFileUtil::CopyOrMoveMode
ObfuscatedFileUtilMemoryDelegate::CopyOrMoveModeForDestination(
    const FileSystemURL& /*dest_url*/,
    bool copy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return copy ? NativeFileUtil::CopyOrMoveMode::COPY_SYNC
              : NativeFileUtil::CopyOrMoveMode::MOVE;
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::CopyOrMoveFile(
    const base::FilePath& src_path,
    const base::FilePath& dest_path,
    FileSystemOperation::CopyOrMoveOptionSet options,
    NativeFileUtil::CopyOrMoveMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<DecomposedPath> src_dp = ParsePath(src_path);
  std::optional<DecomposedPath> dest_dp = ParsePath(dest_path);

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

#if BUILDFLAG(IS_WIN)
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
      // Fail if enough memory is not available.
      if (!IsMemoryAvailable(src_dp->entry->file_content.size()))
        return base::File::FILE_ERROR_NO_SPACE;
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

  if (options.Has(
          FileSystemOperation::CopyOrMoveOption::kPreserveLastModified)) {
    Touch(dest_path, last_modified, last_modified);
  }

  // Don't bother with the kPreserveDestinationPermissions option, since
  // this is not relevant to in-memory files.

  return base::File::FILE_OK;
}

bool ObfuscatedFileUtilMemoryDelegate::MoveDirectoryInternal(
    const DecomposedPath& src_dp,
    const DecomposedPath& dest_dp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<DecomposedPath> dp = ParsePath(path);
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
                                               scoped_refptr<net::IOBuffer> buf,
                                               int buf_len) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<DecomposedPath> dp = ParsePath(path);
  if (!dp || dp->entry->type != Entry::kFile)
    return net::ERR_FILE_NOT_FOUND;

  int64_t remaining = dp->entry->file_content.size() - offset;
  if (offset < 0)
    return net::ERR_INVALID_ARGUMENT;

  // Seeking past the end of the file is ok, but returns nothing.
  // This matches FileStream::Context behavior.
  if (remaining < 0)
    return 0;

  if (buf_len > remaining)
    buf_len = static_cast<int>(remaining);

  base::ranges::copy(
      base::span(dp->entry->file_content).subspan(offset, buf_len),
      buf->data());

  return buf_len;
}

int ObfuscatedFileUtilMemoryDelegate::WriteFile(
    const base::FilePath& path,
    int64_t offset,
    scoped_refptr<net::IOBuffer> buf,
    int buf_len) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<DecomposedPath> dp = ParsePath(path);

  if (!dp || !dp->entry || dp->entry->type != Entry::kFile)
    return net::ERR_FILE_NOT_FOUND;

  size_t offset_u = static_cast<size_t>(offset);
  // Fail if |offset| or |buf_len| not valid.
  if (offset < 0 || buf_len < 0)
    return net::ERR_REQUEST_RANGE_NOT_SATISFIABLE;

  // Fail if result doesn't fit in a std::vector.
  if (std::numeric_limits<size_t>::max() - offset_u <
      static_cast<size_t>(buf_len))
    return net::ERR_REQUEST_RANGE_NOT_SATISFIABLE;

  const size_t last_position = offset_u + buf_len;
  if (last_position > dp->entry->file_content.capacity()) {
    // Fail if enough memory is not available.
    if (!IsMemoryAvailable(last_position))
      return net::ERR_FILE_NO_SPACE;

// If required memory is bigger than half of the max allocatable memory block,
// reserve first to avoid STL getting more than required memory.
// See crbug.com/1043914 for more context.
// |MaxDirectMapped| function is not implemented on FUCHSIA, yet.
// (crbug.com/986608)
#if !BUILDFLAG(IS_FUCHSIA)
    if (last_position >= partition_alloc::MaxDirectMapped() / 2) {
      // TODO(crbug.com/40669351): Allocated memory is rounded up to
      // 100MB blocks to reduce memory allocation delays. Switch to a more
      // proper container to remove this dependency.
      const size_t round_up_size = 100 * 1024 * 1024;
      size_t rounded_up = ((last_position / round_up_size) + 1) * round_up_size;
      if (!IsMemoryAvailable(rounded_up))
        return net::ERR_FILE_NO_SPACE;
      dp->entry->file_content.reserve(rounded_up);
    }
#endif
  }

  if (offset_u == dp->entry->file_content.size()) {
    dp->entry->file_content.insert(dp->entry->file_content.end(), buf->data(),
                                   buf->data() + buf_len);
  } else {
    if (last_position > dp->entry->file_content.size())
      dp->entry->file_content.resize(last_position);

    // if |offset_u| is larger than the original file size, there will be null
    // bytes between the end of the file and |offset_u|.
    memcpy(dp->entry->file_content.data() + offset, buf->data(), buf_len);
  }
  return buf_len;
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::CreateFileForTesting(
    const base::FilePath& path,
    base::span<const char> content) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool created;
  base::File::Error result = EnsureFileExists(path, &created);
  if (result != base::File::FILE_OK)
    return result;

  std::optional<DecomposedPath> dp = ParsePath(path);
  DCHECK(dp && dp->entry->type == Entry::kFile);

  dp->entry->file_content =
      std::vector<uint8_t>(content.begin(), content.end());

  return base::File::FILE_OK;
}

base::File::Error ObfuscatedFileUtilMemoryDelegate::CopyInForeignFile(
    const base::FilePath& src_path,
    const base::FilePath& dest_path,
    FileSystemOperation::CopyOrMoveOptionSet /* options */,
    NativeFileUtil::CopyOrMoveMode /* mode */) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<DecomposedPath> dest_dp = ParsePath(dest_path);

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

  // Fail if enough memory is not available.
  if (!IsMemoryAvailable(static_cast<size_t>(source_info.size)))
    return base::File::FILE_ERROR_NO_SPACE;

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
