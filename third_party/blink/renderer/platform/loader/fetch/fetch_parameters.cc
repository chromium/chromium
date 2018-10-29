/*
 * Copyright (C) 2012 Google, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"

#include <memory>

#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

FetchParameters::FetchParameters(const ResourceRequest& resource_request)
    : resource_request_(resource_request),
      decoder_options_(TextResourceDecoderOptions::kPlainTextContent),
      speculative_preload_type_(SpeculativePreloadType::kNotSpeculative),
      defer_(kNoDefer),
      image_request_optimization_(kNone) {}

FetchParameters::FetchParameters(const ResourceRequest& resource_request,
                                 const ResourceLoaderOptions& options)
    : resource_request_(resource_request),
      decoder_options_(TextResourceDecoderOptions::kPlainTextContent),
      options_(options),
      speculative_preload_type_(SpeculativePreloadType::kNotSpeculative),
      defer_(kNoDefer),
      image_request_optimization_(kNone) {}

FetchParameters::~FetchParameters() = default;

void FetchParameters::SetCrossOriginAccessControl(
    const SecurityOrigin* origin,
    CrossOriginAttributeValue cross_origin) {
  switch (cross_origin) {
    case kCrossOriginAttributeNotSet:
      NOTREACHED();
      break;
    case kCrossOriginAttributeAnonymous:
      SetCrossOriginAccessControl(
          origin, network::mojom::FetchCredentialsMode::kSameOrigin);
      break;
    case kCrossOriginAttributeUseCredentials:
      SetCrossOriginAccessControl(
          origin, network::mojom::FetchCredentialsMode::kInclude);
      break;
  }
}

void FetchParameters::SetCrossOriginAccessControl(
    const SecurityOrigin* origin,
    network::mojom::FetchCredentialsMode credentials_mode) {
  // Currently FetchParametersMode is only used when the request goes to
  // Service Worker.
  resource_request_.SetFetchRequestMode(
      network::mojom::FetchRequestMode::kCORS);
  resource_request_.SetFetchCredentialsMode(credentials_mode);

  resource_request_.SetRequestorOrigin(origin);

  // TODO: Credentials should be removed only when the request is cross origin.
  resource_request_.RemoveUserAndPassFromURL();

  if (origin)
    resource_request_.SetHTTPOrigin(origin);
}

void FetchParameters::SetResourceWidth(ResourceWidth resource_width) {
  if (resource_width.is_set) {
    resource_width_.width = resource_width.width;
    resource_width_.is_set = true;
  }
}

void FetchParameters::SetSpeculativePreloadType(
    SpeculativePreloadType speculative_preload_type,
    double discovery_time) {
  speculative_preload_type_ = speculative_preload_type;
}

void FetchParameters::MakeSynchronous() {
  // Synchronous requests should always be max priority, lest they hang the
  // renderer.
  resource_request_.SetPriority(ResourceLoadPriority::kHighest);
  // Skip ServiceWorker for synchronous loads from the main thread to avoid
  // deadlocks.
  if (IsMainThread())
    resource_request_.SetSkipServiceWorker(true);
  options_.synchronous_policy = kRequestSynchronously;
}

void FetchParameters::SetClientLoFiPlaceholder() {
  resource_request_.SetPreviewsState(resource_request_.GetPreviewsState() |
                                     WebURLRequest::kClientLoFiOn);
  SetAllowImagePlaceholder();
}

void FetchParameters::SetLazyImagePlaceholder() {
  resource_request_.SetPreviewsState(resource_request_.GetPreviewsState() |
                                     WebURLRequest::kLazyImageLoadDeferred);
  SetAllowImagePlaceholder();
}

void FetchParameters::SetAllowImagePlaceholder() {
  DCHECK_EQ(kNone, image_request_optimization_);
  if (!resource_request_.Url().ProtocolIsInHTTPFamily() ||
      resource_request_.HttpMethod() != "GET" ||
      !resource_request_.HttpHeaderField("range").IsNull()) {
    // Make sure that the request isn't marked as using Client Lo-Fi, since
    // without loading an image placeholder, Client Lo-Fi isn't really in use.
    resource_request_.SetPreviewsState(resource_request_.GetPreviewsState() &
                                       ~(WebURLRequest::kClientLoFiOn));
    return;
  }

  image_request_optimization_ = kAllowPlaceholder;

  // Fetch the first few bytes of the image. This number is tuned to both (a)
  // likely capture the entire image for small images and (b) likely contain
  // the dimensions for larger images.
  // TODO(sclittle): Calculate the optimal value for this number.
  resource_request_.SetHTTPHeaderField("range", "bytes=0-2047");

  // TODO(sclittle): Indicate somehow (e.g. through a new request bit) to the
  // embedder that it should return the full resource if the entire resource is
  // fresh in the cache.
}

}  // namespace blink
