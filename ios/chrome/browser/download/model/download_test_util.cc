// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/model/download_test_util.h"

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"

namespace testing {

std::string GetTestFileContents(base::FilePath::StringViewType file_path) {
  std::string contents;
  const base::FilePath full_path =
      base::PathService::CheckedGet(base::DIR_ASSETS).Append(file_path);
  const bool success = base::ReadFileToString(full_path, &contents);
  CHECK(success) << "failed to read file: " << full_path;
  return contents;
}

}  // namespace testing
