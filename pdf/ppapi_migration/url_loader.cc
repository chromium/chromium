// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/url_loader.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "net/base/net_errors.h"
#include "pdf/ppapi_migration/callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/cpp/var.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "url/gurl.h"

namespace chrome_pdf {

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

UrlLoader::UrlLoader() = default;
UrlLoader::~UrlLoader() = default;

BlinkUrlLoader::BlinkUrlLoader(base::WeakPtr<Client> client)
    : client_(std::move(client)) {}

BlinkUrlLoader::~BlinkUrlLoader() = default;

// Modeled on `content::PepperURLLoaderHost::OnHostMsgGrantUniversalAccess()`.
void BlinkUrlLoader::GrantUniversalAccess() {
  DCHECK_EQ(state_, LoadingState::kWaitingToOpen);
  grant_universal_access_ = true;
}

// Modeled on `content::PepperURLLoaderHost::OnHostMsgOpen()`.
void BlinkUrlLoader::Open(const UrlRequest& request, ResultCallback callback) {
  DCHECK_EQ(state_, LoadingState::kWaitingToOpen);
  DCHECK(callback);
  state_ = LoadingState::kOpening;
  open_callback_ = std::move(callback);

  if (!client_) {
    AbortLoad(PP_ERROR_FAILED);
    return;
  }

  // Modeled on `content::CreateWebURLRequest()`.
  blink::WebURLRequest blink_request;
  blink_request.SetUrl(GURL(request.url));
  blink_request.SetHttpMethod(blink::WebString::FromASCII(request.method));

  blink_request.SetRequestContext(blink::mojom::RequestContextType::PLUGIN);
  blink_request.SetRequestDestination(
      network::mojom::RequestDestination::kEmbed);

  blink::WebAssociatedURLLoaderOptions options;
  options.grant_universal_access = grant_universal_access_;
  blink_loader_ = client_->CreateAssociatedURLLoader(options);
  if (!blink_loader_) {
    AbortLoad(PP_ERROR_FAILED);
    return;
  }

  blink_loader_->LoadAsynchronously(blink_request, this);
}

bool BlinkUrlLoader::GetDownloadProgress(
    int64_t& bytes_received,
    int64_t& total_bytes_to_be_received) const {
  NOTIMPLEMENTED();
  return false;
}

// Modeled on `ppapi::proxy::URLLoaderResource::ReadResponseBody()`.
void BlinkUrlLoader::ReadResponseBody(base::span<char> buffer,
                                      ResultCallback callback) {
  // Can be in `kLoadComplete` if still reading after loading finished.
  DCHECK(state_ == LoadingState::kStreamingData ||
         state_ == LoadingState::kLoadComplete)
      << static_cast<int>(state_);

  if (buffer.empty()) {
    std::move(callback).Run(PP_ERROR_BADARGUMENT);
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
void BlinkUrlLoader::Close() {
  if (state_ != LoadingState::kLoadComplete)
    AbortLoad(PP_ERROR_ABORTED);
}

bool BlinkUrlLoader::WillFollowRedirect(
    const blink::WebURL& new_url,
    const blink::WebURLResponse& redirect_response) {
  NOTIMPLEMENTED();
  return false;
}

void BlinkUrlLoader::DidSendData(uint64_t bytes_sent,
                                 uint64_t total_bytes_to_be_sent) {
  // Doesn't apply to PDF viewer requests.
  NOTREACHED();
}

// Modeled on `content::PepperURLLoaderHost::DidReceiveResponse()`.
void BlinkUrlLoader::DidReceiveResponse(const blink::WebURLResponse& response) {
  DCHECK_EQ(state_, LoadingState::kOpening);

  mutable_response().status_code = response.HttpStatusCode();

  state_ = LoadingState::kStreamingData;
  std::move(open_callback_).Run(PP_OK);
}

void BlinkUrlLoader::DidDownloadData(uint64_t data_length) {
  // Doesn't apply to PDF viewer requests.
  NOTREACHED();
}

// Modeled on `content::PepperURLLoaderHost::DidReceiveData()`.
void BlinkUrlLoader::DidReceiveData(const char* data, int data_length) {
  DCHECK_EQ(state_, LoadingState::kStreamingData);

  // It's surprisingly difficult to guarantee that this is always >0.
  if (data_length < 1)
    return;

  buffer_.insert(buffer_.end(), data, data + data_length);
  RunReadCallback();
}

void BlinkUrlLoader::DidReceiveCachedMetadata(const char* data,
                                              int data_length) {
  // Doesn't apply to PDF viewer requests.
  NOTREACHED();
}

// Modeled on `content::PepperURLLoaderHost::DidFinishLoading()`.
void BlinkUrlLoader::DidFinishLoading() {
  DCHECK_EQ(state_, LoadingState::kStreamingData);

  SetLoadComplete(PP_OK);
  RunReadCallback();
}

// Modeled on `content::PepperURLLoaderHost::DidFail()`.
void BlinkUrlLoader::DidFail(const blink::WebURLError& error) {
  DCHECK(state_ == LoadingState::kOpening ||
         state_ == LoadingState::kStreamingData)
      << static_cast<int>(state_);

  int32_t pp_error = PP_ERROR_FAILED;
  switch (error.reason()) {
    case net::ERR_ACCESS_DENIED:
    case net::ERR_NETWORK_ACCESS_DENIED:
      pp_error = PP_ERROR_NOACCESS;
      break;

    default:
      if (error.is_web_security_violation())
        pp_error = PP_ERROR_NOACCESS;
      break;
  }

  AbortLoad(pp_error);
}

void BlinkUrlLoader::AbortLoad(int32_t result) {
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
void BlinkUrlLoader::RunReadCallback() {
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
  } else {
    DCHECK_EQ(state_, LoadingState::kLoadComplete);
    num_bytes = complete_result_;
    DCHECK_LE(num_bytes, 0);
    static_assert(PP_OK == 0, "PP_OK should be equivalent to 0 bytes");
  }

  client_buffer_ = {};
  std::move(read_callback_).Run(num_bytes);
}

void BlinkUrlLoader::SetLoadComplete(int32_t result) {
  DCHECK_NE(state_, LoadingState::kLoadComplete);
  DCHECK_LE(result, 0);

  state_ = LoadingState::kLoadComplete;
  complete_result_ = result;
  blink_loader_.reset();
}

PepperUrlLoader::PepperUrlLoader(pp::InstanceHandle plugin_instance)
    : plugin_instance_(plugin_instance), pepper_loader_(plugin_instance) {}

PepperUrlLoader::~PepperUrlLoader() = default;

void PepperUrlLoader::GrantUniversalAccess() {
  const PPB_URLLoaderTrusted* trusted_interface =
      static_cast<const PPB_URLLoaderTrusted*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_URLLOADERTRUSTED_INTERFACE));
  if (trusted_interface)
    trusted_interface->GrantUniversalAccess(pepper_loader_.pp_resource());
}

void PepperUrlLoader::Open(const UrlRequest& request, ResultCallback callback) {
  pp::URLRequestInfo pp_request(plugin_instance_);
  pp_request.SetURL(request.url);
  pp_request.SetMethod(request.method);

  if (request.ignore_redirects)
    pp_request.SetFollowRedirects(false);

  if (request.custom_referrer_url.has_value())
    pp_request.SetCustomReferrerURL(request.custom_referrer_url.value());

  if (request.headers.has_value())
    pp_request.SetHeaders(request.headers.value());

  if (!request.body.empty())
    pp_request.AppendDataToBody(request.body.data(), request.body.size());

  pp::CompletionCallback pp_callback = PPCompletionCallbackFromResultCallback(
      base::BindOnce(&PepperUrlLoader::DidOpen, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
  int32_t result = pepper_loader_.Open(pp_request, pp_callback);
  if (result != PP_OK_COMPLETIONPENDING)
    pp_callback.Run(result);
}

bool PepperUrlLoader::GetDownloadProgress(
    int64_t& bytes_received,
    int64_t& total_bytes_to_be_received) const {
  return pepper_loader_.GetDownloadProgress(&bytes_received,
                                            &total_bytes_to_be_received);
}

void PepperUrlLoader::ReadResponseBody(base::span<char> buffer,
                                       ResultCallback callback) {
  pp::CompletionCallback pp_callback =
      PPCompletionCallbackFromResultCallback(std::move(callback));
  int32_t result = pepper_loader_.ReadResponseBody(buffer.data(), buffer.size(),
                                                   pp_callback);
  if (result != PP_OK_COMPLETIONPENDING)
    pp_callback.Run(result);
}

void PepperUrlLoader::Close() {
  pepper_loader_.Close();
}

void PepperUrlLoader::DidOpen(ResultCallback callback, int32_t result) {
  pp::URLResponseInfo pp_response = pepper_loader_.GetResponseInfo();
  mutable_response().status_code = pp_response.GetStatusCode();

  pp::Var headers_var = pp_response.GetHeaders();
  if (headers_var.is_string()) {
    mutable_response().headers = headers_var.AsString();
  } else {
    mutable_response().headers.reset();
  }

  std::move(callback).Run(result);
}

}  // namespace chrome_pdf
