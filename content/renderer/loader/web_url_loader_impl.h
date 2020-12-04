// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_WEB_URL_LOADER_IMPL_H_
#define CONTENT_RENDERER_LOADER_WEB_URL_LOADER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/common/frame.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/platform/scheduler/web_resource_loading_task_runner_handle.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"

namespace blink {
class ResourceLoadInfoNotifierWrapper;
class WebURLRequestExtraData;
}  // namespace blink

namespace content {

class ResourceDispatcher;

// Default implementation of WebURLLoaderFactory.
class CONTENT_EXPORT WebURLLoaderFactoryImpl
    : public blink::WebURLLoaderFactory {
 public:
  WebURLLoaderFactoryImpl(
      base::WeakPtr<ResourceDispatcher> resource_dispatcher,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory);
  ~WebURLLoaderFactoryImpl() override;

  std::unique_ptr<blink::WebURLLoader> CreateURLLoader(
      const blink::WebURLRequest& request,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
          freezable_task_runner_handle,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
          unfreezable_task_runner_handle) override;

 private:
  base::WeakPtr<ResourceDispatcher> resource_dispatcher_;
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;
  DISALLOW_COPY_AND_ASSIGN(WebURLLoaderFactoryImpl);
};

class CONTENT_EXPORT WebURLLoaderImpl : public blink::WebURLLoader {
 public:
  // When non-null |keep_alive_handle| is specified, this loader prolongs
  // this render process's lifetime.
  WebURLLoaderImpl(
      ResourceDispatcher* resource_dispatcher,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
          freezable_task_runner_handle,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
          unfreezable_task_runner_handle,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      mojo::PendingRemote<mojom::KeepAliveHandle> keep_alive_handle);
  ~WebURLLoaderImpl() override;

  static void PopulateURLResponse(const blink::WebURL& url,
                                  const network::mojom::URLResponseHead& head,
                                  blink::WebURLResponse* response,
                                  bool report_security_info,
                                  int request_id);
  static blink::WebURLError PopulateURLError(
      const network::URLLoaderCompletionStatus& status,
      const GURL& url);

  // WebURLLoader methods:
  void LoadSynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<blink::WebURLRequestExtraData> url_request_extra_data,
      int requestor_id,
      bool pass_response_pipe_to_client,
      bool no_mime_sniffing,
      base::TimeDelta timeout_interval,
      blink::WebURLLoaderClient* client,
      blink::WebURLResponse& response,
      base::Optional<blink::WebURLError>& error,
      blink::WebData& data,
      int64_t& encoded_data_length,
      int64_t& encoded_body_length,
      blink::WebBlobInfo& downloaded_blob,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper) override;
  void LoadAsynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<blink::WebURLRequestExtraData> url_request_extra_data,
      int requestor_id,
      bool no_mime_sniffing,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      blink::WebURLLoaderClient* client) override;
  void SetDefersLoading(DeferType value) override;
  void DidChangePriority(blink::WebURLRequest::Priority new_priority,
                         int intra_priority_value) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForBodyLoader()
      override;

 private:
  class Context;
  class RequestPeerImpl;

  void Cancel();

  scoped_refptr<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(WebURLLoaderImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_WEB_URL_LOADER_IMPL_H_
