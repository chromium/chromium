// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_resource.h"

#include "base/check.h"
#include "base/files/file_util.h"
#include "extensions/common/extension_features.h"

namespace extensions {

ExtensionResource::ExtensionResource() : follow_symlinks_anywhere_(false) {}

ExtensionResource::ExtensionResource(const ExtensionId& extension_id,
                                     const base::FilePath& extension_root,
                                     const base::FilePath& relative_path)
    : extension_id_(extension_id),
      extension_root_(extension_root),
      relative_path_(relative_path),
      follow_symlinks_anywhere_(false) {}

ExtensionResource::ExtensionResource(const ExtensionResource& other) = default;
ExtensionResource::ExtensionResource(ExtensionResource&& other) = default;
ExtensionResource& ExtensionResource::operator=(ExtensionResource&& other) =
    default;

ExtensionResource::~ExtensionResource() = default;

void ExtensionResource::set_follow_symlinks_anywhere() {
  follow_symlinks_anywhere_ = true;
}

const base::FilePath& ExtensionResource::GetFilePath() const {
  if (extension_root_.empty() || relative_path_.empty()) {
    DCHECK(full_resource_path_.empty());
    return full_resource_path_;
  }

  // We've already checked, just return last value.
  if (!full_resource_path_.empty())
    return full_resource_path_;

  full_resource_path_ = GetFilePath(
      extension_root_, relative_path_,
      follow_symlinks_anywhere_ ?
          FOLLOW_SYMLINKS_ANYWHERE : SYMLINKS_MUST_RESOLVE_WITHIN_ROOT);
  return full_resource_path_;
}

// static
base::FilePath ExtensionResource::GetFilePath(
    const base::FilePath& extension_root,
    const base::FilePath& relative_path,
    SymlinkPolicy symlink_policy) {
  // We need to resolve the parent references in the extension_root
  // path on its own because IsParent doesn't like parent references.
  base::FilePath clean_extension_root(
      base::MakeAbsoluteFilePath(extension_root));
  if (clean_extension_root.empty())
    return base::FilePath();

  base::FilePath full_path = clean_extension_root.Append(relative_path);

  // If we are allowing the file to be a symlink outside of the root, then the
  // path before resolving the symlink must still be within it.
  if (symlink_policy == FOLLOW_SYMLINKS_ANYWHERE) {
    std::vector<base::FilePath::StringType> components =
        relative_path.GetComponents();
    int depth = 0;

    for (std::vector<base::FilePath::StringType>::const_iterator
         i = components.begin(); i != components.end(); i++) {
      if (*i == base::FilePath::kParentDirectory) {
        depth--;
      } else if (*i != base::FilePath::kCurrentDirectory) {
        depth++;
      }
      if (depth < 0) {
        return base::FilePath();
      }
    }
  }

  // We must resolve the absolute path of the combined path when
  // the relative path contains references to a parent folder (i.e., '..').
  // We also check if the path exists because the posix version of
  // MakeAbsoluteFilePath will fail if the path doesn't exist, and we want the
  // same behavior on Windows... So until the posix and Windows version of
  // MakeAbsoluteFilePath are unified, we need an extra call to PathExists,
  // unfortunately.
  // TODO(mad): Fix this once MakeAbsoluteFilePath is unified.
  full_path = base::MakeAbsoluteFilePath(full_path);
  if (!base::PathExists(full_path) ||
      (symlink_policy != FOLLOW_SYMLINKS_ANYWHERE &&
       !clean_extension_root.IsParent(full_path))) {
    return base::FilePath();
  }

#if BUILDFLAG(IS_MAC)
  // Reject file paths ending with a separator. Unlike other platforms, macOS
  // strips the trailing separator when `realpath` is used, which causes
  // inconsistencies. See https://crbug.com/356878412.
  if (base::FeatureList::IsEnabled(
          extensions_features::kMacRejectFilePathsEndingWithSeparator) &&
      relative_path.EndsWithSeparator() && !base::DirectoryExists(full_path)) {
    return base::FilePath();
  }
#endif

  return full_path;
}

}  // namespace extensions
