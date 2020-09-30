// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_chooser.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/file_system_access/native_file_system_error.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "net/base/mime_util.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace content {

namespace {

std::string TypeToString(blink::mojom::ChooseFileSystemEntryType type) {
  switch (type) {
    case blink::mojom::ChooseFileSystemEntryType::kOpenFile:
      return "OpenFile";
    case blink::mojom::ChooseFileSystemEntryType::kOpenMultipleFiles:
      return "OpenMultipleFiles";
    case blink::mojom::ChooseFileSystemEntryType::kSaveFile:
      return "SaveFile";
    case blink::mojom::ChooseFileSystemEntryType::kOpenDirectory:
      return "OpenDirectory";
  }
  NOTREACHED();
  return nullptr;
}

void RecordFileSelectionResult(blink::mojom::ChooseFileSystemEntryType type,
                               int count) {
  base::UmaHistogramCounts1000("NativeFileSystemAPI.FileChooserResult", count);
  base::UmaHistogramCounts1000(
      "NativeFileSystemAPI.FileChooserResult." + TypeToString(type), count);
}

// Converts the accepted mime types and extensions from |option| into a list
// of just extensions to be passed to the file dialog implementation.
// The returned list will start with all the explicit website provided
// extensions in order, followed by (for each mime type) the preferred
// extension for that mime type (if any) and any other extensions associated
// with that mime type. Duplicates are filtered out so each extension only
// occurs once in the returned list.
bool GetFileTypesFromAcceptsOption(
    const blink::mojom::ChooseFileSystemEntryAcceptsOption& option,
    std::vector<base::FilePath::StringType>* extensions,
    base::string16* description) {
  std::set<base::FilePath::StringType> extension_set;

  for (const std::string& extension_string : option.extensions) {
    base::FilePath::StringType extension;
#if defined(OS_WIN)
    extension = base::UTF8ToWide(extension_string);
#else
    extension = extension_string;
#endif
    if (extension_set.insert(extension).second) {
      extensions->push_back(std::move(extension));
    }
  }

  for (const std::string& mime_type : option.mime_types) {
    base::FilePath::StringType preferred_extension;
    if (net::GetPreferredExtensionForMimeType(mime_type,
                                              &preferred_extension)) {
      if (extension_set.insert(preferred_extension).second) {
        extensions->push_back(std::move(preferred_extension));
      }
    }

    std::vector<base::FilePath::StringType> inner;
    net::GetExtensionsForMimeType(mime_type, &inner);
    if (inner.empty())
      continue;
    for (auto& extension : inner) {
      if (extension_set.insert(extension).second) {
        extensions->push_back(std::move(extension));
      }
    }
  }

  if (extensions->empty())
    return false;

  *description = option.description;
  return true;
}

ui::SelectFileDialog::FileTypeInfo ConvertAcceptsToFileTypeInfo(
    const std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr>&
        accepts,
    bool include_accepts_all) {
  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.include_all_files = include_accepts_all;

  for (const auto& option : accepts) {
    std::vector<base::FilePath::StringType> extensions;
    base::string16 description;

    if (!GetFileTypesFromAcceptsOption(*option, &extensions, &description))
      continue;  // No extensions were found for this option, skip it.

    file_types.extensions.push_back(extensions);
    // FileTypeInfo expects each set of extension to have a corresponding
    // description. A blank description will result in a system generated
    // description to be used.
    file_types.extension_description_overrides.push_back(description);
  }

  if (file_types.extensions.empty())
    file_types.include_all_files = true;

  file_types.allowed_paths = ui::SelectFileDialog::FileTypeInfo::ANY_PATH;

  return file_types;
}

}  // namespace

FileSystemChooser::Options::Options(
    blink::mojom::ChooseFileSystemEntryType type,
    std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr> accepts,
    bool include_accepts_all)
    : type_(type),
      file_types_(ConvertAcceptsToFileTypeInfo(accepts, include_accepts_all)),
      default_file_type_index_(file_types_.extensions.empty() ? 0 : 1) {}

