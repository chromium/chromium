// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_FILE_SYSTEM_CHOOSER_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_FILE_SYSTEM_CHOOSER_H_

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/task_runner.h"
#include "content/common/content_export.h"
#include "storage/browser/file_system/isolated_context.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_manager.mojom.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace content {

class WebContents;

// This is a ui::SelectFileDialog::Listener implementation that grants access to
// the selected files to a specific renderer process on success, and then calls
// a callback on a specific task runner. Furthermore the listener will delete
// itself when any of its listener methods are called.
// All of this class has to be called on the UI thread.
class CONTENT_EXPORT FileSystemChooser : public ui::SelectFileDialog::Listener {
 public:
  using ResultCallback =
      base::OnceCallback<void(blink::mojom::NativeFileSystemErrorPtr,
                              std::vector<base::FilePath>)>;

  class CONTENT_EXPORT Options {
   public:
    Options(blink::mojom::ChooseFileSystemEntryType type,
            std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr>
                accepts,
            bool include_accepts_all);
    Options(const Options&) = default;
    Options& operator=(const Options&) = default;

    blink::mojom::ChooseFileSystemEntryType type() const { return type_; }
    const ui::SelectFileDialog::FileTypeInfo& file_type_info() const {
      return file_types_;
    }

   private:
    blink::mojom::ChooseFileSystemEntryType type_;
    ui::SelectFileDialog::FileTypeInfo file_types_;
  };

  static void CreateAndShow(WebContents* web_contents,
                            const Options& options,
                            ResultCallback callback);

  FileSystemChooser(blink::mojom::ChooseFileSystemEntryType type,
                    ResultCallback callback);

 private:
  ~FileSystemChooser() override;

  // ui::SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void MultiFilesSelected(const std::vector<base::FilePath>& files,
                          void* params) override;
  void FileSelectionCanceled(void* params) override;

  ResultCallback callback_;
  blink::mojom::ChooseFileSystemEntryType type_;

  scoped_refptr<ui::SelectFileDialog> dialog_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_FILE_SYSTEM_CHOOSER_H_
