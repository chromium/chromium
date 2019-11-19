// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_JOB_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_JOB_H_

#include <memory>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "net/http/http_byte_range.h"
#include "url/gurl.h"

namespace net {
class HttpRequestHeaders;
class HttpResponseInfo;
}

namespace content {

class AppCacheEntry;
class AppCacheResponseInfo;
class AppCacheResponseReader;
class AppCacheURLLoaderJob;

// Interface for an AppCache job. This is used to send data stored in the
// AppCache to networking consumers.
// Subclasses implement this interface to wrap custom job objects like
// URLRequestJob, URLLoaderJob, etc to ensure that these dependencies stay out
// of the AppCache code.
class CONTENT_EXPORT AppCacheJob {
 public:
  enum class DeliveryType {
    kAwaitingDeliverCall,
    kAppCached,
    kNetwork,
    kError,
  };

  virtual ~AppCacheJob();

  // True if the job was started.
  virtual bool IsStarted() const = 0;

  // True if the job is waiting for instructions.
  bool IsWaiting() const {
    return delivery_type_ == DeliveryType::kAwaitingDeliverCall;
  }

  // True if the job is delivering a response from the cache.
  bool IsDeliveringAppCacheResponse() const {
    return delivery_type_ == DeliveryType::kAppCached;
  }

  // Returns true if the job is delivering a response from the network.
  bool IsDeliveringNetworkResponse() const {
    return delivery_type_ == DeliveryType::kNetwork;
  }

  // Returns true if the job is delivering an error response.
  bool IsDeliveringErrorResponse() const {
    return delivery_type_ == DeliveryType::kError;
  }

  // Returns true if the cache entry was not found in the cache.
  bool IsCacheEntryNotFound() const { return cache_entry_not_found_; }

  // Informs the job of what response it should deliver. Only one of these
  // methods should be called, and only once per job. A job will sit idle and
  // wait indefinitely until one of the deliver methods is called.
  virtual void DeliverAppCachedResponse(const GURL& manifest_url,
                                        int64_t cache_id,
                                        const AppCacheEntry& entry,
                                        bool is_fallback) = 0;

  // Informs the job that it should deliver the response from the network. This
  // is generally controlled by the entries in the manifest file.
  virtual void DeliverNetworkResponse() = 0;

  // Informs the job that it should deliver an error response.
  virtual void DeliverErrorResponse() = 0;

  // Returns a weak pointer reference to the job.
  virtual base::WeakPtr<AppCacheJob> GetWeakPtr() = 0;

  // Returns the underlying ApppCacheURLLoaderJob if any. This only applies to
  // AppCaches loaded via the URLRequest mechanism.
  virtual AppCacheURLLoaderJob* AsURLLoaderJob();

  void set_delivery_type(DeliveryType delivery_type) {
    delivery_type_ = delivery_type;
  }

 protected:
  AppCacheJob();

  bool is_range_request() const { return range_requested_.IsValid(); }

  void InitializeRangeRequestInfo(const net::HttpRequestHeaders& headers);
  void SetupRangeResponse();

  SEQUENCE_CHECKER(sequence_checker_);

  // Set to true if the AppCache entry is not found.
  bool cache_entry_not_found_;

  // The jobs delivery status.
  DeliveryType delivery_type_;

  // Byte range request if any.
  net::HttpByteRange range_requested_;

  std::unique_ptr<net::HttpResponseInfo> range_response_info_;

  // The response details.
  scoped_refptr<AppCacheResponseInfo> info_;

  // Used to read the cache.
  std::unique_ptr<AppCacheResponseReader> reader_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_JOB_H_
