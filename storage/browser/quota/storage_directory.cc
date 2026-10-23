// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/storage_directory.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/constants.h"
#include "storage/browser/quota/storage_directory_util.h"

namespace storage {
namespace {
const base::FilePath::CharType kDoomedPathName[] =
    FILE_PATH_LITERAL("-doomed-");
}

StorageDirectory::StorageDirectory(const base::FilePath& profile_path)
    : web_storage_path_(profile_path.Append(kWebStorageDirectory)) {
  CHECK(!profile_path.empty()) << "Should not be called in incognito mode.";
}

StorageDirectory::~StorageDirectory() = default;

bool StorageDirectory::Create() {
  return base::CreateDirectory(web_storage_path_);
}

bool StorageDirectory::Doom() {
  return DoomPath(web_storage_path_);
}

void StorageDirectory::ClearDoomed() {
  std::set<base::FilePath> paths = EnumerateDoomedDirectories();

  for (const base::FilePath& path : paths)
    base::DeletePathRecursively(path);
}

bool StorageDirectory::CreateBucket(const BucketLocator& bucket) {
  base::FilePath bucket_path =
      CreateBucketPath(web_storage_path_.DirName(), bucket);
  return base::CreateDirectory(bucket_path);
}

bool StorageDirectory::DoomBucket(const BucketLocator& bucket) {
  base::FilePath bucket_path =
      CreateBucketPath(web_storage_path_.DirName(), bucket);
  return DoomPath(bucket_path);
}

void StorageDirectory::ClearDoomedBuckets() {
  std::set<base::FilePath> paths = EnumerateDoomedBuckets();

  for (const base::FilePath& path : paths)
    base::DeletePathRecursively(path);
}

bool StorageDirectory::DoomPath(const base::FilePath& path) {
  if (!base::PathExists(path) && !base::IsLink(path)) {
    return true;
  }

  // Atomically rename `path` onto a fresh name that does not exist yet.
  //
  // Do not pre-create the destination directory: when `rename(2)` fails (e.g.
  // with `EISDIR` on POSIX because `path` has been swapped for a symlink and
  // the destination already exists as a directory), `base::Move()` falls back
  // to the symlink-following `base::CopyDirectory()` and copies the link target
  // into the profile directory. On POSIX, `base::ReplaceFile()` performs a
  // direct `rename(2)` without a copy fallback: a real directory moves as
  // before, while a planted symlink is renamed without being followed and then
  // unlinked by `ClearDoomed*()`. On Windows, `base::ReplaceFile()` creates a
  // temporary backup file and calls Win32 `::ReplaceFile()` (which fails for
  // directories and logs a failed `Windows.ReplaceFileResult` histogram sample
  // before falling back to `::MoveFile()`), so remove reparse points directly
  // and use `base::Move()` onto the non-existent `doomed_dir`.
  base::FilePath doomed_dir = path.DirName().Append(base::StrCat(
      {path.BaseName().value(), kDoomedPathName,
       base::FilePath::FromASCII(
           base::Uuid::GenerateRandomV4().AsLowercaseString())
           .value()}));
#if BUILDFLAG(IS_POSIX)
  return base::ReplaceFile(path, doomed_dir, /*error=*/nullptr);
#else
  if (base::IsLink(path)) {
    return base::DeletePathRecursively(path);
  }
  return base::Move(path, doomed_dir);
#endif
}

std::set<base::FilePath> StorageDirectory::EnumerateDoomedDirectories() {
  base::FileEnumerator enumerator(
      web_storage_path_.DirName(),
      /*recursive=*/false, base::FileEnumerator::NAMES_ONLY,
      base::StrCat(
          {kWebStorageDirectory, kDoomedPathName, FILE_PATH_LITERAL("*")}));

  std::set<base::FilePath> paths;
  base::FilePath path;
  while (path = enumerator.Next(), !path.empty())
    paths.insert(path);
  return paths;
}

std::set<base::FilePath> StorageDirectory::EnumerateDoomedBuckets() {
  base::FileEnumerator enumerator(
      web_storage_path_, /*recursive=*/false, base::FileEnumerator::NAMES_ONLY,
      base::StrCat(
          {FILE_PATH_LITERAL("*"), kDoomedPathName, FILE_PATH_LITERAL("*")}));

  std::set<base::FilePath> paths;
  base::FilePath path;
  while (path = enumerator.Next(), !path.empty())
    paths.insert(path);
  return paths;
}

}  // namespace storage
