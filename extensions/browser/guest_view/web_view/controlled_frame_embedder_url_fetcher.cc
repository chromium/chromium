// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/controlled_frame_embedder_url_fetcher.h"

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extensions_browser_client.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace extensions {

ControlledFrameEmbedderURLFetcher::ControlledFrameEmbedderURLFetcher(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    ControlledFrameEmbedderLoadFileCallback callback)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      url_(url),
      callback_(std::move(callback)) {}

ControlledFrameEmbedderURLFetcher::~ControlledFrameEmbedderURLFetcher() =
    default;

void ControlledFrameEmbedderURLFetcher::Start() {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host) {
    std::move(callback_).Run(false, nullptr);
    return;
  }

  // Get ExtensionBrowserClient instance
  mojo::Remote<network::mojom::URLLoaderFactory> factory(
      extensions::ExtensionsBrowserClient::Get()
          ->GetControlledFrameEmbedderURLLoader(
              render_frame_host->GetLastCommittedOrigin(),
              render_frame_host->GetFrameTreeNodeId(),
              render_frame_host->GetBrowserContext()));

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "controlled_frame_content_scripts_download", R"(
        semantics {
          sender: "Controlled Frame"
          description:
            "When a Controlled Frame is embedded within an embedder, it needs "
            "to fetch the embedder's content scripts from Chromium's network "
            "stack for its content scripts injection API."
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
      factory.get(),
      base::BindOnce(&ControlledFrameEmbedderURLFetcher::OnURLLoaderComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ControlledFrameEmbedderURLFetcher::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = 0;
  if (fetcher_->ResponseInfo() && fetcher_->ResponseInfo()->headers) {
    response_code = fetcher_->ResponseInfo()->headers->response_code();
  }

  fetcher_.reset();
  std::unique_ptr<std::string> data;
  if (response_body) {
    data = std::move(response_body);
  } else {
    data = std::make_unique<std::string>();
  }
  std::move(callback_).Run(response_code == 200, std::move(data));
}

}  // namespace extensions
