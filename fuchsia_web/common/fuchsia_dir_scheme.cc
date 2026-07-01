// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/common/fuchsia_dir_scheme.h"

#include "base/files/file_path.h"
#include "url/url_util.h"

const char kFuchsiaDirScheme[] = "fuchsia-dir";

void RegisterFuchsiaDirScheme() {
  url::AddStandardScheme(kFuchsiaDirScheme, url::SCHEME_WITH_HOST);
  url::AddLocalScheme(kFuchsiaDirScheme);
}

bool IsValidContentDirectoryName(std::string_view content_directory_name) {
  if (content_directory_name.find_first_of(
          base::FilePath::kSeparators, 0,
          base::FilePath::kSeparatorsLength - 1) != std::string_view::npos) {
    return false;
  }
  if (content_directory_name.empty() ||
      content_directory_name == base::FilePath::kCurrentDirectory ||
      content_directory_name == base::FilePath::kParentDirectory) {
    return false;
  }
  return true;
}
