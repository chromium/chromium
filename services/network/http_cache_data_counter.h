// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_HTTP_CACHE_DATA_COUNTER_H_
#define SERVICES_NETWORK_HTTP_CACHE_DATA_COUNTER_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace disk_cache {
class Backend;
}

namespace net {
class URLRequestContext;
}

namespace network {

// Helper to count data in HTTP cache.
// Export is for testing only.
class COMPONENT_EXPORT(NETWORK_SERVICE) HttpCacheDataCounter {
 public:
  using HttpCacheDataCounterCallback = base::OnceCallback<
      void(HttpCacheDataCounter*, bool upper_bound, int64_t size_or_error)>;

  // Computes the amount of disk space taken up by entries last used between
  // [start_time, end_time), and return it, or error.  Note that there may be
  // some approximation with respect to both bytes and dates.
  //
  // Furthermore, if there is no efficient way of computing this information,
  // a very loose upper bound (e.g. total disk space used by the cache) may be
  // returned; in that case |upper_bound| will be set to true.
  //
  // Once complete, invokes |callback|, passing |this| and result.
  //
  // If either |this| or |url_request_context| get destroyed, |callback|
  // will not be invoked.
  static std::unique_ptr<HttpCacheDataCounter> CreateAndStart(
      net::URLRequestContext* url_request_context,
      base::Time start_time,
      base::Time end_time,
      HttpCacheDataCounterCallback callback);

  ~HttpCacheDataCounter();

 private:
  HttpCacheDataCounter(base::Time start_time,
                       base::Time end_time,
                       HttpCacheDataCounterCallback callback);

  void GotBackend(std::unique_ptr<disk_cache::Backend*> backend,
                  int error_code);
  void PostResult(bool is_upper_limit, int64_t result_or_error);

  base::WeakPtr<HttpCacheDataCounter> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  base::Time start_time_;
  base::Time end_time_;
  HttpCacheDataCounterCallback callback_;

  base::WeakPtrFactory<HttpCacheDataCounter> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HttpCacheDataCounter);
};

}  // namespace network

#endif  // SERVICES_NETWORK_HTTP_CACHE_DATA_COUNTER_H_
