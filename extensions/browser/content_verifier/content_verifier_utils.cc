// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verifier/content_verifier_utils.h"

#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace extensions::content_verifier_utils {

CanonicalRelativePath CanonicalizeRelativePath(
    const base::FilePath& relative_path) {
  base::FilePath::StringType canonical_path =
      relative_path.NormalizePathSeparatorsTo('/').value();
  if (!IsFileAccessCaseSensitive()) {
    // Use `FoldCase` to canonicalize case, this is like `ToLower` for many
    // languages but works independently of the current locale.
#if BUILDFLAG(IS_WIN)
    canonical_path =
        base::AsWString(base::i18n::FoldCase(base::AsString16(canonical_path)));
#else
    canonical_path = base::UTF16ToUTF8(
        base::i18n::FoldCase(base::UTF8ToUTF16(canonical_path)));
#endif  // BUILDFLAG(IS_WIN)
  }

  return CanonicalRelativePath(std::move(canonical_path));
}

base::FilePath NormalizePathComponents(const base::FilePath& relative_path) {
  CHECK(!relative_path.IsAbsolute());

  base::FilePath normalized_path;
  for (const auto& component : relative_path.GetComponents()) {
    if (component == base::FilePath::kCurrentDirectory) {
      // Do nothing.
    } else if (component == base::FilePath::kParentDirectory) {
      const auto base_name = normalized_path.BaseName();
      if (base_name.empty() ||
          base_name.value() == base::FilePath::kCurrentDirectory ||
          base_name.value() == base::FilePath::kParentDirectory) {
        normalized_path =
            normalized_path.Append(base::FilePath::kParentDirectory);
      } else {
        normalized_path = normalized_path.DirName();
      }
    } else {
      // This is just a regular component. Append it.
      normalized_path = normalized_path.Append(component);
    }
  }

  if (normalized_path.empty()) {
    normalized_path = base::FilePath(base::FilePath::kCurrentDirectory);
  }

  if (relative_path.EndsWithSeparator()) {
    normalized_path = normalized_path.AsEndingWithSeparator();
  }

  return normalized_path.NormalizePathSeparators();
}

}  // namespace extensions::content_verifier_utils
