// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/devtools_download_manager_delegate.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "content/browser/devtools/protocol/devtools_download_manager_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "net/base/filename_util.h"

namespace content {

class WebContents;

namespace protocol {

namespace {

DevToolsDownloadManagerDelegate* g_devtools_manager_delegate = nullptr;

}  // namespace

DevToolsDownloadManagerDelegate::DevToolsDownloadManagerDelegate()
    : download_manager_(nullptr), proxy_download_delegate_(nullptr) {
  g_devtools_manager_delegate = this;
}

// static
DevToolsDownloadManagerDelegate*
DevToolsDownloadManagerDelegate::GetInstance() {
  if (!g_devtools_manager_delegate)
    new DevToolsDownloadManagerDelegate();

  return g_devtools_manager_delegate;
}

DevToolsDownloadManagerDelegate::~DevToolsDownloadManagerDelegate() {
  // Reset the proxy delegate.
  DevToolsDownloadManagerDelegate* download_delegate = GetInstance();
  download_delegate->download_manager_->SetDelegate(
      download_delegate->proxy_download_delegate_);
  download_delegate->download_manager_ = nullptr;

  if (download_manager_) {
    download_manager_->SetDelegate(proxy_download_delegate_);
    download_manager_ = nullptr;
  }
  g_devtools_manager_delegate = nullptr;
}

scoped_refptr<DevToolsDownloadManagerDelegate>
DevToolsDownloadManagerDelegate::TakeOver(
    content::DownloadManager* download_manager) {
  CHECK(download_manager);
  DevToolsDownloadManagerDelegate* download_delegate = GetInstance();
  if (download_manager == download_delegate->download_manager_)
    return download_delegate;
  // Recover state of previously owned download manager.
  if (download_delegate->download_manager_)
    download_delegate->download_manager_->SetDelegate(
        download_delegate->proxy_download_delegate_);
  download_delegate->proxy_download_delegate_ = download_manager->GetDelegate();
  // Take over delegate in download_manager.
  download_delegate->download_manager_ = download_manager;
  download_manager->SetDelegate(download_delegate);
  return download_delegate;
}

void DevToolsDownloadManagerDelegate::Shutdown() {
  if (proxy_download_delegate_)
    proxy_download_delegate_->Shutdown();
  // Revoke any pending callbacks. download_manager_ et. al. are no longer safe
  // to access after this point.
  download_manager_ = nullptr;
}

bool DevToolsDownloadManagerDelegate::DetermineDownloadTarget(
    download::DownloadItem* item,
    const content::DownloadTargetCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DevToolsDownloadManagerHelper* download_helper =
      DevToolsDownloadManagerHelper::FromWebContents(
          DownloadItemUtils::GetWebContents(item));

  // Check if we should failback to delegate.
  if (proxy_download_delegate_ && !download_helper)
    return proxy_download_delegate_->DetermineDownloadTarget(item, callback);

  // In headless mode there's no no proxy delegate set, so if there's no
  // information associated to the download, we deny it by default.
  if (!download_helper ||
      download_helper->GetDownloadBehavior() !=
          DevToolsDownloadManagerHelper::DownloadBehavior::ALLOW) {
    base::FilePath empty_path = base::FilePath();
    callback.Run(empty_path,
                 download::DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                 download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, empty_path,
                 download::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED);
    return true;
  }

  base::FilePath download_path =
      base::FilePath::FromUTF8Unsafe(download_helper->GetDownloadPath());

  FilenameDeterminedCallback filename_determined_callback =
      base::Bind(&DevToolsDownloadManagerDelegate::OnDownloadPathGenerated,
                 base::Unretained(this), item->GetId(), callback);

  PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&DevToolsDownloadManagerDelegate::GenerateFilename,
                     item->GetURL(), item->GetContentDisposition(),
                     item->GetSuggestedFilename(), item->GetMimeType(),
                     download_path, std::move(filename_determined_callback)));
  return true;
}

bool DevToolsDownloadManagerDelegate::ShouldOpenDownload(
    download::DownloadItem* item,
    const content::DownloadOpenDelayedCallback& callback) {
  DevToolsDownloadManagerHelper* download_helper =
      DevToolsDownloadManagerHelper::FromWebContents(
          DownloadItemUtils::GetWebContents(item));

  if (download_helper)
    return true;
  if (proxy_download_delegate_)
    return proxy_download_delegate_->ShouldOpenDownload(item, callback);
  return false;
}

void DevToolsDownloadManagerDelegate::GetNextId(
    const content::DownloadIdCallback& callback) {
  static uint32_t next_id = download::DownloadItem::kInvalidId + 1;
  // Be sure to follow the proxy delegate Ids to avoid compatibility problems
  // with the download manager.
  if (proxy_download_delegate_) {
    proxy_download_delegate_->GetNextId(callback);
    return;
  }
  callback.Run(next_id++);
}

// static
void DevToolsDownloadManagerDelegate::GenerateFilename(
    const GURL& url,
    const std::string& content_disposition,
    const std::string& suggested_filename,
    const std::string& mime_type,
    const base::FilePath& suggested_directory,
    const FilenameDeterminedCallback& callback) {
  base::FilePath generated_name =
      net::GenerateFileName(url, content_disposition, std::string(),
                            suggested_filename, mime_type, "download");

  if (!base::PathExists(suggested_directory))
    base::CreateDirectory(suggested_directory);

  base::FilePath suggested_path(suggested_directory.Append(generated_name));
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(callback, suggested_path));
}

void DevToolsDownloadManagerDelegate::OnDownloadPathGenerated(
    uint32_t download_id,
    const content::DownloadTargetCallback& callback,
    const base::FilePath& suggested_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  callback.Run(suggested_path,
               download::DownloadItem::TARGET_DISPOSITION_OVERWRITE,
               download::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
               suggested_path.AddExtension(FILE_PATH_LITERAL(".crdownload")),
               download::DOWNLOAD_INTERRUPT_REASON_NONE);
}

}  // namespace protocol
}  // namespace content
