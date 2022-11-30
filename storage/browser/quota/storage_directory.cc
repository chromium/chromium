// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/storage_directory.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "components/services/storage/public/cpp/constants.h"
#include "storage/browser/quota/storage_directory_util.h"

namespace storage {
namespace {
const base::FilePath::CharType kDoomedPathName[] =
    FILE_PATH_LITERAL("-doomed-");
}

StorageDirectory::StorageDirectory(const base::FilePath& profile_path)
    : web_storage_path_(profile_path.Append(kWebStorageDirectory)) {
  DCHECK(!profile_path.empty()) << "Should not be called in incognito mode.";
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
  if (!base::PathExists(path))
    return true;

  base::FilePath doomed_dir;
  base::CreateTemporaryDirInDir(
      path.DirName(), base::StrCat({path.BaseName().value(), kDoomedPathName}),
      &doomed_dir);
  return base::Move(path, doomed_dir);
}

std::set<base::FilePath> StorageDirectory::EnumerateDoomedDirectories() {
  base::FileEnumerator enumerator(
      web_storage_path_.DirName(),
      /*recursive=*/false, base::FileEnumerator::DIRECTORIES,
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
      web_storage_path_, /*recursive=*/false, base::FileEnumerator::DIRECTORIES,
      base::StrCat(
          {FILE_PATH_LITERAL("*"), kDoomedPathName, FILE_PATH_LITERAL("*")}));

  std::set<base::FilePath> paths;
  base::FilePath path;
  while (path = enumerator.Next(), !path.empty())
    paths.insert(path);
  return paths;
}

}  // namespace storage
