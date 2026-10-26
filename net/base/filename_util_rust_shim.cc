// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/filename_util_rust_shim.h"

#include <utility>

#include "net/base/filename_util.h"

namespace net::rust {

std::unique_ptr<base::FilePath> FileURLToFilePath(const GURL& url) {
  base::FilePath file_path;
  if (!net::FileURLToFilePath(url, &file_path)) {
    return nullptr;
  }
  return std::make_unique<base::FilePath>(std::move(file_path));
}

}  // namespace net::rust
