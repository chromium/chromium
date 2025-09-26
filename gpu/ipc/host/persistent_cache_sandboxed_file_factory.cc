// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/host/persistent_cache_sandboxed_file_factory.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"

namespace gpu {

namespace {

struct PersistentCacheFilePaths {
  base::FilePath db_path;
  base::FilePath journal_path;
};

std::string GetVersionSuffix(const std::string& product) {
  // Use produce's version to differentiate the cache files.
  // TODO(crbug.com/399642827): Use Dawn/ANGLE/Skia's versions which change less
  // often. The product string may contain characters that are not valid in a
  // filename, so it must be sanitized.
  std::string version_suffix = product;
  base::ReplaceChars(version_suffix, "/\\", "_", &version_suffix);
  return version_suffix;
}

// Returns the paths to the cache database and journal files. The format is:
// <cache_dir>/<cache_id>/<version>/cache.db
// <cache_dir>/<cache_id>/<version>/cache.journal
PersistentCacheFilePaths GetPersistentCacheFilePaths(
    const base::FilePath& cache_root_dir,
    const base::FilePath::StringType& cache_id,
    const std::string& product) {
  base::FilePath version_dir =
      cache_root_dir.Append(cache_id).AppendASCII(GetVersionSuffix(product));

  return {version_dir.AppendASCII("cache.db"),
          version_dir.AppendASCII("cache.journal")};
}

// Deletes all files in the cache directory that are associated with the given
// cache_id but are not the current database or journal file. This is to clean
// up stale cache files from previous runs or different product versions.
void DeleteStaleFiles(const base::FilePath& cache_root_dir,
                      const base::FilePath::StringType& cache_id,
                      const std::string& product) {
  DCHECK(!cache_root_dir.empty());

  const std::string version_suffix = GetVersionSuffix(product);

  base::FilePath cache_dir = cache_root_dir.Append(cache_id);
  if (!base::PathExists(cache_dir)) {
    return;
  }

  base::FileEnumerator enumerator(cache_dir, false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    if (name.BaseName().MaybeAsASCII() != version_suffix) {
      base::DeletePathRecursively(name);
    }
  }
}

bool CreateCacheDirectory(const base::FilePath& cache_dir) {
  if (!base::CreateDirectory(cache_dir)) {
    LOG(ERROR) << "Failed to create cache directory: " << cache_dir;
    return false;
  }
  return true;
}

}  // namespace

PersistentCacheSandboxedFileFactory::PersistentCacheSandboxedFileFactory(
    const base::FilePath& cache_root_dir,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : cache_root_dir_(cache_root_dir),
      background_task_runner_(std::move(background_task_runner)) {
  if (cache_root_dir_.empty()) {
    cache_root_dir_ = base::FilePath(base::FilePath::kCurrentDirectory);
  } else {
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](const base::FilePath& dir) { CreateCacheDirectory(dir); },
            cache_root_dir_));
  }
}

PersistentCacheSandboxedFileFactory::~PersistentCacheSandboxedFileFactory() =
    default;

std::optional<PersistentCacheSandboxedFiles>
PersistentCacheSandboxedFileFactory::CreateFiles(const CacheIdString& cache_id,
                                                 const std::string& product) {
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteStaleFiles, cache_root_dir_, cache_id, product));

  DCHECK(!cache_root_dir_.empty());

  auto paths = GetPersistentCacheFilePaths(cache_root_dir_, cache_id, product);
  DCHECK_EQ(paths.db_path.DirName(), paths.journal_path.DirName());

  if (!CreateCacheDirectory(paths.db_path.DirName())) {
    return std::nullopt;
  }

  auto open_and_check_file = [](const base::FilePath& path) {
    base::File file(path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                              base::File::FLAG_WRITE);
    if (!file.IsValid()) {
      LOG(ERROR) << "Failed to open persistent cache file: " << path
                 << " error: "
                 << base::File::ErrorToString(file.error_details());
    }
    return file;
  };

  base::File db_file = open_and_check_file(paths.db_path);
  if (!db_file.IsValid()) {
    return std::nullopt;
  }

  base::File journal_file = open_and_check_file(paths.journal_path);
  if (!journal_file.IsValid()) {
    return std::nullopt;
  }

  return PersistentCacheSandboxedFiles{std::move(db_file),
                                       std::move(journal_file)};
}

void PersistentCacheSandboxedFileFactory::CreateFilesAsync(
    const CacheIdString& cache_id,
    const std::string& product,
    CreateFilesCallback callback) {
  // The reply will be posted to the current SequencedTaskRunner.
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PersistentCacheSandboxedFileFactory::CreateFiles, this,
                     cache_id, product),
      std::move(callback));
}

bool PersistentCacheSandboxedFileFactory::ClearFiles(
    const CacheIdString& cache_id,
    const std::string& product) {
  DCHECK(!cache_root_dir_.empty());

  auto paths = GetPersistentCacheFilePaths(cache_root_dir_, cache_id, product);

  // Delete the whole version directory.
  DCHECK_EQ(paths.db_path.DirName(), paths.journal_path.DirName());
  return base::DeletePathRecursively(paths.db_path.DirName());
}

void PersistentCacheSandboxedFileFactory::ClearFilesAsync(
    const CacheIdString& cache_id,
    const std::string& product,
    ClearFilesCallback callback) {
  // The reply will be posted to the current SequencedTaskRunner.
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PersistentCacheSandboxedFileFactory::ClearFiles, this,
                     cache_id, product),
      std::move(callback));
}

}  // namespace gpu
