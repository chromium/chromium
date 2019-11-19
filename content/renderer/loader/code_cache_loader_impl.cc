// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/code_cache_loader_impl.h"
#include "base/bind.h"
#include "base/task/post_task.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/blink/public/platform/platform.h"

namespace content {

CodeCacheLoaderImpl::CodeCacheLoaderImpl() : CodeCacheLoaderImpl(nullptr) {}

CodeCacheLoaderImpl::CodeCacheLoaderImpl(
    base::WaitableEvent* terminate_sync_load_event)
    : terminate_sync_load_event_(terminate_sync_load_event) {}

CodeCacheLoaderImpl::~CodeCacheLoaderImpl() = default;

void CodeCacheLoaderImpl::FetchFromCodeCacheSynchronously(
    const GURL& url,
    base::Time* response_time_out,
    mojo_base::BigBuffer* data_out) {
  base::WaitableEvent fetch_code_cache_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::CreateSingleThreadTaskRunner({base::ThreadPool()});

  // Also watch for terminate requests from the main thread when running on
  // worker threads.
  if (terminate_sync_load_event_) {
    terminate_watcher_.StartWatching(
        terminate_sync_load_event_,
        base::BindOnce(&CodeCacheLoaderImpl::OnTerminate,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::Unretained(&fetch_code_cache_event)),
        task_runner);
  }

  FetchCodeCacheCallback callback =
      base::BindOnce(&CodeCacheLoaderImpl::ReceiveDataForSynchronousFetch,
                     weak_ptr_factory_.GetWeakPtr());

  // It is Ok to pass |fetch_code_cache_event| with base::Unretained. Since
  // this thread is stalled, the fetch_code_cache_event will be kept alive.
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&CodeCacheLoaderImpl::FetchFromCodeCacheImpl,
                                weak_ptr_factory_.GetWeakPtr(),
                                blink::mojom::CodeCacheType::kJavascript, url,
                                std::move(callback),
                                base::Unretained(&fetch_code_cache_event)));

  // Wait for the fetch from code cache to finish.
  fetch_code_cache_event.Wait();

  // Set the output data
  *response_time_out = response_time_for_sync_load_;
  *data_out = std::move(data_for_sync_load_);
}

void CodeCacheLoaderImpl::FetchFromCodeCache(
    blink::mojom::CodeCacheType cache_type,
    const GURL& url,
    FetchCodeCacheCallback callback) {
  FetchFromCodeCacheImpl(cache_type, url, std::move(callback), nullptr);
}

void CodeCacheLoaderImpl::FetchFromCodeCacheImpl(
    blink::mojom::CodeCacheType cache_type,
    const GURL& gurl,
    FetchCodeCacheCallback callback,
    base::WaitableEvent* fetch_event) {
  // This may run on a different thread for synchronous events. It is Ok to pass
  // fetch_event, because the thread is stalled and it will keep the fetch_event
  // alive.
  blink::Platform::Current()->FetchCachedCode(
      cache_type, gurl,
      base::BindOnce(&CodeCacheLoaderImpl::OnReceiveCachedCode,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     fetch_event));
}

void CodeCacheLoaderImpl::OnReceiveCachedCode(FetchCodeCacheCallback callback,
                                              base::WaitableEvent* fetch_event,
                                              base::Time response_time,
                                              mojo_base::BigBuffer data) {
  // The loader would be destroyed once the fetch has completed. On terminate
  // the fetch event would be signalled and the fetch should complete and hence
  // we should not see this callback anymore.
  DCHECK(!terminated_);
  std::move(callback).Run(response_time, std::move(data));
  if (fetch_event)
    fetch_event->Signal();
}

void CodeCacheLoaderImpl::ReceiveDataForSynchronousFetch(
    base::Time response_time,
    mojo_base::BigBuffer data) {
  response_time_for_sync_load_ = response_time;
  data_for_sync_load_ = std::move(data);
}

void CodeCacheLoaderImpl::OnTerminate(base::WaitableEvent* fetch_event,
                                      base::WaitableEvent* terminate_event) {
  DCHECK(!terminated_);
  terminated_ = true;
  DCHECK(fetch_event);
  fetch_event->Signal();
}

}  // namespace content
