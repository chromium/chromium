// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/download_manager_delegate.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {

void DownloadManagerDelegate::GetNextId(DownloadIdCallback callback) {
  std::move(callback).Run(download::DownloadItem::kInvalidId);
}

bool DownloadManagerDelegate::DetermineDownloadTarget(
    download::DownloadItem* item,
    DownloadTargetCallback* callback) {
  return false;
}

bool DownloadManagerDelegate::ShouldAutomaticallyOpenFile(
    const GURL& url,
    const base::FilePath& path) {
  return false;
}

bool DownloadManagerDelegate::ShouldAutomaticallyOpenFileByPolicy(
    const GURL& url,
    const base::FilePath& path) {
  return false;
}

bool DownloadManagerDelegate::ShouldCompleteDownload(
    download::DownloadItem* item,
    base::OnceClosure callback) {
  return true;
}

bool DownloadManagerDelegate::ShouldOpenDownload(
    download::DownloadItem* item,
    DownloadOpenDelayedCallback callback) {
  return true;
}

bool DownloadManagerDelegate::InterceptDownloadIfApplicable(
    const GURL& url,
    const std::string& user_agent,
    const std::string& content_disposition,
    const std::string& mime_type,
    const std::string& request_origin,
    int64_t content_length,
    bool is_transient,
    WebContents* web_contents) {
  return false;
}

std::string DownloadManagerDelegate::ApplicationClientIdForFileScanning() {
  return std::string();
}

void DownloadManagerDelegate::CheckDownloadAllowed(
    const WebContents::Getter& web_contents_getter,
    const GURL& url,
    const std::string& request_method,
    absl::optional<url::Origin> request_initiator,
    bool from_download_cross_origin_redirect,
    bool content_initiated,
    CheckDownloadAllowedCallback check_download_allowed_cb) {
  // TODO: Do this directly, if it doesn't crash.

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const WebContents::Getter& web_contents_getter, const GURL& url,
             const std::string& request_method,
             CheckDownloadAllowedCallback check_download_allowed_cb) {
            WebContents* contents = web_contents_getter.Run();
            if (!contents) {
              // The contents was closed.
              std::move(check_download_allowed_cb).Run(false);
              return;
            }

            WebContentsDelegate* delegate = contents->GetDelegate();
            if (!delegate) {
              // The default behavior is to allow it.
              std::move(check_download_allowed_cb).Run(true);
              return;
            }

            delegate->CanDownload(url, request_method,
                                  std::move(check_download_allowed_cb));
          },
          web_contents_getter, url, request_method,
          std::move(check_download_allowed_cb)));
}

download::QuarantineConnectionCallback
DownloadManagerDelegate::GetQuarantineConnectionCallback() {
  return base::NullCallback();
}

DownloadManagerDelegate::~DownloadManagerDelegate() {}

std::unique_ptr<download::DownloadItemRenameHandler>
DownloadManagerDelegate::GetRenameHandlerForDownload(
    download::DownloadItem* download_item) {
  return nullptr;
}

download::DownloadItem* DownloadManagerDelegate::GetDownloadByGuid(
    const std::string& guid) {
  return nullptr;
}

void DownloadManagerDelegate::CheckSavePackageAllowed(
    download::DownloadItem* download_item,
    base::flat_map<base::FilePath, base::FilePath> save_package_files,
    SavePackageAllowedCallback callback) {
  std::move(callback).Run(true);
}

}  // namespace content
