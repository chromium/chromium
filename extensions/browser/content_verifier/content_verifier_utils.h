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
//   - In Windows, trimming "dot-space" suffix in path.
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
constexpr bool IsDotSpaceFilenameSuffixIgnored() {
#if BUILDFLAG(IS_WIN)
  static_assert(!IsFileAccessCaseSensitive(),
                "DotSpace suffix should only be ignored in case-insensitive"
                "systems");
  return true;
#else
  return false;
#endif
}

// Returns platform specific canonicalized version of |relative_path| for
// content verification system.
CanonicalRelativePath CanonicalizeRelativePath(
    const base::FilePath& relative_path);

}  // namespace extensions::content_verifier_utils

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_VERIFIER_UTILS_H_
