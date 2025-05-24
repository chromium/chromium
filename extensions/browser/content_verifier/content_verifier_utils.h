// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_VERIFIER_UTILS_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_VERIFIER_UTILS_H_

#include "base/files/file_path.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"

namespace extensions::content_verifier_utils {

// Extension relative FilePath's canonical version for content verification
// system. Canonicalization consists of:
//   - Normalizing path separators to '/'.
//     This is done because GURLs generally use '/' separators (that is passed
//     to content verifier via extension_protocols) and manifest.json paths
//     also specify '/' separators.
//   - In case-insensitive OS, lower casing path.
using CanonicalRelativePath =
    ::base::StrongAlias<class CanonicalRelativePathTag,
                        base::FilePath::StringType>;

// Returns true if this system/OS's file access is case sensitive.
constexpr bool IsFileAccessCaseSensitive() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return false;
#else
  return true;
#endif
}

// Returns true if this system/OS ignores (.| )+ suffix in a filepath while
// accessing the file.
// TODO(https://crbug.com/400119351): Remove this in M138.
bool IsDotSpaceFilenameSuffixIgnored();

// Returns platform specific canonicalized version of `relative_path` for
// content verification system.
CanonicalRelativePath CanonicalizeRelativePath(
    const base::FilePath& relative_path);

// Normalize a relative pathname by collapsing redundant separators and up-level
// references, and normalizing separators so that "A//B", "A/./B", "A/foo/../B"
// and "A\\B" all become "A/B". The trailing separator is preserved. This string
// manipulation may change the meaning of a path that contains symbolic links.
//
// This function performs purely string operations without accessing the
// filesystem or considering the current directory. The path doesn't need to
// exist. Use base::NormalizeFilePath() to expand symbolic links and junctions
// and get an absolute normalized file path.
//
// The function CHECKs that the path is not absolute.
[[nodiscard]] base::FilePath NormalizePathComponents(
    const base::FilePath& relative_path);

}  // namespace extensions::content_verifier_utils

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_VERIFIER_UTILS_H_
