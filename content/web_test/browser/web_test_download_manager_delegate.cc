// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_download_manager_delegate.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/web_test/browser/web_test_control_host.h"
#include "net/base/filename_util.h"

#if BUILDFLAG(IS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

// windows.h must come before commdlg.h
#include <windows.h>

#include <commdlg.h>
#endif

namespace content {

WebTestDownloadManagerDelegate::WebTestDownloadManagerDelegate()
    : ShellDownloadManagerDelegate() {}

WebTestDownloadManagerDelegate::~WebTestDownloadManagerDelegate() {}

bool WebTestDownloadManagerDelegate::ShouldOpenDownload(
    download::DownloadItem* item,
    DownloadOpenDelayedCallback callback) {
  if (WebTestControlHost::Get() &&
      WebTestControlHost::Get()->IsMainWindow(
          DownloadItemUtils::GetWebContents(item)) &&
      item->GetMimeType() == "text/html") {
    WebTestControlHost::Get()->OpenURL(
        net::FilePathToFileURL(item->GetFullPath()));
  }
  return true;
}

void WebTestDownloadManagerDelegate::CheckDownloadAllowed(
    const content::WebContents::Getter& web_contents_getter,
    const GURL& url,
    const std::string& request_method,
    std::optional<url::Origin> request_initiator,
    bool from_download_cross_origin_redirect,
    bool content_initiated,
    const std::string& mime_type,
    std::optional<ui::PageTransition> page_transition,
    content::CheckDownloadAllowedCallback check_download_allowed_cb) {
  auto* test_controller = WebTestControlHost::Get();
  bool should_wait_until_external_url_load =
      test_controller->web_test_runtime_flags().wait_until_external_url_load();

  // The if clause below catches all calls to this method not
  // initiated by content, or even if it does, whose web_test
  // does not call TestRunner::WaitUntilExternalUrlLoad().
  if (!content_initiated || !should_wait_until_external_url_load) {
    ShellDownloadManagerDelegate::CheckDownloadAllowed(
        web_contents_getter, url, request_method, request_initiator,
        from_download_cross_origin_redirect, content_initiated, mime_type,
        std::move(page_transition), std::move(check_download_allowed_cb));
    return;
  }

  test_controller->printer()->AddMessageRaw("Download started\n");
  static_cast<mojom::WebTestControlHost*>(test_controller)
      ->TestFinishedInSecondaryRenderer();
  std::move(check_download_allowed_cb).Run(false);
}

}  // namespace content
