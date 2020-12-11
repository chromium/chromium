// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_WRITE_ITEM_H_
#define CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_WRITE_ITEM_H_

#include <cstdint>
#include <string>

#include "base/files/file_path.h"
#include "content/common/content_export.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
class WebContents;

// Represents the state of a NativeFileSystemFileWriter when it is closed,
// containing all the data necessary to do a safe browsing download protection
// check on the data written to disk.
struct CONTENT_EXPORT NativeFileSystemWriteItem {
  NativeFileSystemWriteItem();
  ~NativeFileSystemWriteItem();
  NativeFileSystemWriteItem(const NativeFileSystemWriteItem&) = delete;
  NativeFileSystemWriteItem& operator=(const NativeFileSystemWriteItem&) =
      delete;

  // The path on disk where the data will eventually end up.
  base::FilePath target_file_path;
  // The path on disk where the data was written to. If the safe browsing check
  // completes the file at |full_path| will be moved to |target_file_path|.
  base::FilePath full_path;
  // SHA256 hash of the file contents.
  std::string sha256_hash;
  // Size of the file in bytes.
  int64_t size = 0;

  // URL of the frame in which the write operation took place.
  GURL frame_url;
  // True iff the frame had a transient user activation when the writer was
  // created.
  bool has_user_gesture = false;

  // BrowserContext and WebContents the writer is associated with. These fields
  // can be nullptr when calling
  // NativeFileSystemPermissionContext::PerformSafeBrowsingChecks(), in which
  // case they will be filled by that method.
  WebContents* web_contents = nullptr;
  BrowserContext* browser_context = nullptr;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_WRITE_ITEM_H_
