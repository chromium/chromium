// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REQUEST_HANDLER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REQUEST_HANDLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/common/content_export.h"

class GURL;

namespace network {
struct ResourceRequest;
}  // namespace network

namespace content {

class ServiceWorkerNavigationHandle;
struct NavigationRequestInfo;

// Contains factory methods for creating the NavigationLoaderInterceptors for
// routing requests to service workers.
// TODO(falken): This is just a collection of static functions. Move these to
// non-member functions?
class CONTENT_EXPORT ServiceWorkerRequestHandler {
 public:
  // Returns a loader interceptor for a navigation. May return nullptr if the
  // navigation cannot use service workers. Called on the UI thread.
  static std::unique_ptr<NavigationLoaderInterceptor> CreateForNavigation(
      const GURL& url,
      base::WeakPtr<ServiceWorkerNavigationHandle> navigation_handle,
      const NavigationRequestInfo& request_info);

  // Returns a loader interceptor for a dedicated worker or shared worker. May
  // return nullptr if the worker cannot use service workers. Called on the UI
  // thread.
  static std::unique_ptr<NavigationLoaderInterceptor> CreateForWorker(
      const network::ResourceRequest& resource_request,
      int process_id,
      base::WeakPtr<ServiceWorkerNavigationHandle> navigation_handle);

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRequestHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REQUEST_HANDLER_H_
