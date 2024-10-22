// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/internet_disconnected_url_loader.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_client.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

std::unique_ptr<URLLoader>
InternetDisconnectedURLLoaderFactory::CreateURLLoader(
    const network::ResourceRequest&,
    scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
    mojo::PendingRemote<mojom::blink::KeepAliveHandle> keep_alive_handle,
    BackForwardCacheLoaderHelper* back_forward_cache_loader_helper,
    Vector<std::unique_ptr<URLLoaderThrottle>> throttles) {
  DCHECK(freezable_task_runner);
  return std::make_unique<InternetDisconnectedURLLoader>(
      std::move(freezable_task_runner));
}

InternetDisconnectedURLLoader::InternetDisconnectedURLLoader(
    scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner)
    : task_runner_(std::move(freezable_task_runner)) {}

InternetDisconnectedURLLoader::~InternetDisconnectedURLLoader() = default;

void InternetDisconnectedURLLoader::LoadSynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<const SecurityOrigin> top_frame_origin,
    bool download_to_blob,
    bool no_mime_sniffing,
    base::TimeDelta timeout_interval,
    URLLoaderClient*,
    WebURLResponse&,
    std::optional<WebURLError>&,
    scoped_refptr<SharedBuffer>&,
    int64_t& encoded_data_length,
    uint64_t& encoded_body_length,
    scoped_refptr<BlobDataHandle>& downloaded_blob,
    std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper) {
  NOTREACHED_IN_MIGRATION();
}

void InternetDisconnectedURLLoader::LoadAsynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    scoped_refptr<const SecurityOrigin> top_frame_origin,
    bool no_mime_sniffing,
    std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper,
    CodeCacheHost* code_cache_host,
    URLLoaderClient* client) {
  DCHECK(task_runner_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &InternetDisconnectedURLLoader::DidFail, weak_factory_.GetWeakPtr(),
          // It is safe to use Unretained(client), because |client| is a
          // ResourceLoader which owns |this|, and we are binding with weak ptr
          // of |this| here.
          base::Unretained(client),
          WebURLError(net::ERR_INTERNET_DISCONNECTED, KURL(request->url))));
}

void InternetDisconnectedURLLoader::Freeze(LoaderFreezeMode) {}

void InternetDisconnectedURLLoader::DidChangePriority(WebURLRequest::Priority,
                                                      int) {}

void InternetDisconnectedURLLoader::DidFail(URLLoaderClient* client,
                                            const WebURLError& error) {
  DCHECK(client);
  client->DidFail(
      error, base::TimeTicks::Now(), /*total_encoded_data_length=*/0,
      /*total_encoded_body_length=*/0, /*total_decoded_body_length=*/0);
}

scoped_refptr<base::SingleThreadTaskRunner>
InternetDisconnectedURLLoader::GetTaskRunnerForBodyLoader() {
  return task_runner_;
}

}  // namespace blink
