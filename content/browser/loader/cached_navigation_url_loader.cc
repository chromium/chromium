// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/cached_navigation_url_loader.h"

#include "content/browser/loader/navigation_early_hints_manager.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"

namespace content {

CachedNavigationURLLoader::CachedNavigationURLLoader(
    std::unique_ptr<NavigationRequestInfo> request_info,
    NavigationURLLoaderDelegate* delegate,
    network::mojom::URLResponseHeadPtr cached_response_head)
    : request_info_(std::move(request_info)),
      delegate_(delegate),
      cached_response_head_(std::move(cached_response_head)) {
  // Respond with a fake response. We use PostTask here to mimic the flow of
  // a normal navigation.
  //
  // Normal navigations never call OnResponseStarted on the same message loop
  // iteration that the NavigationURLLoader is created, because they have to
  // make a network request.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CachedNavigationURLLoader::OnResponseStarted,
                                weak_factory_.GetWeakPtr()));
}

void CachedNavigationURLLoader::OnResponseStarted() {
  GlobalRequestID global_id = GlobalRequestID::MakeBrowserInitiated();

  DCHECK(cached_response_head_);
  delegate_->OnResponseStarted(
      /*url_loader_client_endpoints=*/nullptr, std::move(cached_response_head_),
      /*response_body=*/mojo::ScopedDataPipeConsumerHandle(), global_id,
      /*is_download=*/false, blink::NavigationDownloadPolicy(),
      request_info_->isolation_info.network_isolation_key(), absl::nullopt,
      /*early_hints=*/{});
}
CachedNavigationURLLoader::~CachedNavigationURLLoader() {}

// static
std::unique_ptr<NavigationURLLoader> CachedNavigationURLLoader::Create(
    std::unique_ptr<NavigationRequestInfo> request_info,
    NavigationURLLoaderDelegate* delegate,
    network::mojom::URLResponseHeadPtr cached_response_head) {
  return std::make_unique<CachedNavigationURLLoader>(
      std::move(request_info), delegate, std::move(cached_response_head));
}

void CachedNavigationURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    blink::PreviewsState new_previews_state) {
  NOTREACHED();
}

}  // namespace content
