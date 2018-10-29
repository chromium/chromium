// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_DELEGATE_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace net {
struct RedirectInfo;
}

namespace network {
struct ResourceResponse;
struct URLLoaderCompletionStatus;
}

namespace content {

class NavigationData;
struct GlobalRequestID;
struct SubresourceLoaderParams;

// The delegate interface to NavigationURLLoader.
class CONTENT_EXPORT NavigationURLLoaderDelegate {
 public:
  // Called when the request is redirected. Call FollowRedirect to continue
  // processing the request.
  virtual void OnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      const scoped_refptr<network::ResourceResponse>& response) = 0;

  // Called when the request receives its response. No further calls will be
  // made to the delegate. The response body can be retrieved by implementing an
  // URLLoaderClient and binding the |url_loader_client_endpoints|.
  // |navigation_data| is passed to the NavigationHandle.
  // |subresource_loader_params| is used in the network service only for passing
  // necessary info to create a custom subresource loader in the renderer
  // process if the navigated context is controlled by a request interceptor
  // like AppCache or ServiceWorker.
  virtual void OnResponseStarted(
      const scoped_refptr<network::ResourceResponse>& response,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      std::unique_ptr<NavigationData> navigation_data,
      const GlobalRequestID& request_id,
      bool is_download,
      bool is_stream,
      base::Optional<SubresourceLoaderParams> subresource_loader_params) = 0;

  // Called if the request fails before receving a response. Specific
  // fields which are used: |status.error_code| holds the error code
  // for the failure; |status.extended_error_code| holds details if
  // available; |status.exists_in_cache| indicates a stale cache
  // entry; |status.ssl_info| is available when |status.error_code| is
  // a certificate error.
  virtual void OnRequestFailed(
      const network::URLLoaderCompletionStatus& status) = 0;

  // Called after the network request has begun on the IO thread at time
  // |timestamp|. This is just a thread hop but is used to compare timing
  // against the pre-PlzNavigate codepath which didn't start the network request
  // until after the renderer was initialized.
  virtual void OnRequestStarted(base::TimeTicks timestamp) = 0;

 protected:
  NavigationURLLoaderDelegate() {}
  virtual ~NavigationURLLoaderDelegate() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NavigationURLLoaderDelegate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_DELEGATE_H_
