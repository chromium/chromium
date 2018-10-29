// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/fetchers/manifest_fetcher.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/public/renderer/associated_resource_fetcher.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {

ManifestFetcher::ManifestFetcher(const GURL& url)
    : completed_(false) {
  fetcher_.reset(AssociatedResourceFetcher::Create(url));
}

ManifestFetcher::~ManifestFetcher() {
  if (!completed_)
    Cancel();
}

void ManifestFetcher::Start(blink::WebLocalFrame* frame,
                            bool use_credentials,
                            const Callback& callback) {
  callback_ = callback;

  blink::WebAssociatedURLLoaderOptions options;
  fetcher_->SetLoaderOptions(options);

  // See https://w3c.github.io/manifest/. Use "include" when use_credentials is
  // true, and "omit" otherwise.
  fetcher_->Start(
      frame, blink::mojom::RequestContextType::MANIFEST,
      network::mojom::FetchRequestMode::kCORS,
      use_credentials ? network::mojom::FetchCredentialsMode::kInclude
                      : network::mojom::FetchCredentialsMode::kOmit,
      network::mojom::RequestContextFrameType::kNone,
      base::Bind(&ManifestFetcher::OnLoadComplete, base::Unretained(this)));
}

void ManifestFetcher::Cancel() {
  DCHECK(!completed_);
  fetcher_->Cancel();
}

void ManifestFetcher::OnLoadComplete(const blink::WebURLResponse& response,
                                     const std::string& data) {
  DCHECK(!completed_);
  completed_ = true;

  Callback callback = callback_;
  std::move(callback).Run(response, data);
}

}  // namespace content
