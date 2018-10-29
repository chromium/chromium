// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_type.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace network {
struct ResourceResponse;
}

namespace content {

class AppCacheService;
class NavigationData;
class ResourceContext;
class ResourceThrottle;
struct StreamInfo;

// Interface that the embedder provides to ResourceDispatcherHost to allow
// observing and modifying requests.
class CONTENT_EXPORT ResourceDispatcherHostDelegate {
 public:
  virtual ~ResourceDispatcherHostDelegate();

  // Called after ShouldBeginRequest to allow the embedder to add resource
  // throttles.
  virtual void RequestBeginning(
      net::URLRequest* request,
      ResourceContext* resource_context,
      AppCacheService* appcache_service,
      ResourceType resource_type,
      std::vector<std::unique_ptr<ResourceThrottle>>* throttles);

  // Allows an embedder to add additional resource handlers for a download.
  // |must_download| is set if the request must be handled as a download.
  // |is_new_request| is true if this is a call for a new, unstarted request
  // which also means that RequestBeginning has not been and will not be
  // called for this request.
  virtual void DownloadStarting(
      net::URLRequest* request,
      ResourceContext* resource_context,
      bool is_content_initiated,
      bool must_download,
      bool is_new_request,
      std::vector<std::unique_ptr<ResourceThrottle>>* throttles);

  // Returns true and sets |origin| if a Stream should be created for the
  // resource. If true is returned, a new Stream will be created and
  // OnStreamCreated() will be called with a StreamHandle instance for the
  // Stream. The handle contains the URL for reading the Stream etc. The
  // Stream's origin will be set to |origin|.
  //
  // If the stream will be rendered in a BrowserPlugin, |payload| will contain
  // the data that should be given to the old ResourceHandler to forward to the
  // renderer process.
  virtual bool ShouldInterceptResourceAsStream(
      net::URLRequest* request,
      const std::string& mime_type,
      GURL* origin,
      std::string* payload);

  // Informs the delegate that a Stream was created. The Stream can be read from
  // the blob URL of the Stream, but can only be read once.
  virtual void OnStreamCreated(net::URLRequest* request,
                               std::unique_ptr<content::StreamInfo> stream);

  // Informs the delegate that a response has started.
  virtual void OnResponseStarted(net::URLRequest* request,
                                 ResourceContext* resource_context,
                                 network::ResourceResponse* response);

  // Informs the delegate that a request has been redirected.
  virtual void OnRequestRedirected(const GURL& redirect_url,
                                   net::URLRequest* request,
                                   ResourceContext* resource_context,
                                   network::ResourceResponse* response);

  // Notification that a request has completed.
  virtual void RequestComplete(net::URLRequest* url_request, int net_error);
  // Deprecated.
  // TODO(maksims): Remove this once all the callers are modified.
  virtual void RequestComplete(net::URLRequest* url_request);

  // Asks the embedder for NavigationData related to this request. It is only
  // called for navigation requests.
  virtual NavigationData* GetNavigationData(net::URLRequest* request) const;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_DELEGATE_H_
