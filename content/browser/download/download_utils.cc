// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_utils.h"

#include "base/format_macros.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "components/download/database/in_progress/download_entry.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_interrupt_reasons_utils.h"
#include "components/download/public/common/download_save_info.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/download_utils.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/loader/upload_data_stream_builder.h"
#include "content/browser/resource_context_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {

storage::BlobStorageContext* BlobStorageContextGetter(
    ResourceContext* resource_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ChromeBlobStorageContext* blob_context =
      GetChromeBlobStorageContextForResourceContext(resource_context);
  return blob_context->context();
}

std::unique_ptr<net::URLRequest> CreateURLRequestOnIOThread(
    download::DownloadUrlParameters* params,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(params->offset() >= 0);

  // ResourceDispatcherHost{Base} is-not-a URLRequest::Delegate, and
  // download::DownloadUrlParameters can-not include
  // resource_dispatcher_host_impl.h, so we must down cast. RDHI is the only
  // subclass of RDH as of 2012 May 4.
  std::unique_ptr<net::URLRequest> request(
      url_request_context_getter->GetURLRequestContext()->CreateRequest(
          params->url(), net::DEFAULT_PRIORITY, nullptr,
          params->GetNetworkTrafficAnnotation()));
  request->set_method(params->method());

  if (params->post_body()) {
    storage::BlobStorageContext* blob_context =
        params->get_blob_storage_context_getter().Run();
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        base::CreateSingleThreadTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
    std::unique_ptr<net::UploadDataStream> upload_data_stream =
        UploadDataStreamBuilder::Build(params->post_body().get(), blob_context,
                                       nullptr /*FileSystemContext*/,
                                       task_runner.get());
    request->set_upload(std::move(upload_data_stream));
  }

  if (params->post_id() >= 0) {
    // The POST in this case does not have an actual body, and only works
    // when retrieving data from cache. This is done because we don't want
    // to do a re-POST without user consent, and currently don't have a good
    // plan on how to display the UI for that.
    DCHECK(params->prefer_cache());
    DCHECK_EQ("POST", params->method());
    std::vector<std::unique_ptr<net::UploadElementReader>> element_readers;
    request->set_upload(std::make_unique<net::ElementsUploadDataStream>(
        std::move(element_readers), params->post_id()));
  }

  request->SetLoadFlags(download::GetLoadFlags(params, request->get_upload()));

  // Add additional request headers.
  std::unique_ptr<net::HttpRequestHeaders> headers =
      download::GetAdditionalRequestHeaders(params);
  if (!headers->IsEmpty())
    request->SetExtraRequestHeaders(*headers);

  // Downloads are treated as top level navigations. Hence the first-party
  // origin for cookies is always based on the target URL and is updated on
  // redirects.
  request->set_site_for_cookies(params->url());
  request->set_first_party_url_policy(
      net::URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT);
  request->set_initiator(params->initiator());

  return request;
}

}  // namespace content
