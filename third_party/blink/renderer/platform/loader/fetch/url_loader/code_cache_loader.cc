// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/code_cache_loader.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/url_conversion.h"

namespace blink {

CodeCacheLoader::CodeCacheLoader() : CodeCacheLoader(nullptr, nullptr) {}

CodeCacheLoader::CodeCacheLoader(mojom::CodeCacheHost* code_cache_host,
                                 base::WaitableEvent* terminate_sync_load_event)
    : code_cache_host_(code_cache_host),
      terminate_sync_load_event_(terminate_sync_load_event) {}

CodeCacheLoader::~CodeCacheLoader() = default;

void CodeCacheLoader::FetchFromCodeCacheSynchronously(
    const WebURL& url,
    base::Time* response_time_out,
    mojo_base::BigBuffer* data_out) {
  base::WaitableEvent fetch_code_cache_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner({});

  // Also watch for terminate requests from the main thread when running on
  // worker threads.
  if (terminate_sync_load_event_) {
    terminate_watcher_.StartWatching(
        terminate_sync_load_event_,
        base::BindOnce(&CodeCacheLoader::OnTerminate,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::Unretained(&fetch_code_cache_event)),
        task_runner);
  }

  FetchCodeCacheCallback callback =
      base::BindOnce(&CodeCacheLoader::ReceiveDataForSynchronousFetch,
                     weak_ptr_factory_.GetWeakPtr());

  // It is Ok to pass |fetch_code_cache_event| with base::Unretained. Since
  // this thread is stalled, the fetch_code_cache_event will be kept alive.
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&CodeCacheLoader::FetchFromCodeCacheImpl,
                                weak_ptr_factory_.GetWeakPtr(),
                                mojom::CodeCacheType::kJavascript, url,
                                std::move(callback),
                                base::Unretained(&fetch_code_cache_event)));

  // Wait for the fetch from code cache to finish.
  fetch_code_cache_event.Wait();

  // Set the output data
  *response_time_out = response_time_for_sync_load_;
  *data_out = std::move(data_for_sync_load_);
}

void CodeCacheLoader::FetchFromCodeCache(mojom::CodeCacheType cache_type,
                                         const WebURL& url,
                                         FetchCodeCacheCallback callback) {
  FetchFromCodeCacheImpl(cache_type, url, std::move(callback), nullptr);
}

void CodeCacheLoader::FetchFromCodeCacheImpl(mojom::CodeCacheType cache_type,
                                             const WebURL& url,
                                             FetchCodeCacheCallback callback,
                                             base::WaitableEvent* fetch_event) {
  // This may run on a different thread for synchronous events. It is Ok to pass
  // fetch_event, because the thread is stalled and it will keep the fetch_event
  // alive.
  auto receive_callback = base::BindOnce(&CodeCacheLoader::OnReceiveCachedCode,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         std::move(callback), fetch_event);
  if (code_cache_host_) {
    code_cache_host_->FetchCachedCode(
        cache_type, WebStringToGURL(url.GetString()),
        base::BindOnce(
            [](Platform::FetchCachedCodeCallback callback, base::Time time,
               mojo_base::BigBuffer data) {
              std::move(callback).Run(time, std::move(data));
            },
            std::move(receive_callback)));
  } else {
    // TODO(mythria): This path is required for workers currently. Once we
    // update worker requests to go through WorkerHost remove this path.
    Platform::Current()->FetchCachedCode(cache_type, url,
                                         std::move(receive_callback));
  }
}

void CodeCacheLoader::OnReceiveCachedCode(FetchCodeCacheCallback callback,
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

void CodeCacheLoader::ReceiveDataForSynchronousFetch(
    base::Time response_time,
    mojo_base::BigBuffer data) {
  response_time_for_sync_load_ = response_time;
  data_for_sync_load_ = std::move(data);
}

void CodeCacheLoader::OnTerminate(base::WaitableEvent* fetch_event,
                                  base::WaitableEvent* terminate_event) {
  DCHECK(!terminated_);
  terminated_ = true;
  DCHECK(fetch_event);
  fetch_event->Signal();
}

// static
std::unique_ptr<WebCodeCacheLoader> WebCodeCacheLoader::CreateForFrame(
    mojom::CodeCacheHost* code_cache_host) {
  return std::make_unique<CodeCacheLoader>(
      code_cache_host, /*terminate_sync_load_event*/ nullptr);
}

// static
std::unique_ptr<WebCodeCacheLoader> WebCodeCacheLoader::CreateForWorker(
    base::WaitableEvent* terminate_sync_load_event) {
  return std::make_unique<CodeCacheLoader>(/*code_cache_host*/ nullptr,
                                           terminate_sync_load_event);
}

}  // namespace blink
