// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/manifest_fetcher.h"

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"

namespace blink {

ManifestFetcher::ManifestFetcher(const KURL& url)
    : url_(url), completed_(false) {}

ManifestFetcher::~ManifestFetcher() = default;

void ManifestFetcher::Start(Document& document,
                            bool use_credentials,
                            ManifestFetcher::Callback callback) {
  callback_ = std::move(callback);

  ResourceRequest request(url_);
  request.SetRequestContext(mojom::RequestContextType::MANIFEST);
  request.SetMode(network::mojom::RequestMode::kCors);
  // See https://w3c.github.io/manifest/. Use "include" when use_credentials is
  // true, and "omit" otherwise.
  request.SetCredentialsMode(use_credentials
                                 ? network::mojom::CredentialsMode::kInclude
                                 : network::mojom::CredentialsMode::kOmit);

  ResourceLoaderOptions resource_loader_options;
  resource_loader_options.data_buffering_policy = kDoNotBufferData;

  loader_ = MakeGarbageCollected<ThreadableLoader>(document, this,
                                                   resource_loader_options);
  loader_->Start(request);
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

void ManifestFetcher::DidReceiveData(const char* data, unsigned length) {
  if (!length)
    return;

  if (!decoder_) {
    String encoding = response_.TextEncodingName();
    decoder_ = std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
        TextResourceDecoderOptions::kPlainTextContent,
        encoding.IsEmpty() ? UTF8Encoding() : WTF::TextEncoding(encoding)));
  }

  data_.Append(decoder_->Decode(data, length));
}

void ManifestFetcher::DidFinishLoading(uint64_t) {
  DCHECK(!completed_);
  completed_ = true;

  std::move(callback_).Run(response_, data_.ToString());
  data_.Clear();
}

void ManifestFetcher::DidFail(const ResourceError& error) {
  if (!callback_)
    return;

  data_.Clear();

  std::move(callback_).Run(response_, String());
}

void ManifestFetcher::DidFailRedirectCheck() {
  DidFail(ResourceError::Failure(NullURL()));
}

void ManifestFetcher::Trace(blink::Visitor* visitor) {
  visitor->Trace(loader_);
  ThreadableLoaderClient::Trace(visitor);
}

}  // namespace blink
