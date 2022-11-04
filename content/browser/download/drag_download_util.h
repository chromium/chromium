// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_UTIL_H_
#define CONTENT_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_UTIL_H_

#include <string>

#include "base/files/file.h"
#include "content/browser/download/drag_download_file.h"
#include "ui/base/dragdrop/download_file_interface.h"

class GURL;

namespace base {
class FilePath;
}

namespace content {

// Parse the download metadata set in DataTransfer.setData. The metadata
// consists of a set of the following values separated by ":"
// * MIME type
// * File name
// * URL
// If the file name contains special characters, they need to be escaped
// appropriately.
// For example, we can have
//   text/plain:example.txt:http://example.com/example.txt
bool ParseDownloadMetadata(const std::u16string& metadata,
                           std::u16string* mime_type,
                           base::FilePath* file_name,
                           GURL* url);

// Create a new file at the specified path. If the file already exists, try to
// insert the sequential unifier to produce a new file, like foo-01.txt.
// Return a File if successful.
base::File CreateFileForDrop(base::FilePath* file_path);

// Implementation of DownloadFileObserver to finalize the download process.
class PromiseFileFinalizer : public ui::DownloadFileObserver {
 public:
  explicit PromiseFileFinalizer(
      std::unique_ptr<DragDownloadFile> drag_file_downloader);

  PromiseFileFinalizer(const PromiseFileFinalizer&) = delete;
  PromiseFileFinalizer& operator=(const PromiseFileFinalizer&) = delete;

  // DownloadFileObserver methods.
  void OnDownloadCompleted(const base::FilePath& file_path) override;
  void OnDownloadAborted() override;

 protected:
  ~PromiseFileFinalizer() override;

 private:
  void Cleanup();

  std::unique_ptr<DragDownloadFile> drag_file_downloader_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_UTIL_H_
