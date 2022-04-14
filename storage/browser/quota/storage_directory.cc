// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/storage_directory.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "components/services/storage/public/cpp/constants.h"

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
  if (!base::PathExists(web_storage_path_))
    return true;

  base::FilePath doomed_dir;
  base::CreateTemporaryDirInDir(
      web_storage_path_.DirName(),
      base::StrCat({kWebStorageDirectory, kDoomedPathName}), &doomed_dir);
  return base::Move(web_storage_path_, doomed_dir);
}

void StorageDirectory::ClearDoomed() {
  std::set<base::FilePath> paths = EnumerateDoomedDirectories();

  for (const base::FilePath& path : paths)
    base::DeletePathRecursively(path);
}

std::set<base::FilePath> StorageDirectory::EnumerateDoomedDirectories() {
  base::FileEnumerator enumerator(
      web_storage_path_.DirName(), /*recursive=*/false,
      base::FileEnumerator::DIRECTORIES,
      base::StrCat(
          {kWebStorageDirectory, kDoomedPathName, FILE_PATH_LITERAL("*")}));

  std::set<base::FilePath> paths;
  base::FilePath path;
  while (path = enumerator.Next(), !path.empty())
    paths.insert(path);
  return paths;
}

}  // namespace storage
