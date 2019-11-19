// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BACKGROUND_FETCH_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BACKGROUND_FETCH_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "components/download/public/background_service/client.h"
#include "content/public/browser/background_fetch_delegate.h"

class SimpleFactoryKey;

namespace download {
class DownloadService;
}  // namespace download

namespace content {

class BrowserContext;

class WebTestBackgroundFetchDelegate : public BackgroundFetchDelegate {
 public:
  explicit WebTestBackgroundFetchDelegate(BrowserContext* browser_context);
  ~WebTestBackgroundFetchDelegate() override;

  // BackgroundFetchDelegate implementation:
  void GetIconDisplaySize(GetIconDisplaySizeCallback callback) override;
  void GetPermissionForOrigin(const url::Origin& origin,
                              const WebContents::Getter& wc_getter,
                              GetPermissionForOriginCallback callback) override;
  void CreateDownloadJob(
      base::WeakPtr<Client> client,
      std::unique_ptr<BackgroundFetchDescription> fetch_description) override;
  void DownloadUrl(const std::string& job_unique_id,
                   const std::string& download_guid,
                   const std::string& method,
                   const GURL& url,
                   const net::NetworkTrafficAnnotationTag& traffic_annotation,
                   const net::HttpRequestHeaders& headers,
                   bool has_request_body) override;
  void Abort(const std::string& job_unique_id) override;
  void MarkJobComplete(const std::string& job_unique_id) override;
  void UpdateUI(const std::string& job_unique_id,
                const base::Optional<std::string>& title,
                const base::Optional<SkBitmap>& icon) override;

 private:
  class WebTestBackgroundFetchDownloadClient;

  BrowserContext* browser_context_;
  std::unique_ptr<SimpleFactoryKey> simple_factory_key_;

  // In-memory instance of the Download Service lazily created by the delegate.
  std::unique_ptr<download::DownloadService> download_service_;

  // Weak reference to an instance of our download client.
  WebTestBackgroundFetchDownloadClient* background_fetch_client_;

  DISALLOW_COPY_AND_ASSIGN(WebTestBackgroundFetchDelegate);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BACKGROUND_FETCH_DELEGATE_H_
