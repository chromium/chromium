// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FILE_SELECT_LISTENER_H_
#define CONTENT_PUBLIC_BROWSER_FILE_SELECT_LISTENER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"

namespace content {

// Callback interface to receive results of RunFileChooser() and
// EnumerateDirectory() of WebContentsDelegate.
class FileSelectListener : public base::RefCounted<FileSelectListener> {
 public:
  // This function should be called if file selection succeeds.
  // |files| - A list of selected files.
  // |base_dir| - This has non-empty directory path if |mode| argument is
  //              kUploadFolder and kUploadFolder is supported by the
  //              WebContentsDelegate instance. It represents enumeration root
  //              directory.
  //              This is an empty FilePath otherwise.
  // |mode| - kUploadFolder for WebContentsDelegate::EnumerateDirectory(), and
  //          |params.mode| for WebContentsDelegate::RunFileChooser().
  virtual void FileSelected(
      std::vector<blink::mojom::FileChooserFileInfoPtr> files,
      const base::FilePath& base_dir,
      blink::mojom::FileChooserParams::Mode mode) = 0;

  // This function should be called if a user cancels a file selection
  // dialog, or we open no file selection dialog for some reason.
  virtual void FileSelectionCanceled() = 0;

 protected:
  virtual ~FileSelectListener() = default;

  friend class base::RefCounted<FileSelectListener>;
};

}  // namespace content
#endif  // CONTENT_PUBLIC_BROWSER_FILE_SELECT_LISTENER_H_
