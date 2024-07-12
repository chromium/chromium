// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_URL_LOADER_INTERCEPTOR_H_
#define CONTENT_PUBLIC_TEST_URL_LOADER_INTERCEPTOR_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace network {
class URLLoaderFactoryBuilder;
}  // namespace network

namespace content {

// Helper class to intercept URLLoaderFactory calls for tests.
// This intercepts:
//   -frame requests (which start from the browser)
//   -subresource requests from pages, dedicated workers, and shared workers
//     -by sending the renderer an intermediate URLLoaderFactory
//   -subresource requests from service workers and requests of non-installed
//    service worker scripts
//     -at EmbeddedWorkerInstance
//   -requests by the browser
//   -http(s)://mock.failed.request/foo URLs internally, copying the behavior
//    of net::URLRequestFailedJob
//
// Prefer not to use this class. In order of ease of use & simplicity:
//  -if you need to serve static data, use net::test::EmbeddedTestServer and
//   serve data from the source tree (e.g. in content/test/data).
//  -if you need to control the response data at runtime, then use
//   net::test_server::EmbeddedTestServer::RegisterRequestHandler.
//  -if you need to delay when the server sends the response, use
//   net::test_server::ControllableHttpResponse.
//  -otherwise, if you need full control over the net::Error and/or want to
//   inspect and/or modify the C++ structs used by URLLoader interface, then use
//   this helper class.
//
// Notes:
//  -the callback is called on the UI or IO threads depending on the factory
//   that was hooked
//    -this is done to avoid changing message order
//  -intercepting resource requests for subresources changes message order by
//   definition (since they would normally go directly from renderer->network
//   service, but now they're routed through the browser).
class URLLoaderInterceptor {
 public:
  struct RequestParams {
    RequestParams();
    ~RequestParams();
    RequestParams(RequestParams&& other);
    RequestParams& operator=(RequestParams&& other);
    // See the comment for `url_loader_factory::TerminalParams::process_id_`.
    int process_id;
    // The following are the parameters to CreateLoaderAndStart.
    mojo::PendingReceiver<network::mojom::URLLoader> receiver;
    int32_t request_id;
    uint32_t options;
    network::ResourceRequest url_request;
    mojo::Remote<network::mojom::URLLoaderClient> client;
    net::MutableNetworkTrafficAnnotationTag traffic_annotation;
  };
  // Function signature for intercept method.
  // Return true if the request was intercepted. Otherwise this class will
  // forward the request to the original URLLoaderFactory.
  using InterceptCallback =
      base::RepeatingCallback<bool(RequestParams* params)>;

  // Function signature for a loading completion method.
  // This class will listen on loading completion responses from the network,
  // invoke this callback, and delegate the response to the original client.
  using URLLoaderCompletionStatusCallback = base::RepeatingCallback<void(
      const GURL& request_url,
      const network::URLLoaderCompletionStatus& status)>;

  // Create an interceptor which calls |callback|. If |ready_callback| is not
  // provided, a nested RunLoop is used to ensure the interceptor is ready
  // before returning. If |ready_callback| is provided, no RunLoop is called,
  // and instead |ready_callback| is called after the interceptor is installed.
  // If provided, |completion_status_callback| is called when the load
  // completes.
  //
  // In order to hook up `completion_status_callback`, the interceptor wraps all
  // requests that the `intercept_callback` does not intercept, so destroying
  // the URLLoaderInterceptor aborts all non-intercepted requests.
  explicit URLLoaderInterceptor(
      InterceptCallback intercept_callback,
      const URLLoaderCompletionStatusCallback& completion_status_callback = {},
      base::OnceClosure ready_callback = {});

  URLLoaderInterceptor(const URLLoaderInterceptor&) = delete;
  URLLoaderInterceptor& operator=(const URLLoaderInterceptor&) = delete;

  ~URLLoaderInterceptor();

  // Serves static data, similar to net::test::EmbeddedTestServer, for
  // cases where you need a static origin, such as tests with origin trials.
  // Optional callback will notify callers for any accessed urls.
  static std::unique_ptr<URLLoaderInterceptor> ServeFilesFromDirectoryAtOrigin(
      const std::string& relative_base_path,
      const GURL& origin,
      base::RepeatingCallback<void(const GURL&)> callback = base::DoNothing());

