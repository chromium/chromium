// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_url_loader_factory.h"

#include "base/check.h"
#include "base/synchronization/waitable_event.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/platform/web_back_forward_cache_loader_helper.h"
#include "third_party/blink/public/platform/web_url_loader.h"

using blink::scheduler::WebResourceLoadingTaskRunnerHandle;

namespace blink {

WebURLLoaderFactory::WebURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    const WebVector<WebString>& cors_exempt_header_list,
    base::WaitableEvent* terminate_sync_load_event)
    : loader_factory_(std::move(loader_factory)),
      cors_exempt_header_list_(cors_exempt_header_list),
      terminate_sync_load_event_(terminate_sync_load_event) {
  DCHECK(loader_factory_);
}

WebURLLoaderFactory::WebURLLoaderFactory() = default;

WebURLLoaderFactory::~WebURLLoaderFactory() = default;

std::unique_ptr<WebURLLoader> WebURLLoaderFactory::CreateURLLoader(
    const WebURLRequest& request,
    std::unique_ptr<WebResourceLoadingTaskRunnerHandle>
        freezable_task_runner_handle,
    std::unique_ptr<WebResourceLoadingTaskRunnerHandle>
        unfreezable_task_runner_handle,
    CrossVariantMojoRemote<mojom::KeepAliveHandleInterfaceBase>
        keep_alive_handle,
    WebBackForwardCacheLoaderHelper back_forward_cache_loader_helper) {
  DCHECK(freezable_task_runner_handle);
  DCHECK(unfreezable_task_runner_handle);
  return std::make_unique<WebURLLoader>(
      cors_exempt_header_list_, terminate_sync_load_event_,
      std::move(freezable_task_runner_handle),
      std::move(unfreezable_task_runner_handle), loader_factory_,
      std::move(keep_alive_handle), back_forward_cache_loader_helper);
}

}  // namespace blink
