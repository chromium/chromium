// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_FETCHERS_MULTI_RESOLUTION_IMAGE_RESOURCE_FETCHER_H_
#define CONTENT_RENDERER_FETCHERS_MULTI_RESOLUTION_IMAGE_RESOURCE_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "url/gurl.h"

class SkBitmap;

namespace blink {
class WebLocalFrame;
class WebURLResponse;
}

namespace content {

class AssociatedResourceFetcher;

// A resource fetcher that returns all (differently-sized) frames in
// an image. Useful for favicons.
class MultiResolutionImageResourceFetcher {
 public:
  using Callback = base::OnceCallback<void(MultiResolutionImageResourceFetcher*,
                                           const std::vector<SkBitmap>&)>;

  MultiResolutionImageResourceFetcher(
      const GURL& image_url,
      blink::WebLocalFrame* frame,
      int id,
      blink::mojom::RequestContextType request_context,
      blink::mojom::FetchCacheMode cache_mode,
      Callback callback);

  virtual ~MultiResolutionImageResourceFetcher();

  // URL of the image we're downloading.
  const GURL& image_url() const { return image_url_; }

  // Unique identifier for the request.
  int id() const { return id_; }

  // HTTP status code upon fetch completion.
  int http_status_code() const { return http_status_code_; }

  // Called when associated RenderFrame is destructed.
  void OnRenderFrameDestruct();

 private:
  // ResourceFetcher::Callback. Decodes the image and invokes callback_.
  void OnURLFetchComplete(const blink::WebURLResponse& response,
                          const std::string& data);

  Callback callback_;

  // Unique identifier for the request.
  const int id_;

  // HTTP status code upon fetch completion.
  int http_status_code_;

  // URL of the image.
  const GURL image_url_;

  // Does the actual download.
  std::unique_ptr<AssociatedResourceFetcher> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(MultiResolutionImageResourceFetcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_FETCHERS_MULTI_RESOLUTION_IMAGE_RESOURCE_FETCHER_H_
