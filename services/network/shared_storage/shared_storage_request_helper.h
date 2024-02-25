// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_STORAGE_SHARED_STORAGE_REQUEST_HELPER_H_
#define SERVICES_NETWORK_SHARED_STORAGE_SHARED_STORAGE_REQUEST_HELPER_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "net/http/structured_headers.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "url/origin.h"

namespace net {
class URLRequest;
}  // namespace net

namespace network {

// Used by the `URLLoader` to help handle shared-storage-related requests.
// Contains methods to create the helper, process the outgoing request, process
// the incoming response, and reset shared-storage-eligibility when it has been
// lost on a redirect.
//
// Requests are eligible for shared storage when
// `ResourceRequest::shared_storage_writable` is true (which can only occur when
// `blink::features::kSharedStorageAPI` is enabled). When a request is eligible,
// `ProcessOutgoingRequest()` will add the "Sec-Shared-Storage-Writable" request
// header to the outgoing request.
//
// When "Shared-Storage-Write" response header(s) are received, if the request
// is eligible for shared storage, then `ProcessIncomingRequest()` will process
// the header(s) into vectors of operation structs, to be sent via
// `mojom::OnSharedStorageHeaderReceived()`.
class SharedStorageRequestHelper {
 public:
  SharedStorageRequestHelper(bool shared_storage_writable_eligible,
                             mojom::URLLoaderNetworkServiceObserver* observer);

  ~SharedStorageRequestHelper();

  bool shared_storage_writable_eligible() const {
    return shared_storage_writable_eligible_;
  }

  // If `shared_storage_writable_eligible_` is false or there is no `observer_`,
  // then this is a no-op. Otherwise, this method adds the
  // `kSecSharedStorageWritableHeader` request header.
  void ProcessOutgoingRequest(net::URLRequest& request);

  // Processes and removes any shared storage headers and sends any processed
  // information to the `observer_`. Returns `true` if processing asynchronously
  // and will run `done` when done. Otherwise, processing completed
  // synchronously and `done` will not be run.
  bool ProcessIncomingResponse(net::URLRequest& request,
                               base::OnceClosure done);

  // Determines whether, on redirect, a request has lost or regained its
  // eligibility for shared storage writing. If so, updates
  // `shared_storage_writable_eligible_` accordingly. Called in
  // `URLLoader::FollowRedirect()`.
  void UpdateSharedStorageWritableEligible(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers);

 private:
  bool ProcessResponse(net::URLRequest& request,
                       std::string_view value,
                       base::OnceClosure done);

  void OnOperationsQueued(base::OnceClosure done);

  // True if the current request should have the
  // `kSharedStorageWritable` header attached and is eligible to
  // write to shared storage from response headers.
  bool shared_storage_writable_eligible_;

  raw_ptr<mojom::URLLoaderNetworkServiceObserver> observer_;
  base::WeakPtrFactory<SharedStorageRequestHelper> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_STORAGE_SHARED_STORAGE_REQUEST_HELPER_H_
