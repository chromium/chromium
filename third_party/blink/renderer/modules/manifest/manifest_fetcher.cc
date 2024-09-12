// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/manifest_fetcher.h"

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

namespace blink {

ManifestFetcher::ManifestFetcher(const KURL& url)
    : url_(url), completed_(false) {}

ManifestFetcher::~ManifestFetcher() = default;

void ManifestFetcher::Start(LocalDOMWindow& window,
                            bool use_credentials,
                            ResourceFetcher* resource_fetcher,
                            ManifestFetcher::Callback callback) {
  callback_ = std::move(callback);

  ResourceRequest request(url_);
  request.SetRequestContext(mojom::blink::RequestContextType::MANIFEST);
  request.SetRequestDestination(network::mojom::RequestDestination::kManifest);
  request.SetMode(network::mojom::RequestMode::kCors);
  request.SetTargetAddressSpace(network::mojom::IPAddressSpace::kUnknown);
  // See https://w3c.github.io/manifest/. Use "include" when use_credentials is
  // true, and "omit" otherwise.
  request.SetCredentialsMode(use_credentials
                                 ? network::mojom::CredentialsMode::kInclude
                                 : network::mojom::CredentialsMode::kOmit);

  ResourceLoaderOptions resource_loader_options(window.GetCurrentWorld());
  resource_loader_options.initiator_info.name =
      fetch_initiator_type_names::kLink;
  resource_loader_options.data_buffering_policy = kDoNotBufferData;

  loader_ = MakeGarbageCollected<ThreadableLoader>(
      window, this, resource_loader_options, resource_fetcher);
  loader_->Start(std::move(request));
}

void ManifestFetcher::Cancel() {
  if (!loader_)
    return;

  DCHECK(!completed_);

  ThreadableLoader* loader = loader_.Release();
  loader->Cancel();
}

void ManifestFetcher::DidReceiveResponse(uint64_t,
                                         const ResourceResponse& response) {
  response_ = response;
}

void ManifestFetcher::DidReceiveData(base::span<const char> data) {
  if (data.empty()) {
    return;
  }

  if (!decoder_) {
    String encoding = response_.TextEncodingName();
    decoder_ = std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
        TextResourceDecoderOptions::kPlainTextContent,
        encoding.empty() ? UTF8Encoding() : WTF::TextEncoding(encoding)));
  }

  data_.Append(decoder_->Decode(data));
}

void ManifestFetcher::DidFinishLoading(uint64_t) {
  DCHECK(!completed_);
  completed_ = true;

  std::move(callback_).Run(response_, data_.ToString());
  data_.Clear();
}

void ManifestFetcher::DidFail(uint64_t, const ResourceError& error) {
  if (!callback_)
    return;

  data_.Clear();

  std::move(callback_).Run(response_, String());
}

void ManifestFetcher::DidFailRedirectCheck(uint64_t identifier) {
  DidFail(identifier, ResourceError::Failure(NullURL()));
}

void ManifestFetcher::Trace(Visitor* visitor) const {
  visitor->Trace(loader_);
  ThreadableLoaderClient::Trace(visitor);
}

}  // namespace blink
