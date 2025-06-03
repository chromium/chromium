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
  // We need to normalize `extension_root` on its own because `IsParent` doesn't
  // normalize file paths. Without normalization parent references, Windows
  // short paths, or different path capitalization will cause `IsParent` to
  // return false.
  bool extension_root_normalization_skipped = false;
  base::FilePath normalized_extension_root;
  if (!base::NormalizeFilePath(extension_root, &normalized_extension_root)) {
#if BUILDFLAG(IS_WIN)
    // On Windows, `NormalizeFilePath` fails if the path doesn't start with a
    // drive letter (e.g. a network path) or if it exceeds `MAX_PATH` characters
    // in length. Fall back to `MakeAbsoluteFilePath` and proceed if the path
    // exists.
    normalized_extension_root = base::MakeAbsoluteFilePath(extension_root);
    if (normalized_extension_root.empty() ||
        !base::PathExists(normalized_extension_root)) {
      return base::FilePath();
    }

    extension_root_normalization_skipped = true;
#else
    return base::FilePath();
#endif
  }

  base::FilePath full_path = normalized_extension_root.Append(relative_path);

  // If we are allowing the file to be a symlink outside of the root, then the
  // path before resolving the symlink must still be within it.
  if (symlink_policy == FOLLOW_SYMLINKS_ANYWHERE) {
    int depth = 0;
    for (const auto& component : relative_path.GetComponents()) {
      if (component == base::FilePath::kParentDirectory) {
        depth--;
      } else if (component != base::FilePath::kCurrentDirectory) {
        depth++;
      }
      if (depth < 0) {
        return base::FilePath();
      }
    }
  }

  // We must resolve the absolute path of the combined path when
  // the relative path contains references to a parent folder (i.e., '..').
  // NormalizeFilePath will fail if the path doesn't exist.
  if (base::FilePath full_path_normalized;
      !extension_root_normalization_skipped &&
      base::NormalizeFilePath(full_path, &full_path_normalized)) {
    full_path = std::move(full_path_normalized);
  } else {
#if BUILDFLAG(IS_WIN)
    // On Windows, if `NormalizeFilePath` fails, fall back to
    // `MakeAbsoluteFilePath` and proceed if the file exists. This can happen
    // if, for example, the file isn't accessible due to permissions.
    full_path = base::MakeAbsoluteFilePath(full_path);
    if (full_path.empty() || !base::PathExists(full_path)) {
      return base::FilePath();
    }
#else
    return base::FilePath();
#endif
  }

  if (symlink_policy != FOLLOW_SYMLINKS_ANYWHERE &&
      !normalized_extension_root.IsParent(full_path)) {
    return base::FilePath();
  }

#if BUILDFLAG(IS_MAC)
  // Reject file paths ending with a separator. Unlike other platforms, macOS
  // strips the trailing separator when `realpath` is used, which causes
  // inconsistencies. See https://crbug.com/356878412.
  if (relative_path.EndsWithSeparator() && !base::DirectoryExists(full_path)) {
    return base::FilePath();
  }
#endif

#if BUILDFLAG(IS_WIN)
  // Reject paths ending with '.' or ' '. Such suffix is ignored when accessing
  // files on Windows, which causes inconsistencies. See
  // https://crbug.com/400119351.
  if (base::FeatureList::IsEnabled(
          extensions_features::kWinRejectDotSpaceSuffixFilePaths) &&
      !relative_path.empty()) {
    const char last_char = relative_path.value().back();
    if (last_char == '.' || last_char == ' ') {
      return base::FilePath();
    }
  }
#endif

  return full_path;
}

}  // namespace extensions
