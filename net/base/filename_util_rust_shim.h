// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_FILENAME_UTIL_RUST_SHIM_H_
#define NET_BASE_FILENAME_UTIL_RUST_SHIM_H_

#include <memory>

#include "base/files/file_path.h"
#include "url/gurl.h"

namespace net::rust {

// TODO(crbug.com/40226863): Remove this shim once Crubit can generate bindings
// for `net::FileURLToFilePath` directly.
std::unique_ptr<base::FilePath> FileURLToFilePath(const GURL& url);

}  // namespace net::rust

#endif  // NET_BASE_FILENAME_UTIL_RUST_SHIM_H_
