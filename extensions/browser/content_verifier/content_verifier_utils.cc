// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verifier/content_verifier_utils.h"

namespace extensions {
namespace content_verifier_utils {

#if defined(OS_WIN)
bool TrimDotSpaceSuffix(const base::FilePath::StringType& path,
                        base::FilePath::StringType* out_path) {
  base::FilePath::StringType::size_type trim_pos =
      path.find_last_not_of(FILE_PATH_LITERAL(". "));
  if (trim_pos == base::FilePath::StringType::npos)
    return false;

  *out_path = path.substr(0, trim_pos + 1);
  return true;
}
#endif  // defined(OS_WIN)

}  // namespace content_verifier_utils
}  // namespace extensions
