// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_URL_LOADER_FACTORY_H_
#define SERVICES_NETWORK_TEST_TEST_URL_LOADER_FACTORY_H_

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network {
class WeakWrapperSharedURLLoaderFactory;

// A helper class to ease testing code that uses URLLoader interface. A test
// would pass this factory instead of the production factory to code, and
// would prime it with response data for arbitrary URLs.
class TestURLLoaderFactory : public mojom::URLLoaderFactory {
 public:
  // A helper class to bind a URLLoader observe method invocations on it.
  class TestURLLoader final : public network::mojom::URLLoader {
   public:
    struct FollowRedirectParams {
      FollowRedirectParams();
      ~FollowRedirectParams();
      FollowRedirectParams(FollowRedirectParams&& other);
      FollowRedirectParams& operator=(FollowRedirectParams&& other);

      std::vector<std::string> removed_headers;
      net::HttpRequestHeaders modified_headers;
      net::HttpRequestHeaders modified_cors_exempt_headers;
      std::optional<GURL> new_url;
    };

    explicit TestURLLoader(
        mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver);
    ~TestURLLoader() override;

    TestURLLoader(const TestURLLoader&) = delete;
    TestURLLoader& operator=(const TestURLLoader&) = delete;

    // network::mojom::URLLoader overrides.
    void FollowRedirect(
        const std::vector<std::string>& removed_headers,
        const net::HttpRequestHeaders& modified_headers,
        const net::HttpRequestHeaders& modified_cors_exempt_headers,
        const std::optional<GURL>& new_url) override;
    void SetPriority(net::RequestPriority priority,
                     int32_t intra_priority_value) override {}
    void PauseReadingBodyFromNet() override {}
    void ResumeReadingBodyFromNet() override {}

    const std::vector<FollowRedirectParams>& follow_redirect_params() const {
      return follow_redirect_params_;
    }

   private:
    std::vector<FollowRedirectParams> follow_redirect_params_;
    mojo::Receiver<network::mojom::URLLoader> receiver_;
  };

  struct PendingRequest {
    PendingRequest();
    ~PendingRequest();
    PendingRequest(PendingRequest&& other);
    PendingRequest& operator=(PendingRequest&& other);

    std::unique_ptr<TestURLLoader> test_url_loader;
    mojo::Remote<mojom::URLLoaderClient> client;
    int32_t request_id;
    uint32_t options;
    ResourceRequest request;
    net::MutableNetworkTrafficAnnotationTag traffic_annotation;
  };

  // Bitfield that is used with |SimulateResponseForPendingRequest()| to
  // control which request is selected.
  enum ResponseMatchFlags : uint32_t {
    kMatchDefault = 0x0,
    kUrlMatchPrefix = 0x1,   // Whether URLs are a match if they start with the
                             // URL passed in to
                             // SimulateResponseForPendingRequest
    kMostRecentMatch = 0x2,  // Start with the most recent requests.
    kWaitForRequest = 0x4,   // Wait for a matching request, if none is present.
  };

  // Flags used with |AddResponse| to control how it produces a response.
  enum ResponseProduceFlags : uint32_t {
    kResponseDefault = 0,
    kResponseOnlyRedirectsNoDestination = 0x1,
    kSendHeadersOnNetworkError = 0x2,
  };

  explicit TestURLLoaderFactory(bool observe_loader_requests = false);

  TestURLLoaderFactory(const TestURLLoaderFactory&) = delete;
  TestURLLoaderFactory& operator=(const TestURLLoaderFactory&) = delete;

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
                   std::string_view content,
                   const URLLoaderCompletionStatus& status,
                   Redirects redirects = Redirects(),
                   ResponseProduceFlags rp_flags = kResponseDefault);

  // Simpler version of above for the common case of success or error page.
  void AddResponse(std::string_view url,
                   std::string_view content,
                   net::HttpStatusCode status = net::HTTP_OK);

  void EraseResponse(const GURL& url) { responses_.erase(url); }

  // Returns true if there is a request for a given URL with a living client
  // that did not produce a response yet. If |request_out| is non-null,
  // it will give a const pointer to the request.
  // WARNING: This does RunUntilIdle() first.
  bool IsPending(std::string_view url,
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
  // - if kWaitForRequest is set, and no matching request is pending, a nested
  //   run loop will be run until that request arrives.
  bool SimulateResponseForPendingRequest(
      const GURL& url,
      const network::URLLoaderCompletionStatus& completion_status,
      mojom::URLResponseHeadPtr response_head,
      std::string_view content,
      ResponseMatchFlags flags = kMatchDefault);

  // Simpler version of above for the common case of success or error page.
  bool SimulateResponseForPendingRequest(
      std::string_view url,
      std::string_view content,
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
      std::string_view content,
      const URLLoaderCompletionStatus& status);

  // Simpler version of the method above.
  void SimulateResponseWithoutRemovingFromPendingList(PendingRequest* request,
                                                      std::string_view content);

  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(mojo::PendingReceiver<mojom::URLLoader> receiver,
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

  // Returns the total number of requests received during the lifetime of
  // `this`, pending and completed. Useful for catching duplicate requests.
  size_t total_requests() const { return total_requests_; }

 private:
  bool CreateLoaderAndStartInternal(const GURL& url,
                                    mojom::URLLoaderClient* client);

  std::optional<network::TestURLLoaderFactory::PendingRequest>
  FindPendingRequest(const GURL& url, ResponseMatchFlags flags);

  static void SimulateResponse(mojom::URLLoaderClient* client,
                               Redirects redirects,
                               mojom::URLResponseHeadPtr head,
                               std::string_view content,
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

  // If set, this is called when a new pending request arrives.
  base::OnceClosure on_new_pending_request_;

  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory> weak_wrapper_;

  Interceptor interceptor_;
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;
  size_t total_requests_ = 0;

  // Whether the pending URLLoader in `CreateLoaderAndStart()` should be bound
  // to observe the method invocations to it (e.g. FollowRedirect).
  const bool observe_loader_requests_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_URL_LOADER_FACTORY_H_
