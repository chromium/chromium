// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_file_select_helper.h"

#include "base/memory/scoped_refptr.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace content {

// static
void ShellFileSelectHelper::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  // ShellFileSelectHelper will keep itself alive until it sends the result
  // message.
  scoped_refptr<ShellFileSelectHelper> file_select_helper(
      new ShellFileSelectHelper());
  file_select_helper->RunFileChooser(render_frame_host, std::move(listener),
                                     params.Clone());
}

ShellFileSelectHelper::ShellFileSelectHelper() = default;

ShellFileSelectHelper::~ShellFileSelectHelper() {
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

void ShellFileSelectHelper::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<FileSelectListener> listener,
    blink::mojom::FileChooserParamsPtr params) {
  DCHECK(!web_contents_);
  DCHECK(listener);
  DCHECK(!listener_);
  DCHECK(!select_file_dialog_);

  listener_ = std::move(listener);
  web_contents_ = content::WebContents::FromRenderFrameHost(render_frame_host)
                      ->GetWeakPtr();

  select_file_dialog_ = ui::SelectFileDialog::Create(this, nullptr);

  dialog_mode_ = params->mode;
  switch (params->mode) {
    case blink::mojom::FileChooserParams::Mode::kOpen:
      dialog_type_ = ui::SelectFileDialog::SELECT_OPEN_FILE;
      break;
    case blink::mojom::FileChooserParams::Mode::kOpenMultiple:
      dialog_type_ = ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE;
      break;
    case blink::mojom::FileChooserParams::Mode::kUploadFolder:
      dialog_type_ = ui::SelectFileDialog::SELECT_UPLOAD_FOLDER;
      break;
    case blink::mojom::FileChooserParams::Mode::kSave:
      dialog_type_ = ui::SelectFileDialog::SELECT_SAVEAS_FILE;
      break;
    default:
      // Prevent warning.
      dialog_type_ = ui::SelectFileDialog::SELECT_OPEN_FILE;
      NOTREACHED();
  }

  gfx::NativeWindow owning_window = web_contents_->GetTopLevelNativeWindow();
  select_file_dialog_->SelectFile(
      dialog_type_, std::u16string(), base::FilePath(), nullptr, 0,
      base::FilePath::StringType(), owning_window, nullptr);

  // Because this class returns notifications to the RenderViewHost, it is
  // difficult for callers to know how long to keep a reference to this
  // instance. We AddRef() here to keep the instance alive after we return
  // to the caller, until the last callback is received from the file dialog.
  // At that point, we must call RunFileChooserEnd().
  AddRef();
}

void ShellFileSelectHelper::RunFileChooserEnd() {
  if (listener_) {
    listener_->FileSelectionCanceled();
  }

  select_file_dialog_->ListenerDestroyed();
  select_file_dialog_.reset();
  Release();
}

void ShellFileSelectHelper::FileSelected(const base::FilePath& path,
                                         int index,
                                         void* params) {
  FileSelectedWithExtraInfo(ui::SelectedFileInfo(path, path), index, params);
}

void ShellFileSelectHelper::FileSelectedWithExtraInfo(
    const ui::SelectedFileInfo& file,
    int index,
    void* params) {
  ConvertToFileChooserFileInfoList({file});
}

void ShellFileSelectHelper::MultiFilesSelected(
    const std::vector<base::FilePath>& files,
    void* params) {
  std::vector<ui::SelectedFileInfo> selected_files =
      ui::FilePathListToSelectedFileInfoList(files);

  MultiFilesSelectedWithExtraInfo(selected_files, params);
}

void ShellFileSelectHelper::MultiFilesSelectedWithExtraInfo(
    const std::vector<ui::SelectedFileInfo>& files,
    void* params) {
  ConvertToFileChooserFileInfoList(files);
}

void ShellFileSelectHelper::FileSelectionCanceled(void* params) {
  RunFileChooserEnd();
}

void ShellFileSelectHelper::ConvertToFileChooserFileInfoList(
    const std::vector<ui::SelectedFileInfo>& files) {
  if (!web_contents_) {
    RunFileChooserEnd();
    return;
  }

  std::vector<blink::mojom::FileChooserFileInfoPtr> chooser_files;
  for (const auto& file : files) {
    chooser_files.push_back(blink::mojom::FileChooserFileInfo::NewNativeFile(
        blink::mojom::NativeFileInfo::New(
            file.local_path,
            base::FilePath(file.display_name).AsUTF16Unsafe())));
  }

  listener_->FileSelected(std::move(chooser_files), base::FilePath(),
                          dialog_mode_);
  listener_ = nullptr;

  // No members should be accessed from here on.
  RunFileChooserEnd();
}

}  // namespace content
