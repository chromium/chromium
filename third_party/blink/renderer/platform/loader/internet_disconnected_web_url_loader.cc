// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/internet_disconnected_web_url_loader.h"

#include "base/bind.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/scheduler/web_resource_loading_task_runner_handle.h"
#include "third_party/blink/public/platform/web_back_forward_cache_loader_helper.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

std::unique_ptr<WebURLLoader>
InternetDisconnectedWebURLLoaderFactory::CreateURLLoader(
    const WebURLRequest&,
    std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>
        freezable_task_runner_handle,
    std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>
        unfreezable_task_runner_handle,
    CrossVariantMojoRemote<blink::mojom::KeepAliveHandleInterfaceBase>
        keep_alive_handle,
    WebBackForwardCacheLoaderHelper back_forward_cache_loader_helper) {
  DCHECK(freezable_task_runner_handle);
  return std::make_unique<InternetDisconnectedWebURLLoader>(
      std::move(freezable_task_runner_handle));
}

InternetDisconnectedWebURLLoader::InternetDisconnectedWebURLLoader(
    std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>
        freezable_task_runner_handle)
    : task_runner_handle_(std::move(freezable_task_runner_handle)) {}

InternetDisconnectedWebURLLoader::~InternetDisconnectedWebURLLoader() = default;

void InternetDisconnectedWebURLLoader::LoadSynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
    bool pass_response_pipe_to_client,
    bool no_mime_sniffing,
    base::TimeDelta timeout_interval,
    WebURLLoaderClient*,
    WebURLResponse&,
    absl::optional<WebURLError>&,
    WebData&,
    int64_t& encoded_data_length,
    int64_t& encoded_body_length,
    WebBlobInfo& downloaded_blob,
    std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper) {
  NOTREACHED();
}

void InternetDisconnectedWebURLLoader::LoadAsynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
    bool no_mime_sniffing,
    std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper,
    WebURLLoaderClient* client) {
  DCHECK(task_runner_handle_);
  task_runner_handle_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &InternetDisconnectedWebURLLoader::DidFail,
          weak_factory_.GetWeakPtr(),
          // It is safe to use Unretained(client), because |client| is a
          // ResourceLoader which owns |this|, and we are binding with weak ptr
          // of |this| here.
          base::Unretained(client),
          WebURLError(net::ERR_INTERNET_DISCONNECTED, KURL(request->url))));
}

void InternetDisconnectedWebURLLoader::Freeze(WebLoaderFreezeMode) {}

void InternetDisconnectedWebURLLoader::DidChangePriority(
    WebURLRequest::Priority,
    int) {}

void InternetDisconnectedWebURLLoader::DidFail(WebURLLoaderClient* client,
                                               const WebURLError& error) {
  DCHECK(client);
  client->DidFail(
      error, base::TimeTicks::Now(), /*total_encoded_data_length=*/0,
      /*total_encoded_body_length=*/0, /*total_decoded_body_length=*/0);
}

scoped_refptr<base::SingleThreadTaskRunner>
InternetDisconnectedWebURLLoader::GetTaskRunnerForBodyLoader() {
  return task_runner_handle_->GetTaskRunner();
}

}  // namespace blink
