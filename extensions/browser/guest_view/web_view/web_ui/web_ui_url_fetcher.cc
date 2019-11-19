// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/web_ui/web_ui_url_fetcher.h"

#include "base/bind.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

WebUIURLFetcher::WebUIURLFetcher(int render_process_id,
                                 int render_frame_id,
                                 const GURL& url,
                                 WebUILoadFileCallback callback)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      url_(url),
      callback_(std::move(callback)) {}

WebUIURLFetcher::~WebUIURLFetcher() {
}

void WebUIURLFetcher::Start() {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!rfh) {
    std::move(callback_).Run(false, nullptr);
    return;
  }

  auto factory = content::CreateWebUIURLLoader(rfh, url_.scheme(),
                                               base::flat_set<std::string>());

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("webui_content_scripts_download", R"(
        semantics {
          sender: "WebView"
          description:
            "When a WebView is embedded within a WebUI, it needs to fetch the "
            "embedder's content scripts from Chromium's network stack for its "
            "content scripts injection API."
          trigger: "The content script injection API is called."
          data: "URL of the script file to be downloaded."
          destination: LOCAL
        }
        policy {
          cookies_allowed: NO
          setting: "It is not possible to disable this feature from settings."
          policy_exception_justification:
            "Not Implemented, considered not useful as the request doesn't "
            "go to the network."
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  fetcher_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                              traffic_annotation);
  fetcher_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      factory.get(), base::BindOnce(&WebUIURLFetcher::OnURLLoaderComplete,
                                    base::Unretained(this)));
}

void WebUIURLFetcher::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = 0;
  if (fetcher_->ResponseInfo() && fetcher_->ResponseInfo()->headers)
    response_code = fetcher_->ResponseInfo()->headers->response_code();

  fetcher_.reset();
  std::unique_ptr<std::string> data(new std::string());
  if (response_body)
    data = std::move(response_body);
  std::move(callback_).Run(response_code == 200, std::move(data));
}
