// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCOPED_BROWSER_FILE_ACCESS_H_
#define CONTENT_BROWSER_SCOPED_BROWSER_FILE_ACCESS_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"

namespace content {

// A helper that grants the network service the capability to upload specific
// files on the browser's behalf while the object is alive. This is useful for
// browser-initiated uploads that do not use SimpleURLLoader.
class CONTENT_EXPORT ScopedBrowserFileAccess {
 public:
  explicit ScopedBrowserFileAccess(std::vector<base::FilePath> files);
  ScopedBrowserFileAccess(const ScopedBrowserFileAccess&) = delete;
  ScopedBrowserFileAccess& operator=(const ScopedBrowserFileAccess&) = delete;
  ~ScopedBrowserFileAccess();

 private:
  const base::UnguessableToken owner_token_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCOPED_BROWSER_FILE_ACCESS_H_
