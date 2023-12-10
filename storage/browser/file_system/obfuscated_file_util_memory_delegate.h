// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_OBFUSCATED_FILE_UTIL_MEMORY_DELEGATE_H_
#define STORAGE_BROWSER_FILE_SYSTEM_OBFUSCATED_FILE_UTIL_MEMORY_DELEGATE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "storage/browser/file_system/native_file_util.h"
#include "storage/browser/file_system/obfuscated_file_util_delegate.h"

namespace storage {

// This delegate performs all ObfuscatedFileUtil tasks that require touching
// disk and peforms them in memory.

// The given paths should be subpaths of the |file_system_directory| folder
// passed to the constructor. No normalization is done on the this root folder
// and it is expected that all input paths would start with it.

// Directories and files are both stored in |Entry| structures, keeping the type
// of the entry, creation, modification, and last access time, and
// |file_content| or |directory_content| based on the type.

// Directory tree is kept in a tree structure starting from |root_|. All API
// functions that receive a |FilePath|, convert it to a |DecomposedPath| which
// has separated normalized components, is ensured that it is under the root,
// and has the respective |Entry| (if exists) and its parent in the directory
// tree starting from |root_|.

class COMPONENT_EXPORT(STORAGE_BROWSER) ObfuscatedFileUtilMemoryDelegate
    : public ObfuscatedFileUtilDelegate {
 public:
  ObfuscatedFileUtilMemoryDelegate(const base::FilePath& file_system_directory);

  ObfuscatedFileUtilMemoryDelegate(const ObfuscatedFileUtilMemoryDelegate&) =
      delete;
  ObfuscatedFileUtilMemoryDelegate& operator=(
      const ObfuscatedFileUtilMemoryDelegate&) = delete;

  ~ObfuscatedFileUtilMemoryDelegate() override;

  bool DirectoryExists(const base::FilePath& path) override;
  bool DeleteFileOrDirectory(const base::FilePath& path,
                             bool recursive) override;
  bool IsLink(const base::FilePath& file_path) override;
  bool PathExists(const base::FilePath& path) override;

  NativeFileUtil::CopyOrMoveMode CopyOrMoveModeForDestination(
      const FileSystemURL& dest_url,
      bool copy) override;
  base::File CreateOrOpen(const base::FilePath& path,
                          uint32_t file_flags) override;
  base::File::Error EnsureFileExists(const base::FilePath& path,
                                     bool* created) override;
  base::File::Error CreateDirectory(const base::FilePath& path,
                                    bool exclusive,
                                    bool recursive) override;
  base::File::Error GetFileInfo(const base::FilePath& path,
                                base::File::Info* file_info) override;
  base::File::Error Touch(const base::FilePath& path,
                          const base::Time& last_access_time,
                          const base::Time& last_modified_time) override;
  base::File::Error Truncate(const base::FilePath& path,
                             int64_t length) override;
  base::File::Error CopyOrMoveFile(
      const base::FilePath& src_path,
      const base::FilePath& dest_path,
      FileSystemOperation::CopyOrMoveOptionSet options,
      NativeFileUtil::CopyOrMoveMode mode) override;
  base::File::Error CopyInForeignFile(
      const base::FilePath& src_path,
      const base::FilePath& dest_path,
      FileSystemOperation::CopyOrMoveOptionSet options,
      NativeFileUtil::CopyOrMoveMode mode) override;
  base::File::Error DeleteFile(const base::FilePath& path) override;

  // Returns the total number of bytes used by all the files under |path|.
  // If the path does not exist or the total number of bytes doesn't fit in
  // |size_t|, the function returns 0.
  size_t ComputeDirectorySize(const base::FilePath& path) override;

  // Reads |buf_len| bytes from the file at |path|, starting from |offset|.
  // If successful, read bytes are written to |buf| and actual number of read
  // bytes are returned. Otherwise a net::Error value is returned.
  int ReadFile(const base::FilePath& path,
               int64_t offset,
               scoped_refptr<net::IOBuffer> buf,
               int buf_len);

  // Writes |buf_len| bytes to the file at |path|, starting from |offset|.
  // If successful, returns the actual number of written bytes, otherwise a
  // net::Error value is returned.
  int WriteFile(const base::FilePath& path,
                int64_t offset,
                scoped_refptr<net::IOBuffer> buf,
                int buf_len);

  base::File::Error CreateFileForTesting(const base::FilePath& path,
                                         base::span<const char> content);

  base::WeakPtr<ObfuscatedFileUtilMemoryDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  struct Entry;
  struct DecomposedPath;

  // Parses the given path into a decomposed path and performs validity checks
  // and normalization. Returns an empty value if checks fail.
  std::optional<DecomposedPath> ParsePath(const base::FilePath& path);

  // Creates or opens a file specified in |dp|.
  void CreateOrOpenInternal(const DecomposedPath& dp, uint32_t file_flags);

  // Moves a directory from |src_dp| to |dest_dp|.
  bool MoveDirectoryInternal(const DecomposedPath& src_dp,
                             const DecomposedPath& dest_dp);

  // Copies or moves a file from |src_dp| to |dest_dp|.
  bool CopyOrMoveFileInternal(const DecomposedPath& src_dp,
                              const DecomposedPath& dest_dp,
                              bool move);

  SEQUENCE_CHECKER(sequence_checker_);

  // The root of the directory tree.
  std::unique_ptr<Entry> root_;

  // The components of root path, kept for faster processing.
  std::vector<base::FilePath::StringType> root_path_components_;

  base::WeakPtrFactory<ObfuscatedFileUtilMemoryDelegate> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_OBFUSCATED_FILE_UTIL_MEMORY_DELEGATE_H_
