// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_H_

#include <memory>
#include <string>
#include <vector>

#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/devtools_observer.mojom-forward.h"
#include "services/network/public/mojom/shared_dictionary_access_observer.mojom.h"
#include "services/network/public/mojom/trust_token_access_observer.mojom-forward.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"

namespace net {
class HttpRequestHeaders;
}

namespace content {

class BrowserContext;
class NavigationUIData;
class NavigationURLLoaderDelegate;
class NavigationURLLoaderFactory;
class PrefetchedSignedExchangeCache;
class ServiceWorkerMainResourceHandle;
class StoragePartition;
struct NavigationRequestInfo;

// The navigation logic's UI thread entry point into the resource loading stack.
// It exposes an interface to control the request prior to receiving the
// response. If the NavigationURLLoader is destroyed before OnResponseStarted is
// called, the request is aborted.
class CONTENT_EXPORT NavigationURLLoader {
 public:
  enum class LoaderType {
    // Creates a regular NavigationURLLoader.
    kRegular,

    // Creates a noop NavigationURLLoader for BackForwardCache activation.
    kNoopForBackForwardCache,

    // Creates a noop NavigationURLLoader for Prerender activation.
    kNoopForPrerender,
  };

  // Creates a NavigationURLLoader. The caller is responsible for ensuring that
  // `delegate` outlives the loader. `request_body` must not be accessed on the
  // UI thread after this point.
  //
  // If `loader_type` is LoaderType::kNoopForBackForwardCache or
  // LoaderType::kNoopoForPrerender, a noop CachedNavigationURLLoader will be
  // returned.
  //
  // TODO(davidben): When navigation is disentangled from the loader, the
  // request parameters should not come in as a navigation-specific
  // structure. Information like `has_user_gesture` and
  // `should_replace_current_entry` in `request_info->common_params` shouldn't
  // be needed at this layer.
  static std::unique_ptr<NavigationURLLoader> Create(
      BrowserContext* browser_context,
      StoragePartition* storage_partition,
      std::unique_ptr<NavigationRequestInfo> request_info,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      ServiceWorkerMainResourceHandle* service_worker_handle,
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache,
      NavigationURLLoaderDelegate* delegate,
      LoaderType loader_type,
      mojo::PendingRemote<network::mojom::CookieAccessObserver> cookie_observer,
      mojo::PendingRemote<network::mojom::TrustTokenAccessObserver>
          trust_token_observer,
      mojo::PendingRemote<network::mojom::SharedDictionaryAccessObserver>
          shared_dictionary_observer,
      mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_observer,
      mojo::PendingRemote<network::mojom::DevToolsObserver> devtools_observer,
      network::mojom::URLResponseHeadPtr cached_response_head = nullptr,
      std::vector<std::unique_ptr<NavigationLoaderInterceptor>>
          initial_interceptors = {});

  // For testing purposes; sets the factory for use in testing. The factory is
  // not used for prerendered page activation as it needs to run a specific
  // loader to satisfy its unique requirement. See the implementation comment in
  // NavigationURLLoader::Create() for details.
  // TODO(crbug.com/40188852): Update this comment for restoration from
  // BackForwardCache when it also starts depending on the requirement.
  static void SetFactoryForTesting(NavigationURLLoaderFactory* factory);

  NavigationURLLoader(const NavigationURLLoader&) = delete;
  NavigationURLLoader& operator=(const NavigationURLLoader&) = delete;

  virtual ~NavigationURLLoader() {}

  // Called right after the loader is constructed.
  virtual void Start() = 0;

  // Called in response to OnRequestRedirected to continue processing the
  // request.
  virtual void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers) = 0;

  // Sets an overall request timeout for this navigation, which will cause the
  // navigation to fail if it expires before the navigation commits. This is
  // separate from any //net level timeouts. Returns `true` if the timeout was
  // started successfully. Repeated calls will be ignored (they won't reset the
  // timeout) and will return `false`.
  virtual bool SetNavigationTimeout(base::TimeDelta timeout) = 0;
  // Cancels the request timeout for this navigation. If the navigation is still
  // happening, it will continue as if the timer wasn't set. Otherwise, this is
  // a no-op.
  virtual void CancelNavigationTimeout() = 0;

  static uint32_t GetURLLoaderOptions(bool is_outermost_main_frame);

 protected:
  NavigationURLLoader() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_H_
