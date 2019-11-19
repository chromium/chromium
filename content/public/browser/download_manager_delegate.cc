// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/download_manager_delegate.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/download/public/common/download_item.h"

namespace content {

void DownloadManagerDelegate::GetNextId(const DownloadIdCallback& callback) {
  callback.Run(download::DownloadItem::kInvalidId);
}

bool DownloadManagerDelegate::DetermineDownloadTarget(
    download::DownloadItem* item,
    const DownloadTargetCallback& callback) {
  return false;
}

bool DownloadManagerDelegate::ShouldOpenFileBasedOnExtension(
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
    const DownloadOpenDelayedCallback& callback) {
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

bool DownloadManagerDelegate::IsMostRecentDownloadItemAtFilePath(
    download::DownloadItem* download) {
  return true;
}

std::string DownloadManagerDelegate::ApplicationClientIdForFileScanning() {
  return std::string();
}

void DownloadManagerDelegate::CheckDownloadAllowed(
    const WebContents::Getter& web_contents_getter,
    const GURL& url,
    const std::string& request_method,
    base::Optional<url::Origin> request_initiator,
    CheckDownloadAllowedCallback check_download_allowed_cb) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(check_download_allowed_cb), true));
}

download::QuarantineConnectionCallback
DownloadManagerDelegate::GetQuarantineConnectionCallback() {
  return base::NullCallback();
}

DownloadManagerDelegate::~DownloadManagerDelegate() {}

}  // namespace content
