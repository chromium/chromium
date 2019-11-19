// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_JOB_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_JOB_H_

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/browser/appcache/appcache_entry.h"
#include "content/browser/appcache/appcache_job.h"
#include "content/browser/appcache/appcache_request_handler.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/appcache/appcache_storage.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace network {
class NetToMojoPendingBuffer;
}

namespace content {

class AppCacheRequest;

// AppCacheJob wrapper for a network::mojom::URLLoader implementation which
// returns responses stored in the AppCache.
class CONTENT_EXPORT AppCacheURLLoaderJob : public AppCacheJob,
                                            public AppCacheStorage::Delegate,
                                            public network::mojom::URLLoader {
 public:
  // Use AppCacheRequestHandler::CreateJob() instead of calling the constructor
  // directly.
  //
  // The constructor is exposed for std::make_unique.
  AppCacheURLLoaderJob(
      AppCacheRequest* appcache_request,
      AppCacheStorage* storage,
      AppCacheRequestHandler::AppCacheLoaderCallback loader_callback);

  ~AppCacheURLLoaderJob() override;

  // Sets up the bindings.
  void Start(base::OnceClosure continuation,
             const network::ResourceRequest& resource_request,
             mojo::PendingReceiver<network::mojom::URLLoader> receiver,
             mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  // AppCacheJob overrides.
  bool IsStarted() const override;
  void DeliverAppCachedResponse(const GURL& manifest_url,
                                int64_t cache_id,
                                const AppCacheEntry& entry,
                                bool is_fallback) override;
  void DeliverNetworkResponse() override;
  void DeliverErrorResponse() override;
  AppCacheURLLoaderJob* AsURLLoaderJob() override;
  base::WeakPtr<AppCacheJob> GetWeakPtr() override;
  base::WeakPtr<AppCacheURLLoaderJob> GetDerivedWeakPtr();

  // network::mojom::URLLoader implementation:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  void DeleteIfNeeded();

 protected:
  // Invokes the loader callback which is expected to setup the mojo binding.
  void CallLoaderCallback(base::OnceClosure continuation);

  // AppCacheStorage::Delegate methods
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

  base::WeakPtr<AppCacheStorage> storage_;

  // Time when the request started.
  base::TimeTicks start_time_tick_;

  // Timing information for the most recent request.  Its start times are
  // populated in DeliverAppCachedResponse().
  net::LoadTimingInfo load_timing_info_;

  GURL manifest_url_;
  int64_t cache_id_;
  AppCacheEntry entry_;
  bool is_fallback_;

  // Receiver of the URLLoaderClient with us.
  mojo::Receiver<network::mojom::URLLoader> receiver_{this};

  // The URLLoaderClient remote. We call this interface with notifications
  // about the URL load
  mojo::Remote<network::mojom::URLLoaderClient> client_;

  // The data pipe used to transfer AppCache data to the client.
  mojo::DataPipe data_pipe_;
  mojo::ScopedDataPipeProducerHandle response_body_stream_;
  scoped_refptr<network::NetToMojoPendingBuffer> pending_write_;
  mojo::SimpleWatcher writable_handle_watcher_;

  // The Callback to be invoked in the network service land to indicate if
  // the resource request can be serviced via the AppCache.
  AppCacheRequestHandler::AppCacheLoaderCallback loader_callback_;

  // The AppCacheRequest instance, used to inform the loader job about range
  // request headers. Not owned by this class.
  base::WeakPtr<AppCacheRequest> appcache_request_;

  bool is_deleting_soon_ = false;
  bool is_main_resource_load_;

  base::WeakPtrFactory<AppCacheURLLoaderJob> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(AppCacheURLLoaderJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_JOB_H_
