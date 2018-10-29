// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/url_downloader_factory.h"

#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_url_loader_factory_getter.h"
#include "content/browser/download/download_request_core.h"
#include "content/browser/download/url_downloader.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {

UrlDownloaderFactory::UrlDownloaderFactory() = default;

UrlDownloaderFactory::~UrlDownloaderFactory() = default;

download::UrlDownloadHandler::UniqueUrlDownloadHandlerPtr
UrlDownloaderFactory::CreateUrlDownloadHandler(
    std::unique_ptr<download::DownloadUrlParameters> params,
    base::WeakPtr<download::UrlDownloadHandler::Delegate> delegate,
    scoped_refptr<download::DownloadURLLoaderFactoryGetter>
        url_loader_factory_getter,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  std::unique_ptr<net::URLRequest> url_request =
      DownloadRequestCore::CreateRequestOnIOThread(
          true, params.get(), std::move(url_request_context_getter));

  return download::UrlDownloadHandler::UniqueUrlDownloadHandlerPtr(
      UrlDownloader::BeginDownload(delegate, std::move(url_request),
                                   params.get(), true)
          .release(),
      base::OnTaskRunnerDeleter(base::ThreadTaskRunnerHandle::Get()));
}

}  // namespace content
