// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_URL_LOADER_FACTORY_H_
#define SERVICES_NETWORK_TEST_TEST_URL_LOADER_FACTORY_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace network {
class WeakWrapperSharedURLLoaderFactory;

// A helper class to ease testing code that uses URLLoader interface. A test
// would pass this factory instead of the production factory to code, and
// would prime it with response data for arbitrary URLs.
class TestURLLoaderFactory : public mojom::URLLoaderFactory {
 public:
  struct PendingRequest {
    PendingRequest();
    ~PendingRequest();
    PendingRequest(PendingRequest&& other);
    PendingRequest& operator=(PendingRequest&& other);

    mojo::Remote<mojom::URLLoaderClient> client;
    ResourceRequest request;
    uint32_t options;
  };

  // Bitfield that is used with |SimulateResponseForPendingRequest()| to
  // control which request is selected.
  enum ResponseMatchFlags : uint32_t {
    kMatchDefault = 0x0,
    kUrlMatchPrefix = 0x1,   // Whether URLs are a match if they start with the
                             // URL passed in to
                             // SimulateResponseForPendingRequest
    kMostRecentMatch = 0x2,  // Start with the most recent requests.
  };

  // Flags used with |AddResponse| to control how it produces a response.
  enum ResponseProduceFlags : uint32_t {
    kResponseDefault = 0,
    kResponseOnlyRedirectsNoDestination = 0x1,
    kSendHeadersOnNetworkError = 0x2,
  };

  TestURLLoaderFactory();
  ~TestURLLoaderFactory() override;

  using Redirects =
      std::vector<std::pair<net::RedirectInfo, mojom::URLResponseHeadPtr>>;

  // Adds a response to be served. There is one unique response per URL, and if
  // this method is called multiple times for the same URL the last response
  // data is used.
  // This can be called before or after a request is made. If it's called after,
  // then pending requests will be "woken up".
  void AddResponse(const GURL& url,
                   mojom::URLResponseHeadPtr head,
                   const std::string& content,
                   const URLLoaderCompletionStatus& status,
                   Redirects redirects = Redirects(),
                   ResponseProduceFlags rp_flags = kResponseDefault);

  // Simpler version of above for the common case of success or error page.
  void AddResponse(const std::string& url,
                   const std::string& content,
                   net::HttpStatusCode status = net::HTTP_OK);

  // Returns true if there is a request for a given URL with a living client
  // that did not produce a response yet. If |request_out| is non-null,
  // it will give a const pointer to the request.
  // WARNING: This does RunUntilIdle() first.
  bool IsPending(const std::string& url,
                 const ResourceRequest** request_out = nullptr);

  // Returns the total # of pending requests.
  // WARNING: This does RunUntilIdle() first.
  int NumPending();

  // Clear all the responses that were previously set.
  void ClearResponses();

  using Interceptor = base::RepeatingCallback<void(const ResourceRequest&)>;
  void SetInterceptor(const Interceptor& interceptor);

  // Returns a mutable list of pending requests, for consumers that need direct
  // access. It's recommended that consumers use AddResponse() rather than
  // servicing requests themselves, whenever possible.
  std::vector<PendingRequest>* pending_requests() { return &pending_requests_; }

  // Returns the PendingRequest instance available at the given index |index|
  // or null if not existing.
  PendingRequest* GetPendingRequest(size_t index);

  // Sends a response for the first (oldest) pending request with URL |url|.
  // Returns false if no such pending request exists.
  // |flags| can be used to change the default behavior:
  // - if kUrlMatchPrefix is set, the pending request is a match if its URL
  //   starts with |url| (instead of being equal to |url|).
  // - if kMostRecentMatch is set, the most recent (instead of oldest) pending
  //   request matching is used.
  bool SimulateResponseForPendingRequest(
      const GURL& url,
      const network::URLLoaderCompletionStatus& completion_status,
      mojom::URLResponseHeadPtr response_head,
      const std::string& content,
      ResponseMatchFlags flags = kMatchDefault);

  // Simpler version of above for the common case of success or error page.
  bool SimulateResponseForPendingRequest(
      const std::string& url,
      const std::string& content,
      net::HttpStatusCode status = net::HTTP_OK,
      ResponseMatchFlags flags = kMatchDefault);

  // Sends a response for the given request |request|.
  //
  // Differently from its variant above, this method does not remove |request|
  // from |pending_requests_|.
  //
  // This method is useful to process requests at a given pre-defined order.
  void SimulateResponseWithoutRemovingFromPendingList(
      PendingRequest* request,
      mojom::URLResponseHeadPtr head,
      std::string content,
      const URLLoaderCompletionStatus& status);

  // Simpler version of the method above.
  void SimulateResponseWithoutRemovingFromPendingList(PendingRequest* request,
                                                      std::string content);

  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(mojo::PendingReceiver<mojom::URLLoader> receiver,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& url_request,
                            mojo::PendingRemote<mojom::URLLoaderClient> client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) override;

  // Returns a 'safe' ref-counted weak wrapper around this TestURLLoaderFactory
  // instance.
  //
  // Because this is a weak wrapper, it is possible for the underlying
  // TestURLLoaderFactory instance to be destroyed while other code still holds
  // a reference to it.
  //
  // The weak wrapper returned by this method is guaranteed to have had
  // Detach() called before this is destructed, so that any future calls become
  // no-ops, rather than a crash.
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
  GetSafeWeakWrapper();

 private:
  bool CreateLoaderAndStartInternal(const GURL& url,
                                    mojom::URLLoaderClient* client);

  static void SimulateResponse(mojom::URLLoaderClient* client,
                               Redirects redirects,
                               mojom::URLResponseHeadPtr head,
                               std::string content,
                               URLLoaderCompletionStatus status,
                               ResponseProduceFlags response_flags);

  struct Response {
    Response();
    ~Response();
    Response(Response&&);
    Response& operator=(Response&&);
    GURL url;
    Redirects redirects;
    mojom::URLResponseHeadPtr head;
    std::string content;
    URLLoaderCompletionStatus status;
    ResponseProduceFlags flags;
  };
  std::map<GURL, Response> responses_;

  std::vector<PendingRequest> pending_requests_;

  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory> weak_wrapper_;

  Interceptor interceptor_;
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;

  DISALLOW_COPY_AND_ASSIGN(TestURLLoaderFactory);
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_URL_LOADER_FACTORY_H_
