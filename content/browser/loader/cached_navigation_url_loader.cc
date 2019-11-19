// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/cached_navigation_url_loader.h"

#include "base/task/post_task.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

CachedNavigationURLLoader::CachedNavigationURLLoader(
    std::unique_ptr<NavigationRequestInfo> request_info,
    NavigationURLLoaderDelegate* delegate)
    : request_info_(std::move(request_info)), delegate_(delegate) {
  // Respond with a fake response. We use PostTask here to mimic the flow of
  // a normal navigation.
  //
  // Normal navigations never call OnResponseStarted on the same message loop
  // iteration that the NavigationURLLoader is created, because they have to
  // make a network request.
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&CachedNavigationURLLoader::OnResponseStarted,
                                weak_factory_.GetWeakPtr()));
}

void CachedNavigationURLLoader::OnResponseStarted() {
  auto dummy_response =
      scoped_refptr<network::ResourceResponse>(new network::ResourceResponse);

  GlobalRequestID global_id = NavigationURLLoaderImpl::MakeGlobalRequestID();

  delegate_->OnResponseStarted(
      /*url_loader_client_endpoints=*/nullptr, dummy_response,
      /*response_body=*/mojo::ScopedDataPipeConsumerHandle(), global_id,
      /*is_download=*/false, NavigationDownloadPolicy(), base::nullopt);
}
CachedNavigationURLLoader::~CachedNavigationURLLoader() {}

// static
std::unique_ptr<NavigationURLLoader> CachedNavigationURLLoader::Create(
    std::unique_ptr<NavigationRequestInfo> request_info,
    NavigationURLLoaderDelegate* delegate) {
  return std::make_unique<CachedNavigationURLLoader>(std::move(request_info),
                                                     delegate);
}

void CachedNavigationURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    PreviewsState new_previews_state) {
  NOTREACHED();
}
}  // namespace content
