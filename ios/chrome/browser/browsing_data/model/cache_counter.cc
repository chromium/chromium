// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browsing_data/model/cache_counter.h"

#include "base/functional/bind.h"
#include "components/browsing_data/core/pref_names.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

class IOThreadCacheCounter {
 public:
  IOThreadCacheCounter(
      const scoped_refptr<net::URLRequestContextGetter>& context_getter,
      const net::Int64CompletionRepeatingCallback& result_callback)
      : next_step_(STEP_GET_BACKEND),
        context_getter_(context_getter),
        result_callback_(result_callback),
        result_(0),
        backend_(nullptr) {}

  void Count() {
    web::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindRepeating(&IOThreadCacheCounter::CountInternal,
                                       base::Unretained(this), net::OK));
  }

 private:
  enum Step {
    STEP_GET_BACKEND,  // Get the disk_cache::Backend instance.
    STEP_COUNT,        // Run CalculateSizeOfAllEntries() on it.
    STEP_CALLBACK,     // Respond on the UI thread.
  };

  void CountInternal(int64_t rv) {
    DCHECK_CURRENTLY_ON(web::WebThread::IO);

    while (rv != net::ERR_IO_PENDING) {
      // In case of an error, skip to the last step.
      if (rv < 0) {
        next_step_ = STEP_CALLBACK;
      }

      // Process the counting in three steps: STEP_GET_BACKEND -> STEP_COUNT ->
      // -> STEP_CALLBACK.
      switch (next_step_) {
        case STEP_GET_BACKEND: {
          next_step_ = STEP_COUNT;

          net::HttpCache* http_cache = context_getter_->GetURLRequestContext()
                                           ->http_transaction_factory()
                                           ->GetCache();

          std::tie(rv, backend_) = http_cache->GetBackend(base::BindOnce(
              [](IOThreadCacheCounter* self,
                 net::HttpCache::GetBackendResult result) {
                self->backend_ = result.second;
                self->CountInternal(static_cast<int64_t>(result.first));
              },
              base::Unretained(this)));
          break;
        }

        case STEP_COUNT: {
          next_step_ = STEP_CALLBACK;

          DCHECK(backend_);
          rv = backend_->CalculateSizeOfAllEntries(base::BindRepeating(
              &IOThreadCacheCounter::CountInternal, base::Unretained(this)));
          break;
        }

        case STEP_CALLBACK: {
          result_ = rv;

          web::GetUIThreadTaskRunner({})->PostTask(
              FROM_HERE,
              base::BindOnce(&IOThreadCacheCounter::OnCountingFinished,
                             base::Unretained(this)));

          // Return instead of break.
          // The task above deletes this object; app would crash if this object
          // is deleted before reentrance of the loop.
          return;
        }
      }
    }
  }

  void OnCountingFinished() {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    result_callback_.Run(result_);
    delete this;
  }

  Step next_step_;
  scoped_refptr<net::URLRequestContextGetter> context_getter_;
  net::Int64CompletionRepeatingCallback result_callback_;
  int64_t result_;
  raw_ptr<disk_cache::Backend> backend_;
};

}  // namespace

CacheCounter::CacheCounter(ProfileIOS* profile)
    : profile_(profile), weak_ptr_factory_(this) {}

CacheCounter::~CacheCounter() = default;

const char* CacheCounter::GetPrefName() const {
  return browsing_data::prefs::kDeleteCache;
}

void CacheCounter::Count() {
  // Cancel existing requests.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // disk_cache::Backend currently does not implement counting for subsets of
  // cache, only for the entire cache. Thus, ignore the time period setting and
  // always request counting for the unbounded time interval. It is up to the
  // UI to interpret the results for finite time intervals as upper estimates.
  // IOThreadCacheCounter deletes itself when done.
  (new IOThreadCacheCounter(
       profile_->GetRequestContext(),
       base::BindRepeating(&CacheCounter::OnCacheSizeCalculated,
                           weak_ptr_factory_.GetWeakPtr())))
      ->Count();
}

void CacheCounter::OnCacheSizeCalculated(int64_t result_bytes) {
  // A value less than 0 means a net error code.
  if (result_bytes < 0) {
    return;
  }

  ReportResult(result_bytes);
}
