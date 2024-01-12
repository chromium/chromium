// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_factory.h"

#include "base/check.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader.h"

namespace blink {

URLLoaderFactory::URLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    const Vector<String>& cors_exempt_header_list,
    base::WaitableEvent* terminate_sync_load_event)
    : loader_factory_(std::move(loader_factory)),
      cors_exempt_header_list_(cors_exempt_header_list),
      terminate_sync_load_event_(terminate_sync_load_event) {
  DCHECK(loader_factory_);
}

URLLoaderFactory::URLLoaderFactory() = default;

URLLoaderFactory::~URLLoaderFactory() = default;

std::unique_ptr<URLLoader> URLLoaderFactory::CreateURLLoader(
    const network::ResourceRequest& request,
    scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
    mojo::PendingRemote<mojom::blink::KeepAliveHandle> keep_alive_handle,
    BackForwardCacheLoaderHelper* back_forward_cache_loader_helper,
    Vector<std::unique_ptr<URLLoaderThrottle>> throttles) {
  DCHECK(freezable_task_runner);
  DCHECK(unfreezable_task_runner);
  return std::make_unique<URLLoader>(
      cors_exempt_header_list_, terminate_sync_load_event_,
      std::move(freezable_task_runner), std::move(unfreezable_task_runner),
      loader_factory_, std::move(keep_alive_handle),
      back_forward_cache_loader_helper, std::move(throttles));
}

}  // namespace blink
