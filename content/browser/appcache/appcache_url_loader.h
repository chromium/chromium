// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_H_

#include <memory>

#include "base/check.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/browser/appcache/appcache_entry.h"
#include "content/browser/appcache/appcache_request_handler.h"
#include "content/browser/appcache/appcache_storage.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/http/http_byte_range.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom-forward.h"
#include "url/gurl.h"

namespace net {
class HttpRequestHeaders;
class HttpResponseInfo;
}  // namespace net

namespace network {
class NetToMojoPendingBuffer;
}

namespace content {

class AppCacheRequest;
class AppCacheResponseInfo;
class AppCacheResponseReader;

// network::mojom::URLLoader that retrieves responses stored in an AppCache.
class CONTENT_EXPORT AppCacheURLLoader : public AppCacheStorage::Delegate,
                                         public network::mojom::URLLoader {
 public:
  enum class DeliveryType {
    kAwaitingDeliverCall,
    kAppCached,
    kNetwork,
    kError,
  };

  // Use AppCacheRequestHandler::CreateJob() instead of calling the constructor
  // directly.
  //
  // The constructor is exposed for std::make_unique.
  AppCacheURLLoader(
      AppCacheRequest* appcache_request,
      AppCacheStorage* storage,
      AppCacheRequestHandler::AppCacheLoaderCallback loader_callback);

  AppCacheURLLoader(const AppCacheURLLoader&) = delete;
  AppCacheURLLoader& operator=(const AppCacheURLLoader&) = delete;

  ~AppCacheURLLoader() override;

  // Sets up the bindings.
  void Start(base::OnceClosure continuation,
             const network::ResourceRequest& resource_request,
             mojo::PendingReceiver<network::mojom::URLLoader> receiver,
             mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  // True if the loader was started.
  bool IsStarted() const;

  // True if the loader is waiting for instructions.
  bool IsWaiting() const {
    return delivery_type_ == DeliveryType::kAwaitingDeliverCall;
  }

  // True if the loader is delivering a response from the cache.
  bool IsDeliveringAppCacheResponse() const {
    return delivery_type_ == DeliveryType::kAppCached;
  }

  // True if the loader is delivering a response from the network.
  bool IsDeliveringNetworkResponse() const {
    return delivery_type_ == DeliveryType::kNetwork;
  }

  // True if the loader is delivering an error response.
  bool IsDeliveringErrorResponse() const {
    return delivery_type_ == DeliveryType::kError;
  }

  void set_delivery_type(DeliveryType delivery_type) {
    delivery_type_ = delivery_type;
  }

  // True if the cache entry was not found in the cache.
  bool IsCacheEntryNotFound() const { return cache_entry_not_found_; }

  // Informs the loader of what response it should deliver.
  //
  // Each loader should receive exactly one call to a Deliver*() method. Loaders
  // will sit idle and wait indefinitely until one of the Deliver*() methods is
  // called.
  void DeliverAppCachedResponse(const GURL& manifest_url,
                                int64_t cache_id,
                                const AppCacheEntry& entry,
                                bool is_fallback);

  // Informs the loader that it should deliver the response from the network.
  // This is generally controlled by the entries in the manifest file.
  //
  // Each loader should receive exactly one call to a Deliver*() method. Loaders
  // will sit idle and wait indefinitely until one of the Deliver*() methods is
  // called.
  void DeliverNetworkResponse();

  // Informs the loader that it should deliver an error response.
  //
  // Each loader should receive exactly one call to a Deliver*() method. Loaders
  // will sit idle and wait indefinitely until one of the Deliver*() methods is
  // called.
  void DeliverErrorResponse();

  base::WeakPtr<AppCacheURLLoader> GetWeakPtr();

  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  void DeleteIfNeeded();

 private:
  bool is_range_request() const { return range_requested_.IsValid(); }

  void InitializeRangeRequestInfo(const net::HttpRequestHeaders& headers);
  void SetupRangeResponse();

  // Invokes the loader callback which is expected to setup the mojo binding.
  void CallLoaderCallback(base::OnceClosure continuation);

  // AppCacheStorage::Delegate:
  void OnResponseInfoLoaded(AppCacheResponseInfo* response_info,
                            int64_t response_id) override;

  void ContinueOnResponseInfoLoaded(
      scoped_refptr<AppCacheResponseInfo> response_info);

  // AppCacheResponseReader completion callback.
  void OnReadComplete(int result);

  // Callback invoked when the data pipe can be written to.
  void OnResponseBodyStreamReady(MojoResult result);

  // Schedules a task to delete self with some clean-ups. This is also used as
  // a mojo binding error handler.
  void DeleteSoon();

  void SendResponseInfo();
  void ReadMore();
  void NotifyCompleted(int error_code);

  // True if the AppCache entry is not found.
  bool cache_entry_not_found_ = false;

  // The loader's delivery status.
  DeliveryType delivery_type_ = DeliveryType::kAwaitingDeliverCall;

  // Byte range request if any.
  net::HttpByteRange range_requested_;

  std::unique_ptr<net::HttpResponseInfo> range_response_info_;

  // The response details.
  scoped_refptr<AppCacheResponseInfo> info_;

  // Used to read the cache.
  std::unique_ptr<AppCacheResponseReader> reader_;

  base::WeakPtr<AppCacheStorage> storage_;

  // Time when the request started.
  base::TimeTicks start_time_tick_;

  // Timing information for the most recent request.  Its start times are
  // populated in DeliverAppCachedResponse().
  net::LoadTimingInfo load_timing_info_;

  GURL manifest_url_;
  int64_t cache_id_ = blink::mojom::kAppCacheNoCacheId;
  AppCacheEntry entry_;
  bool is_fallback_ = false;

  // Receiver of the URLLoaderClient with us.
  mojo::Receiver<network::mojom::URLLoader> receiver_{this};

  // The URLLoaderClient remote. We call this interface with notifications
  // about the URL load
  mojo::Remote<network::mojom::URLLoaderClient> client_;

  // The data pipe used to transfer AppCache data to the client.
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
  mojo::ScopedDataPipeProducerHandle response_body_stream_;
  scoped_refptr<network::NetToMojoPendingBuffer> pending_write_;
  mojo::SimpleWatcher writable_handle_watcher_;

  // The Callback to be invoked in the network service land to indicate if
  // the resource request can be serviced via the AppCache.
  AppCacheRequestHandler::AppCacheLoaderCallback loader_callback_;

  // The AppCacheRequest instance, used to inform the loader job about range
  // request headers. Not owned by this class.
  const base::WeakPtr<AppCacheRequest> appcache_request_;

  bool is_deleting_soon_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AppCacheURLLoader> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_H_