  // Helper methods for use when intercepting.
  // Writes the given response body, header, and SSL Info to `client`.
  // If `url` is present, also computes the ParsedHeaders for the response.
  static void WriteResponse(std::string_view headers,
                            std::string_view body,
                            network::mojom::URLLoaderClient* client,
                            std::optional<net::SSLInfo> ssl_info = std::nullopt,
                            std::optional<GURL> url = std::nullopt);

  // Reads the given path, relative to the root source directory, and writes it
  // to |client|. For headers:
  //   1) if |headers| is specified, it's used
  //   2) otherwise if an adjoining file that ends in .mock-http-headers is
  //      found, its contents will be used
  //   3) otherwise a simple 200 response will be used, with a Content-Type
  //      guessed from the file extension
  // For SSL info, if |ssl_info| is specified, then it is added to the response.
  // If `url` is present, also computes the ParsedHeaders for the response.
  static void WriteResponse(const std::string& relative_path,
                            network::mojom::URLLoaderClient* client,
                            const std::string* headers = nullptr,
                            std::optional<net::SSLInfo> ssl_info = std::nullopt,
                            std::optional<GURL> url = std::nullopt);

  // Like above, but uses an absolute file path.
  static void WriteResponse(const base::FilePath& file_path,
                            network::mojom::URLLoaderClient* client,
                            const std::string* headers = nullptr,
                            std::optional<net::SSLInfo> ssl_info = std::nullopt,
                            std::optional<GURL> url = std::nullopt);

  // Returns an interceptor that (as long as it says alive) will intercept
  // requests to |url| and fail them using the provided |error|.
  // |ready_callback| is optional and avoids the use of RunLoop, see
  // the constructor for more detail.
  static std::unique_ptr<URLLoaderInterceptor> SetupRequestFailForURL(
      const GURL& url,
      net::Error error,
      base::OnceClosure ready_callback = {});

  // Returns the URL of the last request processed by this interceptor.
  //
  // Use this function instead of creating a WebContentsObserver to observe
  // request headers, if you need the last request url sent in the event of
  // resends or redirects, as the NavigationHandle::GetRequestHeaders() function
  // only returns the initial request's request headers.
  const GURL& GetLastRequestURL();

  // Returns the request headers of the last request processed by this
  // interceptor.
  //
  // Use this function instead of creating a WebContentsObserver to observe
  // request headers, if you need the last request headers sent in the event of
  // resends or redirects, as the NavigationHandle::GetRequestHeaders() function
  // only returns the initial request's request headers.
  const net::HttpRequestHeaders& GetLastRequestHeaders();

 private:
  class IOState;
  class Interceptor;
  class Wrapper;

  // Adds `this` as an interceptor when a `URLLoaderFactory` is about to be
  // created. `Wrapper` plumbs related objects to `Intercept()`.
  void InterceptorCallback(int process_id,
                           network::URLLoaderFactoryBuilder& factory_builder);

  // Attempts to intercept the given request, returning true if it was
  // intercepted.
  bool Intercept(RequestParams* params);

  // Called on IO thread at initialization and shutdown.
  void InitializeOnIOThread(base::OnceClosure closure);

  // Sets the request URL of the last request processed by this interceptor.
  void SetLastRequestURL(const GURL& url);

  // Sets the request headers of the last request processed by this interceptor.
  void SetLastRequestHeaders(const net::HttpRequestHeaders& headers);

  bool use_runloop_;
  base::OnceClosure ready_callback_;
  InterceptCallback callback_;
  scoped_refptr<IOState> io_thread_;
  // For intercepting non-frame requests from the browser process. There is one
  // per StoragePartition. Only accessed on UI thread.
  std::set<std::unique_ptr<Wrapper>> wrappers_on_ui_thread_;

  base::Lock last_request_lock_;
  GURL last_request_url_ GUARDED_BY(last_request_lock_);
  net::HttpRequestHeaders last_request_headers_ GUARDED_BY(last_request_lock_);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_URL_LOADER_INTERCEPTOR_H_
