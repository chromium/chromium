// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/http_cache_helper.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "net/base/completion_repeating_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/quic/quic_session_pool.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace {

// Posts |callback| on |task_runner|.
void PostCallback(const scoped_refptr<base::TaskRunner>& task_runner,
                  net::CompletionOnceCallback callback,
                  int error) {
  task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), error));
}

// Clears the disk_cache::Backend on the IO thread.
void DoomHttpCache(const scoped_refptr<base::TaskRunner>& client_task_runner,
                   const base::Time& delete_begin,
                   const base::Time& delete_end,
                   net::CompletionOnceCallback callback,
                   net::HttpCache::GetBackendResult backend_result) {
  auto [error, backend] = backend_result;
  // `backend` may be null in case of error.
  if (backend) {
    auto callback_pair = base::SplitOnceCallback(std::move(callback));
    const int rv = backend->DoomEntriesBetween(
        delete_begin, delete_end,
        base::BindOnce(&PostCallback, client_task_runner,
                       std::move(callback_pair.first)));
    // DoomEntriesBetween does not invoke callback unless rv is ERR_IO_PENDING.
    if (rv != net::ERR_IO_PENDING) {
      client_task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback_pair.second), rv));
    }
  } else {
    client_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(callback), error));
  }
}

// Clears various caches synchronously and the disk_cache::Backend
// asynchronously.
void ClearHttpCacheOnIOThread(
    const scoped_refptr<net::URLRequestContextGetter>& getter,
    const scoped_refptr<base::TaskRunner>& client_task_runner,
    const base::Time& delete_begin,
    const base::Time& delete_end,
    net::CompletionOnceCallback callback) {
  net::HttpCache* http_cache =
      getter->GetURLRequestContext()->http_transaction_factory()->GetCache();

  // Clear QUIC server information from memory and the disk cache.
  http_cache->GetSession()
      ->quic_session_pool()
      ->ClearCachedStatesInCryptoConfig(
          base::RepeatingCallback<bool(const GURL&)>());

  auto doom_callback_pair = base::SplitOnceCallback(
      base::BindOnce(&DoomHttpCache, client_task_runner, delete_begin,
                     delete_end, std::move(callback)));

  net::HttpCache::GetBackendResult result =
      http_cache->GetBackend(std::move(doom_callback_pair.first));
  if (result.first != net::ERR_IO_PENDING) {
    // GetBackend doesn't call the callback if it completes synchronously, so
    // call it directly here.
    std::move(doom_callback_pair.second).Run(result);
  }
}

}  // namespace

namespace net {

void ClearHttpCache(const scoped_refptr<net::URLRequestContextGetter>& getter,
                    const scoped_refptr<base::TaskRunner>& network_task_runner,
                    const base::Time& delete_begin,
                    const base::Time& delete_end,
                    net::CompletionOnceCallback callback) {
  DCHECK(delete_end != base::Time());
  network_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&ClearHttpCacheOnIOThread, getter,
                     base::SingleThreadTaskRunner::GetCurrentDefault(),
                     delete_begin, delete_end, std::move(callback)));
}

}  // namespace net
