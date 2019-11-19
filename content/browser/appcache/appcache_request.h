// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_REQUEST_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_REQUEST_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "url/gurl.h"

namespace net {
struct RedirectInfo;
class HttpRequestHeaders;
}  // namespace net

namespace network {
struct ResourceRequest;
}

namespace content {

// Interface for an AppCache request. Subclasses implement this interface to
// wrap custom request objects like URLRequest, etc to ensure that these
// dependencies stay out of the AppCache code.
class CONTENT_EXPORT AppCacheRequest {
 public:
  explicit AppCacheRequest(const network::ResourceRequest& request);
  ~AppCacheRequest();

  // The URL for this request.
  const GURL& GetURL() const { return request_.url; }

  // The method for this request
  const std::string& GetMethod() const { return request_.method; }

  // Used for cookie policy.
  const GURL& GetSiteForCookies() const { return request_.site_for_cookies; }

  // The referrer for this request.
  const GURL GetReferrer() const { return request_.referrer; }

  // The HTTP headers of this request.
  const net::HttpRequestHeaders& GetHeaders() const { return request_.headers; }

  // The resource type of this request.
  int GetResourceType() const { return request_.resource_type; }

  // Returns true if the request was successful.
  bool IsSuccess() const;

  // Returns true if the request was cancelled.
  bool IsCancelled() const { return false; }

  // Returns true if the request had an error.
  bool IsError() const { return false; }

  // Returns the HTTP response code.
  int GetResponseCode() const;

  // Get response header(s) by name. Returns an empty string if the header
  // wasn't found,
  std::string GetResponseHeaderByName(const std::string& name) const;

  void UpdateWithRedirectInfo(const net::RedirectInfo& redirect_info);

  void set_request(const network::ResourceRequest& request);
  void set_response(const network::ResourceResponseHead& response);

  base::WeakPtr<AppCacheRequest> GetWeakPtr();

  // Returns true if the scheme and method are supported for AppCache.
  static bool IsSchemeAndMethodSupportedForAppCache(
      const AppCacheRequest* request);

 protected:
  friend class AppCacheRequestHandler;
  // Enables the AppCacheJob to call GetResourceRequest().
  friend class AppCacheJob;

  // Returns the underlying ResourceRequest.
  network::ResourceRequest* GetResourceRequest() { return &request_; }

 private:
  network::ResourceRequest request_;
  network::ResourceResponseHead response_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AppCacheRequest> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppCacheRequest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_REQUEST_H_
