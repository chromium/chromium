// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/web_ui_download_url_loader_factory_getter.h"

#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"

namespace content {

WebUIDownloadURLLoaderFactoryGetter::WebUIDownloadURLLoaderFactoryGetter(
    RenderFrameHost* rfh,
    const GURL& url) {
  auto factory_request = mojo::MakeRequest(&factory_info_);
  factory_ =
      CreateWebUIURLLoader(rfh, url.scheme(), base::flat_set<std::string>());
  factory_->Clone(std::move(factory_request));
}

WebUIDownloadURLLoaderFactoryGetter::~WebUIDownloadURLLoaderFactoryGetter() {
  base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI})
      ->DeleteSoon(FROM_HERE, std::move(factory_));
}

scoped_refptr<network::SharedURLLoaderFactory>
WebUIDownloadURLLoaderFactoryGetter::GetURLLoaderFactory() {
  return base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
      std::move(factory_info_));
}

}  // namespace content
