// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/initial_webui_navigation_url_loader.h"

#include "base/feature_list.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/url_data_source_impl.h"
#include "content/browser/webui/web_ui_data_source_impl.h"
#include "content/common/features.h"
#include "content/common/web_ui_loading_util.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/common/content_client.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"

namespace content {

InitialWebUINavigationURLLoader::InitialWebUINavigationURLLoader(
    BrowserContext* browser_context,
    std::unique_ptr<NavigationRequestInfo> request_info,
    NavigationURLLoaderDelegate* delegate)
    : browser_context_(browser_context),
      request_info_(std::move(request_info)),
      delegate_(delegate) {
  CHECK(base::FeatureList::IsEnabled(
      features::kInitialWebUISyncNavStartToCommit));
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED();
#else
  CHECK(GetContentClient()->browser()->IsInitialWebUIURL(
      request_info_->common_params->url));
#endif
}

InitialWebUINavigationURLLoader::~InitialWebUINavigationURLLoader() = default;

// static
std::unique_ptr<NavigationURLLoader> InitialWebUINavigationURLLoader::Create(
    BrowserContext* browser_context,
    std::unique_ptr<NavigationRequestInfo> request_info,
    NavigationURLLoaderDelegate* delegate) {
  return std::make_unique<InitialWebUINavigationURLLoader>(
      browser_context, std::move(request_info), delegate);
}

void InitialWebUINavigationURLLoader::Start() {
  OnResponseStarted();
}

void InitialWebUINavigationURLLoader::FollowRedirect(
    std::vector<std::string> removed_headers,
    net::HttpRequestHeaders modified_headers,
    net::HttpRequestHeaders modified_cors_exempt_headers) {
  NOTREACHED();
}

void InitialWebUINavigationURLLoader::OnResponseStarted() {
  CHECK(request_info_);
  const GURL& url = request_info_->common_params->url;
  URLDataSourceImpl* source =
      URLDataManagerBackend::GetForBrowserContext(browser_context_)
          ->GetDataSourceFromURL(url);

  // Fill the response head for `url`.
  network::mojom::URLResponseHeadPtr response_head =
      network::mojom::URLResponseHead::New();
  CHECK(source);
  response_head->headers =
      URLDataManagerBackend::GetHeaders(source, url, std::string());
  // Headers from WebUI are trusted, so parsing can happen from a
  // non-sandboxed process.
  response_head->parsed_headers =
      network::PopulateParsedHeaders(response_head->headers.get(), url);
  response_head->mime_type = source->source()->GetMimeType(url);

  // Pass empty client endpoints and body, since the actual values will be set
  // from `RenderFrameImpl::CommitNavigation()` when doing in-renderer body
  // loading. Skipping reading the body on the browser side allows the
  // navigation to run synchronously from navigation start to commit.
  delegate_->OnResponseStarted(
      /*url_loader_client_endpoints=*/nullptr, std::move(response_head),
      /*response_body=*/mojo::ScopedDataPipeConsumerHandle(),
      GlobalRequestID::MakeBrowserInitiated(),
      /*is_download=*/false,
      request_info_->isolation_info.network_anonymization_key(),
      SubresourceLoaderParams(),
      /*early_hints=*/{});
}

bool InitialWebUINavigationURLLoader::SetNavigationTimeout(
    base::TimeDelta timeout) {
  // `false` here means that no timeout was started.
  return false;
}

void InitialWebUINavigationURLLoader::CancelNavigationTimeout() {
  NOTREACHED();
}

}  // namespace content
