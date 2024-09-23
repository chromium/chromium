// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_download_manager_delegate.h"

#include <algorithm>
#include <string>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "components/download/public/common/download_target_info.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <commdlg.h>
#endif

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/common/shell_switches.h"
#include "net/base/filename_util.h"

#if BUILDFLAG(IS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

namespace content {

ShellDownloadManagerDelegate::ShellDownloadManagerDelegate()
    : download_manager_(nullptr), suppress_prompting_(false) {}

ShellDownloadManagerDelegate::~ShellDownloadManagerDelegate() {
  if (download_manager_) {
    download_manager_->SetDelegate(nullptr);
    download_manager_ = nullptr;
  }
}


void ShellDownloadManagerDelegate::SetDownloadManager(
    DownloadManager* download_manager) {
  download_manager_ = download_manager;
}

void ShellDownloadManagerDelegate::Shutdown() {
  // Revoke any pending callbacks. download_manager_ et. al. are no longer safe
  // to access after this point.
  weak_ptr_factory_.InvalidateWeakPtrs();
  download_manager_ = nullptr;
}

bool ShellDownloadManagerDelegate::DetermineDownloadTarget(
    download::DownloadItem* download,
    download::DownloadTargetCallback* callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // This assignment needs to be here because even at the call to
  // SetDownloadManager, the system is not fully initialized.
  if (default_download_path_.empty()) {
    default_download_path_ = download_manager_->GetBrowserContext()
                                 ->GetPath()
#if BUILDFLAG(IS_CHROMEOS)
                                 .Append(FILE_PATH_LITERAL("MyFiles"))
#endif
                                 .Append(FILE_PATH_LITERAL("Downloads"));
  }

  if (!download->GetForcedFilePath().empty()) {
    download::DownloadTargetInfo target_info;
    target_info.target_path = download->GetForcedFilePath();
    target_info.intermediate_path = download->GetForcedFilePath();

    std::move(*callback).Run(std::move(target_info));
    return true;
  }

  FilenameDeterminedCallback filename_determined_callback = base::BindOnce(
      &ShellDownloadManagerDelegate::OnDownloadPathGenerated,
      weak_ptr_factory_.GetWeakPtr(), download->GetId(), std::move(*callback));

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ShellDownloadManagerDelegate::GenerateFilename,
                     download->GetURL(), download->GetContentDisposition(),
                     download->GetSuggestedFilename(), download->GetMimeType(),
                     default_download_path_,
                     std::move(filename_determined_callback)));
  return true;
}

bool ShellDownloadManagerDelegate::ShouldOpenDownload(
    download::DownloadItem* item,
    DownloadOpenDelayedCallback callback) {
  return true;
}

void ShellDownloadManagerDelegate::GetNextId(DownloadIdCallback callback) {
  static uint32_t next_id = download::DownloadItem::kInvalidId + 1;
  std::move(callback).Run(next_id++);
}

// static
void ShellDownloadManagerDelegate::GenerateFilename(
    const GURL& url,
    const std::string& content_disposition,
    const std::string& suggested_filename,
    const std::string& mime_type,
    const base::FilePath& suggested_directory,
    FilenameDeterminedCallback callback) {
  base::FilePath generated_name = net::GenerateFileName(url,
                                                        content_disposition,
                                                        std::string(),
                                                        suggested_filename,
                                                        mime_type,
                                                        "download");

  if (!base::PathExists(suggested_directory))
    base::CreateDirectory(suggested_directory);

  base::FilePath suggested_path(suggested_directory.Append(generated_name));
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), suggested_path));
}

void ShellDownloadManagerDelegate::OnDownloadPathGenerated(
    uint32_t download_id,
    download::DownloadTargetCallback callback,
    const base::FilePath& suggested_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (suppress_prompting_) {
    // Testing exit.
    download::DownloadTargetInfo target_info;
    target_info.target_path = suggested_path;
    target_info.intermediate_path =
        suggested_path.AddExtension(FILE_PATH_LITERAL(".crdownload"));

    std::move(callback).Run(std::move(target_info));
    return;
  }

  ChooseDownloadPath(download_id, std::move(callback), suggested_path);
}

void ShellDownloadManagerDelegate::ChooseDownloadPath(
    uint32_t download_id,
    download::DownloadTargetCallback callback,
    const base::FilePath& suggested_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  download::DownloadItem* item = download_manager_->GetDownload(download_id);
  if (!item || (item->GetState() != download::DownloadItem::IN_PROGRESS))
    return;

  base::FilePath result;
#if BUILDFLAG(IS_WIN)
  std::wstring file_part = base::FilePath(suggested_path).BaseName().value();
  wchar_t file_name[MAX_PATH];
  base::wcslcpy(file_name, file_part.c_str(), std::size(file_name));
  OPENFILENAME save_as;
  ZeroMemory(&save_as, sizeof(save_as));
  save_as.lStructSize = sizeof(OPENFILENAME);
  WebContents* web_contents = DownloadItemUtils::GetWebContents(item);
  // |web_contents| could be null if the tab was quickly closed.
  if (!web_contents)
    return;
  save_as.hwndOwner =
      web_contents->GetNativeView()->GetHost()->GetAcceleratedWidget();
  save_as.lpstrFile = file_name;
  save_as.nMaxFile = std::size(file_name);

  std::wstring directory;
  if (!suggested_path.empty())
    directory = suggested_path.DirName().value();

  save_as.lpstrInitialDir = directory.c_str();
  save_as.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER | OFN_ENABLESIZING |
                  OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;

  if (GetSaveFileName(&save_as))
    result = base::FilePath(std::wstring(save_as.lpstrFile));
#else
  NOTIMPLEMENTED();
#endif

  download::DownloadTargetInfo target_info;
  target_info.target_path = result;
  target_info.intermediate_path = result;
  target_info.target_disposition =
      download::DownloadItem::TARGET_DISPOSITION_PROMPT;

  std::move(callback).Run(std::move(target_info));
}

void ShellDownloadManagerDelegate::SetDownloadBehaviorForTesting(
    const base::FilePath& default_download_path) {
  default_download_path_ = default_download_path;
  suppress_prompting_ = true;
}

}  // namespace content
