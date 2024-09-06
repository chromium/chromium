// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/loader/url_loader.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "net/base/net_errors.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_util.h"
#include "pdf/loader/result_codes.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_http_header_visitor.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "url/gurl.h"

namespace chrome_pdf {

namespace {

// Taken from `content/renderer/pepper/url_response_info_util.cc`.
class HeadersToString final : public blink::WebHTTPHeaderVisitor {
 public:
  explicit HeadersToString(std::string& buffer_ref) : buffer_ref_(buffer_ref) {}

  void VisitHeader(const blink::WebString& name,
                   const blink::WebString& value) override {
    if (!buffer_ref_->empty())
      buffer_ref_->append("\n");
    buffer_ref_->append(name.Utf8());
    buffer_ref_->append(": ");
    buffer_ref_->append(value.Utf8());
  }

 private:
  // Reference allows writing directly into `UrlResponse::headers`.
  const raw_ref<std::string, DanglingUntriaged> buffer_ref_;
};

}  // namespace

UrlRequest::UrlRequest() = default;
UrlRequest::UrlRequest(const UrlRequest& other) = default;
UrlRequest::UrlRequest(UrlRequest&& other) noexcept = default;
UrlRequest& UrlRequest::operator=(const UrlRequest& other) = default;
UrlRequest& UrlRequest::operator=(UrlRequest&& other) noexcept = default;
UrlRequest::~UrlRequest() = default;

UrlResponse::UrlResponse() = default;
UrlResponse::UrlResponse(const UrlResponse& other) = default;
UrlResponse::UrlResponse(UrlResponse&& other) noexcept = default;
UrlResponse& UrlResponse::operator=(const UrlResponse& other) = default;
UrlResponse& UrlResponse::operator=(UrlResponse&& other) noexcept = default;
UrlResponse::~UrlResponse() = default;

UrlLoader::UrlLoader(base::WeakPtr<Client> client)
    : client_(std::move(client)) {}

UrlLoader::~UrlLoader() = default;

// Modeled on `content::PepperURLLoaderHost::OnHostMsgOpen()`.
void UrlLoader::Open(const UrlRequest& request,
                     base::OnceCallback<void(int)> callback) {
  DCHECK_EQ(state_, LoadingState::kWaitingToOpen);
  DCHECK(callback);
  state_ = LoadingState::kOpening;
  open_callback_ = std::move(callback);

  if (!client_ || !client_->IsValid()) {
    AbortLoad(Result::kErrorFailed);
    return;
  }

  // Modeled on `content::CreateWebURLRequest()`.
  // TODO(crbug.com/40149338): The original code performs additional validations
  // that we probably don't need in the new process model.
  blink::WebURLRequest blink_request;
  blink_request.SetUrl(
      client_->CompleteURL(blink::WebString::FromUTF8(request.url)));
  blink_request.SetHttpMethod(blink::WebString::FromASCII(request.method));

  blink_request.SetSiteForCookies(client_->SiteForCookies());
  blink_request.SetSkipServiceWorker(true);

  // Note: The PDF plugin doesn't set the `X-Requested-With` header.
  if (!request.headers.empty()) {
    net::HttpUtil::HeadersIterator it(request.headers.begin(),
                                      request.headers.end(), "\n\r");
    while (it.GetNext()) {
      blink_request.AddHttpHeaderField(blink::WebString::FromUTF8(it.name()),
                                       blink::WebString::FromUTF8(it.values()));
    }
  }

  if (!request.body.empty()) {
    blink::WebHTTPBody body;
    body.Initialize();
    body.AppendData(request.body);
    blink_request.SetHttpBody(body);
  }

  if (!request.custom_referrer_url.empty()) {
    client_->SetReferrerForRequest(blink_request,
                                   GURL(request.custom_referrer_url));
  }

  buffer_lower_threshold_ = request.buffer_lower_threshold;
  buffer_upper_threshold_ = request.buffer_upper_threshold;
  DCHECK_GT(buffer_lower_threshold_, 0u);
  DCHECK_LE(buffer_lower_threshold_, buffer_upper_threshold_);

  blink_request.SetRequestContext(blink::mojom::RequestContextType::PLUGIN);
  blink_request.SetRequestDestination(
      network::mojom::RequestDestination::kEmbed);

  // TODO(crbug.com/40567141): Revisit whether we need universal access.
  blink::WebAssociatedURLLoaderOptions options;
  options.grant_universal_access = true;
  ignore_redirects_ = request.ignore_redirects;
  blink_loader_ = client_->CreateAssociatedURLLoader(options);
  blink_loader_->LoadAsynchronously(blink_request, this);
}

// Modeled on `ppapi::proxy::URLLoaderResource::ReadResponseBody()`.
void UrlLoader::ReadResponseBody(base::span<char> buffer,
                                 base::OnceCallback<void(int)> callback) {
  // Can be in `kLoadComplete` if still reading after loading finished.
  DCHECK(state_ == LoadingState::kStreamingData ||
         state_ == LoadingState::kLoadComplete)
      << static_cast<int>(state_);

  if (buffer.empty()) {
    std::move(callback).Run(Result::kErrorBadArgument);
    return;
  }

  DCHECK(!read_callback_);
  DCHECK(callback);
  read_callback_ = std::move(callback);
  client_buffer_ = buffer;

  if (!buffer_.empty() || state_ == LoadingState::kLoadComplete)
    RunReadCallback();
}

// Modeled on `ppapi::proxy::URLLoadResource::Close()`.
void UrlLoader::Close() {
  if (state_ != LoadingState::kLoadComplete)
    AbortLoad(Result::kErrorAborted);
}

// Modeled on `content::PepperURLLoaderHost::WillFollowRedirect()`.
bool UrlLoader::WillFollowRedirect(
    const blink::WebURL& new_url,
    const blink::WebURLResponse& redirect_response) {
  DCHECK_EQ(state_, LoadingState::kOpening);

  // TODO(crbug.com/40149338): The original code performs additional validations
  // that we probably don't need in the new process model.

  // Note that `pp::URLLoader::FollowRedirect()` is not supported, so the
  // redirect can be canceled immediately by returning `false` here.
  return !ignore_redirects_;
}

void UrlLoader::DidSendData(uint64_t bytes_sent,
                            uint64_t total_bytes_to_be_sent) {
  // Doesn't apply to PDF viewer requests.
  NOTREACHED();
}

// Modeled on `content::PepperURLLoaderHost::DidReceiveResponse()`.
void UrlLoader::DidReceiveResponse(const blink::WebURLResponse& response) {
  DCHECK_EQ(state_, LoadingState::kOpening);

  // Modeled on `content::DataFromWebURLResponse()`.
  response_.status_code = response.HttpStatusCode();

  HeadersToString headers_to_string(response_.headers);
  response.VisitHttpHeaderFields(&headers_to_string);

  state_ = LoadingState::kStreamingData;
  std::move(open_callback_).Run(Result::kSuccess);
}

void UrlLoader::DidDownloadData(uint64_t data_length) {
  // Doesn't apply to PDF viewer requests.
  NOTREACHED();
}

// Modeled on `content::PepperURLLoaderHost::DidReceiveData()`.
void UrlLoader::DidReceiveData(base::span<const char> data) {
  DCHECK_EQ(state_, LoadingState::kStreamingData);

  // It's surprisingly difficult to guarantee that this is always >0.
  if (data.empty()) {
    return;
  }

  buffer_.insert(buffer_.end(), data.begin(), data.end());

  // Defer loading if the buffer is too full.
  if (!deferring_loading_ && buffer_.size() >= buffer_upper_threshold_) {
    deferring_loading_ = true;
    blink_loader_->SetDefersLoading(true);
  }

  RunReadCallback();
}

// Modeled on `content::PepperURLLoaderHost::DidFinishLoading()`.
void UrlLoader::DidFinishLoading() {
  DCHECK_EQ(state_, LoadingState::kStreamingData);

  SetLoadComplete(Result::kSuccess);
  RunReadCallback();
}

// Modeled on `content::PepperURLLoaderHost::DidFail()`.
void UrlLoader::DidFail(const blink::WebURLError& error) {
  DCHECK(state_ == LoadingState::kOpening ||
         state_ == LoadingState::kStreamingData)
      << static_cast<int>(state_);

  int32_t pp_error = Result::kErrorFailed;
  switch (error.reason()) {
    case net::ERR_ACCESS_DENIED:
    case net::ERR_NETWORK_ACCESS_DENIED:
      pp_error = Result::kErrorNoAccess;
      break;

    default:
      if (error.is_web_security_violation())
        pp_error = Result::kErrorNoAccess;
      break;
  }

  AbortLoad(pp_error);
}

void UrlLoader::AbortLoad(int32_t result) {
  DCHECK_LT(result, 0);

  SetLoadComplete(result);
  buffer_.clear();

  if (open_callback_) {
    DCHECK(!read_callback_);
    std::move(open_callback_).Run(complete_result_);
  } else if (read_callback_) {
    RunReadCallback();
  }
}

// Modeled on `ppapi::proxy::URLLoaderResource::FillUserBuffer()`.
void UrlLoader::RunReadCallback() {
  if (!read_callback_)
    return;

  DCHECK(!client_buffer_.empty());
  int32_t num_bytes = std::min(
      {buffer_.size(), client_buffer_.size(), static_cast<size_t>(INT32_MAX)});
  if (num_bytes > 0) {
    auto read_begin = buffer_.begin();
    auto read_end = read_begin + num_bytes;
    std::copy(read_begin, read_end, client_buffer_.data());
    buffer_.erase(read_begin, read_end);

    // Resume loading if the buffer is too empty.
    if (deferring_loading_ && buffer_.size() <= buffer_lower_threshold_) {
      deferring_loading_ = false;
      blink_loader_->SetDefersLoading(false);
    }
  } else {
    DCHECK_EQ(state_, LoadingState::kLoadComplete);
    num_bytes = complete_result_;
    DCHECK_LE(num_bytes, 0);
    static_assert(Result::kSuccess == 0,
                  "Result::kSuccess should be equivalent to 0 bytes");
  }

  client_buffer_ = {};
  std::move(read_callback_).Run(num_bytes);
}

void UrlLoader::SetLoadComplete(int32_t result) {
  DCHECK_NE(state_, LoadingState::kLoadComplete);
  DCHECK_LE(result, 0);

  state_ = LoadingState::kLoadComplete;
  complete_result_ = result;
  blink_loader_.reset();
}

}  // namespace chrome_pdf
