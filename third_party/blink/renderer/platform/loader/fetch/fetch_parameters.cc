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
#include <utility>

#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// static
FetchParameters FetchParameters::CreateForTest(
    ResourceRequest resource_request) {
  return FetchParameters(std::move(resource_request),
                         ResourceLoaderOptions(/*world=*/nullptr));
}

FetchParameters::FetchParameters(ResourceRequest resource_request,
                                 ResourceLoaderOptions options)
    : resource_request_(std::move(resource_request)),
      decoder_options_(TextResourceDecoderOptions::kPlainTextContent),
      options_(std::move(options)) {}

FetchParameters::FetchParameters(FetchParameters&&) = default;

FetchParameters::~FetchParameters() = default;

void FetchParameters::SetCrossOriginAccessControl(
    const SecurityOrigin* origin,
    CrossOriginAttributeValue cross_origin) {
  switch (cross_origin) {
    case kCrossOriginAttributeNotSet:
      NOTREACHED_IN_MIGRATION();
      break;
    case kCrossOriginAttributeAnonymous:
      SetCrossOriginAccessControl(origin,
                                  network::mojom::CredentialsMode::kSameOrigin);
      break;
    case kCrossOriginAttributeUseCredentials:
      SetCrossOriginAccessControl(origin,
                                  network::mojom::CredentialsMode::kInclude);
      break;
  }
}

void FetchParameters::SetCrossOriginAccessControl(
    const SecurityOrigin* origin,
    network::mojom::CredentialsMode credentials_mode) {
  // Currently FetchParametersMode is only used when the request goes to
  // Service Worker.
  resource_request_.SetMode(network::mojom::RequestMode::kCors);
  resource_request_.SetCredentialsMode(credentials_mode);

  resource_request_.SetRequestorOrigin(origin);

  // TODO: Credentials should be removed only when the request is cross origin.
  resource_request_.RemoveUserAndPassFromURL();

  if (origin)
    resource_request_.SetHTTPOrigin(origin);
}

void FetchParameters::SetResourceWidth(
    const std::optional<float> resource_width) {
  resource_width_ = resource_width;
}

void FetchParameters::SetResourceHeight(
    const std::optional<float> resource_height) {
  resource_height_ = resource_height;
}

void FetchParameters::SetSpeculativePreloadType(
    SpeculativePreloadType speculative_preload_type) {
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

void FetchParameters::SetLazyImageDeferred() {
  DCHECK_EQ(ImageRequestBehavior::kNone, image_request_behavior_);
  image_request_behavior_ = ImageRequestBehavior::kDeferImageLoad;
}

void FetchParameters::SetLazyImageNonBlocking() {
  image_request_behavior_ = ImageRequestBehavior::kNonBlockingImage;
}

void FetchParameters::SetModuleScript() {
  DCHECK_EQ(mojom::blink::ScriptType::kClassic, script_type_);
  script_type_ = mojom::blink::ScriptType::kModule;
}

}  // namespace blink
