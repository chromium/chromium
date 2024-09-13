// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verifier/content_verifier_utils.h"

#include "base/strings/string_util.h"

namespace extensions {
namespace content_verifier_utils {

namespace {
#if BUILDFLAG(IS_WIN)
// Returns true if |path| ends with (.| )+.
// |out_path| will contain "." and/or " " suffix removed from |path|.
bool TrimDotSpaceSuffix(const base::FilePath::StringType& path,
                        base::FilePath::StringType* out_path) {
  static_assert(IsDotSpaceFilenameSuffixIgnored(),
                "dot-space suffix shouldn't be trimmed in current system");
  base::FilePath::StringType::size_type trim_pos =
      path.find_last_not_of(FILE_PATH_LITERAL(". "));
  if (trim_pos == base::FilePath::StringType::npos) {
    return false;
  }

  *out_path = path.substr(0, trim_pos + 1);
  return true;
}
#endif  // BUILDFLAG(IS_WIN)
}  // namespace

CanonicalRelativePath CanonicalizeRelativePath(
    const base::FilePath& relative_path) {
  base::FilePath::StringType canonical_path =
      relative_path.NormalizePathSeparatorsTo('/').value();
  if (!IsFileAccessCaseSensitive()) {
    canonical_path = base::ToLowerASCII(canonical_path);
  }

#if BUILDFLAG(IS_WIN)
  static_assert(IsDotSpaceFilenameSuffixIgnored());
  TrimDotSpaceSuffix(canonical_path, &canonical_path);
#else
  static_assert(!IsDotSpaceFilenameSuffixIgnored());
#endif  // BUILDFLAG(IS_WIN)

  return CanonicalRelativePath(std::move(canonical_path));
}

}  // namespace content_verifier_utils
}  // namespace extensions
