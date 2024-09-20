// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/download_manager_delegate.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_item_rename_handler.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {

SavePackagePathPickedParams::SavePackagePathPickedParams() = default;
SavePackagePathPickedParams::~SavePackagePathPickedParams() = default;

SavePackagePathPickedParams::SavePackagePathPickedParams(
    const SavePackagePathPickedParams& other) = default;
SavePackagePathPickedParams& SavePackagePathPickedParams::operator=(
    const SavePackagePathPickedParams& other) = default;
SavePackagePathPickedParams::SavePackagePathPickedParams(
    SavePackagePathPickedParams&& other) = default;
SavePackagePathPickedParams& SavePackagePathPickedParams::operator=(
    SavePackagePathPickedParams&& other) = default;

void DownloadManagerDelegate::GetNextId(DownloadIdCallback callback) {
  std::move(callback).Run(download::DownloadItem::kInvalidId);
}

bool DownloadManagerDelegate::DetermineDownloadTarget(
    download::DownloadItem* item,
    download::DownloadTargetCallback* callback) {
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

bool DownloadManagerDelegate::ShouldObfuscateDownload(
    download::DownloadItem* item) {
  return false;
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
    std::optional<url::Origin> request_initiator,
    bool from_download_cross_origin_redirect,
    bool content_initiated,
    const std::string& mime_type,
    std::optional<ui::PageTransition> page_transition,
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

std::unique_ptr<download::DownloadItemRenameHandler>
DownloadManagerDelegate::GetRenameHandlerForDownload(
    download::DownloadItem* download_item) {
  return nullptr;
}

DownloadManagerDelegate::~DownloadManagerDelegate() {}

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

#if BUILDFLAG(IS_ANDROID)
bool DownloadManagerDelegate::IsFromExternalApp(download::DownloadItem* item) {
  return false;
}

bool DownloadManagerDelegate::ShouldOpenPdfInline() {
  return false;
}

bool DownloadManagerDelegate::IsDownloadRestrictedByPolicy() {
  return false;
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace content
