// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/buckets/bucket_utils.h"

#include "base/strings/string_util.h"

namespace content {

bool IsValidBucketName(const std::string& name) {
  // Details on bucket name validation and reasoning explained in
  // https://github.com/WICG/storage-buckets/blob/gh-pages/explainer.md
  if (name.empty() || name.length() >= 64)
    return false;

  // The name must only contain characters in a restricted set.
  for (char ch : name) {
    if (base::IsAsciiLower(ch))
      continue;
    if (base::IsAsciiDigit(ch))
      continue;
    if (ch == '_' || ch == '-')
      continue;
    return false;
  }

  // The first character in the name is more restricted.
  if (name[0] == '_' || name[0] == '-')
    return false;
  return true;
}

}  // namespace content
