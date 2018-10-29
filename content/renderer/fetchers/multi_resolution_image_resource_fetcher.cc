// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/fetchers/multi_resolution_image_resource_fetcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/child/image_decoder.h"
#include "content/public/renderer/associated_resource_fetcher.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

using blink::WebLocalFrame;
using blink::WebAssociatedURLLoaderOptions;
using blink::WebURLRequest;
using blink::WebURLResponse;

namespace content {

MultiResolutionImageResourceFetcher::MultiResolutionImageResourceFetcher(
    const GURL& image_url,
    WebLocalFrame* frame,
    int id,
    blink::mojom::RequestContextType request_context,
    blink::mojom::FetchCacheMode cache_mode,
    Callback callback)
    : callback_(std::move(callback)),
      id_(id),
      http_status_code_(0),
      image_url_(image_url) {
  fetcher_.reset(AssociatedResourceFetcher::Create(image_url));

  WebAssociatedURLLoaderOptions options;
  fetcher_->SetLoaderOptions(options);

  // To prevent cache tainting, the favicon requests have to by-pass the service
  // workers. This should ideally not happen or at least not all the time.
  // See https://crbug.com/448427
  if (request_context == blink::mojom::RequestContextType::FAVICON)
    fetcher_->SetSkipServiceWorker(true);

  fetcher_->SetCacheMode(cache_mode);

  fetcher_->Start(
      frame, request_context, network::mojom::FetchRequestMode::kNoCORS,
      network::mojom::FetchCredentialsMode::kInclude,
      network::mojom::RequestContextFrameType::kNone,
      base::Bind(&MultiResolutionImageResourceFetcher::OnURLFetchComplete,
                 base::Unretained(this)));
}

MultiResolutionImageResourceFetcher::~MultiResolutionImageResourceFetcher() {
}

void MultiResolutionImageResourceFetcher::OnURLFetchComplete(
    const WebURLResponse& response,
    const std::string& data) {
  std::vector<SkBitmap> bitmaps;
  if (!response.IsNull()) {
    http_status_code_ = response.HttpStatusCode();
    GURL url(response.Url());
    if (http_status_code_ == 200 || url.SchemeIsFile()) {
      // Request succeeded, try to convert it to an image.
      bitmaps = ImageDecoder::DecodeAll(
          reinterpret_cast<const unsigned char*>(data.data()), data.size());
    }
  } // else case:
    // If we get here, it means no image from server or couldn't decode the
    // response as an image. The delegate will see an empty vector.

  // Take local ownership of the callback as running the callback may lead to
  // our destruction.
  base::ResetAndReturn(&callback_).Run(this, bitmaps);
}

void MultiResolutionImageResourceFetcher::OnRenderFrameDestruct() {
  // Take local ownership of the callback as running the callback may lead to
  // our destruction.
  base::ResetAndReturn(&callback_).Run(this, std::vector<SkBitmap>());
}

}  // namespace content
