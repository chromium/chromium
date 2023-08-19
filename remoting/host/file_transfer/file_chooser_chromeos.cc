// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/file_chooser_chromeos.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/chromeos/ash_proxy.h"
#include "remoting/host/file_transfer/file_chooser.h"
#include "remoting/protocol/file_transfer_helpers.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace remoting {

class FileChooserChromeOs::Core : public ui::SelectFileDialog::Listener {
 public:
  explicit Core(ResultCallback callback);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core() override;

  void Show();

  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectedWithExtraInfo(const ui::SelectedFileInfo& file,
                                 int index,
                                 void* params) override;
  void FileSelectionCanceled(void* params) override;

 private:
  void RunCallback(const FileChooser::Result& result);

  void Cleanup();

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  const raw_ref<AshProxy, ExperimentalAsh> ash_;
  FileChooser::ResultCallback callback_;
};

FileChooserChromeOs::FileChooserChromeOs(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    ResultCallback callback) {
  auto callback_on_current_thread =
      base::BindPostTaskToCurrentDefault<void, Result>(std::move(callback));
  file_chooser_core_ = base::SequenceBound<FileChooserChromeOs::Core>(
      std::move(ui_task_runner), std::move(callback_on_current_thread));
}

FileChooserChromeOs::~FileChooserChromeOs() = default;

void FileChooserChromeOs::Show() {
  file_chooser_core_.AsyncCall(&FileChooserChromeOs::Core::Show);
}

////////////////////////////////////////////////////////////////////////////////
//   FileChooserChromeOs::Core
////////////////////////////////////////////////////////////////////////////////

FileChooserChromeOs::Core::Core(ResultCallback callback)
    : select_file_dialog_(ui::SelectFileDialog::Create(
          /*listener=*/this,
          /*policy=*/nullptr)),
      ash_(AshProxy::Get()),
      callback_(std::move(callback)) {}

FileChooserChromeOs::Core::~Core() {
  select_file_dialog_->ListenerDestroyed();
}

void FileChooserChromeOs::Core::FileSelected(const base::FilePath& path,
                                             int index,
                                             void* params) {
  RunCallback(path);
}

void FileChooserChromeOs::Core::FileSelectedWithExtraInfo(
    const ui::SelectedFileInfo& file,
    int index,
    void* params) {
  RunCallback(file.file_path);
}

void FileChooserChromeOs::Core::FileSelectionCanceled(void* params) {
  RunCallback(protocol::MakeFileTransferError(
      FROM_HERE, protocol::FileTransfer_Error_Type_CANCELED));
}

void FileChooserChromeOs::Core::RunCallback(const FileChooser::Result& result) {
  std::move(callback_).Run(result);
}

void FileChooserChromeOs::Core::Show() {
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE,
      /*title=*/std::u16string(),
      /*default_path=*/base::FilePath(),
      /*file_types=*/nullptr,
      /*file_type_index=*/0,
      /*default_extension=*/base::FilePath::StringType(),
      /*owning_window=*/ash_->GetSelectFileContainer(),
      /*params=*/nullptr);
}

std::unique_ptr<FileChooser> FileChooser::Create(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    ResultCallback callback) {
  return std::make_unique<FileChooserChromeOs>(std::move(ui_task_runner),
                                               std::move(callback));
}

}  // namespace remoting
