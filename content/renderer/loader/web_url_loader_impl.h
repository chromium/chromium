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
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/platform/scheduler/web_resource_loading_task_runner_handle.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"

namespace network {
struct ResourceResponseHead;
}

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
          task_runner_handle) override;

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
          task_runner_handle,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      mojo::PendingRemote<mojom::KeepAliveHandle> keep_alive_handle);
  ~WebURLLoaderImpl() override;

  static void PopulateURLResponse(const blink::WebURL& url,
                                  const network::ResourceResponseHead& head,
                                  blink::WebURLResponse* response,
                                  bool report_security_info,
                                  int request_id);
  static void PopulateURLResponse(const blink::WebURL& url,
                                  const network::mojom::URLResponseHead& head,
                                  blink::WebURLResponse* response,
                                  bool report_security_info,
                                  int request_id);
  static blink::WebURLError PopulateURLError(
      const network::URLLoaderCompletionStatus& status,
      const GURL& url);

  // WebURLLoader methods:
  void LoadSynchronously(const blink::WebURLRequest& request,
                         blink::WebURLLoaderClient* client,
                         blink::WebURLResponse& response,
                         base::Optional<blink::WebURLError>& error,
                         blink::WebData& data,
                         int64_t& encoded_data_length,
                         int64_t& encoded_body_length,
                         blink::WebBlobInfo& downloaded_blob) override;
  void LoadAsynchronously(const blink::WebURLRequest& request,
                          blink::WebURLLoaderClient* client) override;
  void SetDefersLoading(bool value) override;
  void DidChangePriority(blink::WebURLRequest::Priority new_priority,
                         int intra_priority_value) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override;

 private:
  class Context;
  class RequestPeerImpl;
  class SinkPeer;

  void Cancel();

  scoped_refptr<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(WebURLLoaderImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_WEB_URL_LOADER_IMPL_H_
