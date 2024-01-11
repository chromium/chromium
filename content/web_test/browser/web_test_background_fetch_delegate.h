// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_BACKGROUND_FETCH_DELEGATE_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_BACKGROUND_FETCH_DELEGATE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "components/download/public/background_service/client.h"
#include "content/public/browser/background_fetch_delegate.h"

class SimpleFactoryKey;

namespace download {
class BackgroundDownloadService;
}  // namespace download

namespace content {

class BrowserContext;

class WebTestBackgroundFetchDelegate : public BackgroundFetchDelegate {
 public:
  explicit WebTestBackgroundFetchDelegate(BrowserContext* browser_context);

  WebTestBackgroundFetchDelegate(const WebTestBackgroundFetchDelegate&) =
      delete;
  WebTestBackgroundFetchDelegate& operator=(
      const WebTestBackgroundFetchDelegate&) = delete;

  ~WebTestBackgroundFetchDelegate() override;

  // BackgroundFetchDelegate implementation:
  void GetIconDisplaySize(GetIconDisplaySizeCallback callback) override;
  void CreateDownloadJob(
      base::WeakPtr<Client> client,
      std::unique_ptr<BackgroundFetchDescription> fetch_description) override;
  void DownloadUrl(const std::string& job_unique_id,
                   const std::string& download_guid,
                   const std::string& method,
                   const GURL& url,
                   ::network::mojom::CredentialsMode credentials_mode,
                   const net::NetworkTrafficAnnotationTag& traffic_annotation,
                   const net::HttpRequestHeaders& headers,
                   bool has_request_body) override;
  void Abort(const std::string& job_unique_id) override;
  void MarkJobComplete(const std::string& job_unique_id) override;
  void UpdateUI(const std::string& job_unique_id,
                const std::optional<std::string>& title,
                const std::optional<SkBitmap>& icon) override;

 private:
  class WebTestBackgroundFetchDownloadClient;

  raw_ptr<BrowserContext> browser_context_;
  std::unique_ptr<SimpleFactoryKey> simple_factory_key_;

  // In-memory instance of the Download Service lazily created by the delegate.
  std::unique_ptr<download::BackgroundDownloadService> download_service_;

  // Weak reference to an instance of our download client.
  raw_ptr<WebTestBackgroundFetchDownloadClient> background_fetch_client_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_BACKGROUND_FETCH_DELEGATE_H_
