// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_DOWNLOAD_MANAGER_DELEGATE_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_DOWNLOAD_MANAGER_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/shell/browser/shell_download_manager_delegate.h"

namespace download {
class DownloadItem;
}

namespace content {

class WebTestDownloadManagerDelegate : public ShellDownloadManagerDelegate {
 public:
  WebTestDownloadManagerDelegate();

  WebTestDownloadManagerDelegate(const WebTestDownloadManagerDelegate&) =
      delete;
  WebTestDownloadManagerDelegate& operator=(
      const WebTestDownloadManagerDelegate&) = delete;

  ~WebTestDownloadManagerDelegate() override;

  // ShellDownloadManagerDelegate implementation.
  bool ShouldOpenDownload(download::DownloadItem* item,
                          DownloadOpenDelayedCallback callback) override;
  void CheckDownloadAllowed(
      const content::WebContents::Getter& web_contents_getter,
      const GURL& url,
      const std::string& request_method,
      std::optional<url::Origin> request_initiator,
      bool from_download_cross_origin_redirect,
      bool content_initiated,
      const std::string& mime_type,
      std::optional<ui::PageTransition> page_transition,
      content::CheckDownloadAllowedCallback check_download_allowed_cb) override;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_DOWNLOAD_MANAGER_DELEGATE_H_
