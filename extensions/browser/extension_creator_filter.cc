// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_creator_filter.h"

#include <stddef.h>

#include <set>
#include <vector>

#include "base/cxx17_backports.h"
#include "build/build_config.h"
#include "extensions/common/constants.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace extensions {

ExtensionCreatorFilter::ExtensionCreatorFilter(
    const base::FilePath& extension_dir)
    : reserved_metadata_dir_(extension_dir.Append(kMetadataFolder)) {}

bool ExtensionCreatorFilter::ShouldPackageFile(
    const base::FilePath& file_path) {
  const base::FilePath& base_name = file_path.BaseName();
  if (base_name.empty()) {
    return false;
  }

  // Exclude the kMetadata folder which is reserved for use by the Extension
  // system.
  if (reserved_metadata_dir_ == file_path ||
      reserved_metadata_dir_.IsParent(file_path)) {
    return false;
  }

  // The file path that contains one of following special components should be
  // excluded. See https://crbug.com/314360 and https://crbug.com/27840.
  const base::FilePath::StringType names_to_exclude[] = {
      FILE_PATH_LITERAL(".DS_Store"),   FILE_PATH_LITERAL(".git"),
      FILE_PATH_LITERAL(".svn"),        FILE_PATH_LITERAL("__MACOSX"),
      FILE_PATH_LITERAL("desktop.ini"), FILE_PATH_LITERAL("Thumbs.db")};
  std::set<base::FilePath::StringType> names_to_exclude_set(
      names_to_exclude, names_to_exclude + base::size(names_to_exclude));
  std::vector<base::FilePath::StringType> components;
  file_path.GetComponents(&components);
  for (size_t i = 0; i < components.size(); i++) {
    if (names_to_exclude_set.count(components[i]))
      return false;
  }

  base::FilePath::CharType first_character = base_name.value().front();
  base::FilePath::CharType last_character = base_name.value().back();

  // dotfile
  if (first_character == '.') {
    return false;
  }
  // Emacs backup file
  if (last_character == '~') {
    return false;
  }
  // Emacs auto-save file
  if (first_character == '#' && last_character == '#') {
    return false;
  }

#if defined(OS_WIN)
  // It's correct that we use file_path, not base_name, here, because we
  // are working with the actual file.
  DWORD file_attributes = ::GetFileAttributes(file_path.value().c_str());
  if (file_attributes == INVALID_FILE_ATTRIBUTES) {
    return false;
  }
  if ((file_attributes & FILE_ATTRIBUTE_HIDDEN) != 0) {
    return false;
  }
#endif

  return true;
}

}  // namespace extensions
