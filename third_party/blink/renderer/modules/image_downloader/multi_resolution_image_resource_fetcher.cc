// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/image_downloader/multi_resolution_image_resource_fetcher.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/web_associated_url_loader_impl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class MultiResolutionImageResourceFetcher::ClientImpl
    : public WebAssociatedURLLoaderClient {
  USING_FAST_MALLOC(MultiResolutionImageResourceFetcher::ClientImpl);

 public:
  explicit ClientImpl(StartCallback callback)
      : completed_(false), status_(kLoading), callback_(std::move(callback)) {}

  ClientImpl(const ClientImpl&) = delete;
  ClientImpl& operator=(const ClientImpl&) = delete;

  ~ClientImpl() override {}

  virtual void Cancel() { OnLoadCompleteInternal(kLoadFailed); }

  bool completed() const { return completed_; }

 private:
  enum LoadStatus {
    kLoading,
    kLoadFailed,
    kLoadSucceeded,
  };

  void OnLoadCompleteInternal(LoadStatus status) {
    DCHECK(!completed_);
    DCHECK_EQ(status_, kLoading);

    completed_ = true;
    status_ = status;

    if (callback_.is_null())
      return;
    std::move(callback_).Run(
        status_ == kLoadFailed ? WebURLResponse() : response_,
        status_ == kLoadFailed ? std::string() : data_);
  }

  // WebAssociatedURLLoaderClient methods:
  void DidReceiveResponse(const WebURLResponse& response) override {
    DCHECK(!completed_);
    response_ = response;
  }
  void DidReceiveData(base::span<const char> data) override {
    // The WebAssociatedURLLoader will continue after a load failure.
    // For example, for an Access Control error.
    if (completed_)
      return;
    DCHECK_GT(data.size(), 0u);

    data_.append(data.data(), data.size());
  }
  void DidFinishLoading() override {
    // The WebAssociatedURLLoader will continue after a load failure.
    // For example, for an Access Control error.
    if (completed_)
      return;
    OnLoadCompleteInternal(kLoadSucceeded);
  }
  void DidFail(const WebURLError& error) override {
    OnLoadCompleteInternal(kLoadFailed);
  }

 private:
  // Set to true once the request is complete.
  bool completed_;

  // Buffer to hold the content from the server.
  std::string data_;

  // A copy of the original resource response.
  WebURLResponse response_;

  LoadStatus status_;

  // Callback when we're done.
  StartCallback callback_;
};

MultiResolutionImageResourceFetcher::MultiResolutionImageResourceFetcher(
    const KURL& image_url,
    LocalFrame* frame,
    bool is_favicon,
    mojom::blink::FetchCacheMode cache_mode,
    Callback callback)
    : callback_(std::move(callback)),
      http_status_code_(0),
      request_(image_url) {
  WebAssociatedURLLoaderOptions options;
  SetLoaderOptions(options);

  if (is_favicon) {
    // To prevent cache tainting, the cross-origin favicon requests have to
    // by-pass the service workers. This should ideally not happen. But Chrome’s
    // FaviconDatabase is using the icon URL as a key of the "favicons" table.
    // So if we don't set the skip flag here, malicious service workers can
    // override the favicon image of any origins.
    if (!frame->DomWindow()->GetSecurityOrigin()->CanAccess(
            SecurityOrigin::Create(image_url).get())) {
      SetSkipServiceWorker(true);
    }
  }

  SetCacheMode(cache_mode);

  Start(frame, is_favicon, network::mojom::RequestMode::kNoCors,
        network::mojom::CredentialsMode::kInclude,
        WTF::BindOnce(&MultiResolutionImageResourceFetcher::OnURLFetchComplete,
                      WTF::Unretained(this)));
}

MultiResolutionImageResourceFetcher::~MultiResolutionImageResourceFetcher() {
  if (!loader_)
    return;

  DCHECK(client_);

  if (!client_->completed())
    loader_->Cancel();
}

void MultiResolutionImageResourceFetcher::OnURLFetchComplete(
    const WebURLResponse& response,
    const std::string& data) {
  if (!response.IsNull()) {
    http_status_code_ = response.HttpStatusCode();
    KURL url(response.CurrentRequestUrl());
    if (http_status_code_ == 200 || url.IsLocalFile()) {
      std::move(callback_).Run(this, data, response.MimeType());
      return;
    }
  }  // else case:
     // If we get here, it means there was no or an error response from the
     // server.

  std::move(callback_).Run(this, std::string(), WebString());
}

void MultiResolutionImageResourceFetcher::Dispose() {
  std::move(callback_).Run(this, std::string(), WebString());
}

void MultiResolutionImageResourceFetcher::SetSkipServiceWorker(
    bool skip_service_worker) {
  DCHECK(!request_.IsNull());
  DCHECK(!loader_);

  request_.SetSkipServiceWorker(skip_service_worker);
}

void MultiResolutionImageResourceFetcher::SetCacheMode(
    mojom::FetchCacheMode mode) {
  DCHECK(!request_.IsNull());
  DCHECK(!loader_);

  request_.SetCacheMode(mode);
}

void MultiResolutionImageResourceFetcher::SetLoaderOptions(
    const WebAssociatedURLLoaderOptions& options) {
  DCHECK(!request_.IsNull());
  DCHECK(!loader_);

  options_ = options;
}

void MultiResolutionImageResourceFetcher::Start(
    LocalFrame* frame,
    bool is_favicon,
    network::mojom::RequestMode request_mode,
    network::mojom::CredentialsMode credentials_mode,
    StartCallback callback) {
  DCHECK(!loader_);
  DCHECK(!client_);
  DCHECK(!request_.IsNull());
  if (!request_.HttpBody().IsNull())
    DCHECK_NE("GET", request_.HttpMethod().Utf8()) << "GETs can't have bodies.";

  mojom::blink::RequestContextType request_context =
      is_favicon ? mojom::blink::RequestContextType::FAVICON
                 : mojom::blink::RequestContextType::IMAGE;
  request_.SetRequestContext(request_context);
  request_.SetSiteForCookies(frame->GetDocument()->SiteForCookies());
  request_.SetMode(request_mode);
  request_.SetCredentialsMode(credentials_mode);
  request_.SetRequestDestination(network::mojom::RequestDestination::kImage);
  request_.SetFavicon(is_favicon);

  client_ = std::make_unique<ClientImpl>(std::move(callback));

  loader_ = std::make_unique<WebAssociatedURLLoaderImpl>(frame->DomWindow(),
                                                         options_);
  loader_->LoadAsynchronously(request_, client_.get());

  // No need to hold on to the request; reset it now.
  request_ = WebURLRequest();
}

void MultiResolutionImageResourceFetcher::Cancel() {
  loader_->Cancel();
  client_->Cancel();
}

}  // namespace blink
