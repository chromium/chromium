// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FILE_SYSTEM_ACCESS_WRITE_ITEM_H_
#define CONTENT_PUBLIC_BROWSER_FILE_SYSTEM_ACCESS_WRITE_ITEM_H_

#include <cstdint>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
class WebContents;

// Represents the state of a FileSystemAccessFileWriter when it is closed,
// containing all the data necessary to do a safe browsing download protection
// check on the data written to disk.
struct CONTENT_EXPORT FileSystemAccessWriteItem {
  FileSystemAccessWriteItem();
  ~FileSystemAccessWriteItem();
  FileSystemAccessWriteItem(const FileSystemAccessWriteItem&) = delete;
  FileSystemAccessWriteItem& operator=(const FileSystemAccessWriteItem&) =
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

  // id of the outermost main frame in which the write operation took place.
  GlobalRenderFrameHostId outermost_main_frame_id;

  // True iff the frame had a transient user activation when the writer was
  // created.
  bool has_user_gesture = false;

  // BrowserContext and WebContents the writer is associated with. These fields
  // can be nullptr when calling
  // FileSystemAccessPermissionContext::PerformAfterWriteChecks(), in which
  // case they will be filled by that method.
  raw_ptr<WebContents> web_contents = nullptr;
  raw_ptr<BrowserContext, DanglingUntriaged> browser_context = nullptr;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FILE_SYSTEM_ACCESS_WRITE_ITEM_H_
