// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/common/content_export.h"
#include "content/public/common/previews_state.h"

namespace net {
class HttpRequestHeaders;
}

namespace content {

class AppCacheNavigationHandle;
class BrowserContext;
class NavigationUIData;
class NavigationURLLoaderDelegate;
class NavigationURLLoaderFactory;
class PrefetchedSignedExchangeCache;
class ServiceWorkerNavigationHandle;
class StoragePartition;
struct NavigationRequestInfo;

// The navigation logic's UI thread entry point into the resource loading stack.
// It exposes an interface to control the request prior to receiving the
// response. If the NavigationURLLoader is destroyed before OnResponseStarted is
// called, the request is aborted.
class CONTENT_EXPORT NavigationURLLoader {
 public:
  // Creates a NavigationURLLoader. The caller is responsible for ensuring that
  // |delegate| outlives the loader. |request_body| must not be accessed on the
  // UI thread after this point.
  //
  // If |is_served_from_back_forward_cache| is true, a dummy
  // CachedNavigationURLLoader will be returned.
  //
  // TODO(davidben): When navigation is disentangled from the loader, the
  // request parameters should not come in as a navigation-specific
  // structure. Information like has_user_gesture and
  // should_replace_current_entry shouldn't be needed at this layer.
  static std::unique_ptr<NavigationURLLoader> Create(
      BrowserContext* browser_context,
      StoragePartition* storage_partition,
      std::unique_ptr<NavigationRequestInfo> request_info,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      ServiceWorkerNavigationHandle* service_worker_handle,
      AppCacheNavigationHandle* appcache_handle,
      scoped_refptr<PrefetchedSignedExchangeCache>
          prefetched_signed_exchange_cache,
      NavigationURLLoaderDelegate* delegate,
      bool is_served_from_back_forward_cache,
      std::vector<std::unique_ptr<NavigationLoaderInterceptor>>
          initial_interceptors = {});

  // For testing purposes; sets the factory for use in testing.
  static void SetFactoryForTesting(NavigationURLLoaderFactory* factory);

  virtual ~NavigationURLLoader() {}

  // Called in response to OnRequestRedirected to continue processing the
  // request. |new_previews_state| will be updated for newly created URLLoaders,
  // but the existing default URLLoader will not see |new_previews_state| unless
  // the URLLoader happens to be reset.
  virtual void FollowRedirect(const std::vector<std::string>& removed_headers,
                              const net::HttpRequestHeaders& modified_headers,
                              PreviewsState new_previews_state) = 0;

 protected:
  NavigationURLLoader() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NavigationURLLoader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_H_
