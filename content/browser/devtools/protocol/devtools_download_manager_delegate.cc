// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/devtools_download_manager_delegate.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/thread_pool.h"
#include "components/download/public/common/download_target_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "net/base/filename_util.h"

namespace content {

class WebContents;

namespace protocol {

const char kDevToolsDownloadManagerDelegateName[] =
    "devtools_download_manager_delegate";

DevToolsDownloadManagerDelegate::DevToolsDownloadManagerDelegate(
    content::BrowserContext* browser_context) {
  download_manager_ = browser_context->GetDownloadManager();
  DCHECK(download_manager_);
  original_download_delegate_ = download_manager_->GetDelegate();
  download_manager_->SetDelegate(this);
}

// static
DevToolsDownloadManagerDelegate*
DevToolsDownloadManagerDelegate::GetOrCreateInstance(BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!DevToolsDownloadManagerDelegate::GetInstance(context)) {
    auto delegate_owned =
        base::WrapUnique(new DevToolsDownloadManagerDelegate(context));
    context->SetUserData(kDevToolsDownloadManagerDelegateName,
                         std::move(delegate_owned));
  }
  return DevToolsDownloadManagerDelegate::GetInstance(context);
}

// static
DevToolsDownloadManagerDelegate* DevToolsDownloadManagerDelegate::GetInstance(
    BrowserContext* context) {
  return static_cast<DevToolsDownloadManagerDelegate*>(
      context->GetUserData(kDevToolsDownloadManagerDelegateName));
}

void DevToolsDownloadManagerDelegate::Shutdown() {
  if (original_download_delegate_)
    original_download_delegate_.ExtractAsDangling()->Shutdown();
  // Revoke any pending callbacks. download_manager_ et. al. are no longer safe
  // to access after this point.
  download_manager_ = nullptr;
}

bool DevToolsDownloadManagerDelegate::DetermineDownloadTarget(
    download::DownloadItem* item,
    download::DownloadTargetCallback* callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Check if we should failback to delegate.
  if (original_download_delegate_ &&
      download_behavior_ == DownloadBehavior::DEFAULT) {
    return original_download_delegate_->DetermineDownloadTarget(item, callback);
  }

  // In headless mode there's no no proxy delegate set, so if there's no
  // information associated to the download, we deny it by default.
  if (download_behavior_ != DownloadBehavior::ALLOW &&
      download_behavior_ != DownloadBehavior::ALLOW_AND_NAME) {
    download::DownloadTargetInfo target_info;
    target_info.interrupt_reason =
        download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED;

    std::move(*callback).Run(std::move(target_info));
    return true;
  }

  base::FilePath download_path = base::FilePath::FromUTF8Unsafe(download_path_);
  if (download_behavior_ == DownloadBehavior::ALLOW_AND_NAME) {
    base::FilePath suggested_path(download_path.AppendASCII(item->GetGuid()));
    OnDownloadPathGenerated(item->GetId(), std::move(*callback),
                            suggested_path);
    return true;
  }

  FilenameDeterminedCallback filename_determined_callback = base::BindOnce(
      &DevToolsDownloadManagerDelegate::OnDownloadPathGenerated,
      base::Unretained(this), item->GetId(), std::move(*callback));
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&DevToolsDownloadManagerDelegate::GenerateFilename,
                     item->GetURL(), item->GetContentDisposition(),
                     item->GetSuggestedFilename(), item->GetMimeType(),
                     download_path, std::move(filename_determined_callback)));
  return true;
}

bool DevToolsDownloadManagerDelegate::ShouldOpenDownload(
    download::DownloadItem* item,
    content::DownloadOpenDelayedCallback callback) {
  if (original_download_delegate_ &&
      download_behavior_ == DownloadBehavior::DEFAULT) {
    return original_download_delegate_->ShouldOpenDownload(item,
                                                           std::move(callback));
  }
  // Immediately transition to the completed stage.
  return true;
}

void DevToolsDownloadManagerDelegate::GetNextId(
    content::DownloadIdCallback callback) {
  static uint32_t next_id = download::DownloadItem::kInvalidId + 1;
  // Be sure to follow the proxy delegate Ids to avoid compatibility problems
  // with the download manager.
  if (original_download_delegate_) {
    original_download_delegate_->GetNextId(std::move(callback));
    return;
  }
  std::move(callback).Run(next_id++);
}

// static
void DevToolsDownloadManagerDelegate::GenerateFilename(
    const GURL& url,
    const std::string& content_disposition,
    const std::string& suggested_filename,
    const std::string& mime_type,
    const base::FilePath& suggested_directory,
    FilenameDeterminedCallback callback) {
  base::FilePath generated_name =
      net::GenerateFileName(url, content_disposition, std::string(),
                            suggested_filename, mime_type, "download");

  if (!base::PathExists(suggested_directory))
    base::CreateDirectory(suggested_directory);

  base::FilePath suggested_path(suggested_directory.Append(generated_name));
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), suggested_path));
}

void DevToolsDownloadManagerDelegate::OnDownloadPathGenerated(
    uint32_t download_id,
    download::DownloadTargetCallback callback,
    const base::FilePath& suggested_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  download::DownloadTargetInfo target_info;
  target_info.target_path = suggested_path;
  target_info.intermediate_path =
      suggested_path.AddExtension(FILE_PATH_LITERAL(".crdownload"));
  target_info.display_name = suggested_path.BaseName();
  target_info.danger_type =
      download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT;

  std::move(callback).Run(std::move(target_info));
}

download::DownloadItem* DevToolsDownloadManagerDelegate::GetDownloadByGuid(
    const std::string& guid) {
  if (!download_manager_) {
    return nullptr;
  }
  return download_manager_->GetDownloadByGuid(guid);
}

}  // namespace protocol
}  // namespace content