// static
void FileSystemChooser::CreateAndShow(
    WebContents* web_contents,
    const Options& options,
    ResultCallback callback,
    base::ScopedClosureRunner fullscreen_block) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* listener = new FileSystemChooser(options.type(), std::move(callback),
                                         std::move(fullscreen_block));
  listener->dialog_ = ui::SelectFileDialog::Create(
      listener,
      GetContentClient()->browser()->CreateSelectFilePolicy(web_contents));

  // In content_shell --run-web-tests, there might be no dialog available. In
  // that case just abort.
  if (!listener->dialog_) {
    listener->FileSelectionCanceled(nullptr);
    return;
  }

  // TODO(https://crbug.com/878581): Better/more specific options to pass to
  //     SelectFile.

  ui::SelectFileDialog::Type dialog_type = ui::SelectFileDialog::SELECT_NONE;
  switch (options.type()) {
    case blink::mojom::ChooseFileSystemEntryType::kOpenFile:
      dialog_type = ui::SelectFileDialog::SELECT_OPEN_FILE;
      break;
    case blink::mojom::ChooseFileSystemEntryType::kOpenMultipleFiles:
      dialog_type = ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE;
      break;
    case blink::mojom::ChooseFileSystemEntryType::kSaveFile:
      dialog_type = ui::SelectFileDialog::SELECT_SAVEAS_FILE;
      break;
    case blink::mojom::ChooseFileSystemEntryType::kOpenDirectory:
      dialog_type = ui::SelectFileDialog::SELECT_FOLDER;
      break;
  }
  DCHECK_NE(dialog_type, ui::SelectFileDialog::SELECT_NONE);

  listener->dialog_->SelectFile(
      dialog_type, /*title=*/base::string16(),
      /*default_path=*/base::FilePath(), &options.file_type_info(),
      options.default_file_type_index(),
      /*default_extension=*/base::FilePath::StringType(),
      web_contents ? web_contents->GetTopLevelNativeWindow() : nullptr,
      /*params=*/nullptr);
}

FileSystemChooser::FileSystemChooser(
    blink::mojom::ChooseFileSystemEntryType type,
    ResultCallback callback,
    base::ScopedClosureRunner fullscreen_block)
    : callback_(std::move(callback)),
      type_(type),
      fullscreen_block_(std::move(fullscreen_block)) {}

FileSystemChooser::~FileSystemChooser() {
  if (dialog_)
    dialog_->ListenerDestroyed();
}

void FileSystemChooser::FileSelected(const base::FilePath& path,
                                     int index,
                                     void* params) {
  MultiFilesSelected({path}, params);
}

void FileSystemChooser::MultiFilesSelected(
    const std::vector<base::FilePath>& files,
    void* params) {
  MultiFilesSelectedWithExtraInfo(ui::FilePathListToSelectedFileInfoList(files),
                                  params);
}

void FileSystemChooser::FileSelectedWithExtraInfo(
    const ui::SelectedFileInfo& file,
    int index,
    void* params) {
  MultiFilesSelectedWithExtraInfo({file}, params);
}

void FileSystemChooser::MultiFilesSelectedWithExtraInfo(
    const std::vector<ui::SelectedFileInfo>& files,
    void* params) {
  std::vector<ResultEntry> result;

  for (const ui::SelectedFileInfo& file : files) {
    if (file.virtual_path.has_value()) {
      result.push_back({PathType::kExternal, *file.virtual_path});
    } else {
      result.push_back({PathType::kLocal, file.local_path.empty()
                                              ? file.file_path
                                              : file.local_path});
    }
  }

  RecordFileSelectionResult(type_, result.size());
  std::move(callback_).Run(native_file_system_error::Ok(), std::move(result));
  delete this;
}

void FileSystemChooser::FileSelectionCanceled(void* params) {
  RecordFileSelectionResult(type_, 0);
  std::move(callback_).Run(
      native_file_system_error::FromStatus(
          blink::mojom::NativeFileSystemStatus::kOperationAborted),
      {});
  delete this;
}

}  // namespace content
