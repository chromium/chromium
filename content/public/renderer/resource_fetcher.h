// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RESOURCE_FETCHER_H_
#define CONTENT_PUBLIC_RENDERER_RESOURCE_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/platform/web_url_request.h"

class GURL;

namespace base {
class TimeDelta;
}

namespace blink {
class WebLocalFrame;
class WebURLResponse;
}

namespace net {
struct NetworkTrafficAnnotationTag;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace content {

// Interface to download resources asynchronously.  Specified callback will be
// called asynchronously after the URL has been fetched, successfully or not.
// If there is a failure, response and data will both be empty.  |response| and
// |data| are both valid until the ResourceFetcher instance is destroyed.  If
// the instance is destroyed before the operation is finished, the request is
// canceled, and the callback will not be called.
class CONTENT_EXPORT ResourceFetcher {
 public:
  using Callback =
      base::OnceCallback<void(const blink::WebURLResponse& response,
                              const std::string& data)>;

  static constexpr size_t kDefaultMaximumDownloadSize = 1024 * 1024;

  virtual ~ResourceFetcher() {}

  // Creates a ResourceFetcher for the specified resource.
  static std::unique_ptr<ResourceFetcher> Create(const GURL& url);

  // Set the corresponding parameters of the request.  Must be called before
  // Start.  By default, requests are GETs with no body and respect the default
  // cache policy.
  virtual void SetMethod(const std::string& method) = 0;
  virtual void SetBody(const std::string& body) = 0;
  virtual void SetHeader(const std::string& header,
                         const std::string& value) = 0;

  // Starts the request using the specified frame.  Calls |callback| when
  // done.
  virtual void Start(
      blink::WebLocalFrame* frame,
      blink::mojom::RequestContextType request_context,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      Callback callback,
      size_t maximum_download_size = kDefaultMaximumDownloadSize) = 0;

  // Sets how long to wait for the server to reply.  By default, there is no
  // timeout.  Must be called after a request is started at most once.
  virtual void SetTimeout(const base::TimeDelta& timeout) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RESOURCE_FETCHER_H_
