// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_URL_LOADER_REQUEST_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_URL_LOADER_REQUEST_H_

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <string>

#include "base/macros.h"
#include "content/browser/appcache/appcache_update_job.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/io_buffer.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace net {
class HttpResponseInfo;
}

namespace content {

// URLLoaderClient subclass for the UpdateRequestBase class. Provides
// functionality to update the AppCache using functionality provided by the
// network URL loader.
class AppCacheUpdateJob::UpdateURLLoaderRequest
    : public network::mojom::URLLoaderClient {
 public:
  UpdateURLLoaderRequest(base::WeakPtr<StoragePartitionImpl> partition,
                         const GURL& url,
                         int buffer_size,
                         URLFetcher* fetcher);
  ~UpdateURLLoaderRequest() override;

  // This method is called to start the request.
  void Start();

  // Sets all extra request headers.  Any extra request headers set by other
  // methods are overwritten by this method.  This method may only be called
  // before Start() is called.  It is an error to call it later.
  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers);

  // Returns the request URL.
  GURL GetURL() const;

  // Sets flags which control the request load. e.g. if it can be loaded
  // from cache, etc.
  void SetLoadFlags(int flags);

  // Gets the load flags on the request.
  int GetLoadFlags() const;

  // Get the mime type.  This method may only be called after the response was
  // started.
  std::string GetMimeType() const;

  // Cookie policy.
  void SetSiteForCookies(const GURL& site_for_cookies);

  // Sets the origin of the context which initiated the request.
  void SetInitiator(const base::Optional<url::Origin>& initiator);

  // Get all response headers, as a HttpResponseHeaders object.  See comments
  // in HttpResponseHeaders class as to the format of the data.
  net::HttpResponseHeaders* GetResponseHeaders() const;

  // Returns the HTTP response code (e.g., 200, 404, and so on).  This method
  // may only be called once the delegate's OnResponseStarted method has been
  // called.  For non-HTTP requests, this method returns -1.
  int GetResponseCode() const;

  // Fetch the X-AppCache-Allowed response header and return the scope based
  // on the header.
  std::string GetAppCacheAllowedHeader() const;

  // Get the HTTP response info in its entirety.
  const net::HttpResponseInfo& GetResponseInfo() const;

  // Initiates an asynchronous read. Multiple concurrent reads are not
  // supported.
  void Read();

  // This method may be called at any time after Start() has been called to
  // cancel the request.
  // Returns net::ERR_ABORTED or any applicable net error.
  int Cancel();

  // Set fetch metadata headers ( only `Sec-Fetch-Dest` for now ) for secure
  // resources.
  // TODO(lyf): Remove this function after moving `Sec-Fetch-Dest` to the
  // network service.
  void SetFetchMetadataHeaders() {
    if (GetURL().SchemeIsCryptographic())
      request_.headers.SetHeader("Sec-Fetch-Dest", "empty");
  }

  // network::mojom::URLLoaderClient implementation.
  // These methods are called by the network loader.
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

 private:
  // Helper function to initiate an asynchronous read on the data pipe.
  void StartReading(MojoResult unused);

  // Helper function to setup the data pipe watcher to start reading from
  // the pipe. We need to do this when the data pipe is available and there is
  // a pending read.
  void MaybeStartReading();

  URLFetcher* fetcher_;
  // |partition_| is used to get the network URLLoader.
  base::WeakPtr<StoragePartitionImpl> partition_;

  network::ResourceRequest request_;
  network::mojom::URLResponseHeadPtr response_;
  network::URLLoaderCompletionStatus response_status_;
  // Response details.
  std::unique_ptr<net::HttpResponseInfo> http_response_info_;
  // Binds the URLLoaderClient interface to the channel.
  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver_{this};
  // The network URL loader.
  mojo::Remote<network::mojom::URLLoader> url_loader_;
  // Caller buffer size.
  int buffer_size_;
  // The mojo data pipe.
  mojo::ScopedDataPipeConsumerHandle handle_;
  // Used to watch the data pipe to initiate reads.
  mojo::SimpleWatcher handle_watcher_;
  // Set to true when the caller issues a read request. We set it to false in
  // the StartReading() function when the mojo BeginReadData API returns a
  // value indicating one of the following:
  // 1. Data is available.
  // 2. End of data has been reached.
  // 3. Error.
  // Please look at the StartReading() function for details.
  bool read_requested_;
  // Adapter for transferring data from a mojo data pipe to net.
  scoped_refptr<network::MojoToNetPendingBuffer> pending_read_;

  DISALLOW_COPY_AND_ASSIGN(UpdateURLLoaderRequest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_URL_LOADER_REQUEST_H_
