// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILEAPI_FILE_SYSTEM_CHOOSER_H_
#define CONTENT_BROWSER_FILEAPI_FILE_SYSTEM_CHOOSER_H_

#include "base/files/file.h"
#include "base/task_runner.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace content {

// This is a ui::SelectFileDialog::Listener implementation that grants access to
// the selected files to a specific renderer process on success, and then calls
// a callback on a specific task runner. Furthermore the listener will delete
// itself when any of its listener methods are called.
// All of this class has to be called on the UI thread.
class CONTENT_EXPORT FileSystemChooser : public ui::SelectFileDialog::Listener {
 public:
  using ResultCallback =
      base::OnceCallback<void(base::File::Error,
                              std::vector<blink::mojom::FileSystemEntryPtr>)>;

  static void CreateAndShow(
      int render_process_id,
      int frame_id,
      blink::mojom::ChooseFileSystemEntryType type,
      std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr> accepts,
      bool include_accepts_all,
      ResultCallback callback,
      scoped_refptr<base::TaskRunner> callback_runner);

  FileSystemChooser(int render_process_id,
                    blink::mojom::ChooseFileSystemEntryType type,
                    ResultCallback callback,
                    scoped_refptr<base::TaskRunner> callback_runner);

 private:
  ~FileSystemChooser() override;

  // ui::SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void MultiFilesSelected(const std::vector<base::FilePath>& files,
                          void* params) override;
  void FileSelectionCanceled(void* params) override;

  int render_process_id_;
  ResultCallback callback_;
  scoped_refptr<base::TaskRunner> callback_runner_;
  blink::mojom::ChooseFileSystemEntryType type_;

  scoped_refptr<ui::SelectFileDialog> dialog_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILEAPI_FILE_SYSTEM_CHOOSER_H_
