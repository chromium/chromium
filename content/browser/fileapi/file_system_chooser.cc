// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fileapi/file_system_chooser.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace content {

namespace {

bool GetFileTypesFromAcceptsOption(
    const blink::mojom::ChooseFileSystemEntryAcceptsOption& option,
    std::vector<base::FilePath::StringType>* extensions,
    base::string16* description) {
  std::set<base::FilePath::StringType> extension_set;

  for (const std::string& extension : option.extensions) {
#if defined(OS_WIN)
    extension_set.insert(base::UTF8ToWide(extension));
#else
    extension_set.insert(extension);
#endif
  }

  for (const std::string& mime_type : option.mime_types) {
    std::vector<base::FilePath::StringType> inner;
    net::GetExtensionsForMimeType(mime_type, &inner);
    if (inner.empty())
      continue;
    extension_set.insert(inner.begin(), inner.end());
  }

  extensions->assign(extension_set.begin(), extension_set.end());

  if (extensions->empty())
    return false;

  *description = option.description;
  return true;
}

void ConvertAcceptsToFileTypeInfo(
    const std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr>& accepts,
    bool include_accepts_all,
    ui::SelectFileDialog::FileTypeInfo* file_types) {
  file_types->include_all_files = include_accepts_all;

  for (const auto& option : accepts) {
    std::vector<base::FilePath::StringType> extensions;
    base::string16 description;

    if (!GetFileTypesFromAcceptsOption(*option, &extensions, &description))
      continue;  // No extensions were found for this option, skip it.

    file_types->extensions.push_back(extensions);
    // FileTypeInfo expects each set of extension to have a corresponding
    // description. A blank description will result in a system generated
    // description to be used.
    file_types->extension_description_overrides.push_back(description);
  }

  if (file_types->extensions.empty())
    file_types->include_all_files = true;
}

}  // namespace

// static
void FileSystemChooser::CreateAndShow(
    int render_process_id,
    int frame_id,
    blink::mojom::ChooseFileSystemEntryType type,
    std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr> accepts,
    bool include_accepts_all,
    ResultCallback callback,
    scoped_refptr<base::TaskRunner> callback_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHost* rfh = RenderFrameHost::FromID(render_process_id, frame_id);
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfh);
  auto* listener = new FileSystemChooser(
      render_process_id, type, std::move(callback), std::move(callback_runner));
  listener->dialog_ = ui::SelectFileDialog::Create(
      listener,
      GetContentClient()->browser()->CreateSelectFilePolicy(web_contents));
  // TODO(https://crbug.com/878581): Better/more specific options to pass to
  //     SelectFile.

  ui::SelectFileDialog::Type dialog_type = ui::SelectFileDialog::SELECT_NONE;
  switch (type) {
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

  ui::SelectFileDialog::FileTypeInfo file_types;
  ConvertAcceptsToFileTypeInfo(accepts, include_accepts_all, &file_types);

  listener->dialog_->SelectFile(
      dialog_type, /*title=*/base::string16(),
      /*default_path=*/base::FilePath(), &file_types,
      /*file_type_index=*/0,
      /*default_extension=*/base::FilePath::StringType(),
      web_contents ? web_contents->GetTopLevelNativeWindow() : nullptr,
      /*params=*/nullptr);
}

FileSystemChooser::FileSystemChooser(
    int render_process_id,
    blink::mojom::ChooseFileSystemEntryType type,
    ResultCallback callback,
    scoped_refptr<base::TaskRunner> callback_runner)
    : render_process_id_(render_process_id),
      callback_(std::move(callback)),
      callback_runner_(std::move(callback_runner)),
      type_(type) {}

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
  auto* isolated_context = storage::IsolatedContext::GetInstance();
  DCHECK(isolated_context);

  auto* security_policy = ChildProcessSecurityPolicy::GetInstance();
  DCHECK(security_policy);

  std::vector<blink::mojom::FileSystemEntryPtr> result;
  result.reserve(files.size());
  for (const auto& path : files) {
    auto entry = blink::mojom::FileSystemEntry::New();
    entry->file_system_id = isolated_context->RegisterFileSystemForPath(
        storage::kFileSystemTypeNativeLocal, std::string(), path,
        &entry->base_name);

    security_policy->GrantReadFileSystem(render_process_id_,
                                         entry->file_system_id);
    // TODO(https://crbug.com/878585): Don't grant write/modify permissions
    // unless a website already has the appropriate permissions.
    if (type_ == blink::mojom::ChooseFileSystemEntryType::kOpenDirectory) {
      // Only grant Create permissions if we're opening a directory. And
      // just granting Create permissions on top of Write and Delete does not
      // actually grant the same permissions as calling this method.
      security_policy->GrantCreateReadWriteFileSystem(render_process_id_,
                                                      entry->file_system_id);
    } else {
      security_policy->GrantWriteFileSystem(render_process_id_,
                                            entry->file_system_id);
      security_policy->GrantDeleteFromFileSystem(render_process_id_,
                                                 entry->file_system_id);
    }

    result.push_back(std::move(entry));
  }

  if (type_ == blink::mojom::ChooseFileSystemEntryType::kSaveFile) {
    // Create files if they don't yet exist.
    // TODO(mek): If we change FileSystemFileHandle to be able to represent a
    // file that doesn't exist on disk, we should be able to get rid of this
    // step and make the whole API slightly more robust.
    base::PostTaskWithTraits(
        FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
        base::BindOnce(
            [](const std::vector<base::FilePath>& files,
               std::vector<blink::mojom::FileSystemEntryPtr> result,
               scoped_refptr<base::TaskRunner> callback_runner,
               ResultCallback callback) {
              for (const auto& path : files) {
                // Checking if a path exists, and then creating it if it doesn't
                // is of course racy, but external applications could just as
                // well be deleting the entire directory, or similar problematic
                // cases, so no matter what we do there are always going to be
                // race conditions and websites will just have to deal with any
                // method being able to fail in unexpected ways.
                if (base::PathExists(path))
                  continue;
                int creation_flags =
                    base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ;
                base::File file(path, creation_flags);

                if (!file.IsValid()) {
                  callback_runner->PostTask(
                      FROM_HERE,
                      base::BindOnce(
                          std::move(callback), base::File::FILE_ERROR_FAILED,
                          std::vector<blink::mojom::FileSystemEntryPtr>()));
                  return;
                }
              }
              callback_runner->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(callback), base::File::FILE_OK,
                                 std::move(result)));
            },
            files, std::move(result), callback_runner_, std::move(callback_)));
    delete this;
    return;
  }
  callback_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), base::File::FILE_OK,
                                std::move(result)));
  delete this;
}

void FileSystemChooser::FileSelectionCanceled(void* params) {
  callback_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), base::File::FILE_ERROR_ABORT,
                     std::vector<blink::mojom::FileSystemEntryPtr>()));
  delete this;
}

}  // namespace content
